// Reuse the exact schema, generator, fetch/consumer and read loop, not a new parser.
#define SQLITE_TEXT_LIBRARY
#include "benchmark.cpp"

int main(int argc, char** argv) {
    try {
        require(argc == 7, "select_projection explicit|star A|B-batch count path check|timing|counters prepare_repeats");
        const std::string route = argv[1], setting = argv[2], path = argv[4], mode = argv[5];
        require(route == "explicit" || route == "star", "route");
        require(setting == "A" || setting == "B-batch", "config");
        require(mode == "check" || mode == "timing" || mode == "counters", "mode");
        const size_t count = std::stoull(argv[3]), repeats = std::stoull(argv[6]);
        require(count <= 1000000 && repeats > 0, "bounds");
        require(!std::filesystem::exists(path), "fresh path");
        const char* query = route == "star" ? "SELECT * FROM component" : select_sql;
        const auto input = generate(count);
        Consumer expected{false, input}, result{mode == "check", input};
        for (const auto& record : input) expected.accept(record);
        sqlite3* database = nullptr;
        auto open = [&] {
            require(sqlite3_open(setting == "A" ? ":memory:" : path.c_str(), &database) == SQLITE_OK, "open");
            if (setting == "A") execute(database, "PRAGMA journal_mode=MEMORY; PRAGMA synchronous=OFF; PRAGMA temp_store=MEMORY; PRAGMA cache_size=-8192; PRAGMA mmap_size=0; PRAGMA foreign_keys=ON;");
        };
        open();
        config(database);
        execute(database, schema_sql);
        execute(database, "BEGIN");
        operation<false, false>(database, true, input, expected);
        execute(database, "COMMIT");
        if (setting != "A") {
            require(sqlite3_close(database) == SQLITE_OK, "close setup");
            warm_file(path);
            open();
        }
        std::cout << "SQL\tread\t" << query << '\n';
        // Full read is measured before the independent prepare-only loop.
        const auto start = Clock::now();
        if (mode == "counters") operation<true, false>(database, false, input, result, query);
        else operation<false, false>(database, false, input, result, query);
        const double read_ms = milliseconds(start, Clock::now());
        require(result.seen == expected.seen && result.digest == expected.digest, "read result");
        if (mode == "timing") {
            // One untimed warm-up; time the entire loop, never per prepare call.
            auto prepare_once = [&] {
                sqlite3_stmt* statement = nullptr;
                require(sqlite3_prepare_v2(database, query, -1, &statement, nullptr) == SQLITE_OK, "prepare");
                require(sqlite3_finalize(statement) == SQLITE_OK, "finalize");
            };
            prepare_once();
            const auto begin = Clock::now();
            for (size_t index = 0; index < repeats; ++index) prepare_once();
            const double prepare_ms = milliseconds(begin, Clock::now());
            std::cout << std::fixed << std::setprecision(6) << "RESULT\t" << read_ms << '\t'
                      << prepare_ms << '\t' << repeats << '\n';
        }
        for (const auto& counter : counters)
            std::cout << "COUNTER\t" << counter.name << '\t' << counter.value << '\n';
        // Schema/column metadata and complete bytecode are separate from timing.
        sqlite3_stmt* statement = nullptr;
        const auto explain_sql = std::string("EXPLAIN ") + query;
        require(sqlite3_prepare_v2(database, explain_sql.c_str(), -1, &statement, nullptr) == SQLITE_OK, "explain");
        int status;
        while ((status = sqlite3_step(statement)) == SQLITE_ROW) {
            std::cout << "EXPLAIN";
            for (int column = 0; column < 8; ++column) {
                const auto* value = sqlite3_column_text(statement, column);
                std::cout << '\t' << (value ? reinterpret_cast<const char*>(value) : "NULL");
            }
            std::cout << '\n';
        }
        require(status == SQLITE_DONE, "explain done");
        require(sqlite3_finalize(statement) == SQLITE_OK, "explain finalize");
        require(sqlite3_close(database) == SQLITE_OK, "close");
        std::cout << "CHECK\tPASS\t" << result.seen << '\t' << result.digest << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
