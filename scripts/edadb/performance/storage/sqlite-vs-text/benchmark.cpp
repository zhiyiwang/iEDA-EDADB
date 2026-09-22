#include <sqlite3.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <vector>

using Clock = std::chrono::steady_clock;
void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}
// Only the separate perf target has gates; the timing target compiles to a no-op.
void perf_gate(std::string_view phase, bool enable) {
#ifdef SQLITE_TEXT_PERF
    const char* target = std::getenv("SQLITE_TEXT_PERF_PHASE");
    if (!target || phase != target) return;
    const char* control_path = std::getenv("SQLITE_TEXT_PERF_CONTROL");
    const char* ack_path = std::getenv("SQLITE_TEXT_PERF_ACK");
    require(control_path && ack_path, "perf FIFO paths");
    static std::ofstream control(control_path);
    static std::ifstream acknowledge(ack_path);
    require(control.good() && acknowledge.good(), "perf FIFO open");
    control << (enable ? "enable\n" : "disable\n") << std::flush;
    std::string response;
    require(static_cast<bool>(std::getline(acknowledge, response)), "perf acknowledge read");
    // perf 5.15 writes sizeof("ack\n"), including a trailing NUL before the next ack.
    response.erase(std::remove(response.begin(), response.end(), '\0'), response.end());
    require(response == "ack", "perf acknowledge value");
#else
    (void)phase;
    (void)enable;
#endif
}
double milliseconds(Clock::time_point begin, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}
struct Record {
    std::string name, master_name;
    int32_t source, status, orient, x, y;
    int64_t record_order;
    auto fields() const { return std::tie(name, master_name, source, status, orient, x, y, record_order); }
};
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
struct Consumer {
    bool verify;
    const std::vector<Record>& expected;
    uint64_t digest = 0;
    size_t seen = 0;
    void accept(const Record& record) {
        if (verify) {
            require(record.record_order >= 0 && static_cast<size_t>(record.record_order) < expected.size(), "order bounds");
            require(record.fields() == expected[record.record_order].fields(), "field mismatch");
            require(record.record_order == static_cast<int64_t>(seen), "sequence mismatch");
        }
        digest += record.name.size() + record.master_name.size() + static_cast<uint64_t>(record.source)
            + record.status + record.orient + record.x + record.y + record.record_order;
        ++seen;
    }
};
struct Metrics {
    double init = -1, create = -1, begin = -1, data = 0, commit = -1, close = -1;
};
struct Counter {
    std::string phase, name;
    int64_t value;
};
std::vector<Counter> counters;
constexpr std::array<int, 4> cache_options{
    SQLITE_DBSTATUS_CACHE_HIT, SQLITE_DBSTATUS_CACHE_MISS,
    SQLITE_DBSTATUS_CACHE_WRITE, SQLITE_DBSTATUS_CACHE_SPILL};
