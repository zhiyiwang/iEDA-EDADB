#!/usr/bin/env bash

# Compare native DEF read/write with EDADB write/read without modifying iEDA,
# the adapter, or EDADB core. Each sample must pass an exact DEF diff.
# The runner reports two groups:
#   cold: input DEF/DB pages are evicted from the OS page cache before reading.
#   warm: input DEF/DB files are read into the OS page cache before reading.
# All samples and modes run strictly one after another. Do not parallelize this
# benchmark because concurrent iEDA processes would distort CPU, memory and I/O.
#
# Run a quick smoke test from the repository root:
#   cd /home/zhiyiwang/cs/arch/eda/iEDA-EDADB
#   PERF_WARMUPS=0 PERF_RUNS=1 \
#     OUT_DIR=/tmp/iedadb_perf_smoke \
#     bash scripts/edadb/performance/run.sh \
#       scripts/design/sky130_gcd/result/iPL_filler_result.def
#
# Run the initial performance test:
#   cd /home/zhiyiwang/cs/arch/eda/iEDA-EDADB
#   PERF_WARMUPS=1 PERF_RUNS=5 \
#     OUT_DIR=/tmp/iedadb_perf_filler \
#     bash scripts/edadb/performance/run.sh \
#       scripts/design/sky130_gcd/result/iPL_filler_result.def
#
# Inspect the measured times:
#   column -t -s $'\t' /tmp/iedadb_perf_filler/result.tsv
#
# Inspect one cold-cache sample's native, EDADB write, and EDADB read logs:
#   less /tmp/iedadb_perf_filler/cold/run-1/native.log
#   less /tmp/iedadb_perf_filler/cold/run-1/write.log
#   less /tmp/iedadb_perf_filler/cold/run-1/read.log
#
# benchmark.tcl prints one tab-separated EDADB_PERF record for each timed
# command. This script collects those records into result.tsv.
set -euo pipefail

# Resolve paths from this file, so the runner does not depend on the caller's
# current working directory.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

if [[ "$#" -ne 1 ]]; then
  echo "Usage: $0 INPUT_DEF" >&2
  exit 2
fi

# Benchmark controls. Environment variables allow another dataset/profile to
# reuse the runner without editing this file.
PERF_WARMUPS="${PERF_WARMUPS:-1}" # Number of warm-up runs, default 1
PERF_RUNS="${PERF_RUNS:-5}" # Number of measured runs, default 5
PERF_SETTLE_SECONDS="${PERF_SETTLE_SECONDS:-5}" # Quiet interval before each sample
# Performance numbers must use the separately built optimized Release binary.
# Override IEDA_BIN only when benchmarking another verified Release build.
IEDA_BIN="${IEDA_BIN:-$REPO_ROOT/bin-release/iEDA}"
DESIGN_PROFILE_DIR="${DESIGN_PROFILE_DIR:-$REPO_ROOT/scripts/design/sky130_gcd}" # Path to the design profile
DESIGN_TCL_SCRIPT_DIR="${DESIGN_TCL_SCRIPT_DIR:-$DESIGN_PROFILE_DIR/script}" # Path to the design Tcl scripts
FOUNDRY_DIR="${FOUNDRY_DIR:-$REPO_ROOT/scripts/foundry/sky130}" # Path to the foundry PDK
INPUT_DEF="$(realpath "$1")" # Path to the input DEF file
OUT_DIR="${OUT_DIR:-/tmp/iedadb_performance}" # Path to the output directory
RESULT_TSV="$OUT_DIR/result.tsv" # Path to the result TSV file

# Warm-up may be zero; measured runs must be at least one.
if ! [[ "$PERF_WARMUPS" =~ ^[0-9]+$ && "$PERF_RUNS" =~ ^[1-9][0-9]*$ && "$PERF_SETTLE_SECONDS" =~ ^[0-9]+$ ]]; then
  echo "ERROR: invalid PERF_WARMUPS, PERF_RUNS or PERF_SETTLE_SECONDS" >&2
  exit 2
