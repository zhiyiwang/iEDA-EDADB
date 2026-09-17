// Two-table benchmark only: no iEDA adapter or production core changes.
// Build once; choose schema/route in a fresh process for each sample.
#include "edadb.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

// 数据模型：ComponentRecord是父对象，pins保存其子对象。
// PinRecord不重复保存父name；写入时由bind_pin或EDADB递归上下文提供FK。
// fields()仅用于逐值校验，不包含父子vector关系。
struct PinRecord {
    std::string pin_name;
    int32_t direction = 0, x = 0, y = 0;
    auto fields() const { return std::tie(pin_name, direction, x, y); }
};
struct ComponentRecord {
    std::string name, master_name;
    int32_t source = 0, status = 0, orient = 0, x = 0, y = 0;
    int64_t record_order = 0;
    std::vector<PinRecord> pins;
    auto fields() const { return std::tie(name, master_name, source, status, orient, x, y, record_order); }
};
// 映射声明决定列及子表关系；Schema在首次获取元数据前设置主键模式。
// 子表实际名称组合父表名、vector成员名及子类型表名。
TABLE4CLASS(PinRecord, "instance_pin", (pin_name, direction, x, y));
TABLE4CLASS_WVEC(ComponentRecord, "component",
    (name, master_name, source, status, orient, x, y, record_order), (pins));

using Clock = std::chrono::steady_clock;
void require(bool valid, const std::string& message) {
    if (!valid) throw std::runtime_error(message);
}
double elapsed(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
std::string quote(const std::string& name) { return "\"" + name + "\""; }
void exec(sqlite3* db, const std::string& sql) {
    char* error = nullptr;
    int status = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error);
    std::string message = error ? error : "";
    sqlite3_free(error);
    require(status == SQLITE_OK, sql + ": " + message);
}

// 手写SQLite路径的statement封装：构造时prepare，finish时finalize；
// 析构是异常路径的清理兜底。text/number绑定参数，insert执行并reset，
// next执行SELECT的一次step。父子语句在循环外构造，不逐父重新prepare。
struct Statement {
    sqlite3_stmt* handle = nullptr;
    Statement(sqlite3* db, const std::string& sql) {
        require(sqlite3_prepare_v2(db, sql.c_str(), -1, &handle, nullptr) == SQLITE_OK,
                "prepare: " + sql + ": " + sqlite3_errmsg(db));
    }
    Statement(const Statement&) = delete;
    ~Statement() { if (handle) sqlite3_finalize(handle); }
    void finish() {
        int status = sqlite3_finalize(handle);
        handle = nullptr;
        require(status == SQLITE_OK, "finalize");
    }
    void text(int column, const std::string& value) {
        require(sqlite3_bind_text(handle, column, value.data(), static_cast<int>(value.size()),
                                 SQLITE_TRANSIENT) == SQLITE_OK, "bind text");
    }
    void number(int column, int64_t value) {
        require(sqlite3_bind_int64(handle, column, value) == SQLITE_OK, "bind integer");
    }
    void insert() {
        require(sqlite3_step(handle) == SQLITE_DONE, "INSERT: " + std::string(sqlite3_errmsg(sqlite3_db_handle(handle))));
        require(sqlite3_reset(handle) == SQLITE_OK, "reset INSERT");
    }
    bool next() {
        int status = sqlite3_step(handle);
        require(status == SQLITE_ROW || status == SQLITE_DONE, "SELECT step");
        return status == SQLITE_ROW;
    }
};
std::string column_text(sqlite3_stmt* statement, int column) {
    const auto* data = sqlite3_column_text(statement, column);
    require(data != nullptr, "unexpected NULL text");
    return {reinterpret_cast<const char*>(data), static_cast<size_t>(sqlite3_column_bytes(statement, column))};
}

