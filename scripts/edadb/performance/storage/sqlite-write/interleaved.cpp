#define SQLITE_TEXT_LIBRARY
#include "../sqlite-vs-text/benchmark.cpp"

using Inputs = std::array<std::vector<Record>, 3>;

template <class Action>
void scheduled(size_t count, size_t block, Action action) {
    for (size_t offset = 0; offset < count; offset += block)
        for (size_t table = 0; table < 3; ++table)
            for (size_t position = offset; position < std::min(count, offset + block); ++position)
                action(table, position);
}

template <bool Diagnose>
std::array<uint64_t, 3> sql_phase(sqlite3* database, bool writing, const Inputs& input, size_t block, bool verify) {
    std::array<sqlite3_stmt*, 3> statements{};
    std::array<Consumer, 3> consumers{{{verify, input[0]}, {verify, input[1]}, {verify, input[2]}}};
    for (size_t table = 0; table < 3; ++table) {
        std::string sql = writing ? insert_sql : select_sql;
        sql.replace(sql.find("component"), 9, "t" + std::to_string(table));
        if (!writing) sql += " ORDER BY rowid";
        require(sqlite3_prepare_v2(database, sql.c_str(), -1, &statements[table], nullptr) == SQLITE_OK, "prepare");
    }
    std::array<Record, 3> records;
    scheduled(input[0].size(), block, [&](size_t table, size_t position) {
        auto* statement = statements[table];
        if (writing) {
            bind_record<false>(statement, input[table][position]);
            require(sqlite3_step(statement) == SQLITE_DONE, "insert");
            require(sqlite3_reset(statement) == SQLITE_OK, "reset");
        } else {
            require(sqlite3_step(statement) == SQLITE_ROW, "read row");
            fetch_record(statement, records[table]);
            consumers[table].accept(records[table]);
        }
    });
    for (size_t table = 0; table < 3; ++table) {
        auto* statement = statements[table];
        if (!writing) require(sqlite3_step(statement) == SQLITE_DONE, "extra row");
        if constexpr (Diagnose) {
            for (auto entry : {std::pair{SQLITE_STMTSTATUS_RUN, "run"}, std::pair{SQLITE_STMTSTATUS_VM_STEP, "vm_step"},
                               std::pair{SQLITE_STMTSTATUS_SORT, "sort"}, std::pair{SQLITE_STMTSTATUS_REPREPARE, "reprepare"}})
                counters.push_back({writing ? "write" : "read", entry.second, sqlite3_stmt_status(statement, entry.first, 0)});
        }
        require(sqlite3_finalize(statement) == SQLITE_OK, "finalize");
    }
    return {consumers[0].digest, consumers[1].digest, consumers[2].digest};
}

std::array<uint64_t, 3> text_read(const std::string& path, const Inputs& input, size_t block, bool verify, Metrics& metrics) {
    auto start = Clock::now();
    std::array<std::ifstream, 3> files;
    for (size_t table = 0; table < 3; ++table) {
        files[table].open(path + "." + std::to_string(table));
        require(files[table].good(), "text read open");
    }
    metrics.init = milliseconds(start, Clock::now());
    std::array<Consumer, 3> consumers{{{verify, input[0]}, {verify, input[1]}, {verify, input[2]}}};
    std::array<Record, 3> records;
    start = Clock::now();
    scheduled(input[0].size(), block, [&](size_t table, size_t) {
        auto& record = records[table];
        require(bool(files[table] >> record.name >> record.master_name >> record.source >> record.status
                     >> record.orient >> record.x >> record.y >> record.record_order), "text parse");
        consumers[table].accept(record);
    });
    for (auto& file : files) {
        std::string extra;
        require(!(file >> extra) && file.eof(), "extra text record");
    }
    metrics.data = milliseconds(start, Clock::now());
    start = Clock::now();
    for (auto& file : files) { file.clear(); file.close(); }
    metrics.close = milliseconds(start, Clock::now());
    return {consumers[0].digest, consumers[1].digest, consumers[2].digest};
}

