#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../../.." && pwd)"
FIXTURE_DIR="$SCRIPT_DIR/fixtures/sky130_generated_via"
FIXTURE_DEF="$FIXTURE_DIR/generated_via.def"
RUN_TCL="$FIXTURE_DIR/run_irt_snapshot.tcl"
WRITE_TCL="$SCRIPT_DIR/../tcl/def2edadb_generic.tcl"
CHECKER="$SCRIPT_DIR/check_generated_via_fixture.py"
MANIFEST="$SCRIPT_DIR/create_manifest.py"
IEDA_BIN="${IEDA_BIN:-$REPO_ROOT/bin/iEDA}"
OUT_ROOT="${OUT_ROOT:-/tmp/iedadb_generated_via_fixture/$(date +%Y%m%d-%H%M%S)}"
EXPECT_KNOWN_DEFECT="${EXPECT_KNOWN_DEFECT:-0}"
MIN_PARALLEL_MEMORY_GIB="${MIN_PARALLEL_MEMORY_GIB:-48}"
FIXTURE_RUN_JOBS="${FIXTURE_RUN_JOBS:-auto}"
child_pids=()

cleanup_children() {
    local pid
    for pid in "${child_pids[@]}"; do
        kill "$pid" 2>/dev/null || true
    done
}
trap cleanup_children EXIT INT TERM

for required in "$IEDA_BIN" "$FIXTURE_DEF" "$RUN_TCL" "$WRITE_TCL" "$CHECKER" "$MANIFEST"; do
    [[ -f "$required" ]] || {
        echo "ERROR: missing required file: $required" >&2
        exit 1
    }
done
[[ "$EXPECT_KNOWN_DEFECT" == "0" || "$EXPECT_KNOWN_DEFECT" == "1" ]] || {
    echo "ERROR: EXPECT_KNOWN_DEFECT must be 0 or 1" >&2
    exit 1
}
[[ "$MIN_PARALLEL_MEMORY_GIB" =~ ^[1-9][0-9]*$ ]] || {
    echo "ERROR: MIN_PARALLEL_MEMORY_GIB must be positive" >&2
    exit 1
}
[[ "$FIXTURE_RUN_JOBS" == "auto" || "$FIXTURE_RUN_JOBS" == "1" || "$FIXTURE_RUN_JOBS" == "2" ]] || {
    echo "ERROR: FIXTURE_RUN_JOBS must be auto, 1, or 2" >&2
    exit 1
}

WORKSPACE="$REPO_ROOT/scripts/design/sky130_gcd"
export CONFIG_DIR="$WORKSPACE/iEDA_config"
export FOUNDRY_DIR="$WORKSPACE/../../foundry/sky130"
export TCL_SCRIPT_DIR="$WORKSPACE/script"
export DESIGN_TCL_SCRIPT_DIR="$TCL_SCRIPT_DIR"
export SDC_FILE="$FOUNDRY_DIR/sdc/gcd.sdc"

NATIVE_DIR="$OUT_ROOT/native"
EDADB_DIR="$OUT_ROOT/edadb"
WRITE_DIR="$OUT_ROOT/write"
EDADB_DB="$WRITE_DIR/edadb.db"
mkdir -p "$NATIVE_DIR" "$EDADB_DIR" "$WRITE_DIR"

run_snapshot() {
    local mode="$1"
    local result_dir="$2"
    local status=0
    INPUT_MODE="$mode" \
    INPUT_DEF="$FIXTURE_DEF" \
    RESULT_DIR="$result_dir" \
    EDADB_DB_PATH="$EDADB_DB" \
    RT_THREAD_NUMBER=1 \
        "$IEDA_BIN" -script "$RUN_TCL" >"$result_dir/run.log" 2>&1 || status=$?
    python3 "$MANIFEST" \
        --repo "$REPO_ROOT" \
        --output "$result_dir/manifest.json" \
        --dataset sky130_generated_via \
        --stage irt_input_snapshot \
        --mode "$mode" \
        --input "$FIXTURE_DEF" \
        --command "$IEDA_BIN -script $RUN_TCL" \
        --status "$status" \
        --config "$CONFIG_DIR/flow_config.json" \
        --config "$CONFIG_DIR/db_default_config.json"
    return "$status"
}

write_edadb() {
    local status=0
    INPUT_DEF="$FIXTURE_DEF" \
    EDADB_DB_PATH="$EDADB_DB" \
        "$IEDA_BIN" -script "$WRITE_TCL" >"$WRITE_DIR/run.log" 2>&1 || status=$?
    python3 "$MANIFEST" \
        --repo "$REPO_ROOT" \
        --output "$WRITE_DIR/manifest.json" \
        --dataset sky130_generated_via \
        --stage edadb_write \
        --mode edadb_write \
        --input "$FIXTURE_DEF" \
        --command "$IEDA_BIN -script $WRITE_TCL" \
        --status "$status" \
        --config "$CONFIG_DIR/flow_config.json" \
        --config "$CONFIG_DIR/db_default_config.json"
    return "$status"
}

available_kib="$(awk '/^MemAvailable:/ {print $2}' /proc/meminfo)"
available_gib=$((available_kib / 1024 / 1024))
fixture_jobs="$FIXTURE_RUN_JOBS"
if [[ "$fixture_jobs" == "auto" ]]; then
    if [[ "$(nproc)" -ge 4 && "$available_gib" -ge "$MIN_PARALLEL_MEMORY_GIB" ]]; then
        fixture_jobs=2
    else
        fixture_jobs=1
    fi
fi

if [[ "$fixture_jobs" -eq 2 ]]; then
    echo "==> Run independent native snapshot and EDADB write concurrently (available=${available_gib}GiB)"
    run_snapshot native "$NATIVE_DIR" &
    native_pid=$!
    child_pids+=("$native_pid")
    write_edadb &
    write_pid=$!
    child_pids+=("$write_pid")

    failed=0
    wait "$native_pid" || {
        echo "FAIL: native snapshot; see $NATIVE_DIR/run.log" >&2
        failed=1
    }
    wait "$write_pid" || {
        echo "FAIL: EDADB write; see $WRITE_DIR/run.log" >&2
        failed=1
    }
    child_pids=()
    [[ "$failed" -eq 0 ]] || exit 1
else
    echo "==> Run native snapshot and EDADB write serially (available=${available_gib}GiB)"
    run_snapshot native "$NATIVE_DIR" || {
        echo "FAIL: native snapshot; see $NATIVE_DIR/run.log" >&2
        exit 1
    }
    write_edadb || {
        echo "FAIL: EDADB write; see $WRITE_DIR/run.log" >&2
        exit 1
    }
fi

echo "==> Read EDADB in a fresh process and create iRT snapshot"
run_snapshot edadb "$EDADB_DIR" || {
    echo "FAIL: EDADB snapshot; see $EDADB_DIR/run.log" >&2
    exit 1
}

native_snapshot="$NATIVE_DIR/rt/data_manager/env_map.json"
edadb_snapshot="$EDADB_DIR/rt/data_manager/env_map.json"
checker_args=("$native_snapshot" "$edadb_snapshot")
if [[ "$EXPECT_KNOWN_DEFECT" == "1" ]]; then
    checker_args+=(--expect-known-defect)
fi

python3 "$CHECKER" "${checker_args[@]}"
echo "Artifacts: $OUT_ROOT"