fi

# Fail before starting any expensive iEDA process if the binary, input, timing
# Tcl, or LEF initialization scripts are missing.
for path in \
  "$IEDA_BIN" \
  "$INPUT_DEF" \
  "$SCRIPT_DIR/benchmark.tcl" \
  "$DESIGN_TCL_SCRIPT_DIR/DB_script/db_path_setting.tcl" \
  "$DESIGN_TCL_SCRIPT_DIR/DB_script/db_init_lef.tcl"; do
  if [[ ! -f "$path" ]]; then
    echo "ERROR: missing file: $path" >&2
    exit 1
  fi
done

if ! command -v python3 >/dev/null 2>&1; then
  echo "ERROR: python3 is required for cold-cache file eviction" >&2
  exit 1
fi

# Recreate the result table for this invocation. Sample artifacts are stored in
# OUT_DIR/<sample>/ so each run can be inspected independently.
mkdir -p "$OUT_DIR"
printf 'cache_mode\tsample\ttime_unit\tnative_def_read_us\tnative_def_write_us\tedadb_write_us\tedadb_read_us\n' >"$RESULT_TSV"

# The Sky130 DB path scripts expect these variables. This benchmark loads only
# LEF and DEF, so netlist/timing inputs use inert defaults.
export DESIGN_TCL_SCRIPT_DIR FOUNDRY_DIR INPUT_DEF
export WORKSPACE="${WORKSPACE:-$DESIGN_PROFILE_DIR}"
export CONFIG_DIR="${CONFIG_DIR:-$DESIGN_PROFILE_DIR/iEDA_config}"
export TCL_SCRIPT_DIR="${TCL_SCRIPT_DIR:-$DESIGN_TCL_SCRIPT_DIR}"
export DESIGN_TOP="${DESIGN_TOP:-performance_only}"
export NETLIST_FILE="${NETLIST_FILE:-/dev/null}"
export SDC_FILE="${SDC_FILE:-/dev/null}"
export SPEF_FILE="${SPEF_FILE:-/dev/null}"

# Evict one file from the Linux page cache without dropping the machine-wide
# cache. posix_fadvise is advisory, but it is direct and does not require root.
evict_file_cache() {
  local path="$1"

  sync -f "$path"
  python3 - "$path" <<'PY'
import os
import sys

descriptor = os.open(sys.argv[1], os.O_RDONLY)
try:
    os.posix_fadvise(descriptor, 0, 0, os.POSIX_FADV_DONTNEED)
finally:
    os.close(descriptor)
PY
}

# Prepare only the input files for the selected group. Output files are always
# newly created, so they have no reusable cached contents before a write.
prepare_file_cache() {
  local cache_mode="$1"
  local path="$2"

  if [[ "$cache_mode" == "warm" ]]; then
    cat "$path" >/dev/null
  else
    evict_file_cache "$path"
  fi
}

# Start one fresh iEDA process in native, write, or read mode. Separating modes
# prevents an earlier operation from leaving objects in the active iDB.
run_mode() {
  local mode="$1"
  local log_file="$2"

  PERF_MODE="$mode" "$IEDA_BIN" -script "$SCRIPT_DIR/benchmark.tcl" \
    >"$log_file" 2>&1
}

# Read one EDADB_PERF<TAB>phase<TAB>elapsed_us record from an iEDA log.
# Missing timing output is treated as an error instead of silently writing zero.
read_phase() {
  local phase="$1"
  local log_file="$2"

  awk -F '\t' -v phase="$phase" \
    '$1 == "EDADB_PERF" && $2 == phase {value = $3} END {if (value == "") exit 1; print value}' \
    "$log_file"
}

