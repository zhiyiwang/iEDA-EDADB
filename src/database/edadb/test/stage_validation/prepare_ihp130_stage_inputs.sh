#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../../.." && pwd)"
IEDA_BIN="${IEDA_BIN:-$REPO_ROOT/bin/iEDA}"
DATASET="${DATASET:-ihp130_aes}"
OUT_ROOT="${OUT_ROOT:-/tmp/iedadb_stage_inputs}"
PREPARE_THROUGH="${PREPARE_THROUGH:-ipl_lg}"
PREPARE_CLEAN="${PREPARE_CLEAN:-0}"

WORKSPACE="$REPO_ROOT/scripts/design/ihp130_gcd"
RESULT_DIR="$OUT_ROOT/$DATASET/result"
CONFIG_DIR="$WORKSPACE/iEDA_config"
TCL_SCRIPT_DIR="$WORKSPACE/script"
FOUNDRY_DIR="$REPO_ROOT/scripts/foundry/ihp130"
SDC_FILE="$WORKSPACE/default.sdc"

die() {
    echo "ERROR: $*" >&2
    exit 1
}

require_file() {
    [[ -f "$1" ]] || die "missing required file: $1"
}

case "$DATASET" in
    ihp130_aes)
        TOP_NAME=aes_cipher_top
        NETLIST_FILE="$WORKSPACE/result/verilog/aes_nl.v"
        DIE_BBOX="0 0 547.1137448407128 547.1137448407128"
        CORE_BBOX="10 10 537.1137448407128 537.1137448407128"
        ;;
    ihp130_picorv32a)
        TOP_NAME=picorv32a
        NETLIST_FILE="$WORKSPACE/result/verilog/picorv32a_nl.v"
        DIE_BBOX="0 0 629.3543698046318 629.3543698046318"
        CORE_BBOX="10 10 619.3543698046318 619.3543698046318"
        ;;
    *)
        die "unsupported DATASET=$DATASET"
        ;;
esac

case "$PREPARE_THROUGH" in
    ifp|ino|ipl|icts|ito_drv|ito_hold|ipl_lg) ;;
    *) die "unsupported PREPARE_THROUGH=$PREPARE_THROUGH" ;;
esac

require_file "$IEDA_BIN"
require_file "$NETLIST_FILE"
require_file "$SDC_FILE"

if [[ -d "$RESULT_DIR" && "$PREPARE_CLEAN" == "1" ]]; then
    rm -rf "$RESULT_DIR"
elif [[ -d "$RESULT_DIR" && -n "$(find "$RESULT_DIR" -mindepth 1 -print -quit)" ]]; then
    die "$RESULT_DIR is not empty; choose another OUT_ROOT or set PREPARE_CLEAN=1"
fi

mkdir -p "$RESULT_DIR"/{cts,feature,metric,pl,report,to}

export RESULT_DIR CONFIG_DIR TCL_SCRIPT_DIR FOUNDRY_DIR SDC_FILE
export IEDA_CONFIG_DIR="$CONFIG_DIR"
export IEDA_TCL_SCRIPT_DIR="$TCL_SCRIPT_DIR"
export TOP_NAME NETLIST_FILE

run_step() {
    local step="$1"
    local script="$2"
    local expected_output="$3"
    shift 3
    local log_path="$OUT_ROOT/$DATASET/prepare_${step}.log"

    echo "==> [$DATASET] prepare $step"
    env "$@" "$IEDA_BIN" -script "$script" >"$log_path" 2>&1
    if rg -q 'Can not create file|Create DEF file failed' "$log_path"; then
        die "$step reported a DEF save failure; log: $log_path"
    fi
    [[ -s "$expected_output" ]] || die "$step did not create $expected_output; log: $log_path"
    echo "PASS: $step -> $expected_output"
}

stop_after() {
    if [[ "$PREPARE_THROUGH" == "$1" ]]; then
        echo "Prepared $DATASET through $1 at $RESULT_DIR"
        exit 0
    fi
}

