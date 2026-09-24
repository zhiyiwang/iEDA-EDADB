#!/usr/bin/env bash
# Compare the pre-EDADB native binary with the current implementation.
# Usage: bash run.sh ORIGINAL_IEDA CURRENT_IEDA INPUT_DEF
# OUT_DIR must be new. Run after all compilation has stopped.
# Timing uses the unchanged benchmark.tcl; this experiment measures warm cache.
set -euo pipefail
[[ $# == 3 ]] || { echo "Usage: $0 ORIGINAL_IEDA CURRENT_IEDA INPUT_DEF"; exit 2; }
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
ORIGINAL_BIN="$(realpath "$1")"
CURRENT_BIN="$(realpath "$2")"
export INPUT_DEF="$(realpath "$3")"
OUT_DIR="${OUT_DIR:-/tmp/iedadb_native_original_validation}"
RUNS="${PERF_RUNS:-5}"
CPU="${PERF_CPU:-2}"
SETTLE="${PERF_SETTLE_SECONDS:-5}"
[[ "$RUNS" =~ ^[1-9][0-9]*$ ]] || exit 2
[[ ! -e "$OUT_DIR" ]] || { echo "Refusing to overwrite $OUT_DIR"; exit 2; }
mkdir -p "$OUT_DIR"
export DESIGN_TCL_SCRIPT_DIR="$REPO_ROOT/scripts/design/sky130_gcd/script"
export FOUNDRY_DIR="$REPO_ROOT/scripts/foundry/sky130"
export WORKSPACE="$REPO_ROOT/scripts/design/sky130_gcd"
export CONFIG_DIR="$WORKSPACE/iEDA_config"
export TCL_SCRIPT_DIR="$DESIGN_TCL_SCRIPT_DIR"
export DESIGN_TOP=performance_only NETLIST_FILE=/dev/null SDC_FILE=/dev/null SPEF_FILE=/dev/null
TCL="$REPO_ROOT/scripts/edadb/performance/benchmark.tcl"
sha256sum "$ORIGINAL_BIN" "$CURRENT_BIN" "$INPUT_DEF" "$TCL" > "$OUT_DIR/manifest.sha256"
printf 'version\tround\tmetric\ttime_us\n' > "$OUT_DIR/samples.tsv"

run_mode() {
    local binary="$1" mode="$2" folder="$3" timing="$4"
    cat "$INPUT_DEF" >/dev/null
    [[ "$mode" != read ]] || cat "$folder/edadb.db" >/dev/null
    EDADB_STAGE_TIMING="$timing" PERF_MODE="$mode" EDADB_DB_PATH="$folder/edadb.db" \
        OUTPUT_DEF="$folder/$mode.def" \
        taskset -c "$CPU" "$binary" -script "$TCL" > "$folder/$mode.log" 2>&1
}

record() {
    local version="$1" round="$2" folder="$3"
    (( round != 0 )) || return 0
    awk -F '\t' -v version="$version" -v round="$round" \
        '$1=="EDADB_PERF" {print version "\t" round "\t" $2 "\t" $3}' \
        "$folder"/*.log >> "$OUT_DIR/samples.tsv"
}

# Round zero warms up both implementations but is excluded from timing statistics.
# Alternate order to reduce drift. There are no concurrent performance processes.
for ((round=0; round<=RUNS; ++round)); do
    versions="original optimized"
    (( round % 2 == 0 )) || versions="optimized original"
    for version in $versions; do
        echo "$(date -Is) round=$round version=$version"
        folder="$OUT_DIR/$version/round-$round"
        mkdir -p "$folder"
        sleep "$SETTLE"
        if [[ "$version" == original ]]; then
            run_mode "$ORIGINAL_BIN" native "$folder" 0
        else
            run_mode "$CURRENT_BIN" native "$folder" 0
            run_mode "$CURRENT_BIN" write "$folder" 0
            run_mode "$CURRENT_BIN" read "$folder" 0
            diff -u "$folder/native.def" "$folder/read.def" > "$folder/roundtrip.diff"
        fi
        record "$version" "$round" "$folder"
    done
    # Cross-version equality is checked outside timing; do not normalize away differences.
    diff -u "$OUT_DIR/original/round-$round/native.def" \
        "$OUT_DIR/optimized/round-$round/native.def" > "$OUT_DIR/cross-version-$round.diff"
done

# Separate observational run: same current binary, coarse stage timers enabled.
# Do not subtract these stage samples from the profiling-OFF command samples.
for ((round=0; round<=RUNS; ++round)); do
    echo "$(date -Is) round=$round version=stages"
    folder="$OUT_DIR/stages/round-$round"
    mkdir -p "$folder"
    sleep "$SETTLE"
    run_mode "$CURRENT_BIN" write "$folder" 1
    run_mode "$CURRENT_BIN" read "$folder" 1
    diff -u "$OUT_DIR/original/round-$round/native.def" "$folder/read.def" > "$folder/roundtrip.diff"
done
echo "PASS: $OUT_DIR"