# Run one complete correctness-checked sample.
# record=0 is warm-up only; record=1 appends timings to result.tsv.
run_sample() {
  local cache_mode="$1"
  local sample="$2"
  local record="$3"
  local sample_dir="$OUT_DIR/$cache_mode/$sample"
  local native_def="$sample_dir/native.def"
  local edadb_def="$sample_dir/edadb.def"

  mkdir -p "$sample_dir"
  export EDADB_DB_PATH="$sample_dir/edadb.db"

  # Native path: DEF -> iDB -> canonical native.def.
  prepare_file_cache "$cache_mode" "$INPUT_DEF"
  OUTPUT_DEF="$native_def" run_mode native "$sample_dir/native.log"

  # EDADB write path: DEF -> iDB -> fresh edadb.db. Remove SQLite side files so
  # no data from an earlier invocation can affect the measurement.
  rm -f "$EDADB_DB_PATH" "$EDADB_DB_PATH-wal" "$EDADB_DB_PATH-shm"
  prepare_file_cache "$cache_mode" "$INPUT_DEF"
  run_mode write "$sample_dir/write.log"

  # EDADB read path: edadb.db -> restored iDB -> canonical edadb.def.
  prepare_file_cache "$cache_mode" "$INPUT_DEF"
  prepare_file_cache "$cache_mode" "$EDADB_DB_PATH"
  OUTPUT_DEF="$edadb_def" run_mode read "$sample_dir/read.log"

  # Timing is accepted only when the native and EDADB-restored iDB states write
  # exactly the same DEF text. Keep the diff only when the check fails.
  diff -u "$native_def" "$edadb_def" >"$sample_dir/native_vs_edadb.diff" || {
    echo "ERROR: DEF mismatch; see $sample_dir/native_vs_edadb.diff" >&2
    return 1
  }
  rm -f "$sample_dir/native_vs_edadb.diff"

  # Warm-up samples execute the same work but are intentionally not reported.
  if [[ "$record" -eq 1 ]]; then
    # Read is compared only with read, and write only with write. The runner
    # intentionally does not add read and write into one end-to-end number.
    local native_read_us
    local native_write_us
    local edadb_write_us
    local edadb_read_us
    native_read_us="$(read_phase native_def_read "$sample_dir/native.log")"
    native_write_us="$(read_phase native_def_write "$sample_dir/native.log")"
    edadb_write_us="$(read_phase edadb_write "$sample_dir/write.log")"
    edadb_read_us="$(read_phase edadb_read "$sample_dir/read.log")"
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
      "$cache_mode" \
      "$sample" \
      "us" \
      "$native_read_us" \
      "$native_write_us" \
      "$edadb_write_us" \
      "$edadb_read_us" \
      >>"$RESULT_TSV"
  fi
}

echo "Input:   $INPUT_DEF"
echo "Output:  $OUT_DIR"
echo "Groups:  cold and warm OS page cache"
echo "Samples: $PERF_WARMUPS warm-up + $PERF_RUNS measured per group"
echo "Settle:  ${PERF_SETTLE_SECONDS}s before each sample"

# Run sequentially. Parallel performance samples would measure CPU/memory/I/O
# contention rather than the cost of one DEF or EDADB operation.
for cache_mode in cold warm; do
  for ((index = 1; index <= PERF_WARMUPS; ++index)); do
    echo "$cache_mode warm-up $index/$PERF_WARMUPS"
    sleep "$PERF_SETTLE_SECONDS"
    run_sample "$cache_mode" "warmup-$index" 0 # 0 = warm-up only, not recorded
  done

  for ((index = 1; index <= PERF_RUNS; ++index)); do
    echo "$cache_mode run $index/$PERF_RUNS"
    sleep "$PERF_SETTLE_SECONDS"
    run_sample "$cache_mode" "run-$index" 1 # 1 = record timings in result.tsv
  done
done

# Print a human-readable table when column(1) exists; result.tsv remains the
# machine-readable source of truth in either case.
echo
column -t -s $'\t' "$RESULT_TSV" 2>/dev/null || cat "$RESULT_TSV"
echo "Result: $RESULT_TSV"