run_step ifp "$TCL_SCRIPT_DIR/iFP_script/run_iFP.tcl" "$RESULT_DIR/iFP_result.def" \
    USE_FIXED_BBOX=True \
    DIE_BBOX="$DIE_BBOX" \
    CORE_BBOX="$CORE_BBOX" \
    OUTPUT_DEF="$RESULT_DIR/iFP_result.def" \
    DESIGN_STAT_TEXT="$RESULT_DIR/report/floorplan_stat.rpt" \
    DESIGN_STAT_JSON="$RESULT_DIR/report/floorplan_stat.json"
stop_after ifp

run_step ino "$TCL_SCRIPT_DIR/iNO_script/run_iNO_fix_fanout.tcl" "$RESULT_DIR/iNO_fix_fanout_result.def" \
    INPUT_DEF="$RESULT_DIR/iFP_result.def" \
    OUTPUT_DEF="$RESULT_DIR/iNO_fix_fanout_result.def" \
    OUTPUT_VERILOG="$RESULT_DIR/iNO_fix_fanout_result.v" \
    DESIGN_STAT_TEXT="$RESULT_DIR/report/fix_fanout_db.rpt" \
    DESIGN_STAT_JSON="$RESULT_DIR/report/fix_fanout_db.json" \
    TOOL_METRICS_JSON="$RESULT_DIR/metric/iNO_metrics.json"
stop_after ino

run_step ipl "$TCL_SCRIPT_DIR/iPL_script/run_iPL.tcl" "$RESULT_DIR/iPL_result.def" \
    INPUT_DEF="$RESULT_DIR/iNO_fix_fanout_result.def" \
    OUTPUT_DEF="$RESULT_DIR/iPL_result.def" \
    OUTPUT_VERILOG="$RESULT_DIR/iPL_result.v" \
    DESIGN_STAT_TEXT="$RESULT_DIR/report/placement_stat.rpt" \
    DESIGN_STAT_JSON="$RESULT_DIR/report/placement_stat.json" \
    TOOL_METRICS_JSON="$RESULT_DIR/metric/iPL_metrics.json" \
    TOOL_REPORT_DIR="$RESULT_DIR/pl"
stop_after ipl

run_step icts "$TCL_SCRIPT_DIR/iCTS_script/run_iCTS.tcl" "$RESULT_DIR/iCTS_result.def" \
    INPUT_DEF="$RESULT_DIR/iPL_result.def" \
    OUTPUT_DEF="$RESULT_DIR/iCTS_result.def" \
    OUTPUT_VERILOG="$RESULT_DIR/iCTS_result.v" \
    DESIGN_STAT_TEXT="$RESULT_DIR/report/cts_stat.rpt" \
    DESIGN_STAT_JSON="$RESULT_DIR/report/cts_stat.json" \
    TOOL_METRICS_JSON="$RESULT_DIR/metric/iCTS_metrics.json" \
    TOOL_REPORT_DIR="$RESULT_DIR/cts"
stop_after icts

run_step ito_drv "$TCL_SCRIPT_DIR/iTO_script/run_iTO_drv.tcl" "$RESULT_DIR/iTO_drv_result.def" \
    INPUT_DEF="$RESULT_DIR/iCTS_result.def" \
    OUTPUT_DEF="$RESULT_DIR/iTO_drv_result.def"
stop_after ito_drv

run_step ito_hold "$TCL_SCRIPT_DIR/iTO_script/run_iTO_hold.tcl" "$RESULT_DIR/iTO_hold_result.def" \
    INPUT_DEF="$RESULT_DIR/iTO_drv_result.def" \
    OUTPUT_DEF="$RESULT_DIR/iTO_hold_result.def"
stop_after ito_hold

run_step ipl_lg "$TCL_SCRIPT_DIR/iPL_script/run_iPL_legalization.tcl" "$RESULT_DIR/iPL_lg_result.def" \
    INPUT_DEF="$RESULT_DIR/iTO_hold_result.def" \
    OUTPUT_DEF="$RESULT_DIR/iPL_lg_result.def" \
    OUTPUT_VERILOG="$RESULT_DIR/iPL_lg_result.v" \
    DESIGN_STAT_TEXT="$RESULT_DIR/report/legalization_stat.rpt" \
    DESIGN_STAT_JSON="$RESULT_DIR/report/legalization_stat.json"

echo "Prepared all $DATASET stage inputs at $RESULT_DIR"
