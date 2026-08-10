#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../../.." && pwd)"
IEDA_BIN="${IEDA_BIN:-$REPO_ROOT/bin/iEDA}"
OUT_ROOT="${OUT_ROOT:-/tmp/iedadb_stage_validation}"
DATASET="${DATASET:-sky130_gcd}"
NORMALIZER="$SCRIPT_DIR/../normalize_def_for_diff.py"
COMPARE="$SCRIPT_DIR/compare_stage_runs.py"
MANIFEST="$SCRIPT_DIR/create_manifest.py"
COMPARE_IRT_INPUT="$SCRIPT_DIR/compare_irt_input_snapshots.py"
SUMMARIZE_IRT_VARIABILITY="$SCRIPT_DIR/summarize_irt_variability.py"
RUN_STAGE_TCL="$SCRIPT_DIR/tcl/run_stage.tcl"
WRITE_EDADB_TCL="$SCRIPT_DIR/../tcl/def2edadb_generic.tcl"
NATIVE_RUNS="${NATIVE_RUNS:-3}"
STAGE_RUN_JOBS="${STAGE_RUN_JOBS:-auto}"
IEDA_PROCESS_MEMORY_GIB="${IEDA_PROCESS_MEMORY_GIB:-16}"
IEDA_MEMORY_RESERVE_GIB="${IEDA_MEMORY_RESERVE_GIB:-16}"
IRT_INPUT_GATE_ONLY="${IRT_INPUT_GATE_ONLY:-0}"

ALL_STAGES=(ipl icts ito_drv ito_hold ipl_lg irt)

die() {
    echo "ERROR: $*" >&2
    exit 1
}

require_file() {
    [[ -f "$1" ]] || die "missing required file: $1"
}

stage_input() {
    case "$1" in
        ipl) echo "$DATASET_RESULT_DIR/$IPL_INPUT_DEF" ;;
        icts) echo "$DATASET_RESULT_DIR/iPL_result.def" ;;
        ito_drv) echo "$DATASET_RESULT_DIR/iCTS_result.def" ;;
        ito_hold) echo "$DATASET_RESULT_DIR/iTO_drv_result.def" ;;
        ipl_lg) echo "$DATASET_RESULT_DIR/iTO_hold_result.def" ;;
        irt) echo "$DATASET_RESULT_DIR/iPL_lg_result.def" ;;
        *) die "unsupported stage: $1" ;;
    esac
}

stage_configs() {
    local stage="$1"
    printf '%s\n' "$CONFIG_DIR/flow_config.json" "$CONFIG_DIR/db_default_config.json"
    case "$stage" in
        ipl|ipl_lg) echo "$CONFIG_DIR/pl_default_config.json" ;;
        icts) echo "$CTS_CONFIG_FILE" ;;
        ito_drv) echo "$CONFIG_DIR/to_default_config_drv.json" ;;
        ito_hold) echo "$CONFIG_DIR/to_default_config_hold.json" ;;
        irt) ;;
    esac
}

stage_run_jobs() {
    local stage="$1"
    if [[ "$STAGE_RUN_JOBS" != "auto" ]]; then
        echo "$STAGE_RUN_JOBS"
        return
    fi

    local cpu_jobs
    case "$stage" in
        ipl|ipl_lg)
            local logical_cpus
            logical_cpus="$(nproc)"
            if [[ "$logical_cpus" -ge 24 ]]; then
                cpu_jobs=3
            elif [[ "$logical_cpus" -ge 12 ]]; then
                cpu_jobs=2
            else
                cpu_jobs=1
            fi
            ;;
        *)
            cpu_jobs=1
            ;;
    esac

    local available_kib available_gib memory_jobs
    available_kib="$(awk '/^MemAvailable:/ {print $2}' /proc/meminfo)"
    available_gib=$((available_kib / 1024 / 1024))
    if [[ "$available_gib" -le "$IEDA_MEMORY_RESERVE_GIB" ]]; then
        memory_jobs=1
    else
        memory_jobs=$(((available_gib - IEDA_MEMORY_RESERVE_GIB) / IEDA_PROCESS_MEMORY_GIB))
        [[ "$memory_jobs" -ge 1 ]] || memory_jobs=1
    fi

    if [[ "$memory_jobs" -lt "$cpu_jobs" ]]; then
        echo "$memory_jobs"
    else
        echo "$cpu_jobs"
    fi
}

