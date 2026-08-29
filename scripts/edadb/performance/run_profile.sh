#!/usr/bin/env bash

# Compare an uninstrumented Release binary with a profiling-enabled Release
# binary. Samples remain sequential because concurrent benchmark processes
# would measure resource contention instead of instrumentation overhead.
#
# Run from the repository root:
#   PROFILE_WARMUPS=1 PROFILE_RUNS=5 \
#     PROFILE_OUT_DIR=/tmp/iedadb_profile_filler \
#     bash scripts/edadb/performance/run_profile.sh \
#       scripts/design/sky130_gcd/result/iPL_filler_result.def
#
# Inspect the generated summaries:
#   column -t -s $'\t' /tmp/iedadb_profile_filler/overhead.tsv
#   column -t -s $'\t' /tmp/iedadb_profile_filler/phase_summary.tsv
#   column -t -s $'\t' /tmp/iedadb_profile_filler/metric_summary.tsv
#   column -t -s $'\t' /tmp/iedadb_profile_filler/api_summary.tsv
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

if [[ "$#" -ne 1 ]]; then
  echo "Usage: $0 INPUT_DEF" >&2
  exit 2
fi

INPUT_DEF="$(realpath "$1")"
PROFILE_WARMUPS="${PROFILE_WARMUPS:-1}"
PROFILE_RUNS="${PROFILE_RUNS:-5}"
PROFILE_SETTLE_SECONDS="${PROFILE_SETTLE_SECONDS:-5}"
PROFILE_OUT_DIR="${PROFILE_OUT_DIR:-/tmp/iedadb_profile}"
BASELINE_BIN="${BASELINE_BIN:-$REPO_ROOT/bin-release/iEDA}"
PROFILE_BIN="${PROFILE_BIN:-$REPO_ROOT/bin-profile/iEDA}"

for path in "$INPUT_DEF" "$BASELINE_BIN" "$PROFILE_BIN"; do
  if [[ ! -f "$path" ]]; then
    echo "ERROR: missing file: $path" >&2
    exit 1
  fi
done

mkdir -p "$PROFILE_OUT_DIR"

echo "Running profiling-OFF Release baseline"
PERF_WARMUPS="$PROFILE_WARMUPS" \
PERF_RUNS="$PROFILE_RUNS" \
PERF_SETTLE_SECONDS="$PROFILE_SETTLE_SECONDS" \
IEDA_BIN="$BASELINE_BIN" \
OUT_DIR="$PROFILE_OUT_DIR/baseline" \
  bash "$SCRIPT_DIR/run.sh" "$INPUT_DEF"

echo "Running profiling-ON Release measurement"
PERF_WARMUPS="$PROFILE_WARMUPS" \
PERF_RUNS="$PROFILE_RUNS" \
PERF_SETTLE_SECONDS="$PROFILE_SETTLE_SECONDS" \
IEDA_BIN="$PROFILE_BIN" \
OUT_DIR="$PROFILE_OUT_DIR/profiled" \
  bash "$SCRIPT_DIR/run.sh" "$INPUT_DEF"

python3 "$SCRIPT_DIR/summarize_profile.py" \
  "$PROFILE_OUT_DIR/baseline/result.tsv" \
  "$PROFILE_OUT_DIR/profiled/result.tsv" \
  "$PROFILE_OUT_DIR/profiled" \
  "$PROFILE_OUT_DIR"

echo
echo "Instrumentation overhead:"
column -t -s $'\t' "$PROFILE_OUT_DIR/overhead.tsv" 2>/dev/null \
  || cat "$PROFILE_OUT_DIR/overhead.tsv"
echo
echo "Phase breakdown:"
column -t -s $'\t' "$PROFILE_OUT_DIR/phase_summary.tsv" 2>/dev/null \
  || cat "$PROFILE_OUT_DIR/phase_summary.tsv"
echo "Profile result: $PROFILE_OUT_DIR"
