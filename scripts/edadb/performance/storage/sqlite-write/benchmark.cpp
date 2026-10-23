#define SQLITE_TEXT_LIBRARY
#include "../sqlite-vs-text/benchmark.cpp"

template <bool Diagnose>
void interleave(sqlite3* database, const std::vector<Record>& input, size_t block) {
    std::array<sqlite3_stmt*, 3> statements{};
    for (size_t table = 0; table < statements.size(); ++table) {
        const auto sql = "INSERT INTO t" + std::to_string(table) + " VALUES(?,?,?,?,?,?,?,?)";
        require(sqlite3_prepare_v2(database, sql.c_str(), -1, &statements[table], nullptr) == SQLITE_OK, "prepare three tables");
    }
    for (size_t offset = 0; offset < input.size(); offset += block)
        for (auto* statement : statements)
            for (size_t position = offset; position < std::min(offset + block, input.size()); ++position) {
                bind_record<false>(statement, input[position]);
                require(sqlite3_step(statement) == SQLITE_DONE, "insert three tables");
                require(sqlite3_reset(statement) == SQLITE_OK, "reset three tables");
            }
    for (auto* statement : statements) {
        if constexpr (Diagnose) {
            for (auto entry : {std::pair{SQLITE_STMTSTATUS_RUN, "run"},
                               std::pair{SQLITE_STMTSTATUS_VM_STEP, "vm_step"},
                               std::pair{SQLITE_STMTSTATUS_REPREPARE, "reprepare"}})
                counters.push_back({"write", entry.second, sqlite3_stmt_status(statement, entry.first, 0)});
        }
        require(sqlite3_finalize(statement) == SQLITE_OK, "finalize three tables");
    }
}

int main(int argc, char** argv) {
    try {
        require(argc == 6, "route memory|disk count fresh-path timing|check|counters");
        const std::string route = argv[1], setting = argv[2], path = argv[4], mode = argv[5];
        const size_t count = std::stoull(argv[3]);
        require(count <= 1000000 && (setting == "memory" || setting == "disk"), "arguments");
        require(mode == "timing" || mode == "check" || mode == "counters", "mode");
        const bool diagnose = mode == "counters";
        const bool triple = route == "switch1" || route == "switch10" || route == "grouped";
        require(triple || route == "text" || route == "single" || route == "batch10" || route == "batch100", "route");
        require(!std::filesystem::exists(path), "fresh path required");
        const auto input = generate(count);
        Metrics metric;
        Consumer consumer{true, input};
        if (route == "text") {
            auto start = Clock::now();
            std::ofstream output(path);
            require(output.good(), "text open");
            metric.init = milliseconds(start, Clock::now());
            start = Clock::now();
            for (const auto& record : input)
                output << record.name << ' ' << record.master_name << ' ' << record.source << ' '
                       << record.status << ' ' << record.orient << ' ' << record.x << ' ' << record.y
                       << ' ' << record.record_order << '\n';
            output.flush();
            require(output.good(), "text flush");
            metric.data = milliseconds(start, Clock::now());
            start = Clock::now(); output.close(); require(!output.fail(), "text close");
            metric.close = milliseconds(start, Clock::now());
            std::ifstream source(path);
            Record record;
            while (source >> record.name) {
                require(bool(source >> record.master_name >> record.source >> record.status >> record.orient
                                    >> record.x >> record.y >> record.record_order), "text truncated");
                consumer.accept(record);
            }
            require(source.eof() && consumer.seen == count, "text verification");
        } else {
            sqlite3* database = nullptr;
            auto start = Clock::now();
            require(sqlite3_open(setting == "memory" ? ":memory:" : path.c_str(), &database) == SQLITE_OK, "open");
            if (setting == "memory")
                execute(database, "PRAGMA journal_mode=MEMORY; PRAGMA synchronous=OFF; PRAGMA temp_store=MEMORY; PRAGMA cache_size=-8192; PRAGMA mmap_size=0; PRAGMA foreign_keys=ON;");
            else execute(database, "PRAGMA journal_mode=DELETE; PRAGMA synchronous=FULL;");
            metric.init = milliseconds(start, Clock::now());
            config(database);
            metric.create = stage(database, diagnose, "create", [&] {
                execute(database, "BEGIN");
                if (triple) {
                    for (size_t table = 0; table < 3; ++table) {
                        std::string sql = schema_sql;
                        sql.replace(sql.find("component"), 9, "t" + std::to_string(table));
                        execute(database, sql.c_str());
                    }
                } else execute(database, schema_sql);
                execute(database, "COMMIT");
            });
            metric.begin = stage(database, diagnose, "begin", [&] { execute(database, "BEGIN"); });
            metric.data = stage(database, diagnose, "write", [&] {
                if (triple) {
                    const size_t block = route == "switch1" ? 1 : route == "switch10" ? 10 : std::max(size_t{1}, count);
                    if (diagnose) interleave<true>(database, input, block);
                    else interleave<false>(database, input, block);
                } else if (diagnose) {
                    if (route == "batch10") operation<true, false, 10>(database, true, input, consumer);
                    else if (route == "batch100") operation<true, false, 100>(database, true, input, consumer);
                    else operation<true, false>(database, true, input, consumer);
                } else {
                    if (route == "batch10") operation<false, false, 10>(database, true, input, consumer);
                    else if (route == "batch100") operation<false, false, 100>(database, true, input, consumer);
                    else operation<false, false>(database, true, input, consumer);
                }
            });
            metric.commit = stage(database, diagnose, "commit", [&] { execute(database, "COMMIT"); });
            if (setting == "disk") {
                start = Clock::now(); require(sqlite3_close(database) == SQLITE_OK, "close");
                metric.close = milliseconds(start, Clock::now());
                require(sqlite3_open_v2(path.c_str(), &database, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK, "verify reopen");
            }
            for (size_t table = 0; table < (triple ? 3u : 1u); ++table) {
                Consumer checked{true, input};
                std::string sql = select_sql;
                if (triple) sql.replace(sql.find("component"), 9, "t" + std::to_string(table));
                sql += " ORDER BY rowid";
                operation<false, false>(database, false, input, checked, sql.c_str());
                require(checked.seen == count, "SQLite verification count");
            }
            require(sqlite3_close(database) == SQLITE_OK, "verification close");
        }
        report("write", metric, consumer, diagnose);
        for (const auto& counter : counters)
            std::cout << "COUNTER\t" << counter.phase << '\t' << counter.name << '\t' << counter.value << '\n';
        std::cout << "CHECK\tfull fields\tPASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
