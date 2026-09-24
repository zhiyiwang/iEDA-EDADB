// Test-only LD_PRELOAD shim: make the second Net insert fail inside real SQLite.
// Production binaries and adapter code remain unchanged. Never use for performance.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <sqlite3.h>

using Step = int (*)(sqlite3_stmt*);
using Exec = int (*)(sqlite3*, const char*, int (*)(void*, int, char**, char**), void*, char**);
static Step real_step = reinterpret_cast<Step>(dlsym(RTLD_NEXT, "sqlite3_step"));
static Exec real_exec = reinterpret_cast<Exec>(dlsym(RTLD_NEXT, "sqlite3_exec"));
static bool injected = false;
static int net_inserts = 0;
static bool conversion_mode = std::getenv("ADAPTER_CONVERSION_FAULT") != nullptr;

static int count_rows(sqlite3* database, const char* sql) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) != SQLITE_OK)
        std::abort();
    if (real_step(statement) != SQLITE_ROW) std::abort();
    int count = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    return count;
}

extern "C" int sqlite3_step(sqlite3_stmt* statement) {
    const char* sql = sqlite3_sql(statement);
    if (conversion_mode) {
        return real_step(statement);
    }
    if (!injected && sql && std::strstr(sql, "INSERT INTO \"iNetSD\"")) {
        if (++net_inserts == 2) {
            injected = true;
            sqlite3* database = sqlite3_db_handle(statement);
            int instances = count_rows(database, "SELECT count(*) FROM iInstSD");
            int nets = count_rows(database, "SELECT count(*) FROM iNetSD");
            if (sqlite3_get_autocommit(database) || instances <= 0 || nets != 1) std::abort();
            // ABORT fails only this SQL statement, not the outer transaction.
            // Therefore removing prior root rows requires the adapter's ROLLBACK.
            if (real_exec(database,
                    "CREATE TEMP TRIGGER adapter_test_abort BEFORE INSERT ON iNetSD "
                    "BEGIN SELECT RAISE(ABORT, 'adapter acceptance injected failure'); END",
                    nullptr, nullptr, nullptr) != SQLITE_OK) std::abort();
            int result = real_step(statement);
            std::fprintf(stderr, "FAULT fired instances=%d nets=%d autocommit=%d result=%d\n",
                         instances, nets, sqlite3_get_autocommit(database), result);
            if ((result & 255) != SQLITE_CONSTRAINT) std::abort();
            return result;
        }
    }
    return real_step(statement);
}

extern "C" int sqlite3_exec(sqlite3* database, const char* sql,
                            int (*callback)(void*, int, char**, char**), void* argument, char** error) {
    if (conversion_mode && sql && std::strstr(sql, "ROLLBACK")) {
        int instances = count_rows(database, "SELECT count(*) FROM iInstSD");
        int nets = count_rows(database, "SELECT count(*) FROM iNetSD");
        if (instances <= 0 || nets != 1 || sqlite3_get_autocommit(database)) std::abort();
        std::fprintf(stderr, "FAULT conversion rollback pending instances=%d nets=%d\n", instances, nets);
        injected = true;
    }
    int result = real_exec(database, sql, callback, argument, error);
    if (injected && sql && std::strstr(sql, "ROLLBACK")) {
        int instances = count_rows(database, "SELECT count(*) FROM iInstSD");
        int nets = count_rows(database, "SELECT count(*) FROM iNetSD");
        std::fprintf(stderr, "FAULT rollback result=%d autocommit=%d instances=%d nets=%d\n",
                     result, sqlite3_get_autocommit(database), instances, nets);
        if (result != SQLITE_OK || !sqlite3_get_autocommit(database) || instances || nets) std::abort();
    }
    return result;
}