write_manifest() {
    local output_dir="$1"
    local stage="$2"
    local mode="$3"
    local input_def="$4"
    local command_text="$5"
    local status="$6"
    local args=()
    while IFS= read -r config; do
        args+=(--config "$config")
    done < <(stage_configs "$stage")
    python3 "$MANIFEST" \
        --repo "$REPO_ROOT" \
        --output "$output_dir/manifest.json" \
        --dataset "$DATASET" \
        --stage "$stage" \
        --mode "$mode" \
        --input "$input_def" \
        --command "$command_text" \
        --status "$status" \
        "${args[@]}"
}

run_ieda_process() {
    local output_dir="$1"
    local stage="$2"
    local mode="$3"
    local input_def="$4"
    local run_tool="$5"
    local edadb_db="$6"
    local command_text="$IEDA_BIN -script $RUN_STAGE_TCL"

    mkdir -p "$output_dir"
    local status=0
    RESULT_DIR="$output_dir" \
    INPUT_DEF="$input_def" \
    INPUT_MODE="$mode" \
    PRE_TOOL_DEF="$output_dir/pre_tool.def" \
    PRE_TOOL_REPORT="$output_dir/pre_tool_db.rpt" \
    OUTPUT_DEF="$output_dir/post_tool.def" \
    STAGE="$stage" \
    RUN_TOOL="$run_tool" \
    EDADB_DB_PATH="$edadb_db" \
    RT_THREAD_NUMBER="${RT_THREAD_NUMBER:-64}" \
    RT_ENABLE_NOTIFICATION="${RT_ENABLE_NOTIFICATION:-0}" \
    RT_SNAPSHOT_ONLY="${RT_SNAPSHOT_ONLY:-0}" \
        "$IEDA_BIN" -script "$RUN_STAGE_TCL" >"$output_dir/run.log" 2>&1 || status=$?
    write_manifest "$output_dir" "$stage" "$mode" "$input_def" "$command_text" "$status"
    if [[ "$status" -ne 0 ]]; then
        echo "FAIL: $mode $stage exited with $status; log: $output_dir/run.log" >&2
        return "$status"
    fi
}

run_process_batch() {
    local stage="$1"
    local mode="$2"
    local input_def="$3"
    local edadb_db="$4"
    local first_run="$5"
    local last_run="$6"
    local stage_root="$7"
    local jobs
    jobs="$(stage_run_jobs "$stage")"
    local -a pids=()
    local failed=0
    local run pid

    for run in $(seq "$first_run" "$last_run"); do
        run_ieda_process "$stage_root/$mode-$run" "$stage" "$mode" "$input_def" 1 "$edadb_db" &
        pids+=("$!")
        if [[ "${#pids[@]}" -ge "$jobs" ]]; then
            for pid in "${pids[@]}"; do
                wait "$pid" || failed=1
            done
            pids=()
        fi
    done
    for pid in "${pids[@]}"; do
        wait "$pid" || failed=1
    done
    return "$failed"
}

write_edadb() {
    local output_dir="$1"
    local stage="$2"
    local input_def="$3"
    local edadb_db="$4"
    mkdir -p "$output_dir"
    local command_text="$IEDA_BIN -script $WRITE_EDADB_TCL"
    local status=0
    INPUT_DEF="$input_def" EDADB_DB_PATH="$edadb_db" \
        "$IEDA_BIN" -script "$WRITE_EDADB_TCL" >"$output_dir/run.log" 2>&1 || status=$?
    write_manifest "$output_dir" "$stage" edadb_write "$input_def" "$command_text" "$status"
    if [[ "$status" -ne 0 ]]; then
        echo "FAIL: EDADB write for $stage exited with $status; log: $output_dir/run.log" >&2
        return "$status"
    fi
}

compare_runs() {
    local reference="$1"
    local candidate="$2"
    shift 2
    python3 "$COMPARE" "$reference" "$candidate" --normalizer "$NORMALIZER" "$@"
}

