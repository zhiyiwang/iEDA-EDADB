// Current benchmark entry point; shared helpers contain no other main().
#include "benchmark_support.h"
#include <thread>

struct Sample {
    double init = 0, create = -1, begin = -1, data = 0, commit = -1, close = 0;
};

struct Consumer {
    bool verify;
    const std::vector<ComponentRecord>& expected;
    uint64_t digest = 0;
    size_t seen = 0;
    void accept(const ComponentRecord& record) {
        // Performance consumes all columns; full byte comparisons only in check mode.
        if (verify) {
            require(record.record_order >= 0 && static_cast<size_t>(record.record_order) < expected.size(), "order bounds");
            require(record.fields() == expected[record.record_order].fields(), "field mismatch");
        }
        digest += record.name.size() + record.master_name.size() + static_cast<uint64_t>(record.source)
            + record.status + record.orient + record.x + record.y + record.record_order;
        ++seen;
    }
};

void report(const char* operation, const Sample& sample, const Consumer& consumer) {
    double complete = sample.data + std::max(0.0, sample.begin) + std::max(0.0, sample.commit);
    std::cout << std::fixed << std::setprecision(6) << "STREAM\t" << operation << '\t'
        << sample.init << '\t' << sample.create << '\t' << sample.begin << '\t' << sample.data
        << '\t' << sample.commit << '\t' << sample.close << '\t' << complete << '\t'
        << consumer.seen << '\t' << consumer.digest << '\n';
}

void print_config(sqlite3* database) {
    // Called outside data timers, on the actual connection, not a reopened probe.
    for (const char* name : {"journal_mode", "synchronous", "cache_size", "temp_store", "mmap_size",
                             "foreign_keys", "page_size", "encoding", "compile_options"}) {
        sqlite3_stmt* statement = nullptr;
        require(sqlite3_prepare_v2(database, (std::string("PRAGMA ") + name).c_str(), -1, &statement, nullptr) == SQLITE_OK, "config query");
        int result;
        while ((result = sqlite3_step(statement)) == SQLITE_ROW)
            std::cout << "CONFIG\t" << name << '\t' << sqlite3_column_text(statement, 0) << '\n';
        require(result == SQLITE_DONE, "config result");
        require(sqlite3_finalize(statement) == SQLITE_OK, "config finalize");
    }
    std::cout << "CONFIG\tversion\t" << sqlite3_libversion() << "\nCONFIG\tsource_id\t" << sqlite3_sourceid() << '\n';
}

void data_operation(sqlite3* database, bool edadb_route, bool write,
                    std::vector<ComponentRecord>& input, Consumer& consumer) {
    if (edadb_route) {
        if (write) {
            auto writer = edadb::makeInsertOp<ComponentRecord>();
            for (auto& record : input) require(writer.insert(&record) >= 0, "EDADB insert");
        } else {
            auto reader = edadb::makeReadAllOp<ComponentRecord>();
            ComponentRecord record;
            int result;
            while ((result = edadb::readNext(reader, &record)) > 0) consumer.accept(record);
            require(result == 0, "EDADB read");
        }
        return;
    }
    sqlite3_stmt* statement = nullptr;
    require(sqlite3_prepare_v2(database, write ? "INSERT INTO component VALUES(?,?,?,?,?,?,?,?)" :
        "SELECT name,master_name,source,status,orient,x,y,record_order FROM component", -1, &statement, nullptr) == SQLITE_OK, "prepare");
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> guard(statement, sqlite3_finalize);
    if (write) {
        for (const auto& record : input) {
            require(sqlite3_bind_text(statement, 1, record.name.data(), record.name.size(), SQLITE_TRANSIENT) == SQLITE_OK, "bind");
            require(sqlite3_bind_text(statement, 2, record.master_name.data(), record.master_name.size(), SQLITE_TRANSIENT) == SQLITE_OK, "bind");
            std::array<int64_t, 6> values{record.source, record.status, record.orient, record.x, record.y, record.record_order};
            for (size_t column = 0; column < values.size(); ++column)
                require(sqlite3_bind_int64(statement, column + 3, values[column]) == SQLITE_OK, "bind");
            require(sqlite3_step(statement) == SQLITE_DONE, "insert");
            require(sqlite3_reset(statement) == SQLITE_OK, "reset");
        }
    } else {
        ComponentRecord record;
        int result;
        while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
            record.name.assign(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0)), sqlite3_column_bytes(statement, 0));
            record.master_name.assign(reinterpret_cast<const char*>(sqlite3_column_text(statement, 1)), sqlite3_column_bytes(statement, 1));
            record.source = sqlite3_column_int(statement, 2);
            record.status = sqlite3_column_int(statement, 3);
            record.orient = sqlite3_column_int(statement, 4);
            record.x = sqlite3_column_int(statement, 5);
            record.y = sqlite3_column_int(statement, 6);
            record.record_order = sqlite3_column_int64(statement, 7);
            consumer.accept(record);
        }
        require(result == SQLITE_DONE, "read");
    }
    require(sqlite3_finalize(guard.release()) == SQLITE_OK, "finalize");
}

