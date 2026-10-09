// Diagnostic-only preload. Never use its elapsed times as formal performance samples.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <map>
#include <string>
#include <time.h>
#include <sys/resource.h>
#include <vector>
#include <sqlite3.h>

namespace {
template <typename Function> Function next(const char* name) {
    auto address = reinterpret_cast<Function>(dlsym(RTLD_NEXT, name));
    if (!address) std::abort();
    return address;
}
long long timestamp() {
    timespec time{};
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec * 1000000000LL + time.tv_nsec;
}
#ifndef MARKERS_ONLY
struct Statement {
    std::string phase;
    std::string sql;
    unsigned resets = 0;
};
struct Diagnostics {
    sqlite3* database = nullptr;
    std::vector<std::string> phases;
    std::map<sqlite3_stmt*, Statement> statements;
    std::map<std::pair<std::string, std::string>, std::array<long long, 5>> totals;
    std::map<std::pair<std::string, std::string>, long long> calls;
};
Diagnostics& state() { static auto* value = new Diagnostics; return *value; }
std::string phase() { return state().phases.empty() ? "outside" : state().phases.back(); }
void count(const char* api) { ++state().calls[{phase(), api}]; }
void collect(sqlite3_stmt* statement) {
    auto found = state().statements.find(statement);
    if (found == state().statements.end()) return;
    const int codes[] = {SQLITE_STMTSTATUS_RUN, SQLITE_STMTSTATUS_VM_STEP,
        SQLITE_STMTSTATUS_FULLSCAN_STEP, SQLITE_STMTSTATUS_SORT, SQLITE_STMTSTATUS_REPREPARE};
    auto& total = state().totals[{found->second.phase, found->second.sql}];
    for (unsigned index = 0; index < total.size(); ++index) {
        const int value = sqlite3_stmt_status(statement, codes[index], 1);
        if (value < 0) std::abort();
        total[index] += value;
    }
}
void snapshot(const char* operation, const char* name, int entering) {
    if (!state().database) return;
    const int codes[] = {SQLITE_DBSTATUS_CACHE_HIT, SQLITE_DBSTATUS_CACHE_MISS,
        SQLITE_DBSTATUS_CACHE_WRITE, SQLITE_DBSTATUS_CACHE_SPILL, SQLITE_DBSTATUS_CACHE_USED};
    std::fprintf(stderr, "DBSTATUS\t%s\t%s\t%d", operation, name, entering);
    for (int code : codes) {
        int current = 0, high = 0;
        if (sqlite3_db_status(state().database, code, &current, &high, 0) != SQLITE_OK) std::abort();
        std::fprintf(stderr, "\t%d", current);
    }
    std::fprintf(stderr, "\n");
}
void configuration() {
    if (!state().database) return;
    auto prepare = next<decltype(&sqlite3_prepare_v2)>("sqlite3_prepare_v2");
    auto step = next<decltype(&sqlite3_step)>("sqlite3_step");
    auto finalize = next<decltype(&sqlite3_finalize)>("sqlite3_finalize");
    std::fprintf(stderr, "SQLITE_VERSION\t%s\t%s\n", sqlite3_libversion(), sqlite3_sourceid());
    for (const char* pragma : {"journal_mode", "synchronous", "foreign_keys", "cache_size", "page_size", "mmap_size"}) {
        sqlite3_stmt* statement = nullptr;
        const std::string sql = std::string("PRAGMA ") + pragma;
        if (prepare(state().database, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK) std::abort();
        if (step(statement) == SQLITE_ROW) {
            std::fprintf(stderr, "CONFIG\t%s\t%s\n", pragma, sqlite3_column_text(statement, 0));
        }
        finalize(statement);
    }
}
#endif
}

extern "C" void edadb_profile_boundary(const char* operation, const char* name, int entering) {
    timespec wall{};
    clock_gettime(CLOCK_REALTIME, &wall);
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    std::fprintf(stderr, "BOUNDARY\t%s\t%s\t%d\t%lld\t%lld\t%lld\t%lld\n", operation, name, entering, timestamp(),
                 wall.tv_sec * 1000000000LL + wall.tv_nsec,
                 usage.ru_utime.tv_sec * 1000000000LL + usage.ru_utime.tv_usec * 1000LL,
                 usage.ru_stime.tv_sec * 1000000000LL + usage.ru_stime.tv_usec * 1000LL);
#ifndef MARKERS_ONLY
    snapshot(operation, name, entering);
    if (entering) state().phases.emplace_back(name);
    else {
        if (state().phases.empty() || state().phases.back() != name) std::abort();
        state().phases.pop_back();
        if (std::strcmp(name, "command") == 0) configuration();
    }
#endif
}

#ifndef MARKERS_ONLY
extern "C" int sqlite3_open(const char* path, sqlite3** database) {
    static auto real = next<decltype(&sqlite3_open)>("sqlite3_open");
    count("open");
    const int result = real(path, database);
    if (result == SQLITE_OK) state().database = *database;
    return result;
}
extern "C" int sqlite3_close_v2(sqlite3* database) {
    static auto real = next<decltype(&sqlite3_close_v2)>("sqlite3_close_v2");
    count("close");
    if (state().database == database) state().database = nullptr;
    return real(database);
}
extern "C" int sqlite3_prepare_v2(sqlite3* database, const char* sql, int bytes, sqlite3_stmt** statement, const char** tail) {
    static auto real = next<decltype(&sqlite3_prepare_v2)>("sqlite3_prepare_v2");
    count("prepare");
    const int result = real(database, sql, bytes, statement, tail);
    if (result == SQLITE_OK && *statement) {
        std::string text = sqlite3_sql(*statement);
        std::replace(text.begin(), text.end(), '\n', ' ');
        std::replace(text.begin(), text.end(), '\t', ' ');
        state().statements[*statement] = {phase(), text, 0};
    }
    return result;
}
extern "C" int sqlite3_finalize(sqlite3_stmt* statement) {
    static auto real = next<decltype(&sqlite3_finalize)>("sqlite3_finalize");
    count("finalize");
    collect(statement);
    state().statements.erase(statement);
    return real(statement);
}
extern "C" int sqlite3_reset(sqlite3_stmt* statement) {
    static auto real = next<decltype(&sqlite3_reset)>("sqlite3_reset");
    count("reset");
    auto found = state().statements.find(statement);
    // Bound accumulation on reused statements; indexed fixtures have short executions.
    if (found != state().statements.end() && ++found->second.resets % 1000 == 0) collect(statement);
    return real(statement);
}
extern "C" int sqlite3_exec(sqlite3* database, const char* sql, int (*callback)(void*,int,char**,char**), void* argument, char** error) {
    static auto real = next<decltype(&sqlite3_exec)>("sqlite3_exec");
    count("exec");
    std::fprintf(stderr, "EXEC\t%s\t%s\n", phase().c_str(), sql);
    return real(database, sql, callback, argument, error);
}
#define WRAP(name, result, signature, arguments) \
extern "C" result name signature { \
    static auto real = next<decltype(&name)>(#name); \
    count(#name); \
    return real arguments; \
}
WRAP(sqlite3_step, int, (sqlite3_stmt* statement), (statement))
WRAP(sqlite3_clear_bindings, int, (sqlite3_stmt* statement), (statement))
WRAP(sqlite3_bind_int, int, (sqlite3_stmt* statement, int index, int value), (statement, index, value))
WRAP(sqlite3_bind_int64, int, (sqlite3_stmt* statement, int index, sqlite3_int64 value), (statement, index, value))
WRAP(sqlite3_bind_double, int, (sqlite3_stmt* statement, int index, double value), (statement, index, value))
WRAP(sqlite3_bind_null, int, (sqlite3_stmt* statement, int index), (statement, index))
WRAP(sqlite3_bind_text64, int, (sqlite3_stmt* statement, int index, const char* value, sqlite3_uint64 bytes, void (*destroy)(void*), unsigned char encoding), (statement, index, value, bytes, destroy, encoding))
WRAP(sqlite3_bind_text, int, (sqlite3_stmt* statement, int index, const char* value, int bytes, void (*destroy)(void*)), (statement, index, value, bytes, destroy))
WRAP(sqlite3_column_type, int, (sqlite3_stmt* statement, int index), (statement, index))
WRAP(sqlite3_column_text, const unsigned char*, (sqlite3_stmt* statement, int index), (statement, index))
WRAP(sqlite3_column_bytes, int, (sqlite3_stmt* statement, int index), (statement, index))
WRAP(sqlite3_column_int, int, (sqlite3_stmt* statement, int index), (statement, index))
WRAP(sqlite3_column_int64, sqlite3_int64, (sqlite3_stmt* statement, int index), (statement, index))
WRAP(sqlite3_column_double, double, (sqlite3_stmt* statement, int index), (statement, index))
#undef WRAP
__attribute__((destructor)) static void report() {
    for (const auto& entry : state().statements) collect(entry.first);
    for (const auto& entry : state().totals) {
        std::fprintf(stderr, "STMT\t%s", entry.first.first.c_str());
        for (auto value : entry.second) std::fprintf(stderr, "\t%lld", value);
        std::fprintf(stderr, "\t%s\n", entry.first.second.c_str());
    }
    for (const auto& entry : state().calls) {
        std::fprintf(stderr, "API\t%s\t%s\t%lld\n", entry.first.first.c_str(), entry.first.second.c_str(), entry.second);
    }
}
#endif
