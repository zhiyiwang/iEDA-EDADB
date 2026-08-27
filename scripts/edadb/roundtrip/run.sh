#!/usr/bin/env bash

set -euo pipefail

# Generic iEDA + EDADB DEF roundtrip runner.
#
# The roundtrip/ directory separates the reusable verification mechanism from
# demo/ selection policy. demo/demo.sh chooses a known Sky130 result DEF and
# delegates here; this script runs the actual native/EDADB comparison and can
# also be called directly with any DEF whose LEF/design profile is supplied.
# Keeping this directory is not a runtime requirement of iEDA or EDADB, but it
# avoids duplicating the three Tcl flows in every demo or dataset wrapper.
#
# Default Sky130 invocation:
#   bash scripts/edadb/roundtrip/run.sh \
#     scripts/design/sky130_gcd/result/iPL_filler_result.def
#
# Invocation through the recommended demo wrapper:
#   bash scripts/edadb/demo/demo.sh
#
# Generated files are written below RUN_DIR. The important artifacts are:
#   direct.def    native DEF -> iDB -> DEF baseline
#   edadb.db      database written from the input DEF
#   edadb.def     fresh iDB restored from edadb.db and written as DEF
#   *.log/*.diff  phase logs and failure diagnostics

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

usage() {
    cat <<EOF
Usage: $0 INPUT_DEF

Environment overrides:
  IEDA_BIN                 iEDA executable (default: <repo>/bin/iEDA)
  DESIGN_PROFILE_DIR       design profile containing script/ and iEDA_config/
  DESIGN_TCL_SCRIPT_DIR    Tcl root containing DB_script/
  FOUNDRY_DIR              matching PDK root
  RUN_DIR                  exact output directory
EOF
}

if [[ "$#" -ne 1 ]]; then
    usage >&2
    exit 2
fi

IEDA_BIN="${IEDA_BIN:-$REPO_ROOT/bin/iEDA}"
# These three paths must describe one compatible design/PDK environment. The
# default profile is Sky130 GCD; callers may override them for another dataset.
DESIGN_PROFILE_DIR="${DESIGN_PROFILE_DIR:-$REPO_ROOT/scripts/design/sky130_gcd}"
DESIGN_TCL_SCRIPT_DIR="${DESIGN_TCL_SCRIPT_DIR:-$DESIGN_PROFILE_DIR/script}"
FOUNDRY_DIR="${FOUNDRY_DIR:-$REPO_ROOT/scripts/foundry/sky130}"
INPUT_DEF="$(realpath "$1")"
# EDADB_TODO: Re-enable only after normalized DEF equivalence is reviewed.
# NORMALIZER="$REPO_ROOT/src/database/edadb/test/normalize_def_for_diff.py"

input_name="$(basename "${INPUT_DEF%.*}")"
if [[ -z "${RUN_DIR:-}" ]]; then
    run_stamp="$(date +%Y%m%d-%H%M%S)"
    RUN_DIR="$SCRIPT_DIR/result/${input_name}-${run_stamp}"
fi

DIRECT_DEF="$RUN_DIR/direct.def"
EDADB_DEF="$RUN_DIR/edadb.def"
EDADB_DB_PATH="$RUN_DIR/edadb.db"

for required in \
    "$IEDA_BIN" \
    "$INPUT_DEF" \
    "$DESIGN_TCL_SCRIPT_DIR/DB_script/db_path_setting.tcl" \
    "$DESIGN_TCL_SCRIPT_DIR/DB_script/db_init_lef.tcl"; do
    if [[ ! -f "$required" ]]; then
        echo "ERROR: missing required file: $required" >&2
        exit 1
    fi
done

mkdir -p "$RUN_DIR"
rm -f \
    "$DIRECT_DEF" "$EDADB_DEF" "$EDADB_DB_PATH" \
    "$RUN_DIR/input_vs_direct.diff" "$RUN_DIR/direct_vs_edadb.diff" \
    "$RUN_DIR/direct.log" "$RUN_DIR/def2edadb.log" "$RUN_DIR/edadb2def.log"
# EDADB_TODO: Normalized comparison is disabled, so these artifacts are not
# generated or cleaned by the active runner:
#   "$RUN_DIR/direct.norm.def" "$RUN_DIR/edadb.norm.def"
#   "$RUN_DIR/direct_vs_edadb.norm.diff"

