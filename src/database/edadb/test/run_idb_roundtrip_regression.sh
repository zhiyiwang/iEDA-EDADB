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

assert_contains() {
    local file="$1"
    local pattern="$2"
    local label="$3"
    if ! grep -Fq "$pattern" "$file"; then
        echo "FAIL: $label: missing '$pattern' in $file" >&2
        exit 1
    fi
    echo "PASS: $label contains '$pattern'"
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

check_default_sql() {
    local name="$1"
    local edadb_db="$2"
    local def2edadb_log="$3"
    local edadb2def_log="$4"
    local demo_counts="${5:-1|1|39|12|0|0|0}"

    assert_eq "$(sql_value "$edadb_db" "select _design_name || '|' || _version || '|' || _units__micron_dbu || '|' || char(_bus_bit_chars__left_delimiter) || '|' || char(_bus_bit_chars__right_delimiter) from iDesign;")" \
        "gcd|5.8|1000|[|]" "$name design fields"
    assert_eq "$(sql_value "$edadb_db" "select (select count(*) from iDesign) || '|' || (select count(*) from iDieSD) || '|' || (select count(*) from iRow) || '|' || (select count(*) from iTrackGridSD) || '|' || (select count(*) from iGCellGrid) || '|' || (select count(*) from iRegion) || '|' || (select count(*) from iSlotSD);")" \
        "$demo_counts" "$name demo object counts"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_x_sd || ',' || _y_sd, ';') from (select _x_sd, _y_sd from iDieSD_points_sd_iCoordSD order by _vec_idx);")" \
        "0,0;149960,150128" "$name die points"
    assert_eq "$(sql_value "$edadb_db" "select _name_sd || '|' || _site_name_sd || '|' || _origin_x_sd || ',' || _origin_y_sd || '|' || _row_num_x_sd || '|' || _row_num_y_sd || '|' || _step_x_sd || '|' || _step_y_sd from iRow where _name_sd='ROW_0';")" \
        "ROW_0|unit|9600,9990|271|1|480|0" "$name row fields"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_order_sd || ':' || _name_sd, ',') from (select _order_sd, _name_sd from iRow order by _order_sd limit 5);")" \
        "0:ROW_0,1:ROW_1,2:ROW_2,3:ROW_3,4:ROW_4" "$name row order prefix"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_order_sd || ':' || _track_sd__direction || ':' || _track_sd__start || ':' || _track_num_sd || ':' || _track_sd__pitch, ';') from (select * from iTrackGridSD order by _order_sd limit 4);")" \
        "0:1:240:311:480;1:2:185:405:370;2:1:185:404:370;3:2:185:405:370" "$name track fields"
    assert_eq "$(sql_value "$edadb_db" "select count(*) || '|' || group_concat(value, ',') from (select value from iTrackGridSD__layer_name_vec_sd___edadb_primitive_vector order by iTrackGridSD_primary_key, __edadb_vec_idx);")" \
        "12|li1,li1,met1,met1,met2,met2,met3,met3,met4,met4,met5,met5" "$name track layer primitive vector"

    assert_contains "$def2edadb_log" "[EDADB-IDB] writeChip2Edadb demo Design/Die/Row/TrackGrid/GCell/Region/Slot enabled" "$name write demo log"
    assert_contains "$edadb2def_log" "[EDADB-IDB] createDbByEdadb demo Design/Die/Row/TrackGrid/GCell/Region/Slot enabled" "$name read demo log"
}

check_aux_optional_sql() {
    local name="$1"
    local edadb_db="$2"

    assert_eq "$(sql_value "$edadb_db" "select count(*) from iRegion;")" "1" "$name region count"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iSlotSD;")" "1" "$name slot count"
    assert_eq "$(sql_value "$edadb_db" "select _name_sd || '|' || _order_sd || '|' || _type_sd from iRegion;")" "test_region|0|1" "$name region fields"
    assert_eq "$(sql_value "$edadb_db" "select _lx || '|' || _ly || '|' || _hx || '|' || _hy from iRegion__boundary_list_sd_IdbRect;")" \
        "1000|1000|10000|10000" "$name region rect"
    assert_eq "$(sql_value "$edadb_db" "select _order_sd || '|' || _layer_name_sd from iSlotSD;")" "0|met1" "$name slot layer"
    assert_eq "$(sql_value "$edadb_db" "select _lx || '|' || _ly || '|' || _hx || '|' || _hy from iSlotSD__rect_list_sd_IdbRect;")" \
        "5000|5000|6000|6000" "$name slot rect"
}

check_routed_sql() {
    local name="$1"
    local edadb_db="$2"
    local def2edadb_log="$3"
    local edadb2def_log="$4"

    assert_eq "$(sql_value "$edadb_db" "select count(*) from iGCellGrid;")" "6" "$name gcell grid count"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_order_sd || ':' || _direction_sd || ':' || _start_sd || ':' || _num_sd || ':' || _space_sd, ';') from (select * from iGCellGrid order by _order_sd);")" \
        "0:1:0:2:3600;1:1:3600:43:3360;2:1:144720:2:5240;3:2:0:2:3600;4:2:3600:43:3360;5:2:144720:2:5408" "$name gcell grid fields"
    assert_contains "$def2edadb_log" "[EDADB-IDB] writeChip2Edadb demo Design/Die/Row/TrackGrid/GCell/Region/Slot enabled" "$name write demo log"
    assert_contains "$edadb2def_log" "[EDADB-IDB] createDbByEdadb demo Design/Die/Row/TrackGrid/GCell/Region/Slot enabled" "$name read demo log"
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
                print "    - test_group ctrl/_34_ ctrl/_35_ + REGION test_region ;"
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
    local check_mode="$3"
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
    case "$check_mode" in
        default)
            check_default_sql "$name" "$edadb_db" "$case_dir/def2edadb.log" "$case_dir/edadb2def.log"
            ;;
        aux)
            check_default_sql "$name" "$edadb_db" "$case_dir/def2edadb.log" "$case_dir/edadb2def.log" "1|1|39|12|0|1|1"
            check_aux_optional_sql "$name" "$edadb_db"
            ;;
        routed)
            check_routed_sql "$name" "$edadb_db" "$case_dir/def2edadb.log" "$case_dir/edadb2def.log"
            ;;
        none)
            ;;
        *)
            echo "unknown check mode: $check_mode" >&2
            exit 1
            ;;
    esac

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

    run_case "default_ipl" "$BASE_DEF" "default"
    run_case "aux_optional" "$aux_def" "aux"
    run_case "routed_irt" "$ROUTED_DEF" "routed"

    echo "All EDADB iDB roundtrip regression tests passed."
}

main "$@"
