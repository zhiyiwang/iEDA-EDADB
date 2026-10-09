#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" void edadb_profile_boundary(const char*, const char*, int) __attribute__((weak));

namespace idb::edadb_adapter::stage_timing {

enum class Phase : unsigned { Init, Create, Data, Count };
using Clock = std::chrono::steady_clock;

struct State {
    bool active = false;
    const char* operation = nullptr;
    std::array<int64_t, static_cast<unsigned>(Phase::Count)> elapsed_ns{};
    struct Detail {
        const char* name = nullptr;
        int64_t elapsed_ns = 0;
        unsigned calls = 0;
    };
    std::array<Detail, 64> details{};
    unsigned detail_count = 0;
};

inline thread_local State state;

inline void boundary(const char* name, int entering) {
    if (edadb_profile_boundary != nullptr) {
        edadb_profile_boundary(state.operation, name, entering);
    }
}

inline const char* phaseName(Phase phase) {
    return phase == Phase::Init ? "init" : phase == Phase::Create ? "create" : "data";
}

// One command owns the counters. No per-row timers or output are introduced.
// The default-off switch is independent of EDADB_ENABLE_PROFILING.
class Session {
public:
    explicit Session(const char* operation) : operation_(operation) {
        const char* enabled = std::getenv("EDADB_STAGE_TIMING");
        if (!state.active && enabled != nullptr && std::strcmp(enabled, "1") == 0) {
            state = State{};
            state.active = true;
            state.operation = operation;
            owner_ = true;
            start_ = Clock::now();
            boundary("command", 1);
        }
    }

    ~Session() {
        if (!owner_) {
            return;
        }
        const auto total = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start_).count();
        boundary("command", 0);
        state.active = false;
        const char* names[] = {"init", "create", operation_};
        int64_t accounted = 0;
        for (unsigned index = 0; index < static_cast<unsigned>(Phase::Count); ++index) {
            accounted += state.elapsed_ns[index];
            report(names[index], state.elapsed_ns[index]);
        }
        report("other", total - accounted);
        report("command", total);
        for (unsigned index = 0; index < state.detail_count; ++index) {
            const auto& detail = state.details[index];
            std::printf("EDADB_DETAIL\t%s\t%s\tns\t%lld\t%u\n", operation_, detail.name,
                        static_cast<long long>(detail.elapsed_ns), detail.calls);
        }
    }

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

private:
    void report(const char* phase, int64_t duration_ns) const {
        // Stop all timers before printing; the outer Tcl timer still includes reporting.
        std::printf("EDADB_STAGE\t%s\t%s\tns\t%lld\n", operation_, phase,
                    static_cast<long long>(duration_ns));
    }

    const char* operation_;
    bool owner_ = false;
    Clock::time_point start_{};
};

// Stage scopes must not overlap: command = init + create + data + other.
class ScopedTimer {
public:
    explicit ScopedTimer(Phase phase) : phase_(phase), active_(state.active) {
        if (active_) {
            start_ = Clock::now();
            boundary(phaseName(phase_), 1);
        }
    }

    ~ScopedTimer() {
        if (active_) {
            state.elapsed_ns[static_cast<unsigned>(phase_)] +=
                std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start_).count();
            boundary(phaseName(phase_), 0);
        }
    }

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    Phase phase_;
    bool active_;
    Clock::time_point start_{};
};

// Coarse nested observations only: details are contained in stages, never added to them.
// Names must be stable string literals; no allocation, output or timer inside row loops.
class DetailTimer {
public:
    explicit DetailTimer(const char* name) : name_(name), active_(state.active) {
        if (!active_) return;
        for (index_ = 0; index_ < state.detail_count; ++index_) {
            if (std::strcmp(state.details[index_].name, name) == 0) break;
        }
        if (index_ == state.detail_count) {
            if (state.detail_count == state.details.size()) std::abort();
            state.details[state.detail_count++].name = name;
        }
        start_ = Clock::now();
        boundary(name_, 1);
    }
    ~DetailTimer() {
        if (!active_) return;
        auto& detail = state.details[index_];
        detail.elapsed_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start_).count();
        ++detail.calls;
        boundary(name_, 0);
    }
    DetailTimer(const DetailTimer&) = delete;
    DetailTimer& operator=(const DetailTimer&) = delete;
private:
    const char* name_;
    bool active_;
    unsigned index_ = 0;
    Clock::time_point start_{};
};

template <typename Function>
auto observe(const char* name, Function&& function) {
    DetailTimer timer(name);
    return function();
}

} // namespace idb::edadb_adapter::stage_timing