int main(int argc, char** argv) {
    try {
        require(argc == 8, "usage: stream_benchmark ROUTE CONFIG COUNT PATH FIXTURE check|perf CACHE");
        std::string route = argv[1], config = argv[2], path = argv[4], fixture = argv[5], cache = argv[7];
        bool verify = std::string(argv[6]) == "check";
        size_t count = std::stoull(argv[3]);
        auto input = generate(count);
        Consumer expected{false, input};
        for (const auto& record : input) expected.accept(record);
        Consumer consumer{verify, input};
        auto settle_read = [&]() {
            if (!verify && std::getenv("STREAM_SETTLE"))
                std::this_thread::sleep_for(std::chrono::duration<double>(std::stod(std::getenv("STREAM_SETTLE"))));
        };
        Sample write, read;
        if (route == "sqlite" || route == "edadb") {
            require(member(config, {"A", "A-no-journal", "B-default", "B-batch"}), "config");
            bool memory = config == "A" || config == "A-no-journal";
            bool batch = config != "B-default", use_edadb = route == "edadb";
            // EDADB connect forces FK ON. Restore the linked SQLite build's default
            // in the benchmark only, so B matches a pristine SQLite connection.
            sqlite3* defaults = nullptr;
            require(sqlite3_open(":memory:", &defaults) == SQLITE_OK, "defaults open");
            int default_fk = pragma_int(defaults, "PRAGMA foreign_keys");
            require(sqlite3_close(defaults) == SQLITE_OK, "defaults close");
            require(!std::filesystem::exists(path), "fresh output required");
            sqlite3* database = nullptr;
            auto open = [&]() {
                if (use_edadb) {
                    require(edadb::initDatabase(memory ? ":memory:" : path), "open EDADB");
                    edadb::DbStatement probe;
                    require(edadb::DbManager::i().initStatement(probe), "probe");
                    database = probe.db;
                    edadb::Cpp2SqlTypeTrait<ComponentRecord>::hasPrimKey = false;
                    if (!memory) exec_sql(database, "PRAGMA foreign_keys=" + std::to_string(default_fk));
                } else require(sqlite3_open(memory ? ":memory:" : path.c_str(), &database) == SQLITE_OK, "open SQLite");
                if (memory) exec_sql(database, std::string("PRAGMA journal_mode=") + (config == "A" ? "MEMORY" : "OFF") +
                    "; PRAGMA synchronous=OFF; PRAGMA temp_store=MEMORY; PRAGMA cache_size=-8192; PRAGMA mmap_size=0; PRAGMA foreign_keys=ON;");
            };
            auto close = [&]() {
                if (use_edadb) require(edadb::closeDatabase(), "close EDADB");
                else require(sqlite3_close(database) == SQLITE_OK, "close SQLite");
            };
            auto start = Clock::now(); open(); write.init = Metrics::ms(start, Clock::now());
            print_config(database);
            if (memory) {
                require(pragma_int(database, "PRAGMA synchronous") == 0, "memory sync");
                require(pragma_int(database, "PRAGMA cache_size") == -8192, "memory cache");
                require(pragma_int(database, "PRAGMA foreign_keys") == 1, "memory FK");
            }
            start = Clock::now();
            if (use_edadb) require(edadb::createTable<ComponentRecord>(), "create");
            else exec_sql(database, "BEGIN; CREATE TABLE component(name TEXT,master_name TEXT,source INTEGER,status INTEGER,orient INTEGER,x INTEGER,y INTEGER,record_order BIGINT); COMMIT;");
            write.create = Metrics::ms(start, Clock::now());
            start = Clock::now();
            if (batch) exec_sql(database, "BEGIN");
            if (batch) write.begin = Metrics::ms(start, Clock::now());
            start = Clock::now(); data_operation(database, use_edadb, true, input, consumer);
            write.data = Metrics::ms(start, Clock::now());
            start = Clock::now();
            if (batch) exec_sql(database, "COMMIT");
            if (batch) write.commit = Metrics::ms(start, Clock::now());
            settle_read();
            if (!memory) {
                start = Clock::now(); close(); write.close = Metrics::ms(start, Clock::now());
                require(prepare_cache(path, cache == "repeat" ? "warm" : cache).verified, "file cache");
                start = Clock::now(); open(); read.init = Metrics::ms(start, Clock::now());
            } else { write.close = -1; read.init = -1; }
            if (cache == "repeat") {
                Consumer warm{false, input}; data_operation(database, use_edadb, false, input, warm);
            }
            start = Clock::now(); data_operation(database, use_edadb, false, input, consumer);
            read.data = Metrics::ms(start, Clock::now());
            if (verify && count) {
                // Same-length corruption proves that the correctness path checks bytes.
                exec_sql(database, "UPDATE component SET name='V0000000' WHERE record_order=0");
                bool rejected = false;
                try { Consumer damaged{true, input}; data_operation(database, use_edadb, false, input, damaged); }
                catch (const std::exception&) { rejected = true; }
                require(rejected, "corruption not rejected");
                std::cout << "CHECK\tsame-length corruption rejected\n";
                // This validation-only database is discarded, not reused for performance.
            }
            start = Clock::now(); close(); read.close = Metrics::ms(start, Clock::now());
        } else if (route == "text") {
            auto start = Clock::now(); std::ofstream output(path); require(output.good(), "text open");
            write.init = Metrics::ms(start, Clock::now()); start = Clock::now();
            for (const auto& record : input) output << record.name << ' ' << record.master_name << ' ' << record.source << ' '
                << record.status << ' ' << record.orient << ' ' << record.x << ' ' << record.y << ' ' << record.record_order << '\n';
            output.flush(); require(output.good(), "text write"); write.data = Metrics::ms(start, Clock::now());
            start = Clock::now(); output.close(); write.close = Metrics::ms(start, Clock::now());
            settle_read();
            require(prepare_cache(path, cache).verified, "text cache");
            start = Clock::now(); std::ifstream source(path); require(source.good(), "text read open");
            read.init = Metrics::ms(start, Clock::now()); start = Clock::now();
            ComponentRecord record;
            while (source >> record.name) {
                require(static_cast<bool>(source >> record.master_name >> record.source >> record.status >> record.orient >> record.x >> record.y >> record.record_order), "text truncated");
                consumer.accept(record);
            }
            require(source.eof(), "text read"); read.data = Metrics::ms(start, Clock::now());
            start = Clock::now(); source.clear(); source.close(); read.close = Metrics::ms(start, Clock::now());
        } else {
            require(route == "native" || route == "adapter", "route");
            IdbLefService lef; LefRead lef_reader(&lef);
            require(lef_reader.createDb((fixture + "/bench.lef").c_str()), "LEF");
            IdbDefService source(lef.get_layout()); native_load(source, fixture + "/canonical.def");
            auto start = Clock::now();
            if (route == "native") { native_save(source, path); write.data = Metrics::ms(start, Clock::now()); }
            else {
                require(idb::edadb_adapter::initWriteDb(path.c_str()) >= 0, "init adapter");
                write.init = Metrics::ms(start, Clock::now());
                AdapterWriter writer(&source); start = Clock::now();
                require(writer.writeChip2Edadb(), "adapter write"); write.data = Metrics::ms(start, Clock::now());
                edadb::DbStatement probe; require(edadb::DbManager::i().initStatement(probe), "probe"); print_config(probe.db);
                start = Clock::now(); require(edadb::closeDatabase(), "close"); write.close = Metrics::ms(start, Clock::now());
            }
            IdbLefService read_lef; LefRead read_lef_parser(&read_lef);
            require(read_lef_parser.createDb((fixture + "/bench.lef").c_str()), "read LEF");
            IdbDefService restored(read_lef.get_layout());
            settle_read();
            require(prepare_cache(path, cache).verified, "app cache");
            start = Clock::now();
            if (route == "native") { native_load(restored, path); read.data = Metrics::ms(start, Clock::now()); }
            else {
                AdapterReader reader(&restored);
                require(idb::edadb_adapter::EdadbIdbHelper::setIdbDefService(&restored), "helper");
                start = Clock::now(); require(idb::edadb_adapter::initReadDb(path.c_str()) >= 0, "read init");
                read.init = Metrics::ms(start, Clock::now()); start = Clock::now();
                require(reader.createDbByEdadb(path.c_str()), "adapter read");
                read.data = Metrics::ms(start, Clock::now()); start = Clock::now();
                require(edadb::closeDatabase(), "close"); read.close = Metrics::ms(start, Clock::now());
            }
            // Native and adapter necessarily materialize iDB. Projection is outside timing.
            for (const auto& record : project(restored)) consumer.accept(record);
            if (verify) native_save(restored, path + ".restored.def");
        }
        require(consumer.seen == count, "count mismatch");
        require(consumer.digest == expected.digest, "digest mismatch");
        report("write", write, Consumer{false, input}); report("read", read, consumer);
        return 0;
    } catch (const std::exception& error) { std::cerr << "ERROR: " << error.what() << '\n'; return 1; }
}