// 保存两条路径共用的DDL和DML文本，不在构造时执行建表。
// 构造：EDADB元数据 -> SQL生成 -> 实验模式调整；create才执行建表事务。
struct Schema {
    std::string parent, child, foreign_key, parent_ddl, child_ddl, index_ddl;
    std::string parent_insert, child_insert, parent_read, child_read, child_scan;
    explicit Schema(const std::string& mode) {
        // Traits must be set before any table metadata is cached.
        edadb::Cpp2SqlTypeTrait<ComponentRecord>::hasPrimKey = true;
        edadb::Cpp2SqlTypeTrait<PinRecord>::hasPrimKey = mode == "pk";
        const auto* parent_def = dynamic_cast<const edadb::DbTableDef<ComponentRecord>*>(edadb::getTableDef<ComponentRecord>());
        require(parent_def && parent_def->getChildCount() == 1, "exactly one child table");
        const auto* child_def = dynamic_cast<const edadb::DbTableDef<PinRecord>*>(parent_def->getChild(0));
        require(child_def != nullptr, "child metadata type");
        parent = parent_def->getTableName(); child = child_def->getTableName();
        using ParentSql = edadb::SqlStatement<ComponentRecord>;
        using ChildSql = edadb::SqlStatement<PinRecord>;
        // 实际生成的父表DDL（三种模式相同；这里只换行展示）：
        // CREATE TABLE "component" (
        //   "name" TEXT NOT NULL, "master_name" TEXT, "source" INTEGER,
        //   "status" INTEGER, "orient" INTEGER, "x" INTEGER, "y" INTEGER,
        //   "record_order" BIGINT, PRIMARY KEY ("name"));
        parent_ddl = ParentSql::createTableStatement(*parent_def);
        child_ddl = ChildSql::createTableStatement(*child_def);
        // Only normalize the unrelated NULL constraint; preserve generated names/types/FK.
        if (mode != "pk") {
            const std::string prefix = "\"pin_name\" TEXT";
            auto position = child_ddl.find(prefix);
            require(position != std::string::npos, "pin_name declaration");
            child_ddl.insert(position + prefix.size(), " NOT NULL");
        }
        // 上述生成及NOT NULL处理后的子表，index/none模式为：
        // CREATE TABLE "component_pins_instance_pin" (
        //   "pin_name" TEXT NOT NULL, "direction" INTEGER, "x" INTEGER,
        //   "y" INTEGER, "component_name" TEXT NOT NULL,
        //   FOREIGN KEY ("component_name") REFERENCES "component" ("name")
        //   ON DELETE CASCADE ON UPDATE CASCADE);
        // pk模式在FOREIGN KEY前增加 PRIMARY KEY ("component_name", "pin_name")；
        // index模式不声明子表主键，另建普通索引；none不建子表索引。
        std::vector<std::string> keys;
        child_def->getPrimKeyColumnNames(keys);
        require(keys.size() == 2 && keys.back() == "pin_name", "parent/local key path");
        foreign_key = keys.front();
        // 仅index模式：CREATE INDEX "component_pins_instance_pin__owner_name_idx"
        // ON "component_pins_instance_pin"("component_name","pin_name");
        if (mode == "index") index_ddl = "CREATE INDEX " + quote(child + "__owner_name_idx") +
            " ON " + quote(child) + "(" + quote(foreign_key) + ",\"pin_name\");";
        // INSERT INTO "component" ("name", "master_name", "source", "status",
        //   "orient", "x", "y", "record_order") VALUES (?, ?, ?, ?, ?, ?, ?, ?);
        parent_insert = ParentSql::insertPlaceHolderStatement(*parent_def);
        // INSERT INTO "component_pins_instance_pin"
        //   ("pin_name", "direction", "x", "y", "component_name") VALUES (?, ?, ?, ?, ?);
        child_insert = ChildSql::insertPlaceHolderStatement(*child_def);
        // SELECT "name", "master_name", "source", "status", "orient", "x", "y",
        //   "record_order" FROM "component";
        parent_read = ParentSql::readAllStatement(*parent_def);
        // SELECT "pin_name", "direction", "x", "y", "component_name"
        //   FROM "component_pins_instance_pin" WHERE "component_name" = ?;
        // 参数绑定当前父name；图读取复用此语句，每个父执行一次，无ORDER BY。
        child_read = ChildSql::queryForeignKeyStatement(*child_def);
        // SELECT "pin_name", "direction", "x", "y", "component_name"
        //   FROM "component_pins_instance_pin";
        // flat对照使用此全表扫描，不恢复父子对象关系。
        child_scan = ChildSql::readAllStatement(*child_def);
    }
    void create(sqlite3* db) const {
        // BEGIN -> 父CREATE TABLE -> 子CREATE TABLE -> 可选CREATE INDEX -> COMMIT。
        // 这里只提交建表事务；main另用BEGIN/COMMIT包住全部父子数据写入。
        exec(db, "BEGIN"); exec(db, parent_ddl); exec(db, child_ddl);
        if (!index_ddl.empty()) exec(db, index_ddl);
        exec(db, "COMMIT");
    }
};