export WORKSPACE="${WORKSPACE:-$DESIGN_PROFILE_DIR}"
export CONFIG_DIR="${CONFIG_DIR:-$DESIGN_PROFILE_DIR/iEDA_config}"
export DESIGN_TCL_SCRIPT_DIR
export TCL_SCRIPT_DIR="${TCL_SCRIPT_DIR:-$DESIGN_TCL_SCRIPT_DIR}"
export FOUNDRY_DIR
export RESULT_DIR="$RUN_DIR"
export INPUT_DEF
export EDADB_DB_PATH
# Some design-profile path scripts define all flow paths in one file even when
# this roundtrip loads only LEF. Provide inert defaults for those declarations;
# no netlist, SDC, or SPEF initialization Tcl is sourced below.
export DESIGN_TOP="${DESIGN_TOP:-roundtrip_only}"
export NETLIST_FILE="${NETLIST_FILE:-/dev/null}"
export SDC_FILE="${SDC_FILE:-/dev/null}"
export SPEF_FILE="${SPEF_FILE:-/dev/null}"

run_ieda() {
    local tcl_script="$1"
    local log_file="$2"
    (
        cd "$(dirname "$IEDA_BIN")"
        "./$(basename "$IEDA_BIN")" -script "$tcl_script"
    ) 2>&1 | tee "$log_file"
}

echo "==> iEDA + EDADB generic DEF roundtrip"
echo "input DEF:        $INPUT_DEF"
echo "design profile:   $DESIGN_PROFILE_DIR"
echo "design Tcl root:  $DESIGN_TCL_SCRIPT_DIR"
echo "foundry/PDK root: $FOUNDRY_DIR"
echo "output directory: $RUN_DIR"

# Phase 1 establishes the executable oracle. Comparing against direct.def,
# rather than the original text alone, tolerates canonical formatting emitted
# by the native iEDA DEF writer.
echo "==> 1/3 Native DEF -> iDB -> DEF baseline"
export OUTPUT_DEF="$DIRECT_DEF"
run_ieda "$SCRIPT_DIR/tcl/direct_def_roundtrip.tcl" "$RUN_DIR/direct.log"

# Phase 2 uses the same LEF and input DEF, then writes all enabled root families
# and their nested children into the SQLite-backed EDADB database.
echo "==> 2/3 DEF -> iDB -> EDADB"
run_ieda "$SCRIPT_DIR/tcl/def2edadb.tcl" "$RUN_DIR/def2edadb.log"

# Phase 3 starts a fresh iEDA process, rebuilds iDB from EDADB, and emits DEF.
# A fresh process prevents phase-2 in-memory objects from masking read defects.
echo "==> 3/3 EDADB -> fresh iDB -> DEF"
export OUTPUT_DEF="$EDADB_DEF"
run_ieda "$SCRIPT_DIR/tcl/edadb2def.tcl" "$RUN_DIR/edadb2def.log"

if diff -u "$INPUT_DEF" "$DIRECT_DEF" >"$RUN_DIR/input_vs_direct.diff"; then
    rm -f "$RUN_DIR/input_vs_direct.diff"
    echo "PASS: input DEF is already canonical for the native iEDA writer"
else
    echo "INFO: native iEDA canonicalized the input; see $RUN_DIR/input_vs_direct.diff"
fi

# Exact writer-output equality is the preferred result.
if diff -u "$DIRECT_DEF" "$EDADB_DEF" >"$RUN_DIR/direct_vs_edadb.diff"; then
    rm -f "$RUN_DIR/direct_vs_edadb.diff"
    echo "PASS: native and EDADB DEF outputs match exactly"
    echo "Artifacts: $RUN_DIR"
    exit 0
fi

# EDADB_TODO: The normalized comparison is intentionally disabled. For the
# current milestone, any textual difference between direct.def and edadb.def is
# a test failure, including root-record order differences.
# python3 "$NORMALIZER" "$DIRECT_DEF" >"$RUN_DIR/direct.norm.def"
# python3 "$NORMALIZER" "$EDADB_DEF" >"$RUN_DIR/edadb.norm.def"
# if diff -u "$RUN_DIR/direct.norm.def" "$RUN_DIR/edadb.norm.def" \
#         >"$RUN_DIR/direct_vs_edadb.norm.diff"; then
#     rm -f "$RUN_DIR/direct_vs_edadb.norm.diff"
#     echo "PASS: outputs differ only by allowed Level-D root record order"
#     echo "Raw diff: $RUN_DIR/direct_vs_edadb.diff"
#     echo "Artifacts: $RUN_DIR"
#     exit 0
# fi

echo "FAIL: native and EDADB DEF outputs differ" >&2
echo "Raw diff:  $RUN_DIR/direct_vs_edadb.diff" >&2
echo "Artifacts: $RUN_DIR" >&2
exit 1