validate_stage() {
    local stage="$1"
    local input_def
    input_def="$(stage_input "$stage")"
    require_file "$input_def"

    local stage_root="$OUT_ROOT/$DATASET/$stage"
    rm -rf "$stage_root"
    mkdir -p "$stage_root"
    local edadb_db="$stage_root/input/edadb.db"

    echo "==> [$stage] create native and EDADB pre-tool snapshots"
    run_ieda_process "$stage_root/precheck/native" "$stage" native "$input_def" 0 "$edadb_db"
    write_edadb "$stage_root/input" "$stage" "$input_def" "$edadb_db"
    run_ieda_process "$stage_root/precheck/edadb" "$stage" edadb "$input_def" 0 "$edadb_db"
    compare_runs "$stage_root/precheck/native" "$stage_root/precheck/edadb" --pre-only \
        >"$stage_root/precheck/compare.log" 2>&1 || {
            echo "FAIL: pre-tool state differs for $stage; point tool was not run" >&2
            cat "$stage_root/precheck/compare.log" >&2
            return 1
        }

    if [[ "$stage" == "irt" ]]; then
        echo "==> [irt] compare iRT-wrapped input environments"
        RT_THREAD_NUMBER=1 RT_ENABLE_NOTIFICATION=1 RT_SNAPSHOT_ONLY=1 \
            run_ieda_process "$stage_root/precheck/irt-native" "$stage" native "$input_def" 1 "$edadb_db"
        RT_THREAD_NUMBER=1 RT_ENABLE_NOTIFICATION=1 RT_SNAPSHOT_ONLY=1 \
            run_ieda_process "$stage_root/precheck/irt-edadb" "$stage" edadb "$input_def" 1 "$edadb_db"
        python3 "$COMPARE_IRT_INPUT" \
            "$stage_root/precheck/irt-native/rt/data_manager/input_snapshot.json" \
            "$stage_root/precheck/irt-edadb/rt/data_manager/input_snapshot.json" \
            >"$stage_root/precheck/irt-input-compare.log" 2>&1 || {
                echo "FAIL: iRT semantic input database differs; routing was not run" >&2
                cat "$stage_root/precheck/irt-input-compare.log" >&2
                return 1
            }
        cat "$stage_root/precheck/irt-input-compare.log"
        if [[ "$IRT_INPUT_GATE_ONLY" == "1" ]]; then
            echo "PASS: iRT native and EDADB semantic input databases match"
            return 0
        fi
    fi

    local jobs
    jobs="$(stage_run_jobs "$stage")"
    echo "==> [$stage] run $NATIVE_RUNS native controls with jobs=$jobs"
    local run
    run_process_batch "$stage" native "$input_def" "$edadb_db" 1 "$NATIVE_RUNS" "$stage_root"

    local native_stable=1
    for run in $(seq 2 "$NATIVE_RUNS"); do
        if ! compare_runs "$stage_root/native-1" "$stage_root/native-$run" \
            >"$stage_root/native-$run/compare-to-native-1.log" 2>&1; then
            native_stable=0
        fi
    done

    echo "==> [$stage] run first EDADB control"
    run_ieda_process "$stage_root/edadb-1" "$stage" edadb "$input_def" 1 "$edadb_db"
    local first_edadb_matches=1
    if ! compare_runs "$stage_root/native-1" "$stage_root/edadb-1" \
        >"$stage_root/edadb-1/compare-to-native-1.log" 2>&1; then
        first_edadb_matches=0
    fi

    if [[ "$native_stable" -eq 1 && "$first_edadb_matches" -eq 1 ]]; then
        echo "PASS: $stage native controls are stable and EDADB matches"
        return 0
    fi

    echo "==> [$stage] variability or mismatch detected; run two additional EDADB controls"
    run_process_batch "$stage" edadb "$input_def" "$edadb_db" 2 3 "$stage_root"
    for run in 2 3; do
        compare_runs "$stage_root/native-1" "$stage_root/edadb-$run" \
            >"$stage_root/edadb-$run/compare-to-native-1.log" 2>&1 || true
    done

    if [[ "$native_stable" -eq 0 ]]; then
        if [[ "$stage" == "irt" ]]; then
            python3 "$SUMMARIZE_IRT_VARIABILITY" "$stage_root" --output "$stage_root/variability_summary.json"
        fi
        echo "REVIEW: native $stage controls are not deterministic; artifacts retained at $stage_root" >&2
    else
        echo "FAIL: EDADB $stage result differs from stable native result; artifacts retained at $stage_root" >&2
    fi
    return 1
}

