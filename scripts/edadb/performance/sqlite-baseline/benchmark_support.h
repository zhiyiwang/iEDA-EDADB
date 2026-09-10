#pragma once
// Shared helpers for the current benchmark, without historical entry points.
#include <sqlite3.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
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
#include <tuple>
#include <vector>
#include "def_read.h"
#include "def_write.h"
#include "lef_read.h"
#include "edadb.h"
#include "def_read_edadb.h"
#include "def_write_edadb.h"
#include "edadb_idb_helper.h"

using namespace idb;
using Clock = std::chrono::steady_clock;

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

bool member(const std::string& value, std::initializer_list<std::string_view> values) {
    for (auto candidate : values) if (candidate == value) return true;
    return false;
}

struct Metrics {
    static double ms(Clock::time_point first, Clock::time_point last) {
        return std::chrono::duration<double, std::milli>(last - first).count();
    }
};

// File/page-cache preparation is outside every operation timer. mincore does
// not touch the mapping, so verification itself does not warm file data pages.
struct CacheState {
    int64_t pages = 0, resident = 0, attempts = 0;
    bool verified = true;
};

CacheState prepare_cache(const std::string& path, const std::string& mode) {
    CacheState state;
    if (mode == "none") return state;
    int descriptor = open(path.c_str(), O_RDONLY);
    require(descriptor >= 0, "cannot open cache input");
    struct stat information{};
    if (fstat(descriptor, &information) != 0 || information.st_size <= 0) {
        close(descriptor);
        throw std::runtime_error("invalid cache input size");
    }
    auto size = static_cast<size_t>(information.st_size);
    long page_size = sysconf(_SC_PAGESIZE);
    state.pages = (size + page_size - 1) / page_size;
    std::vector<unsigned char> residency(state.pages);
    std::array<char, 1024 * 1024> buffer{};
    for (int attempt = 1; attempt <= 3; ++attempt) {
        state.attempts = attempt;
        bool prepared = true;
        if (mode == "warm") {
            prepared = lseek(descriptor, 0, SEEK_SET) == 0;
            ssize_t count = 0;
            while (prepared && (count = ::read(descriptor, buffer.data(), buffer.size())) > 0) {}
            prepared = prepared && count == 0;
        } else {
            prepared = fsync(descriptor) == 0 && posix_fadvise(descriptor, 0, 0, POSIX_FADV_DONTNEED) == 0;
        }
        void* mapping = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, descriptor, 0);
        bool checked = mapping != MAP_FAILED;
        if (checked) {
            checked = mincore(mapping, size, residency.data()) == 0;
            munmap(mapping, size);
        }
        state.resident = 0;
        if (checked) for (auto page : residency) state.resident += (page & 1) != 0;
        state.verified = prepared && checked && (mode == "warm" ? state.resident == state.pages : state.resident == 0);
        if (state.verified) break;
    }
    close(descriptor);
    return state;
}

void native_load(IdbDefService& service, const std::string& path) {
    DefRead reader(&service);
    require(reader.createDb(path.c_str()), "native DefRead failed");
}

void native_save(IdbDefService& service, const std::string& path) {
    DefWrite writer(&service);
    // This branch returns fclose's 0 as bool false on SUCCESS. Keep original
    // code unchanged; runner additionally checks file, END DESIGN and content.
    require(!writer.writeDb(path.c_str()), "native DefWrite close failed");
    require(std::filesystem::is_regular_file(path), "native output missing");
}

struct ComponentRecord {
    std::string name, master_name;
    int32_t source, status, orient, x, y;
    int64_t record_order;
    auto fields() const { return std::tie(name, master_name, source, status, orient, x, y, record_order); }
};
TABLE4CLASS(ComponentRecord, "component", (name, master_name, source, status, orient, x, y, record_order));

// Expose the EXISTING chip writer, without copying its implementation.
// This matches writeDb2Edadb(kChip)'s init + dispatch and checks the actual result.
class AdapterWriter : public idb::DefWriteEdadb {
public:
    using idb::DefWriteEdadb::DefWriteEdadb;
    using idb::DefWriteEdadb::writeChip2Edadb;
};

class AdapterReader : public idb::DefReadEdadb {
public:
    using idb::DefReadEdadb::DefReadEdadb;
    using idb::DefReadEdadb::createDbByEdadb;
};

std::vector<ComponentRecord> generate(size_t count) {
    std::vector<ComponentRecord> records;
    records.reserve(count);
    for (size_t index = 0; index < count; ++index) {
        std::ostringstream name;
        name << 'U' << std::setfill('0') << std::setw(7) << index;
        records.push_back({name.str(), "bench_cell", static_cast<int>(IdbInstanceType::kDist),
            static_cast<int>(IdbPlacementStatus::kPlaced), static_cast<int>(IdbOrient::kN_R0),
            static_cast<int32_t>(index % 1000 * 100), static_cast<int32_t>(index / 1000 * 100),
            static_cast<int64_t>(index)});
    }
    return records;
}

void exec_sql(sqlite3* database, const std::string& sql) {
    char* error = nullptr;
    int status = sqlite3_exec(database, sql.c_str(), nullptr, nullptr, &error);
    std::string message = error ? error : "";
    sqlite3_free(error);
    require(status == SQLITE_OK, "SQL: " + message);
}
int pragma_int(sqlite3* database, const char* sql) {
    sqlite3_stmt* statement = nullptr;
    require(sqlite3_prepare_v2(database, sql, -1, &statement, nullptr) == SQLITE_OK, "pragma prepare");
    require(sqlite3_step(statement) == SQLITE_ROW, "pragma row");
    int value = sqlite3_column_int(statement, 0);
    require(sqlite3_finalize(statement) == SQLITE_OK, "pragma finalize");
    return value;
}
std::vector<ComponentRecord> project(IdbDefService& service) {
    std::vector<ComponentRecord> records;
    for (auto* instance : service.get_design()->get_instance_list()->get_instance_list())
        records.push_back({instance->get_name(), instance->get_cell_master()->get_name(),
            static_cast<int>(instance->get_type()), static_cast<int>(instance->get_status()),
            static_cast<int>(instance->get_orient()), instance->get_coordinate()->get_x(),
            instance->get_coordinate()->get_y(), static_cast<int64_t>(records.size())});
    return records;
}
