#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

IEDA_BIN="${IEDA_BIN:-$REPO_ROOT/bin/iEDA}"
OUT_DIR="${OUT_DIR:-/tmp/iedadb_regression}"
SKY130_WORKSPACE="$REPO_ROOT/scripts/design/sky130_gcd"
BASE_DEF="$SKY130_WORKSPACE/result/iPL_result.def"
ROUTED_DEF="$SKY130_WORKSPACE/result/iRT_result.def"
DESIGN_TCL_SCRIPT_DIR="$SKY130_WORKSPACE/script"

export WORKSPACE="$SKY130_WORKSPACE"
export CONFIG_DIR="$WORKSPACE/iEDA_config"
export FOUNDRY_DIR="$WORKSPACE/../../foundry/sky130"
export TCL_SCRIPT_DIR="$DESIGN_TCL_SCRIPT_DIR"
export DESIGN_TCL_SCRIPT_DIR
export DESIGN_TOP=gcd
export NETLIST_FILE="$WORKSPACE/result/verilog/gcd.v"
export SDC_FILE="$FOUNDRY_DIR/sdc/gcd.sdc"
export SPEF_FILE="$FOUNDRY_DIR/spef/gcd.spef"

require_file() {
    local path="$1"
    if [[ ! -f "$path" ]]; then
        echo "missing required file: $path" >&2
        exit 1
    fi
}

run_ieda() {
    local tcl_script="$1"
    local log_file="$2"
    mkdir -p "$(dirname "$log_file")"
    (
        cd "$(dirname "$IEDA_BIN")"
        "./$(basename "$IEDA_BIN")" -script "$tcl_script"
    ) >"$log_file" 2>&1
}

sql_value() {
    local db="$1"
    local sql="$2"
    sqlite3 "$db" "$sql"
}

assert_eq() {
    local got="$1"
    local expected="$2"
    local label="$3"
    if [[ "$got" != "$expected" ]]; then
        echo "FAIL: $label: expected '$expected', got '$got'" >&2
        exit 1
    fi
    echo "PASS: $label = $got"
}

assert_diff_clean() {
    local expected="$1"
    local actual="$2"
    local diff_file="$3"
    if ! diff -u "$expected" "$actual" >"$diff_file"; then
        echo "FAIL: DEF mismatch: $expected vs $actual" >&2
        echo "diff saved to: $diff_file" >&2
        exit 1
    fi
    echo "PASS: DEF files match: $actual"
}

generate_aux_optional_fixture() {
    local input="$1"
    local output="$2"
    awk '
        {
            if ($0 ~ /^- VDD \( \* VPWR \)/) {
                print
                print "  + SOURCE NETLIST"
                print "  + ORIGINAL orig_vdd_net"
                print "  + WEIGHT 5"
                next
            }
            if ($0 ~ /^- ctrl\$a_mux_sel\\\[0\\\]/) {
                print
                print "  + SOURCE USER"
                print "  + ORIGINAL orig_ctrl_net"
                print "  + WEIGHT 7"
                print "  + XTALK 11"
                print "  + FIXEDBUMP"
                print "  + FREQUENCY 250"
                next
            }
            print
            if ($0 ~ /^END PINS$/) {
                print ""
                print "BLOCKAGES 2 ;"
                print "    - LAYER met1 + PUSHDOWN + EXCEPTPGNET RECT ( 1000 1000 ) ( 2000 2000 ) ;"
                print "    - PLACEMENT RECT ( 3000 3000 ) ( 4000 4000 ) ;"
                print "END BLOCKAGES"
                print ""
                print "REGIONS 1 ;"
                print "    - test_region ( 1000 1000 ) ( 10000 10000 ) + TYPE FENCE ;"
                print "END REGIONS"
                print ""
                print "SLOTS 1 ;"
                print "    - LAYER met1 RECT ( 5000 5000 ) ( 6000 6000 ) ;"
                print "END SLOTS"
                print ""
                print "GROUPS 1 ;"
                print "    - test_group ctrl/_34_ + REGION test_region ;"
                print "END GROUPS"
                print ""
                print "FILLS 2 ;"
                print "    - LAYER met1 RECT ( 7000 7000 ) ( 8000 8000 ) ;"
                print "    - VIA via_1600x480 ( 9000 9000 ) ;"
                print "END FILLS"
            }
        }
    ' "$input" >"$output"
}