constexpr std::array<const char*, 4> cache_names{"cache_hit", "cache_miss", "cache_write", "cache_spill"};
std::array<int, 4> cache_snapshot(sqlite3* database) {
    std::array<int, 4> values{};
    for (size_t index = 0; index < values.size(); ++index) {
        int highwater = -1;
        require(sqlite3_db_status(database, cache_options[index], &values[index], &highwater, 0)
                == SQLITE_OK, "db_status unsupported/error");
        require(values[index] >= 0 && highwater == 0, "cache counter range/semantics");
    }
    return values;
}
// Diagnostics run in a separate process. Timing mode never reads counters.
template <class Function>
double stage(sqlite3* database, bool diagnose, const char* phase, Function action) {
    if (diagnose) {
        const auto before = cache_snapshot(database);
        action();
        const auto after = cache_snapshot(database);
        for (size_t index = 0; index < before.size(); ++index) {
            require(after[index] >= before[index], "cache counter overflow/reset");
            counters.push_back({phase, cache_names[index], after[index] - before[index]});
        }
        return -1;
    }
    const auto start = Clock::now();
    action();
    return milliseconds(start, Clock::now());
}
const char* insert_sql = "INSERT INTO component VALUES(?,?,?,?,?,?,?,?)";
std::string batch_insert_sql(size_t batch) {
    std::string sql = "INSERT INTO component VALUES";
    for (size_t index = 0; index < batch; ++index)
        sql += (index ? "," : "") + std::string("(?,?,?,?,?,?,?,?)");
    return sql;
}
const char* select_sql = "SELECT name,master_name,source,status,orient,x,y,record_order FROM component";
const char* schema_sql = "CREATE TABLE component(name TEXT,master_name TEXT,source INTEGER,status INTEGER,orient INTEGER,x INTEGER,y INTEGER,record_order BIGINT);";
void execute(sqlite3* database, const char* sql) {
    require(sqlite3_exec(database, sql, nullptr, nullptr, nullptr) == SQLITE_OK, "SQL execution");
}
void config(sqlite3* database) {
    for (const char* name : {"journal_mode", "synchronous", "cache_size", "temp_store", "mmap_size",
                             "foreign_keys", "page_size", "encoding", "compile_options"}) {
        sqlite3_stmt* statement = nullptr;
        const auto sql = std::string("PRAGMA ") + name;
        require(sqlite3_prepare_v2(database, sql.c_str(), -1, &statement, nullptr) == SQLITE_OK, "pragma prepare");
        int status;
        while ((status = sqlite3_step(statement)) == SQLITE_ROW)
            std::cout << "CONFIG\t" << name << '\t' << sqlite3_column_text(statement, 0) << '\n';
        require(status == SQLITE_DONE, "pragma done");
        require(sqlite3_finalize(statement) == SQLITE_OK, "pragma finalize");
    }
    std::cout << "CONFIG\tversion\t" << sqlite3_libversion() << "\nCONFIG\tsource_id\t" << sqlite3_sourceid() << '\n';
}
void explain(sqlite3* database, const std::string& write_sql) {
    sqlite3_stmt* plan = nullptr;
    const auto query = std::string("EXPLAIN QUERY PLAN ") + select_sql;
    require(sqlite3_prepare_v2(database, query.c_str(), -1, &plan, nullptr) == SQLITE_OK, "EQP prepare");
    int plan_status;
    while ((plan_status = sqlite3_step(plan)) == SQLITE_ROW)
        std::cout << "EQP\t" << sqlite3_column_text(plan, 3) << '\n';
    require(plan_status == SQLITE_DONE, "EQP done");
    require(sqlite3_finalize(plan) == SQLITE_OK, "EQP finalize");
    for (const char* sql : {write_sql.c_str(), select_sql}) {
        sqlite3_stmt* statement = nullptr;
        std::string request = std::string("EXPLAIN ") + sql;
        require(sqlite3_prepare_v2(database, request.c_str(), -1, &statement, nullptr) == SQLITE_OK, "explain prepare");
        int status;
        while ((status = sqlite3_step(statement)) == SQLITE_ROW) {
            std::cout << "EXPLAIN\t" << (sql == select_sql ? "read" : "write");
            for (int column = 0; column < sqlite3_column_count(statement); ++column) {
                const auto* value = sqlite3_column_text(statement, column);
                std::cout << '\t' << (value ? reinterpret_cast<const char*>(value) : "");
            }
            std::cout << '\n';
        }
        require(status == SQLITE_DONE, "explain done");
        require(sqlite3_finalize(statement) == SQLITE_OK, "explain finalize");
    }
}
// Input owns string storage until finalize, making STATIC safe in this control.
template <bool Static>
void bind_record(sqlite3_stmt* statement, const Record& record, int offset = 0) {
    auto lifetime = Static ? SQLITE_STATIC : SQLITE_TRANSIENT;
    require(sqlite3_bind_text(statement, offset + 1, record.name.data(), record.name.size(), lifetime) == SQLITE_OK, "bind");
    require(sqlite3_bind_text(statement, offset + 2, record.master_name.data(), record.master_name.size(), lifetime) == SQLITE_OK, "bind");
    std::array<int64_t, 6> values{record.source, record.status, record.orient, record.x, record.y, record.record_order};
    for (size_t column = 0; column < values.size(); ++column)
        require(sqlite3_bind_int64(statement, offset + column + 3, values[column]) == SQLITE_OK, "bind");
}
void fetch_record(sqlite3_stmt* statement, Record& record) {
    record.name.assign(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0)), sqlite3_column_bytes(statement, 0));
    record.master_name.assign(reinterpret_cast<const char*>(sqlite3_column_text(statement, 1)), sqlite3_column_bytes(statement, 1));
    record.source = sqlite3_column_int(statement, 2);
    record.status = sqlite3_column_int(statement, 3);
    record.orient = sqlite3_column_int(statement, 4);
    record.x = sqlite3_column_int(statement, 5);
    record.y = sqlite3_column_int(statement, 6);
    record.record_order = sqlite3_column_int64(statement, 7);
}
// Same SQL and row loop in both modes; only diagnostic instantiations query counters.
template <bool Diagnose, bool Static, size_t Batch = 1, bool CountOnly = false>
void operation(sqlite3* database, bool write, const std::vector<Record>& input, Consumer& consumer,
               const char* read_sql = select_sql) {
    sqlite3_stmt* statement = nullptr;
    std::string batch_sql;
    if constexpr (Batch > 1) {
        // SQL construction belongs to write data; the tail uses the original single-row statement.
        if (write) {
            batch_sql = batch_insert_sql(Batch);
        }
    }
    const char* sql = write ? (Batch > 1 ? batch_sql.c_str() : insert_sql) : read_sql;
    require(sqlite3_prepare_v2(database, sql, -1, &statement, nullptr)
            == SQLITE_OK, "prepare");
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> guard(statement, sqlite3_finalize);
    // A fresh statement starts at zero; no prior execution needs resetting.
    if (write) {
        size_t position = 0;
        for (; position + Batch <= input.size(); position += Batch) {
            for (size_t slot = 0; slot < Batch; ++slot)
                bind_record<Static>(statement, input[position + slot], static_cast<int>(slot * 8));
            require(sqlite3_step(statement) == SQLITE_DONE, "insert");
            require(sqlite3_reset(statement) == SQLITE_OK, "reset");
        }
        if constexpr (Batch > 1) {
            if (position < input.size()) {
                std::vector<Record> tail(input.begin() + position, input.end());
                operation<Diagnose, Static>(database, true, tail, consumer);
            }
        }
    } else {
        Record record;
        for (;;) {
            const int status = sqlite3_step(statement);
            if (status == SQLITE_DONE) break;
            require(status == SQLITE_ROW, "row");
            if constexpr (CountOnly) {
                // Diagnostic ablation only: not equivalent to returning eight fields.
                ++consumer.seen;
            } else {
                fetch_record(statement, record);
                consumer.accept(record);
            }
        }
    }
    if constexpr (Diagnose) {
        constexpr std::array<int, 6> options{SQLITE_STMTSTATUS_VM_STEP, SQLITE_STMTSTATUS_RUN,
            SQLITE_STMTSTATUS_REPREPARE, SQLITE_STMTSTATUS_FULLSCAN_STEP,
            SQLITE_STMTSTATUS_SORT, SQLITE_STMTSTATUS_AUTOINDEX};
        constexpr std::array<const char*, 6> names{
            "vm_step", "run", "reprepare", "fullscan_step", "sort", "autoindex"};
        for (size_t index = 0; index < options.size(); ++index) {
            const int value = sqlite3_stmt_status(statement, options[index], 0);
            require(value >= 0, "statement counter overflow");
            counters.push_back({write ? "write" : "read", names[index], value});
        }
    }
    require(sqlite3_finalize(guard.release()) == SQLITE_OK, "finalize");
}
void warm_file(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    require(stream.good(), "warm open");
    std::array<char, 65536> buffer;
    while (stream.read(buffer.data(), buffer.size()) || stream.gcount()) {}
    require(stream.eof(), "warm read");
}
void report(const char* name, const Metrics& metric, const Consumer& consumer, bool diagnose) {
    if (!diagnose)
        std::cout << std::fixed << std::setprecision(6)
                  << "TIME\t" << name << '\t' << metric.init << '\t' << metric.create << '\t' << metric.begin
                  << '\t' << metric.data << '\t' << metric.commit << '\t' << metric.close << '\n';
    std::cout << "ROWS\t" << name << '\t' << consumer.seen << '\t' << consumer.digest << '\n';
}
#ifndef SQLITE_TEXT_LIBRARY
int main(int argc, char** argv) {
    try {
        // Reserved argument keeps the phase-gated perf launcher interface stable.
        require(argc == 8, "benchmark text|sqlite|static A|B-batch count path check|timing|counters reserved settle");
        const std::string route = argv[1], setting = argv[2], path = argv[4], mode = argv[5];
        const bool verify = mode == "check", diagnose = mode == "counters";
        require(mode == "check" || mode == "timing" || diagnose, "mode");
        require(route == "text" || route == "sqlite" || route == "static" || route == "batch10"
                || route == "batch100" || route == "step-only", "route");
        require(!diagnose || route != "text", "text has no SQLite counters");
        require(setting == "A" || setting == "B-batch", "config");
        const size_t count = std::stoull(argv[3]);
        require(count <= 1000000, "count limit");
        require(!std::filesystem::exists(path), "fresh path required");
        const double settle_seconds = std::stod(argv[7]);
        require(settle_seconds >= 0, "settle");
        const auto input = generate(count);
        Consumer expected{false, input}, consumer{verify, input};
        for (const auto& record : input) expected.accept(record);
        auto settle = [&] {
            if (!verify) std::this_thread::sleep_for(std::chrono::duration<double>(settle_seconds));
        };
        Metrics write, read;
        if (route == "text") {
            auto start = Clock::now(); std::ofstream output(path); require(output.good(), "text open");
            write.init = milliseconds(start, Clock::now());
            perf_gate("write", true); start = Clock::now();
            for (const auto& record : input) output << record.name << ' ' << record.master_name << ' ' << record.source << ' '
                << record.status << ' ' << record.orient << ' ' << record.x << ' ' << record.y << ' ' << record.record_order << '\n';
            output.flush(); require(output.good(), "text write"); write.data = milliseconds(start, Clock::now());
            perf_gate("write", false);
            start = Clock::now(); output.close(); require(!output.fail(), "text close"); write.close = milliseconds(start, Clock::now());
            settle();
            warm_file(path);
            start = Clock::now(); std::ifstream source(path); require(source.good(), "text open");
            read.init = milliseconds(start, Clock::now());
            perf_gate("read", true); start = Clock::now();
            Record record;
            while (source >> record.name) {
                require(static_cast<bool>(source >> record.master_name >> record.source >> record.status >> record.orient >> record.x >> record.y >> record.record_order), "truncated");
                consumer.accept(record);
            }
            require(source.eof(), "text read"); read.data = milliseconds(start, Clock::now());
            perf_gate("read", false);
            start = Clock::now(); source.clear(); source.close(); read.close = milliseconds(start, Clock::now());
        } else {
            sqlite3* database = nullptr;
            const bool memory = setting == "A";
            auto open = [&] {
                require(sqlite3_open(memory ? ":memory:" : path.c_str(), &database) == SQLITE_OK, "open");
                if (memory) execute(database, "PRAGMA journal_mode=MEMORY; PRAGMA synchronous=OFF; PRAGMA temp_store=MEMORY; PRAGMA cache_size=-8192; PRAGMA mmap_size=0; PRAGMA foreign_keys=ON;");
            };
            auto start = Clock::now(); open(); write.init = milliseconds(start, Clock::now());
            config(database);
            const auto actual_insert = batch_insert_sql(route == "batch10" ? 10 : route == "batch100" ? 100 : 1);
            std::cout << "SQL\tcreate\t" << schema_sql << "\nSQL\twrite\t" << actual_insert << "\nSQL\tread\t" << select_sql << '\n';
            write.create = stage(database, diagnose, "create", [&] {
                execute(database, "BEGIN"); execute(database, schema_sql); execute(database, "COMMIT");
            });
            auto run = [&](bool writing, Consumer& output) {
                if (diagnose) {
                    if (route == "batch10") operation<true, false, 10>(database, writing, input, output);
                    else if (route == "batch100") operation<true, false, 100>(database, writing, input, output);
                    else if (route == "step-only") operation<true, false, 1, true>(database, writing, input, output);
                    else if (route == "static") operation<true, true>(database, writing, input, output);
                    else operation<true, false>(database, writing, input, output);
                } else {
                    if (route == "batch10") operation<false, false, 10>(database, writing, input, output);
                    else if (route == "batch100") operation<false, false, 100>(database, writing, input, output);
                    else if (route == "step-only" && !verify) operation<false, false, 1, true>(database, writing, input, output);
                    else if (route == "static") operation<false, true>(database, writing, input, output);
                    else operation<false, false>(database, writing, input, output);
                }
            };
            write.begin = stage(database, diagnose, "begin", [&] { execute(database, "BEGIN"); });
            perf_gate("write", true);
            write.data = stage(database, diagnose, "write", [&] { run(true, consumer); });
            perf_gate("write", false);
            write.commit = stage(database, diagnose, "commit", [&] { execute(database, "COMMIT"); });
            settle();
            if (!memory) {
                start = Clock::now(); require(sqlite3_close(database) == SQLITE_OK, "close"); write.close = milliseconds(start, Clock::now());
                warm_file(path);
                start = Clock::now(); open(); read.init = milliseconds(start, Clock::now());
            }
            perf_gate("read", true);
            read.data = stage(database, diagnose, "read", [&] { run(false, consumer); });
            perf_gate("read", false);
            if (diagnose) explain(database, actual_insert);
            if (verify && count) {
                execute(database, "UPDATE component SET name='V0000000' WHERE record_order=0");
                Consumer damaged{true, input};
                bool rejected = false;
                try { operation<false, false>(database, false, input, damaged); }
                catch (const std::runtime_error&) { rejected = true; }
                require(rejected, "same-length corruption accepted");
                std::cout << "CHECK\tcorruption rejected\n";
            }
            start = Clock::now(); require(sqlite3_close(database) == SQLITE_OK, "close"); read.close = milliseconds(start, Clock::now());
        }
        require(consumer.seen == expected.seen, "row count mismatch");
        require((route == "step-only" && !verify) || consumer.digest == expected.digest, "result mismatch");
        report("write", write, expected, diagnose);
        report("read", read, consumer, diagnose);
        for (const auto& counter : counters)
            std::cout << "COUNTER\t" << counter.phase << '\t' << counter.name << '\t' << counter.value << '\n';
        std::cout << "CHECK\t" << (verify ? "full fields" : "digest") << "\tPASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
#endif
