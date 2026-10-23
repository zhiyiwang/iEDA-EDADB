#define SQLITE_TEXT_PERF
#define main write_benchmark_entry
#include "benchmark.cpp"
#undef main

int main(int argc, char** argv) {
    try {
        require(argc == 3, "profile single|batch100|switch1|grouped control|sample");
        const std::string route = argv[1];
        const bool sample = std::string(argv[2]) == "sample";
        const bool triple = route == "switch1" || route == "grouped";
        require(triple || route == "single" || route == "batch100", "profile route");
        const auto input = generate(triple ? 100000 : 1000000);
        sqlite3* database = nullptr;
        require(sqlite3_open(":memory:", &database) == SQLITE_OK, "profile open");
        execute(database, "PRAGMA journal_mode=MEMORY; PRAGMA synchronous=OFF; PRAGMA temp_store=MEMORY; PRAGMA cache_size=-8192; PRAGMA mmap_size=0; PRAGMA foreign_keys=ON;");
        for (size_t table = 0; table < (triple ? 3u : 1u); ++table) {
            std::string sql = schema_sql;
            if (triple) sql.replace(sql.find("component"), 9, "t" + std::to_string(table));
            execute(database, sql.c_str());
        }
        execute(database, "BEGIN");
        Consumer consumer{true, input};
        if (sample) perf_gate("write", true);
        const auto start = Clock::now();
        if (triple) interleave<false>(database, input, route == "switch1" ? 1 : input.size());
        else if (route == "batch100") operation<false, false, 100>(database, true, input, consumer);
        else operation<false, false>(database, true, input, consumer);
        const auto elapsed = milliseconds(start, Clock::now());
        if (sample) perf_gate("write", false);
        execute(database, "COMMIT");
        for (size_t table = 0; table < (triple ? 3u : 1u); ++table) {
            Consumer checked{true, input};
            std::string sql = select_sql;
            if (triple) sql.replace(sql.find("component"), 9, "t" + std::to_string(table));
            sql += " ORDER BY rowid";
            operation<false, false>(database, false, input, checked, sql.c_str());
            require(checked.seen == input.size(), "profile full verification");
        }
        require(sqlite3_close(database) == SQLITE_OK, "profile close");
        std::cout << "PROFILE_MS\t" << elapsed << "\nCHECK\tPASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
