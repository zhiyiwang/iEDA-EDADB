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
        parent_ddl = ParentSql::createTableStatement(*parent_def);
        child_ddl = ChildSql::createTableStatement(*child_def);
        // Only normalize the unrelated NULL constraint; preserve generated names/types/FK.
        if (mode != "pk") {
            const std::string prefix = "\"pin_name\" TEXT";
            auto position = child_ddl.find(prefix);
            require(position != std::string::npos, "pin_name declaration");
            child_ddl.insert(position + prefix.size(), " NOT NULL");
        }
        std::vector<std::string> keys;
        child_def->getPrimKeyColumnNames(keys);
        require(keys.size() == 2 && keys.back() == "pin_name", "parent/local key path");
        foreign_key = keys.front();
        if (mode == "index") index_ddl = "CREATE INDEX " + quote(child + "__owner_name_idx") +
            " ON " + quote(child) + "(" + quote(foreign_key) + ",\"pin_name\");";
        parent_insert = ParentSql::insertPlaceHolderStatement(*parent_def);
        child_insert = ChildSql::insertPlaceHolderStatement(*child_def);
        parent_read = ParentSql::readAllStatement(*parent_def);
        child_read = ChildSql::queryForeignKeyStatement(*child_def);
        child_scan = ChildSql::readAllStatement(*child_def);
    }
    void create(sqlite3* db) const {
        exec(db, "BEGIN"); exec(db, parent_ddl); exec(db, child_ddl);
        if (!index_ddl.empty()) exec(db, index_ddl);
        exec(db, "COMMIT");
    }
};

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
void bind_parent(Statement& statement, const ComponentRecord& record) {
    statement.text(1, record.name); statement.text(2, record.master_name);
    statement.number(3, record.source); statement.number(4, record.status); statement.number(5, record.orient);
    statement.number(6, record.x); statement.number(7, record.y); statement.number(8, record.record_order);
}
void bind_pin(Statement& statement, const PinRecord& record, const std::string& owner) {
    statement.text(1, record.pin_name); statement.number(2, record.direction);
    statement.number(3, record.x); statement.number(4, record.y); statement.text(5, owner);
}
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
struct Trace {
    uint64_t statements = 0;
    static int callback(unsigned, void* data, void* handle, void*) {
        auto* trace = static_cast<Trace*>(data);
        const char* sql = sqlite3_sql(static_cast<sqlite3_stmt*>(handle));
        if (sql && std::string_view(sql).substr(0, 6) == "SELECT") ++trace->statements;
        return 0;
    }
};
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

struct Sample {
    double init = 0, create = -1, begin = -1, data = 0, commit = -1, close = -1;
    void print(const std::string& operation, const Consumer& consumer) const {
        std::cout << std::fixed << std::setprecision(6) << "RESULT\t" << operation << '\t'
                  << init << '\t' << create << '\t' << begin << '\t' << data << '\t' << commit << '\t' << close << '\t'
                  << data + std::max(begin, 0.0) + std::max(commit, 0.0) << '\t'
                  << consumer.parents << '\t' << consumer.children << '\t' << consumer.digest << '\n';
    }
};

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