run_case() {
    local name="$1"
    local input_def="$2"
    local check_sql="$3"
    local check_routed="${4:-0}"
    local case_dir="$OUT_DIR/$name"
    local direct_def="$case_dir/direct.def"
    local edadb_def="$case_dir/edadb.def"
    local edadb_db="$case_dir/edadb.db"

    rm -rf "$case_dir"
    mkdir -p "$case_dir"

    export RESULT_DIR="$case_dir"
    export INPUT_DEF="$input_def"
    export OUTPUT_DEF="$direct_def"
    run_ieda "$SCRIPT_DIR/tcl/direct_def_roundtrip.tcl" "$case_dir/direct.log"

    rm -f "$edadb_db"
    export EDADB_DB_PATH="$edadb_db"
    run_ieda "$SCRIPT_DIR/tcl/def2edadb_generic.tcl" "$case_dir/def2edadb.log"

    export OUTPUT_DEF="$edadb_def"
    run_ieda "$SCRIPT_DIR/tcl/edadb2def_generic.tcl" "$case_dir/edadb2def.log"

    assert_diff_clean "$direct_def" "$edadb_def" "$case_dir/direct_vs_edadb.diff"
    if [[ "$check_sql" == "1" ]]; then
        assert_eq "$(sql_value "$edadb_db" "select count(*) from iBlockageSD;")" "2" "$name blockage count"
        assert_eq "$(sql_value "$edadb_db" "select count(*) from iRegion;")" "1" "$name region count"
        assert_eq "$(sql_value "$edadb_db" "select count(*) from iSlotSD;")" "1" "$name slot count"
        assert_eq "$(sql_value "$edadb_db" "select count(*) from iGroupSD;")" "1" "$name group count"
        assert_eq "$(sql_value "$edadb_db" "select _group_name_sd || '|' || _region_name_sd from iGroupSD;")" "test_group|test_region" "$name group region"
        assert_eq "$(sql_value "$edadb_db" "select value from iGroupSD__instance_name_vec_sd___edadb_primitive_vector;")" "ctrl/_34_" "$name group member"
        assert_eq "$(sql_value "$edadb_db" "select count(*) from iFillSD;")" "2" "$name fill count"
        assert_eq "$(sql_value "$edadb_db" "select count(*) from iFillSD__rect_list_sd_IdbRect;")" "1" "$name fill rect count"
        assert_eq "$(sql_value "$edadb_db" "select count(*) from iFillSD__coordinate_list_sd_iCoordSD;")" "1" "$name fill via coordinate count"
        assert_eq "$(sql_value "$edadb_db" "select _original_net_name_sd || '|' || _source_type_sd || '|' || _weight_sd from iSpecNetSD where _net_name_sd='VDD';")" "orig_vdd_net|1|5" "$name special net optional fields"
        assert_eq "$(sql_value "$edadb_db" "select _original_net_name_sd || '|' || _source_type_sd || '|' || _weight_sd || '|' || _xtalk_sd || '|' || _fix_bump_sd || '|' || _frequency_sd from iNetSD where _net_name_sd='ctrl\$a_mux_sel[0]';")" "orig_ctrl_net|3|7|11|1|250.0" "$name regular net optional fields"
    fi
    if [[ "$check_routed" == "1" ]]; then
        assert_eq "$(sql_value "$edadb_db" "select count(*) from iNetSD;")" "677" "$name net count"
        assert_eq "$(sql_value "$edadb_db" "select count(*) from iNetSD__wire_list_sd_iRegWireSD;")" "677" "$name regular wire count"
        assert_eq "$(sql_value "$edadb_db" "select count(*) from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD;")" "8997" "$name regular wire segment count"
        assert_eq "$(sql_value "$edadb_db" "select count(*) from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__point_list_sd_iCoordSD;")" "14256" "$name regular wire point count"
    fi

    echo "case output: $case_dir"
}

main() {
    require_file "$IEDA_BIN"
    require_file "$BASE_DEF"
    require_file "$ROUTED_DEF"
    require_file "$DESIGN_TCL_SCRIPT_DIR/DB_script/db_path_setting.tcl"
    require_file "$DESIGN_TCL_SCRIPT_DIR/DB_script/db_init_lef.tcl"
    command -v sqlite3 >/dev/null

    rm -rf "$OUT_DIR"
    mkdir -p "$OUT_DIR/fixtures"

    local aux_def="$OUT_DIR/fixtures/aux_optional.def"
    generate_aux_optional_fixture "$BASE_DEF" "$aux_def"

    echo "EDADB regression output dir: $OUT_DIR"
    echo "iEDA binary: $IEDA_BIN"
    echo "base fixture: $BASE_DEF"
    echo "generated fixture: $aux_def"

    run_case "default_ipl" "$BASE_DEF" "0"
    run_case "aux_optional" "$aux_def" "1"
    run_case "routed_irt" "$ROUTED_DEF" "0" "1"

    echo "All EDADB iDB roundtrip regression tests passed."
}

main "$@"
