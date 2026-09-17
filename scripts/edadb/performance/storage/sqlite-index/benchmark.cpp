#include <sqlite3.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

using Clock = std::chrono::steady_clock;
void require(bool ok, std::string_view message) {
    if (!ok) throw std::runtime_error(std::string(message));
}
void execute(sqlite3* database, const std::string& sql) {
    if (sqlite3_exec(database, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK)
        throw std::runtime_error(sql + ": " + sqlite3_errmsg(database));
}
struct Statement {
    sqlite3_stmt* handle = nullptr;
    Statement(sqlite3* database, const std::string& sql) {
        if (sqlite3_prepare_v2(database, sql.c_str(), -1, &handle, nullptr) != SQLITE_OK)
            throw std::runtime_error(sql + ": " + sqlite3_errmsg(database));
    }
    ~Statement() { if (handle) sqlite3_finalize(handle); }
    void finish() {
        int status = sqlite3_finalize(handle);
        handle = nullptr;
        require(status == SQLITE_OK, "finalize");
    }
};
struct Record {
    std::string name, master_name;
    int32_t source, status, orient, x, y;
    int64_t record_order;
    auto fields() const { return std::tie(name, master_name, source, status, orient, x, y, record_order); }
};
// Same eight fields and values as benchmark/benchmark_support.h::generate().
std::vector<Record> generate(size_t count) {
    std::vector<Record> records;
    records.reserve(count);
    for (size_t index = 0; index < count; ++index) {
        std::ostringstream name;
        name << 'U' << std::setfill('0') << std::setw(7) << index;
        records.push_back({name.str(), "bench_cell", 2, 3, 1,
            static_cast<int32_t>(index % 1000 * 100),
            static_cast<int32_t>(index / 1000 * 100), static_cast<int64_t>(index)});
    }
    return records;
}
const std::array<std::string, 8> layouts{
    "none", "index", "unique", "text-pk", "text-pk-without-rowid",
    "integer-unique", "integer-rowid", "integer-without-rowid"};
bool integer_key(const std::string& layout) { return layout.starts_with("integer-"); }
bool without_rowid(const std::string& layout) { return layout.ends_with("without-rowid"); }
const std::string columns = "name,master_name,source,status,orient,x,y,record_order";
const std::string insert_sql = "INSERT INTO component(" + columns + ") VALUES(?,?,?,?,?,?,?,?)";
const std::string select_sql = "SELECT " + columns + " FROM component";
std::string point_sql(const std::string& layout) {
    return select_sql + " WHERE " + (integer_key(layout) ? "record_order" : "name") + "=?";
}
std::string schema(const std::string& layout) {
    std::string order = "BIGINT";
    if (layout == "integer-unique") order = "INTEGER NOT NULL";
    if (layout == "integer-rowid" || layout == "integer-without-rowid") order = "INTEGER PRIMARY KEY";
    std::string sql = "CREATE TABLE component(name TEXT NOT NULL,master_name TEXT,"
        "source INTEGER,status INTEGER,orient INTEGER,x INTEGER,y INTEGER,record_order " + order;
    if (layout == "text-pk" || layout == "text-pk-without-rowid") sql += ",PRIMARY KEY(name)";
    sql += ")";
    if (without_rowid(layout)) sql += " WITHOUT ROWID";
    sql += ";";
    if (layout == "index") sql += "CREATE INDEX component_name_idx ON component(name);";
    if (layout == "unique") sql += "CREATE UNIQUE INDEX component_name_idx ON component(name);";
    if (layout == "integer-unique") sql += "CREATE UNIQUE INDEX component_order_idx ON component(record_order);";
    return sql;
}
void bind_record(sqlite3_stmt* statement, const Record& record) {
    require(sqlite3_bind_text(statement, 1, record.name.data(), record.name.size(), SQLITE_TRANSIENT) == SQLITE_OK, "bind name");
    require(sqlite3_bind_text(statement, 2, record.master_name.data(), record.master_name.size(), SQLITE_TRANSIENT) == SQLITE_OK, "bind master");
    require(sqlite3_bind_int(statement, 3, record.source) == SQLITE_OK, "bind source");
    require(sqlite3_bind_int(statement, 4, record.status) == SQLITE_OK, "bind status");
    require(sqlite3_bind_int(statement, 5, record.orient) == SQLITE_OK, "bind orient");
    require(sqlite3_bind_int(statement, 6, record.x) == SQLITE_OK, "bind x");
    require(sqlite3_bind_int(statement, 7, record.y) == SQLITE_OK, "bind y");
    require(sqlite3_bind_int64(statement, 8, record.record_order) == SQLITE_OK, "bind order");
}
void fetch_record(sqlite3_stmt* statement, Record& record) {
    const auto* name = sqlite3_column_text(statement, 0);
    record.name.assign(reinterpret_cast<const char*>(name), sqlite3_column_bytes(statement, 0));
    const auto* master = sqlite3_column_text(statement, 1);
    record.master_name.assign(reinterpret_cast<const char*>(master), sqlite3_column_bytes(statement, 1));
    record.source = sqlite3_column_int(statement, 2);
    record.status = sqlite3_column_int(statement, 3);
    record.orient = sqlite3_column_int(statement, 4);
    record.x = sqlite3_column_int(statement, 5);
    record.y = sqlite3_column_int(statement, 6);
    record.record_order = sqlite3_column_int64(statement, 7);
}
uint64_t digest(const Record& record) {
    return record.name.size() + record.master_name.size() + static_cast<uint64_t>(record.source)
        + record.status + record.orient + record.x + record.y + record.record_order;
}
struct Consumed {
    uint64_t rows = 0, sum = 0;
    bool operator==(const Consumed&) const = default;
    void accept(const Record& record) { ++rows; sum += digest(record); }
};
// Complete field checks compile out of formal timing runs.
template<bool Verify>
void verify_record(const Record& record, const std::vector<Record>& records) {
    if constexpr (Verify) {
        require(record.record_order >= 0 && static_cast<size_t>(record.record_order) < records.size(), "record bounds");
        require(record.fields() == records[record.record_order].fields(), "field mismatch");
    }
}
void statement_counters(sqlite3_stmt* statement, const char* phase) {
    for (auto [name, operation] : std::array<std::pair<const char*, int>, 5>{{
        {"run", SQLITE_STMTSTATUS_RUN}, {"vm_step", SQLITE_STMTSTATUS_VM_STEP},
        {"fullscan_step", SQLITE_STMTSTATUS_FULLSCAN_STEP}, {"sort", SQLITE_STMTSTATUS_SORT},
        {"reprepare", SQLITE_STMTSTATUS_REPREPARE}}})
        std::cout << "COUNTER\t" << phase << '\t' << name << '\t' << sqlite3_stmt_status(statement, operation, 0) << '\n';
}
template<bool Diagnose>
void write_records(sqlite3* database, const std::vector<Record>& records) {
    Statement statement(database, insert_sql);
    for (const Record& record : records) {
        bind_record(statement.handle, record);
        require(sqlite3_step(statement.handle) == SQLITE_DONE, "insert");
        require(sqlite3_reset(statement.handle) == SQLITE_OK, "insert reset");
    }
    if constexpr (Diagnose) statement_counters(statement.handle, "write");
    statement.finish();
}
template<bool Verify, bool Diagnose>
Consumed scan(sqlite3* database, const std::vector<Record>& records) {
    Statement statement(database, select_sql);
    Record record;
    Consumed result;
    int status;
    while ((status = sqlite3_step(statement.handle)) == SQLITE_ROW) {
        fetch_record(statement.handle, record);
        verify_record<Verify>(record, records);
        result.accept(record);
    }
    require(status == SQLITE_DONE, "scan done");
    if constexpr (Diagnose) statement_counters(statement.handle, "scan");
    statement.finish();
    return result;
}
template<bool Verify, bool Diagnose>
Consumed point_read(sqlite3* database, const std::string& sql, bool integer,
                    const std::vector<Record>& records, const std::vector<size_t>& requests) {
    Statement statement(database, sql);
    Record record;
    Consumed result;
    for (size_t index : requests) {
        const Record& requested = records[index];
        int bound = integer
            ? sqlite3_bind_int64(statement.handle, 1, requested.record_order)
            : sqlite3_bind_text(statement.handle, 1, requested.name.data(), requested.name.size(), SQLITE_TRANSIENT);
        require(bound == SQLITE_OK, "point bind");
        int status;
        size_t found = 0;
        while ((status = sqlite3_step(statement.handle)) == SQLITE_ROW) {
            fetch_record(statement.handle, record);
            verify_record<Verify>(record, records);
            if constexpr (Verify) require(record.fields() == requested.fields(), "wrong lookup result");
            result.accept(record);
            ++found;
        }
        require(status == SQLITE_DONE && found == 1, "lookup row count");
        require(sqlite3_reset(statement.handle) == SQLITE_OK, "lookup reset");
    }
    if constexpr (Diagnose) statement_counters(statement.handle, "point");
    statement.finish();
    return result;
}
int64_t scalar(sqlite3* database, const std::string& sql) {
    Statement statement(database, sql);
    require(sqlite3_step(statement.handle) == SQLITE_ROW, "scalar row");
    int64_t value = sqlite3_column_int64(statement.handle, 0);
    require(sqlite3_step(statement.handle) == SQLITE_DONE, "scalar done");
    statement.finish();
    return value;
}
void dump(sqlite3* database, const std::string& label, const std::string& sql) {
    Statement statement(database, sql);
    int status;
    while ((status = sqlite3_step(statement.handle)) == SQLITE_ROW) {
        std::cout << label;
        for (int column = 0; column < sqlite3_column_count(statement.handle); ++column) {
            auto* value = sqlite3_column_text(statement.handle, column);
            std::cout << '\t' << (value ? reinterpret_cast<const char*>(value) : "NULL");
        }
        std::cout << '\n';
    }
    require(status == SQLITE_DONE, "dump done");
    statement.finish();
}
sqlite3* open(const std::string& path, bool memory) {
    sqlite3* database = nullptr;
    require(sqlite3_open(path.c_str(), &database) == SQLITE_OK, "open");
    if (memory)
        execute(database, "PRAGMA journal_mode=MEMORY;PRAGMA synchronous=OFF;PRAGMA temp_store=MEMORY;"
                          "PRAGMA cache_size=-8192;PRAGMA mmap_size=0;PRAGMA foreign_keys=ON;");
    return database;
}
void close(sqlite3*& database) {
    require(sqlite3_close(database) == SQLITE_OK, "close");
    database = nullptr;
}
// All measured stages use two clocks only. Diagnostics use independent executions.
template<bool Diagnose, class Function>
double measure(sqlite3* database, const char* phase, Function action) {
    constexpr std::array<int, 4> operations{SQLITE_DBSTATUS_CACHE_HIT, SQLITE_DBSTATUS_CACHE_MISS,
        SQLITE_DBSTATUS_CACHE_WRITE, SQLITE_DBSTATUS_CACHE_SPILL};
    constexpr std::array<const char*, 4> names{"cache_hit", "cache_miss", "cache_write", "cache_spill"};
    std::array<int, 4> before{};
    if constexpr (Diagnose) {
        for (size_t index = 0; index < operations.size(); ++index) {
            int highwater;
            require(sqlite3_db_status(database, operations[index], &before[index], &highwater, 0) == SQLITE_OK, "counter before");
        }
        action();
        for (size_t index = 0; index < operations.size(); ++index) {
            int after, highwater;
            require(sqlite3_db_status(database, operations[index], &after, &highwater, 0) == SQLITE_OK, "counter after");
            require(after >= before[index], "counter overflow");
            std::cout << "COUNTER\t" << phase << '\t' << names[index] << '\t' << after - before[index] << '\n';
        }
        return -1;
    } else {
        auto start = Clock::now();
        action();
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }
}
template<class Function> double elapsed(Function action) {
    auto start = Clock::now();
    action();
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
void validate(sqlite3* database, const std::string& layout, size_t count) {
    require(scalar(database, "SELECT count(*) FROM component") == static_cast<int64_t>(count), "count");
    Statement integrity(database, "PRAGMA integrity_check");
    require(sqlite3_step(integrity.handle) == SQLITE_ROW
        && std::string(reinterpret_cast<const char*>(sqlite3_column_text(integrity.handle, 0))) == "ok", "integrity");
    require(sqlite3_step(integrity.handle) == SQLITE_DONE, "integrity end");
    integrity.finish();
    sqlite3_stmt* rowid = nullptr;
    int status = sqlite3_prepare_v2(database, "SELECT rowid FROM component", -1, &rowid, nullptr);
    require(without_rowid(layout) ? status != SQLITE_OK : status == SQLITE_OK, "rowid availability");
    if (rowid) sqlite3_finalize(rowid);
    if (layout == "integer-rowid")
        require(scalar(database, "SELECT count(*) FROM component WHERE rowid!=record_order") == 0, "rowid alias");
    int trees = (layout == "index" || layout == "unique" || layout == "text-pk" || layout == "integer-unique") ? 2 : 1;
    require(scalar(database, "SELECT count(*) FROM sqlite_schema WHERE rootpage>0 AND (name='component' OR tbl_name='component')") == trees, "tree count");
    require(scalar(database, "SELECT count(*) FROM component WHERE name='missing'") == 0, "missing name");
    require(scalar(database, "SELECT count(*) FROM component WHERE record_order=-99") == 0, "missing integer");
    // Each constraint probe uses a savepoint and rolls back even accepted rows.
    auto probe = [&](const std::string& sql, bool succeeds) {
        execute(database, "SAVEPOINT probe");
        int result = sqlite3_exec(database, sql.c_str(), nullptr, nullptr, nullptr);
        require(succeeds ? result == SQLITE_OK : result == SQLITE_CONSTRAINT, "constraint expectation");
        execute(database, "ROLLBACK TO probe;RELEASE probe");
    };
    execute(database, "SAVEPOINT original;DELETE FROM component");
    execute(database, "INSERT INTO component VALUES('probe','bench_cell',2,3,1,0,0,0)");
    probe("INSERT INTO component VALUES('probe','bench_cell',2,3,1,0,0,1)",
          layout == "none" || layout == "index" || integer_key(layout));
    probe("INSERT INTO component VALUES('different','bench_cell',2,3,1,0,0,0)", !integer_key(layout));
    probe("INSERT INTO component VALUES(NULL,'bench_cell',2,3,1,0,0,2)", false);
    probe("INSERT INTO component VALUES('null-order','bench_cell',2,3,1,0,0,NULL)",
          !integer_key(layout) || layout == "integer-rowid");
    execute(database, "ROLLBACK TO original;RELEASE original");
    require(scalar(database, "SELECT count(*) FROM component") == static_cast<int64_t>(count), "rollback count");
}
void evidence(sqlite3* database, const std::string& layout) {
    std::cout << "DDL\t" << schema(layout) << "\nSQL\twrite\t" << insert_sql
              << "\nSQL\tscan\t" << select_sql << "\nSQL\tpoint\t" << point_sql(layout) << '\n';
    dump(database, "SCHEMA", "SELECT type,name,tbl_name,rootpage,sql FROM sqlite_schema ORDER BY name");
    dump(database, "INDEX", "PRAGMA index_list(component)");
    std::vector<std::string> index_names;
    {
        Statement indexes(database, "PRAGMA index_list(component)");
        int status;
        while ((status = sqlite3_step(indexes.handle)) == SQLITE_ROW)
            index_names.emplace_back(reinterpret_cast<const char*>(sqlite3_column_text(indexes.handle, 1)));
        require(status == SQLITE_DONE, "indexes done");
    }
    for (const auto& name : index_names) dump(database, "INDEX_COLUMNS " + name, "PRAGMA index_xinfo('" + name + "')");
    for (const auto& [phase, sql] : std::array<std::pair<std::string, std::string>, 3>{{
             {"write", insert_sql}, {"scan", select_sql}, {"point", point_sql(layout)}}}) {
        dump(database, "EQP " + phase, "EXPLAIN QUERY PLAN " + sql);
        dump(database, "VM " + phase, "EXPLAIN " + sql);
    }
    if (sqlite3_compileoption_used("ENABLE_DBSTAT_VTAB"))
        dump(database, "TREE", "SELECT name,count(*),sum(pgsize),sum(payload),sum(unused) FROM dbstat GROUP BY name");
    else std::cout << "TREE\tunavailable\n";
}
template<bool Verify, bool Diagnose>
void run(const std::string& layout, bool memory, size_t count, const std::string& file) {
    require(memory || !std::filesystem::exists(file), "refuse existing DB");
    const auto records = generate(count);
    std::vector<size_t> requests;
    if (count) for (size_t index = 0; index < 100; ++index) requests.push_back((2 * index + 1) * count / 200);
    std::mt19937 generator(42);
    std::shuffle(requests.begin(), requests.end(), generator);
    Consumed expected_scan, expected_point;
    for (const auto& record : records) expected_scan.accept(record);
    for (size_t index : requests) expected_point.accept(records[index]);
    // Byte-based input hash is computed outside all measured stages.
    uint64_t hash = 14695981039346656037ULL;
    for (const auto& record : records) {
        std::ostringstream encoded;
        encoded << record.name << '\t' << record.master_name << '\t' << record.source << '\t'
            << record.status << '\t' << record.orient << '\t' << record.x << '\t' << record.y << '\t' << record.record_order << '\n';
        for (unsigned char byte : encoded.str()) { hash ^= byte; hash *= 1099511628211ULL; }
    }
    std::cout << "INPUT\t" << count << '\t' << hash << '\n';
    std::cout << "REQUESTS";
    for (size_t index : requests) std::cout << '\t' << index;
    std::cout << '\n';
    const auto ddl = schema(layout);
    const auto lookup = point_sql(layout);
    const bool integer = integer_key(layout);
    const auto path = memory ? ":memory:" : file;
    sqlite3* database = nullptr;
    double init = elapsed([&] { database = open(path, memory); });
    std::cout << "VERSION\t" << sqlite3_libversion() << '\t' << sqlite3_sourceid() << '\n';
    for (const char* name : {"journal_mode", "synchronous", "cache_size", "temp_store", "mmap_size", "foreign_keys", "page_size", "compile_options"})
        dump(database, std::string("CONFIG ") + name, std::string("PRAGMA ") + name);
    double create = elapsed([&] { execute(database, "BEGIN;" + ddl + "COMMIT;"); });
    double begin = elapsed([&] { execute(database, "BEGIN"); });
    double write = measure<Diagnose>(database, "write", [&] { write_records<Diagnose>(database, records); });
    double commit = measure<Diagnose>(database, "commit", [&] { execute(database, "COMMIT"); });
    double write_close = -1;
    if (!memory) write_close = elapsed([&] { close(database); });
    double scan_init = 0;
    if (!memory) scan_init = elapsed([&] { database = open(path, false); });
    auto warmed_scan = scan<false, false>(database, records);
    require(warmed_scan == expected_scan, "scan warmup");
    // New SQLite connection, but OS-warm DB. Memory must retain its connection.
    if (!memory) { close(database); scan_init = elapsed([&] { database = open(path, false); }); }
    Consumed scanned;
    double read_scan = measure<Diagnose>(database, "scan", [&] { scanned = scan<Verify, Diagnose>(database, records); });
    require(scanned == expected_scan, "scan result");
    double scan_close = -1, point_init = 0;
    if (!memory) { scan_close = elapsed([&] { close(database); }); database = open(path, false); }
    auto warmed_point = point_read<false, false>(database, lookup, integer, records, requests);
    require(warmed_point == expected_point, "point warmup");
    if (!memory) { close(database); point_init = elapsed([&] { database = open(path, false); }); }
    Consumed found;
    double read_point = measure<Diagnose>(database, "point", [&] {
        found = point_read<Verify, Diagnose>(database, lookup, integer, records, requests);
    });
    require(found == expected_point, "point result");
    // Metadata and full validation are never inside performance stages.
    auto page_count = scalar(database, "PRAGMA page_count");
    auto page_size = scalar(database, "PRAGMA page_size");
    if constexpr (Verify) validate(database, layout, count);
    if constexpr (Diagnose || Verify) evidence(database, layout);
    double point_close = elapsed([&] { close(database); });
    for (auto [name, value] : std::vector<std::pair<std::string, double>>{
        {"init", init}, {"create", create}, {"begin", begin}, {"write", write}, {"commit", commit},
        {"write_close", write_close}, {"scan_init", scan_init}, {"scan", read_scan}, {"scan_close", scan_close},
        {"point_init", point_init}, {"point", read_point}, {"read_close", point_close},
        {"write_complete", Diagnose ? -1 : begin + write + commit}})
        std::cout << "TIME\t" << name << '\t' << std::fixed << std::setprecision(6) << value << "\tms\n";
    std::cout << "SPACE\t" << page_count << '\t' << page_size << '\t' << page_count * page_size << '\n';
    std::cout << "CONSUMED\t" << scanned.rows << '\t' << scanned.sum << '\t' << found.rows << '\t' << found.sum << '\n';
    std::cout << "PASS\n";
}
int main(int argc, char** argv) {
    try {
        require(argc == 6, "usage: index_benchmark <check|timing|diagnose> <layout> <A|B-batch> <count> <new-db-path>");
        std::string mode = argv[1], layout = argv[2], config = argv[3];
        require(std::find(layouts.begin(), layouts.end(), layout) != layouts.end(), "layout");
        require(config == "A" || config == "B-batch", "config");
        size_t count = std::stoull(argv[4]);
        require(count <= 1000000, "maximum count 1000000");
        if (mode == "check") run<true, false>(layout, config == "A", count, argv[5]);
        else if (mode == "timing") run<false, false>(layout, config == "A", count, argv[5]);
        else if (mode == "diagnose") run<true, true>(layout, config == "A", count, argv[5]);
        else throw std::runtime_error("mode");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
