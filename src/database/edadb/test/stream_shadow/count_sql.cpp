// Independent diagnostic counters; never preload this library in timed samples.
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <sqlite3.h>

static unsigned long long prepares = 0, inserts = 0, begins = 0, commits = 0, rollbacks = 0;
extern "C" int sqlite3_prepare_v2(sqlite3* database, const char* sql, int bytes,
                                 sqlite3_stmt** statement, const char** tail) {
    using Function = int (*)(sqlite3*, const char*, int, sqlite3_stmt**, const char**);
    static auto real = reinterpret_cast<Function>(dlsym(RTLD_NEXT, "sqlite3_prepare_v2"));
    ++prepares;
    return real(database, sql, bytes, statement, tail);
}
extern "C" int sqlite3_step(sqlite3_stmt* statement) {
    using Function = int (*)(sqlite3_stmt*);
    static auto real = reinterpret_cast<Function>(dlsym(RTLD_NEXT, "sqlite3_step"));
    const char* sql = sqlite3_sql(statement);
    if (sql && std::strncmp(sql, "INSERT INTO", 11) == 0) ++inserts;
    return real(statement);
}
extern "C" int sqlite3_exec(sqlite3* database, const char* sql,
                            int (*callback)(void*, int, char**, char**), void* context, char** error) {
    using Function = int (*)(sqlite3*, const char*, int (*)(void*, int, char**, char**), void*, char**);
    static auto real = reinterpret_cast<Function>(dlsym(RTLD_NEXT, "sqlite3_exec"));
    if (sql && std::strncmp(sql, "BEGIN", 5) == 0) ++begins;
    if (sql && std::strncmp(sql, "COMMIT", 6) == 0) ++commits;
    if (sql && std::strncmp(sql, "ROLLBACK", 8) == 0) ++rollbacks;
    return real(database, sql, callback, context, error);
}
__attribute__((destructor)) static void report() {
    std::fprintf(stderr, "SQL_COUNTS prepare=%llu insert=%llu begin=%llu commit=%llu rollback=%llu\n",
                 prepares, inserts, begins, commits, rollbacks);
}