// 管理连接生命周期：SQLite直接open，EDADB通过DbManager取得同一后端句柄。
// memory设实验A参数；disk保留构建默认配置，二者均显式设置FK开关。
// close必须与打开连接的API配对；内存库读完前不能关闭，否则数据消失。
struct Connection {
    bool edadb_route, memory, foreign_keys;
    std::string path;
    sqlite3* db = nullptr;
    Connection(bool route, bool in_memory, bool fk, std::string file)
        : edadb_route(route), memory(in_memory), foreign_keys(fk), path(std::move(file)) {}
    void open() {
        require(db == nullptr, "already open");
        if (edadb_route) {
            require(edadb::initDatabase(memory ? ":memory:" : path), "EDADB connect");
            edadb::DbStatement probe;
            require(edadb::DbManager::i().initStatement(probe), "EDADB connection handle");
            db = probe.db;
        } else require(sqlite3_open(memory ? ":memory:" : path.c_str(), &db) == SQLITE_OK, "SQLite connect");
        if (memory) exec(db, "PRAGMA journal_mode=MEMORY; PRAGMA synchronous=OFF; PRAGMA temp_store=MEMORY; PRAGMA cache_size=-8192; PRAGMA mmap_size=0;");
        exec(db, std::string("PRAGMA foreign_keys=") + (foreign_keys ? "ON" : "OFF"));
    }
    void close() {
        require(db != nullptr, "not open");
        bool success = edadb_route ? edadb::closeDatabase() : sqlite3_close(db) == SQLITE_OK;
        require(success, "close"); db = nullptr;
    }
    ~Connection() { if (db) { if (edadb_route) edadb::closeDatabase(); else sqlite3_close_v2(db); } }
};

std::string name_at(char prefix, size_t index, int width) {
    std::ostringstream output; output << prefix << std::setw(width) << std::setfill('0') << index;
    return output.str();
}
// 计时外生成确定性输入：regular每父固定子数；mixed包含空/单/多子并反转顺序。
// mixed用于检查恢复关系而非依赖查询返回顺序，不是主性能矩阵的数据分布。
std::vector<ComponentRecord> generate(size_t parents, size_t children, bool mixed) {
    std::vector<ComponentRecord> input;
    input.reserve(parents);
    for (size_t index = 0; index < parents; ++index) {
        ComponentRecord record;
        record.name = name_at('U', index, 7); record.master_name = "bench_cell";
        record.source = 2; record.status = 3; record.orient = 1;
        record.x = (index % 1000) * 100; record.y = (index / 1000) * 100; record.record_order = index;
        size_t count = mixed ? (index % 3 == 0 ? 0 : (index % 3 == 1 ? 1 : children)) : children;
        for (size_t pin = 0; pin < count; ++pin)
            record.pins.push_back({name_at('P', pin, 4), 1, record.x + static_cast<int32_t>(10 * (pin + 1)), record.y});
        if (mixed) std::reverse(record.pins.begin(), record.pins.end());
        input.push_back(std::move(record));
    }
    if (mixed) std::reverse(input.begin(), input.end());
    return input;
}

