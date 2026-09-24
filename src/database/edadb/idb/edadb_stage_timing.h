#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace idb::edadb_adapter::stage_timing {

enum class Phase : unsigned { Init, Create, Data, Count };
using Clock = std::chrono::steady_clock;

struct State {
    bool active = false;
    std::array<int64_t, static_cast<unsigned>(Phase::Count)> elapsed_ns{};
};

inline thread_local State state;

// One command owns the counters. No per-row timers or output are introduced.
// The default-off switch is independent of EDADB_ENABLE_PROFILING.
class Session {
public:
    explicit Session(const char* operation) : operation_(operation) {
        const char* enabled = std::getenv("EDADB_STAGE_TIMING");
        if (!state.active && enabled != nullptr && std::strcmp(enabled, "1") == 0) {
            state = State{};
            state.active = true;
            owner_ = true;
            start_ = Clock::now();
        }
    }

    ~Session() {
        if (!owner_) {
            return;
        }
        const auto total = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start_).count();
        state.active = false;
        const char* names[] = {"init", "create", operation_};
        int64_t accounted = 0;
        for (unsigned index = 0; index < static_cast<unsigned>(Phase::Count); ++index) {
            accounted += state.elapsed_ns[index];
            report(names[index], state.elapsed_ns[index]);
        }
        report("other", total - accounted);
        report("command", total);
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
        }
    }

    ~ScopedTimer() {
        if (active_) {
            state.elapsed_ns[static_cast<unsigned>(phase_)] +=
                std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start_).count();
        }
    }

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    Phase phase_;
    bool active_;
    Clock::time_point start_{};
};

} // namespace idb::edadb_adapter::stage_timing
