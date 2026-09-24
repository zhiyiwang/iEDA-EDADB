#include "edadb_stage_timing.h"

#include <cassert>
#include <thread>

namespace timing = idb::edadb_adapter::stage_timing;

void run_command(const char* operation) {
    timing::Session session(operation);
    {
        timing::ScopedTimer timer(timing::Phase::Init);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    {
        timing::ScopedTimer timer(timing::Phase::Data);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int main() {
    // OFF must neither read clocks inside scopes nor accumulate stage durations.
    unsetenv("EDADB_STAGE_TIMING");
    run_command("disabled");
    assert(!timing::state.active);
    for (const auto elapsed : timing::state.elapsed_ns) {
        assert(elapsed == 0);
    }

    setenv("EDADB_STAGE_TIMING", "1", 1);
    run_command("write");
    assert(!timing::state.active);
    assert(timing::state.elapsed_ns[static_cast<unsigned>(timing::Phase::Init)] > 0);
    run_command("read");
    assert(!timing::state.active);
    assert(timing::state.elapsed_ns[static_cast<unsigned>(timing::Phase::Create)] == 0);

    // A return through an error path must still close both scopes safely.
    {
        timing::Session session("early_return");
        const auto operation = []() {
            timing::ScopedTimer timer(timing::Phase::Data);
            return false;
        };
        assert(!operation());
    }
    assert(!timing::state.active);
}