// 消费读取结果：所有模式累加数量和轻量digest，这部分包含在read计时内。
// 只有check模式设置expected并执行逐字段、去重和排序校验；其耗时不作性能结果。
// digest与计数不能替代完整正确性检查；完整检查在独立check运行中完成。
struct Consumer {
    uint64_t parents = 0, children = 0, digest = 0;
    const std::map<std::string, ComponentRecord>* expected = nullptr;
    std::map<std::string, bool> seen;
    void parent(const ComponentRecord& record) {
        ++parents;
        digest += record.name.size() + record.master_name.size() + record.source + record.status +
                  record.orient + record.x + record.y + record.record_order;
    }
    void pin(const PinRecord& record, size_t owner_size) {
        ++children; digest += owner_size + record.pin_name.size() + record.direction + record.x + record.y;
    }
    void accept(const ComponentRecord& record) {
        parent(record);
        for (const auto& child : record.pins) pin(child, record.name.size());
        if (!expected) return;
        const auto& reference = expected->at(record.name);
        require(!seen[record.name], "duplicate returned parent"); seen[record.name] = true;
        require(record.fields() == reference.fields(), "parent fields");
        auto actual = record.pins, correct = reference.pins;
        auto less = [](const auto& first, const auto& second) { return first.pin_name < second.pin_name; };
        std::sort(actual.begin(), actual.end(), less); std::sort(correct.begin(), correct.end(), less);
        require(actual.size() == correct.size(), "child count");
        for (size_t index = 0; index < actual.size(); ++index)
            require(actual[index].fields() == correct[index].fields(), "child fields");
    }
};
// 参数序号从1开始，与Schema中的INSERT列序一致；子表最后一列绑定所属父name。
void bind_parent(Statement& statement, const ComponentRecord& record) {
    statement.text(1, record.name); statement.text(2, record.master_name);
    statement.number(3, record.source); statement.number(4, record.status); statement.number(5, record.orient);
    statement.number(6, record.x); statement.number(7, record.y); statement.number(8, record.record_order);
}
void bind_pin(Statement& statement, const PinRecord& record, const std::string& owner) {
    statement.text(1, record.pin_name); statement.number(2, record.direction);
    statement.number(3, record.x); statement.number(4, record.y); statement.text(5, owner);
}
// main已开启数据事务；此函数只负责prepare/逐条插入/finalize，不提交事务。
// SQLite：bind_parent -> insert父 -> bind_pin -> insert各子，复用两条statement。
// EDADB：makeInsertOp -> insert父对象，框架递归处理pins；op析构计入write。
void write_data(Connection& connection, const Schema& schema, std::vector<ComponentRecord>& input) {
    if (connection.edadb_route) {
        auto writer = edadb::makeInsertOp<ComponentRecord>();
        for (auto& record : input) require(writer.insert(&record) >= 0, "EDADB insert graph");
    } else {
        Statement parent(connection.db, schema.parent_insert), child(connection.db, schema.child_insert);
        for (const auto& record : input) {
            bind_parent(parent, record); parent.insert();
            for (const auto& pin : record.pins) { bind_pin(child, pin, record.name); child.insert(); }
        }
        child.finish(); parent.finish();
    }
}
// SELECT列下标从0开始；字符串复制进Record，不能跨step保存SQLite返回的借用指针。
// fetch_parent/fetch_pin只取字段；父子关联由read_data的当前父对象确定。
void fetch_parent(sqlite3_stmt* statement, ComponentRecord& record) {
    record.name = column_text(statement, 0); record.master_name = column_text(statement, 1);
    record.source = sqlite3_column_int(statement, 2); record.status = sqlite3_column_int(statement, 3);
    record.orient = sqlite3_column_int(statement, 4); record.x = sqlite3_column_int(statement, 5);
    record.y = sqlite3_column_int(statement, 6); record.record_order = sqlite3_column_int64(statement, 7);
}
PinRecord fetch_pin(sqlite3_stmt* statement) {
    return {column_text(statement, 0), sqlite3_column_int(statement, 1),
            sqlite3_column_int(statement, 2), sqlite3_column_int(statement, 3)};
}
// 图读取恢复一个父及其子vector，然后交给Consumer；不累计完整结果树。
// SQLite：父next -> fetch_parent -> 子绑定父name -> 子next/fetch_pin -> reset。
// EDADB：makeReadAllOp -> readNext递归恢复同类对象 -> Consumer::accept。
// 共1次父SELECT加每父1次子SELECT；复用statement不等于消除了N+1查询。
void read_data(Connection& connection, const Schema& schema, Consumer& consumer) {
    ComponentRecord record;
    if (connection.edadb_route) {
        auto reader = edadb::makeReadAllOp<ComponentRecord>();
        int status;
        while ((status = edadb::readNext(reader, &record)) > 0) consumer.accept(record);
        require(status == 0, "EDADB read graph");
    } else {
        Statement parent(connection.db, schema.parent_read), child(connection.db, schema.child_read);
        while (parent.next()) {
            fetch_parent(parent.handle, record);
            // Materialize one parent's children, like the core's staged child-vector fetch.
            std::vector<PinRecord> pins;
            child.text(1, record.name);
            while (child.next()) pins.push_back(fetch_pin(child.handle));
            require(sqlite3_reset(child.handle) == SQLITE_OK, "reset child SELECT");
            record.pins = std::move(pins);
            consumer.accept(record);
        }
        child.finish(); parent.finish();
    }
}
// 仅SQLite的补充对照：分别扫描父、子两张表，读取FK但不组装pins。
// 因输出对象结构与graph不同，时间差不能全部解释成N+1本身的开销。
void read_flat(Connection& connection, const Schema& schema, Consumer& consumer,
               const std::map<std::string, ComponentRecord>* expected) {
    Statement parent(connection.db, schema.parent_read), child(connection.db, schema.child_scan);
    ComponentRecord record;
    while (parent.next()) {
        fetch_parent(parent.handle, record); consumer.parent(record);
        if (expected) require(record.fields() == expected->at(record.name).fields(), "flat parent fields");
    }
    std::map<std::pair<std::string, std::string>, bool> seen;
    while (child.next()) {
        auto pin = fetch_pin(child.handle);
        auto owner = column_text(child.handle, 4);
        consumer.pin(pin, owner.size());
        if (expected) {
            const auto& pins = expected->at(owner).pins;
            auto found = std::find_if(pins.begin(), pins.end(), [&](const auto& value) { return value.pin_name == pin.pin_name; });
            require(found != pins.end() && found->fields() == pin.fields(), "flat child fields");
            require(!seen[{owner, pin.pin_name}], "flat duplicate child"); seen[{owner, pin.pin_name}] = true;
        }
    }
    child.finish(); parent.finish();
}