main() {
    case "$DATASET" in
        sky130_gcd)
            export WORKSPACE="$REPO_ROOT/scripts/design/sky130_gcd"
            export DATASET_RESULT_DIR="${DATASET_RESULT_DIR:-$WORKSPACE/result}"
            export CONFIG_DIR="$WORKSPACE/iEDA_config"
            export FOUNDRY_DIR="$WORKSPACE/../../foundry/sky130"
            export TCL_SCRIPT_DIR="$WORKSPACE/script"
            export DESIGN_TOP=gcd
            export NETLIST_FILE="$WORKSPACE/result/verilog/gcd.v"
            export SDC_FILE="$FOUNDRY_DIR/sdc/gcd.sdc"
            export SPEF_FILE="$FOUNDRY_DIR/spef/gcd.spef"
            export IPL_INPUT_DEF=iTO_fix_fanout_result.def
            export RT_BOTTOM_LAYER=met1
            export RT_TOP_LAYER=met4
            ;;
        ihp130_aes)
            export WORKSPACE="$REPO_ROOT/scripts/design/ihp130_gcd"
            export DATASET_RESULT_DIR="${DATASET_RESULT_DIR:-/tmp/iedadb_stage_inputs/ihp130_aes/result}"
            export CONFIG_DIR="$WORKSPACE/iEDA_config"
            export FOUNDRY_DIR="$REPO_ROOT/scripts/foundry/ihp130"
            export TCL_SCRIPT_DIR="$WORKSPACE/script"
            export DESIGN_TOP=aes_cipher_top
            export NETLIST_FILE="$WORKSPACE/result/verilog/aes_nl.v"
            export SDC_FILE="$WORKSPACE/default.sdc"
            unset SPEF_FILE || true
            export IPL_INPUT_DEF=iNO_fix_fanout_result.def
            export RT_BOTTOM_LAYER=Metal2
            export RT_TOP_LAYER=Metal5
            ;;
        ihp130_picorv32a)
            export WORKSPACE="$REPO_ROOT/scripts/design/ihp130_gcd"
            export DATASET_RESULT_DIR="${DATASET_RESULT_DIR:-/tmp/iedadb_stage_inputs/ihp130_picorv32a/result}"
            export CONFIG_DIR="$WORKSPACE/iEDA_config"
            export FOUNDRY_DIR="$REPO_ROOT/scripts/foundry/ihp130"
            export TCL_SCRIPT_DIR="$WORKSPACE/script"
            export DESIGN_TOP=picorv32a
            export NETLIST_FILE="$WORKSPACE/result/verilog/picorv32a_nl.v"
            export SDC_FILE="$WORKSPACE/default.sdc"
            unset SPEF_FILE || true
            export IPL_INPUT_DEF=iNO_fix_fanout_result.def
            export RT_BOTTOM_LAYER=Metal2
            export RT_TOP_LAYER=Metal5
            ;;
        *)
            die "unsupported DATASET=$DATASET"
            ;;
    esac

    [[ "$NATIVE_RUNS" =~ ^[1-9][0-9]*$ ]] || die "NATIVE_RUNS must be positive"
    [[ "$STAGE_RUN_JOBS" == "auto" || "$STAGE_RUN_JOBS" =~ ^[1-9][0-9]*$ ]] || die "STAGE_RUN_JOBS must be auto or positive"
    [[ "$IEDA_PROCESS_MEMORY_GIB" =~ ^[1-9][0-9]*$ ]] || die "IEDA_PROCESS_MEMORY_GIB must be positive"
    [[ "$IEDA_MEMORY_RESERVE_GIB" =~ ^[1-9][0-9]*$ ]] || die "IEDA_MEMORY_RESERVE_GIB must be positive"
    [[ "$IRT_INPUT_GATE_ONLY" == "0" || "$IRT_INPUT_GATE_ONLY" == "1" ]] || die "IRT_INPUT_GATE_ONLY must be 0 or 1"
    require_file "$IEDA_BIN"
    require_file "$RUN_STAGE_TCL"
    require_file "$WRITE_EDADB_TCL"
    require_file "$NORMALIZER"
    require_file "$COMPARE_IRT_INPUT"
    require_file "$SUMMARIZE_IRT_VARIABILITY"

    export DESIGN_TCL_SCRIPT_DIR="$TCL_SCRIPT_DIR"
    export CTS_CONFIG_FILE="${CTS_CONFIG_FILE:-$CONFIG_DIR/cts_default_config.json}"
    require_file "$NETLIST_FILE"
    require_file "$SDC_FILE"
    require_file "$CTS_CONFIG_FILE"

    local stages=("$@")
    if [[ "${#stages[@]}" -eq 0 ]]; then
        stages=("${ALL_STAGES[@]}")
    fi
    local stage
    for stage in "${stages[@]}"; do
        validate_stage "$stage"
    done
}

main "$@"
