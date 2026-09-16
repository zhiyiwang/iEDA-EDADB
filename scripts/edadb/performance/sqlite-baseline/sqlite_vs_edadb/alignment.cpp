// Isolated control experiment: baseline sources and production APIs are unchanged.
#include "../benchmark/benchmark_support.h"
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


// Direct aligned SQL is generated once OUTSIDE the data timer. EDADB still
// generates/prepares its own SQL inside the unchanged operation lifecycle.
std::string aligned_insert, aligned_select;
bool checks_enabled = false, alignment_enabled = false;

// Correctness runs only: observe the SQL actually executed, without expanded
// parameter strings or trace overhead in performance samples.
std::string expected_sql;
bool observed_sql_matches = true;
size_t observed_statements = 0;
int check_sql(unsigned, void*, void* statement, void*) {
    const char* sql = sqlite3_sql(static_cast<sqlite3_stmt*>(statement));
    observed_sql_matches = observed_sql_matches && sql && expected_sql == sql;
    ++observed_statements;
    return 0;
}

void begin_sql_check(sqlite3* database, bool verify, bool edadb_route, bool write) {
    if (!verify) return;
    expected_sql = (edadb_route || alignment_enabled) ? (write ? aligned_insert : aligned_select) :
        (write ? "INSERT INTO component VALUES(?,?,?,?,?,?,?,?)" :
         "SELECT name,master_name,source,status,orient,x,y,record_order FROM component");
    observed_sql_matches = true;
    observed_statements = 0;
    require(sqlite3_trace_v2(database, SQLITE_TRACE_STMT, check_sql, nullptr) == SQLITE_OK, "trace check");
}

void end_sql_check(sqlite3* database, bool verify, size_t expected_count) {
    if (!verify) return;
    require(sqlite3_trace_v2(database, 0, nullptr, nullptr) == SQLITE_OK, "trace stop");
    require(observed_sql_matches && observed_statements == expected_count, "executed SQL mismatch");
    std::cout << "CHECK\tSQL statements\t" << observed_statements << '\n';
}

template <bool Align>
void fetch_string(sqlite3_stmt* statement, int column, std::string& value) {
    if constexpr (Align) {
        const uint32_t size = sqlite3_column_bytes(statement, column);
        const char* text = reinterpret_cast<const char*>(sqlite3_column_text(statement, column));
        value.assign(text, size);
    } else {
        value.assign(reinterpret_cast<const char*>(sqlite3_column_text(statement, column)),
                     sqlite3_column_bytes(statement, column));
    }
}
template <bool Checks, bool Align>
void controlled_operation(sqlite3* database, bool edadb_route, bool write,
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
    const char* sql = Align ? (write ? aligned_insert.c_str() : aligned_select.c_str()) :
        (write ? "INSERT INTO component VALUES(?,?,?,?,?,?,?,?)" :
         "SELECT name,master_name,source,status,orient,x,y,record_order FROM component");
    require(sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) == SQLITE_OK, "prepare");
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> guard(statement, sqlite3_finalize);
    if (write) {
        // Select controls once. No runtime experiment branch inside the loop.
        auto insert = [&]<bool MatchBindings, bool ClearBindings>() {
        for (const auto& record : input) {
            if constexpr (MatchBindings) {
                require(sqlite3_bind_text64(statement, 1, record.name.data(), record.name.size(), SQLITE_TRANSIENT, SQLITE_UTF8) == SQLITE_OK, "bind");
                require(sqlite3_bind_text64(statement, 2, record.master_name.data(), record.master_name.size(), SQLITE_TRANSIENT, SQLITE_UTF8) == SQLITE_OK, "bind");
                require(sqlite3_bind_int(statement, 3, record.source) == SQLITE_OK, "bind");
                require(sqlite3_bind_int(statement, 4, record.status) == SQLITE_OK, "bind");
                require(sqlite3_bind_int(statement, 5, record.orient) == SQLITE_OK, "bind");
                require(sqlite3_bind_int(statement, 6, record.x) == SQLITE_OK, "bind");
                require(sqlite3_bind_int(statement, 7, record.y) == SQLITE_OK, "bind");
                require(sqlite3_bind_int64(statement, 8, record.record_order) == SQLITE_OK, "bind");
            } else {
            require(sqlite3_bind_text(statement, 1, record.name.data(), record.name.size(), SQLITE_TRANSIENT) == SQLITE_OK, "bind");
            require(sqlite3_bind_text(statement, 2, record.master_name.data(), record.master_name.size(), SQLITE_TRANSIENT) == SQLITE_OK, "bind");
            std::array<int64_t, 6> values{record.source, record.status, record.orient, record.x, record.y, record.record_order};
            for (size_t column = 0; column < values.size(); ++column)
                require(sqlite3_bind_int64(statement, column + 3, values[column]) == SQLITE_OK, "bind");
            }
            require(sqlite3_step(statement) == SQLITE_DONE, "insert");
            // Match EDADB resetForReuse: clear first, then reset.
            if constexpr (ClearBindings)
                require(sqlite3_clear_bindings(statement) == SQLITE_OK, "clear bindings");
            require(sqlite3_reset(statement) == SQLITE_OK, "reset");
        }
        };
        insert.template operator()<Align, Checks>();
    } else {
        // Dispatch once, outside the row loop. The control instantiation has no
        // additional per-field branch; the checked one mirrors EDADB NULL guards.
        auto fetch = [&]<bool CheckNull>() {
            ComponentRecord record;
            int result;
            while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
                if (!CheckNull || sqlite3_column_type(statement, 0) != SQLITE_NULL)
                    fetch_string<Align>(statement, 0, record.name);
                if (!CheckNull || sqlite3_column_type(statement, 1) != SQLITE_NULL)
                    fetch_string<Align>(statement, 1, record.master_name);
                if (!CheckNull || sqlite3_column_type(statement, 2) != SQLITE_NULL)
                    record.source = sqlite3_column_int(statement, 2);
                if (!CheckNull || sqlite3_column_type(statement, 3) != SQLITE_NULL)
                    record.status = sqlite3_column_int(statement, 3);
                if (!CheckNull || sqlite3_column_type(statement, 4) != SQLITE_NULL)
                    record.orient = sqlite3_column_int(statement, 4);
                if (!CheckNull || sqlite3_column_type(statement, 5) != SQLITE_NULL)
                    record.x = sqlite3_column_int(statement, 5);
                if (!CheckNull || sqlite3_column_type(statement, 6) != SQLITE_NULL)
                    record.y = sqlite3_column_int(statement, 6);
                if (!CheckNull || sqlite3_column_type(statement, 7) != SQLITE_NULL)
                    record.record_order = sqlite3_column_int64(statement, 7);
                consumer.accept(record);
            }
            require(result == SQLITE_DONE, "read");
        };
        fetch.template operator()<Checks>();
        // READ_ALL performs this once at DONE, not once per row.
        if constexpr (Align) {
            require(sqlite3_clear_bindings(statement) == SQLITE_OK, "read clear");
            require(sqlite3_reset(statement) == SQLITE_OK, "read reset");
        }
    }
    require(sqlite3_finalize(guard.release()) == SQLITE_OK, "finalize");
}