// 计时区间之外查询并输出配置/结构证据，供runner和审计程序核对实际运行条件。
void evidence(sqlite3* db, const std::string& label, const std::string& sql) {
    Statement query(db, sql);
    while (query.next()) {
        std::cout << "EVIDENCE\t" << label;
        for (int column = 0; column < sqlite3_column_count(query.handle); ++column) {
            std::cout << '\t';
            if (sqlite3_column_type(query.handle, column) == SQLITE_NULL) std::cout << "<NULL>";
            else std::cout << column_text(query.handle, column);
        }
        std::cout << '\n';
    }
    query.finish();
}
// 仅check运行启用，统计SELECT执行次数；正式perf不注册此回调。
struct Trace {
    uint64_t statements = 0;
    static int callback(unsigned, void* data, void* handle, void*) {
        auto* trace = static_cast<Trace*>(data);
        const char* sql = sqlite3_sql(static_cast<sqlite3_stmt*>(handle));
        if (sql && std::string_view(sql).substr(0, 6) == "SELECT") ++trace->statements;
        return 0;
    }
};
// check读取后核验实际DDL、列、索引、FK和查询计划，不混入正式read计时。
void inspect(sqlite3* db, const Schema& schema) {
    evidence(db, "schema", "SELECT type,name,tbl_name,sql FROM sqlite_schema WHERE name NOT LIKE 'sqlite_%' ORDER BY type,name");
    evidence(db, "parent_columns", "PRAGMA table_info(" + quote(schema.parent) + ")");
    evidence(db, "child_columns", "PRAGMA table_info(" + quote(schema.child) + ")");
    evidence(db, "parent_indexes", "PRAGMA index_list(" + quote(schema.parent) + ")");
    evidence(db, "child_indexes", "PRAGMA index_list(" + quote(schema.child) + ")");
    Statement indexes(db, "PRAGMA index_list(" + quote(schema.child) + ")");
    while (indexes.next()) evidence(db, "index_columns", "PRAGMA index_info(" + quote(column_text(indexes.handle, 1)) + ")");
    indexes.finish();
    evidence(db, "foreign_key", "PRAGMA foreign_key_list(" + quote(schema.child) + ")");
    evidence(db, "plan_child", "EXPLAIN QUERY PLAN " + schema.child_read);
    evidence(db, "plan_scan", "EXPLAIN QUERY PLAN " + schema.child_scan);
    Statement violation(db, "PRAGMA foreign_key_check"); require(!violation.next(), "foreign key check"); violation.finish();
}
int64_t row_count(sqlite3* db, const std::string& table) {
    Statement query(db, "SELECT count(*) FROM " + quote(table)); require(query.next(), "count");
    int64_t count = sqlite3_column_int64(query.handle, 0); query.finish(); return count;
}
// 独立检查缺失父、重复子键及API重复父失败；每次ROLLBACK后核对行数不变。
// 预期行为依FK开关及pk/index/none模式变化，不把非法记录用于性能样本。
void constraints(Connection& connection, const Schema& schema, const std::string& mode) {
    auto parents = row_count(connection.db, schema.parent), children = row_count(connection.db, schema.child);
    auto run = [&](bool orphan) {
        exec(connection.db, "BEGIN");
        int result;
        {
            Statement pin(connection.db, schema.child_insert);
            bind_pin(pin, {"duplicate", 1, 2, 3}, orphan ? "missing-owner" : "constraint-owner");
            if (!orphan) {
                Statement parent(connection.db, schema.parent_insert);
                ComponentRecord record; record.name = "constraint-owner"; record.master_name = "bench_cell";
                bind_parent(parent, record); parent.insert(); parent.finish();
                pin.insert();
            }
            result = sqlite3_step(pin.handle);
            // Failed statements finalize with their prior error; RAII cleanup intentionally handles that case.
        }
        exec(connection.db, "ROLLBACK");
        bool rejected = orphan ? connection.foreign_keys : mode == "pk";
        require(rejected ? (result & 255) == SQLITE_CONSTRAINT : result == SQLITE_DONE, "constraint outcome");
        require(row_count(connection.db, schema.parent) == parents && row_count(connection.db, schema.child) == children, "rollback row counts");
    };
    run(true); run(false);
    // Also exercise the chosen graph-writing API's duplicate-parent failure and rollback.
    exec(connection.db, "BEGIN");
    auto duplicate = generate(1, 2, false); duplicate[0].name = "new-constraint-owner";
    duplicate.push_back(duplicate.front());
    bool failed = false;
    try { write_data(connection, schema, duplicate); } catch (const std::exception&) { failed = true; }
    exec(connection.db, "ROLLBACK");
    require(failed && row_count(connection.db, schema.parent) == parents && row_count(connection.db, schema.child) == children, "API failure rollback");
    std::cout << "CHECK\tconstraints_and_rollback\tPASS\n";
}