int main(int argc, char** argv) {
    try {
        require(argc == 8, "sqlite|text memory|disk count path write-block read-block check|timing|counters");
        const std::string backend = argv[1], setting = argv[2], path = argv[4], mode = argv[7];
        const size_t count = std::stoull(argv[3]);
        require(count <= 1000000 && (backend == "sqlite" || backend == "text"), "arguments");
        require(setting == "memory" || setting == "disk", "setting");
        require(mode == "check" || mode == "timing" || mode == "counters", "mode");
        const auto block = [&](const char* argument) {
            const std::string value = argument;
            require(value == "1" || value == "10" || value == "all", "block");
            return value == "all" ? std::max(size_t{1}, count) : std::stoull(value);
        };
        const size_t write_block = block(argv[5]), read_block = block(argv[6]);
        const bool diagnose = mode == "counters";
        Inputs input;
        std::array<uint64_t, 3> expected_digests{}, read_digests{};
        for (size_t table = 0; table < 3; ++table) {
            input[table] = generate(count);
            for (auto& record : input[table]) record.source += table;
            require(!std::filesystem::exists(path + "." + std::to_string(table)), "fresh text path");
            Consumer expected{false, input[table]};
            for (const auto& record : input[table]) expected.accept(record);
            expected_digests[table] = expected.digest;
            std::cout << "EXPECTED\t" << table << '\t' << expected.seen << '\t' << expected.digest << '\n';
        }
        require(!std::filesystem::exists(path), "fresh SQLite path");
        Metrics writing, reading;
        if (backend == "text") {
            require(!diagnose, "text counters unavailable");
            auto start = Clock::now();
            std::array<std::ofstream, 3> files;
            for (size_t table = 0; table < 3; ++table) {
                files[table].open(path + "." + std::to_string(table));
                require(files[table].good(), "text open");
            }
            writing.init = milliseconds(start, Clock::now());
            start = Clock::now();
            scheduled(count, write_block, [&](size_t table, size_t position) {
                const auto& record = input[table][position];
                files[table] << record.name << ' ' << record.master_name << ' ' << record.source << ' '
                             << record.status << ' ' << record.orient << ' ' << record.x << ' ' << record.y << ' '
                             << record.record_order << '\n';
            });
            for (auto& file : files) { file.flush(); require(file.good(), "text flush"); }
            writing.data = milliseconds(start, Clock::now());
            start = Clock::now();
            for (auto& file : files) { file.close(); require(!file.fail(), "text close"); }
            writing.close = milliseconds(start, Clock::now());
            for (size_t table = 0; table < 3; ++table) warm_file(path + "." + std::to_string(table));
            read_digests = text_read(path, input, read_block, false, reading);
            Metrics ignored;
            text_read(path, input, read_block, true, ignored);
        } else {
            sqlite3* database = nullptr;
            auto start = Clock::now();
            require(sqlite3_open(setting == "memory" ? ":memory:" : path.c_str(), &database) == SQLITE_OK, "open");
            if (setting == "memory") execute(database, "PRAGMA journal_mode=MEMORY;PRAGMA synchronous=OFF;PRAGMA temp_store=MEMORY;PRAGMA cache_size=-8192;PRAGMA mmap_size=0;PRAGMA foreign_keys=ON;");
            else execute(database, "PRAGMA journal_mode=DELETE;PRAGMA synchronous=FULL;");
            writing.init = milliseconds(start, Clock::now());
            config(database);
            writing.create = stage(database, diagnose, "create", [&] {
                execute(database, "BEGIN");
                for (size_t table = 0; table < 3; ++table) {
                    std::string sql = schema_sql;
                    sql.replace(sql.find("component"), 9, "t" + std::to_string(table));
                    execute(database, sql.c_str());
                }
                execute(database, "COMMIT");
            });
            writing.begin = stage(database, diagnose, "begin", [&] { execute(database, "BEGIN"); });
            writing.data = stage(database, diagnose, "write", [&] {
                if (diagnose) sql_phase<true>(database, true, input, write_block, false);
                else sql_phase<false>(database, true, input, write_block, false);
            });
            writing.commit = stage(database, diagnose, "commit", [&] { execute(database, "COMMIT"); });
            if (setting == "disk") {
                start = Clock::now(); require(sqlite3_close(database) == SQLITE_OK, "write close");
                writing.close = milliseconds(start, Clock::now());
                warm_file(path);
                start = Clock::now();
                require(sqlite3_open_v2(path.c_str(), &database, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK, "read open");
                reading.init = milliseconds(start, Clock::now());
            }
            reading.data = stage(database, diagnose, "read", [&] {
                if (diagnose) read_digests = sql_phase<true>(database, false, input, read_block, false);
                else read_digests = sql_phase<false>(database, false, input, read_block, false);
            });
            sql_phase<false>(database, false, input, read_block, true);
            start = Clock::now(); require(sqlite3_close(database) == SQLITE_OK, "read close");
            reading.close = milliseconds(start, Clock::now());
        }
        Consumer unused{false, input[0]};
        require(read_digests == expected_digests, "timed read digest mismatch");
        unused.seen = count * 3;
        for (auto value : expected_digests) unused.digest += value;
        report("write", writing, unused, diagnose);
        report("read", reading, unused, diagnose);
        for (const auto& counter : counters)
            std::cout << "COUNTER\t" << counter.phase << '\t' << counter.name << '\t' << counter.value << '\n';
        std::cout << "CHECK\tfull fields\tPASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