void data_operation(sqlite3* database, bool edadb_route, bool write,
                    std::vector<ComponentRecord>& input, Consumer& consumer) {
    // Select once per whole operation, never add a per-row experiment branch.
    if (checks_enabled && alignment_enabled)
        controlled_operation<true, true>(database, edadb_route, write, input, consumer);
    else if (checks_enabled)
        controlled_operation<true, false>(database, edadb_route, write, input, consumer);
    else if (alignment_enabled)
        controlled_operation<false, true>(database, edadb_route, write, input, consumer);
    else
        controlled_operation<false, false>(database, edadb_route, write, input, consumer);
}

int main(int argc, char** argv) {
    try {
        require(argc == 9, "usage: alignment ROUTE CONFIG COUNT PATH FIXTURE check|perf CACHE raw|checks|other|aligned|edadb");
        std::string route = argv[1], config = argv[2], path = argv[4], fixture = argv[5], cache = argv[7];
        const std::string variant = argv[8];
        require(member(variant, {"raw", "checks", "other", "aligned", "edadb"}), "variant");
        require((route == "edadb" && variant == "edadb") || (route == "sqlite" && variant != "edadb"), "route/variant");
        checks_enabled = variant == "checks" || variant == "aligned";
        alignment_enabled = variant == "other" || variant == "aligned";
        edadb::Cpp2SqlTypeTrait<ComponentRecord>::hasPrimKey = false;
        const auto* definition = dynamic_cast<const edadb::DbTableDef<ComponentRecord>*>(
            edadb::detail::tableDef<ComponentRecord>());
        require(definition != nullptr, "table definition");
        aligned_insert = edadb::SqlStatement<ComponentRecord>::insertPlaceHolderStatement(*definition);
        aligned_select = edadb::SqlStatement<ComponentRecord>::readAllStatement(*definition);
        std::cout << "SQL\tinsert\t" << aligned_insert << "\nSQL\tselect\t" << aligned_select << '\n';
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
            begin_sql_check(database, verify, use_edadb, true);
            start = Clock::now(); data_operation(database, use_edadb, true, input, consumer);
            write.data = Metrics::ms(start, Clock::now());
            end_sql_check(database, verify, count);
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
            begin_sql_check(database, verify, use_edadb, false);
            start = Clock::now(); data_operation(database, use_edadb, false, input, consumer);
            read.data = Metrics::ms(start, Clock::now());
            end_sql_check(database, verify, 1);
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
        }
        require(consumer.seen == count, "count mismatch");
        require(consumer.digest == expected.digest, "digest mismatch");
        report("write", write, Consumer{false, input}); report("read", read, consumer);
        return 0;
    } catch (const std::exception& error) { std::cerr << "ERROR: " << error.what() << '\n'; return 1; }
}