// 保存一个阶段的毫秒计时；-1表示不适用。print供Python汇总，不在热点循环输出。
// write complete = begin + data + commit，排除init/create/close；
// read没有显式BEGIN/COMMIT，因此对应合计就是read data。
struct Sample {
    double init = 0, create = -1, begin = -1, data = 0, commit = -1, close = -1;
    void print(const std::string& operation, const Consumer& consumer) const {
        std::cout << std::fixed << std::setprecision(6) << "RESULT\t" << operation << '\t'
                  << init << '\t' << create << '\t' << begin << '\t' << data << '\t' << commit << '\t' << close << '\t'
                  << data + std::max(begin, 0.0) + std::max(commit, 0.0) << '\t'
                  << consumer.parents << '\t' << consumer.children << '\t' << consumer.digest << '\n';
    }
};

// 单次样本入口（主矩阵、重复次数和统计由run_pk_fk.py调度）：
// 参数 -> generate/预期结果 -> Schema/连接 -> create -> BEGIN/write_data/COMMIT
// -> graph或flat读取 -> 输出Sample；check额外执行inspect/constraints。
// 各阶段独立粗粒度计时，无逐行时钟；不同schema通过独立进程运行，隔离元数据缓存。
int main(int argc, char** argv) {
    try {
        std::map<std::string, std::string> options{{"--foreign-keys", "on"}, {"--mode", "perf"}, {"--fixture", "regular"}, {"--read", "graph"}};
        require(argc % 2 == 1, "arguments are --key value pairs");
        for (int index = 1; index < argc; index += 2) options[argv[index]] = argv[index + 1];
        auto route = options.at("--route"), mode = options.at("--schema"), storage = options.at("--storage");
        bool use_edadb = route == "edadb", memory = storage == "memory", check = options.at("--mode") == "check";
        bool foreign_keys = options.at("--foreign-keys") == "on", flat = options.at("--read") == "flat";
        require(route == "sqlite" || use_edadb, "route");
        require(mode == "pk" || mode == "index" || mode == "none", "schema");
        require(storage == "disk" || memory, "storage"); require(!flat || !use_edadb, "flat is SQLite only");
        require(options.at("--mode") == "perf" || check, "mode");
        require(options.at("--foreign-keys") == "on" || options.at("--foreign-keys") == "off", "FK mode");
        size_t parent_count = std::stoull(options.at("--parents")), pin_count = std::stoull(options.at("--children-per-parent"));
        require(parent_count <= 1000000 && pin_count <= 1000, "data bounds");
        // 数据和校验参照在init计时前准备；perf只构建输入与预期数量/digest。
        auto input = generate(parent_count, pin_count, options.at("--fixture") == "mixed");
        Consumer expected;
        std::map<std::string, ComponentRecord> reference;
        for (const auto& record : input) { expected.accept(record); if (check) reference.emplace(record.name, record); }
        std::cout << "INPUT\t" << expected.parents << '\t' << expected.children << '\t' << expected.digest << '\n';
        std::string path = options.at("--db");
        require(memory || !std::filesystem::exists(path), "fresh database path required");
        Connection connection(use_edadb, memory, foreign_keys, path);
        Sample write;
        auto start = Clock::now();
        Schema schema(mode); connection.open(); write.init = elapsed(start);
        for (const auto& parameter : {"journal_mode", "synchronous", "cache_size", "temp_store", "mmap_size", "foreign_keys", "page_size", "encoding"})
            evidence(connection.db, "config_" + std::string(parameter), "PRAGMA " + std::string(parameter));
        evidence(connection.db, "sqlite_version", "SELECT sqlite_version(),sqlite_source_id()");
        if (check) evidence(connection.db, "compile_options", "PRAGMA compile_options");
        std::cout << "SQL\tparent_insert\t" << schema.parent_insert << "\nSQL\tchild_insert\t" << schema.child_insert
                  << "\nSQL\tparent_read\t" << schema.parent_read << "\nSQL\tchild_read\t" << schema.child_read
                  << "\nSQL\tchild_scan\t" << schema.child_scan << '\n';
        // 建表、数据BEGIN、插入及COMMIT各自计时，避免把一次性建表成本算入write data。
        start = Clock::now(); schema.create(connection.db); write.create = elapsed(start);
        start = Clock::now(); exec(connection.db, "BEGIN"); write.begin = elapsed(start);
        start = Clock::now(); write_data(connection, schema, input); write.data = elapsed(start);
        start = Clock::now(); exec(connection.db, "COMMIT"); write.commit = elapsed(start);
        if (!memory) { start = Clock::now(); connection.close(); write.close = elapsed(start); }
        write.print("write", expected);
        if (!memory) {
            // OS-warm file pages, but a new SQLite connection/page cache for the measured read.
            std::ifstream file(path, std::ios::binary); std::vector<char> buffer(1024 * 1024);
            while (file.read(buffer.data(), buffer.size()) || file.gcount()) {}
            require(file.eof(), "prewarm file");
        }
        // memory保留连接读两次；disk先预读文件再新建连接读一次，并非冷盘测试。
        // 两次memory读取均在read_data中重新创建reader，first也不等于冷缓存。
        for (int pass = 0; pass < (memory ? 2 : 1); ++pass) {
            Sample read;
            if (!memory) { start = Clock::now(); connection.open(); read.init = elapsed(start); }
            if (const char* settle = std::getenv("PK_FK_SETTLE"))
                if (!check) std::this_thread::sleep_for(std::chrono::duration<double>(std::stod(settle)));
            Consumer actual; actual.expected = check ? &reference : nullptr;
            Trace trace;
            if (check) require(sqlite3_trace_v2(connection.db, SQLITE_TRACE_STMT, Trace::callback, &trace) == SQLITE_OK, "trace enable");
            start = Clock::now();
            if (flat) read_flat(connection, schema, actual, check ? &reference : nullptr);
            else read_data(connection, schema, actual);
            read.data = elapsed(start);
            if (check) {
                require(sqlite3_trace_v2(connection.db, 0, nullptr, nullptr) == SQLITE_OK, "trace disable");
                uint64_t expected_queries = flat ? 2 : 1 + expected.parents;
                require(trace.statements == expected_queries, "SELECT execution count");
                std::cout << "CHECK\tselect_executions\t" << trace.statements << '\n';
            }
            require(actual.parents == expected.parents && actual.children == expected.children && actual.digest == expected.digest, "read digest/count");
            if (check && (pass == (memory ? 1 : 0))) { inspect(connection.db, schema); constraints(connection, schema, mode); }
            if (pass == (memory ? 1 : 0)) { start = Clock::now(); connection.close(); read.close = elapsed(start); }
            read.print(memory ? (pass == 0 ? "read_first" : "read_repeat") : "read_warm", actual);
        }
        std::cout << "PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n'; return 1;
    }
}
