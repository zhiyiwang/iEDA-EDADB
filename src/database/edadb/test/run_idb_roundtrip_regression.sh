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
NORMALIZE_DEF_FOR_DIFF="$SCRIPT_DIR/normalize_def_for_diff.py"

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

assert_not_contains() {
    local file="$1"
    local pattern="$2"
    local label="$3"
    if grep -Fq "$pattern" "$file"; then
        echo "FAIL: $label: unexpected '$pattern' in $file" >&2
        exit 1
    fi
    echo "PASS: $label excludes '$pattern'"
}

assert_def_equivalent() {
    local expected="$1"
    local actual="$2"
    local diff_file="$3"
    if diff -u "$expected" "$actual" >"$diff_file"; then
        echo "PASS: DEF files match: $actual"
        return
    fi

    local expected_norm="${diff_file%.diff}.expected.norm.def"
    local actual_norm="${diff_file%.diff}.actual.norm.def"
    local norm_diff="${diff_file%.diff}.normalized.diff"
    python3 "$NORMALIZE_DEF_FOR_DIFF" "$expected" >"$expected_norm"
    python3 "$NORMALIZE_DEF_FOR_DIFF" "$actual" >"$actual_norm"

    if diff -u "$expected_norm" "$actual_norm" >"$norm_diff"; then
        echo "PASS: DEF semantic match with enabled EDADB root order differences: $actual"
        echo "raw diff saved to: $diff_file"
        return
    fi

    echo "FAIL: DEF mismatch: $expected vs $actual" >&2
    echo "raw diff saved to: $diff_file" >&2
    echo "normalized diff saved to: $norm_diff" >&2
    exit 1
}

assert_disabled_tables_absent() {
    local name="$1"
    local edadb_db="$2"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from sqlite_master where type='table' and (name in ('iPinSD','iFillSD','iSpecNetSD','iNetSD','iSpecPinRef','iNetPinRef') or name glob 'iPinSD__*' or name glob 'iFillSD__*' or name glob 'iSpecNetSD__*' or name glob 'iNetSD__*');")" \
        "0" "$name disabled EDADB tables absent"
}

check_fallback_logs() {
    local name="$1"
    local def2edadb_log="$2"
    local edadb2def_log="$3"
    local family
    for family in Pin Fill SpecialNet Net; do
        assert_not_contains "$def2edadb_log" "[EDADB-IDB] writeIdb${family}" "$name write ${family} fallback"
        assert_not_contains "$edadb2def_log" "[EDADB-IDB] readIdb${family}" "$name read ${family} fallback"
    done
}

check_default_sql() {
    local name="$1"
    local edadb_db="$2"
    local def2edadb_log="$3"
    local edadb2def_log="$4"

    assert_eq "$(sql_value "$edadb_db" "select _design_name || '|' || _version || '|' || _units__micron_dbu || '|' || char(_bus_bit_chars__left_delimiter) || '|' || char(_bus_bit_chars__right_delimiter) from iDesign;")" \
        "gcd|5.8|1000|[|]" "$name design fields"
    assert_eq "$(sql_value "$edadb_db" "select (select count(*) from iDesign) || '|' || (select count(*) from iDieSD) || '|' || (select count(*) from iRow) || '|' || (select count(*) from iTrackGridSD) || '|' || (select count(*) from iGCellGrid) || '|' || (select count(*) from iVia) || '|' || (select count(*) from iInstSD);")" \
        "1|1|39|12|0|4|1458" "$name common enabled object counts"
    assert_eq "$(sql_value "$edadb_db" "WITH roots(name) AS (VALUES('iDesign'),('iDieSD'),('iRow'),('iTrackGridSD'),('iGCellGrid'),('iVia'),('iInstSD'),('iBlockageSD'),('iRegion'),('iSlotSD'),('iGroupSD')) SELECT count(*) FROM roots JOIN pragma_table_info(roots.name) p WHERE p.name='_order_sd';")" \
        "0" "$name enabled roots have no order column"
    assert_disabled_tables_absent "$name" "$edadb_db"

    assert_eq "$(sql_value "$edadb_db" "select group_concat(_x_sd || ',' || _y_sd, ';') from (select _x_sd, _y_sd from iDieSD_points_sd_iCoordSD order by _vec_idx);")" \
        "0,0;149960,150128" "$name die points"
    assert_eq "$(sql_value "$edadb_db" "select _name_sd || '|' || _site_name_sd || '|' || _origin_x_sd || ',' || _origin_y_sd || '|' || _row_num_x_sd || '|' || _row_num_y_sd || '|' || _step_x_sd || '|' || _step_y_sd from iRow where _name_sd='ROW_0';")" \
        "ROW_0|unit|9600,9990|271|1|480|0" "$name row fields"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_track_sd__direction || ':' || _track_sd__start || ':' || _track_num_sd || ':' || _track_sd__pitch || ':' || value, ';') from (select g._track_sd__direction, g._track_sd__start, g._track_num_sd, g._track_sd__pitch, v.value from iTrackGridSD g join iTrackGridSD__layer_name_vec_sd___edadb_primitive_vector v on v.iTrackGridSD_primary_key=g.primary_key and v.__edadb_vec_idx=0 order by g._track_sd__direction, g._track_sd__start, g._track_num_sd, g._track_sd__pitch, v.value);")" \
        "1:185:44:3330:met5;1:185:404:370:met1;1:240:311:480:li1;1:240:311:480:met2;1:370:202:740:met3;1:480:155:960:met4;2:185:45:3330:met5;2:185:405:370:li1;2:185:405:370:met1;2:240:312:480:met2;2:370:202:740:met3;2:480:155:960:met4" "$name track fields"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_name, ',') from (select _name from iVia order by _name);")" \
        "via2_1600x480,via3_1600x480,via4_1600x1600,via_1600x480" "$name via names"
    assert_eq "$(sql_value "$edadb_db" "select _name_sd || '|' || _cell_master_name_sd || '|' || _status_sd || '|' || _orient_sd || '|' || _coordinate_sd__x_sd || ',' || _coordinate_sd__y_sd from iInstSD where _name_sd='ENDCAP_0';")" \
        "ENDCAP_0|sky130_fd_sc_hs__fill_1|1|7|9600,9990" "$name instance fields"

    assert_contains "$def2edadb_log" "[EDADB-IDB] writeIdbVia insert via_count=4" "$name write via log"
    assert_contains "$edadb2def_log" "[EDADB-IDB] readIdbVia restored via_count=4" "$name read via log"
    assert_contains "$def2edadb_log" "[EDADB-IDB] writeIdbInstance insert instance_count=1458" "$name write instance log"
    assert_contains "$edadb2def_log" "[EDADB-IDB] readIdbInstance restored instance_count=1458" "$name read instance log"
    check_fallback_logs "$name" "$def2edadb_log" "$edadb2def_log"
}

check_aux_optional_sql() {
    local name="$1"
    local edadb_db="$2"

    assert_eq "$(sql_value "$edadb_db" "select (select count(*) from iBlockageSD) || '|' || (select count(*) from iRegion) || '|' || (select count(*) from iSlotSD) || '|' || (select count(*) from iGroupSD);")" \
        "2|1|1|1" "$name optional enabled counts"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_type_sd || '|' || coalesce(_layer_name_sd,'') || '|' || _is_pushdown_sd || '|' || _is_except_pgnet_sd, ';') from (select * from iBlockageSD order by _type_sd, coalesce(_layer_name_sd,''), _is_pushdown_sd, _is_except_pgnet_sd);")" \
        "1|met1|1|1;2||0|0" "$name blockage fields"
    assert_eq "$(sql_value "$edadb_db" "select _name || '|' || _type from iRegion;")" \
        "test_region|1" "$name region fields"
    assert_eq "$(sql_value "$edadb_db" "select _layer_name_sd from iSlotSD;")" \
        "met1" "$name slot layer"
    assert_eq "$(sql_value "$edadb_db" "select _group_name_sd || '|' || _region_name_sd from iGroupSD;")" \
        "test_group|test_region" "$name group region"
    assert_eq "$(sql_value "$edadb_db" "select _weight_sd || '|' || _region_name_sd from iInstSD where _name_sd='ctrl/_34_';")" \
        "13|test_region" "$name instance weight region"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(__edadb_vec_idx || ':' || value, ',') from (select __edadb_vec_idx, value from iGroupSD__instance_name_vec_sd___edadb_primitive_vector order by __edadb_vec_idx);")" \
        "0:ctrl/_34_,1:ctrl/_35_" "$name group member order"
    assert_disabled_tables_absent "$name" "$edadb_db"
}

check_routed_sql() {
    local name="$1"
    local edadb_db="$2"
    local def2edadb_log="$3"
    local edadb2def_log="$4"

    assert_eq "$(sql_value "$edadb_db" "select count(*) from iGCellGrid;")" \
        "6" "$name gcell grid count"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_direction || ':' || _start || ':' || _num || ':' || _space, ';') from (select * from iGCellGrid order by _direction, _start, _num, _space);")" \
        "1:0:2:3600;1:3600:43:3360;1:144720:2:5240;2:0:2:3600;2:3600:43:3360;2:144720:2:5408" "$name gcell grid fields"
    assert_disabled_tables_absent "$name" "$edadb_db"
    check_fallback_logs "$name" "$def2edadb_log" "$edadb2def_log"
}

generate_aux_optional_fixture() {
    local input="$1"
    local output="$2"
    awk '
        {
            if ($0 ~ /^ - clk /) {
                print " - clk + NET clk + SPECIAL + DIRECTION INPUT  + USE SIGNAL"
                print "  + PORT"
                print "   + LAYER met5 ( -1000 -1000 ) ( 1000 1000 ) + PLACED ( 1000 9990 ) N"
                print ";"
                skip_pin = 1
                next
            }
            if (skip_pin) {
                if ($0 ~ /^;$/) {
                    skip_pin = 0
                }
                next
            }
            if ($0 ~ /^COMPONENTS /) {
                print "REGIONS 1 ;"
                print "    - test_region ( 1000 1000 ) ( 10000 10000 ) + TYPE FENCE ;"
                print "END REGIONS"
                print ""
                print
                next
            }
            if ($0 ~ /^    - ctrl\/_34_ /) {
                print
                print "      + WEIGHT 13"
                print "      + REGION test_region"
                next
            }
            if ($0 ~ /^- VDD \( \* VPWR \)/) {
                print
                print "  + SOURCE NETLIST"
                print "  + ORIGINAL orig_vdd_net"
                print "  + WEIGHT 5"
                next
            }
            if ($0 ~ /^- VSS \( \* VGND \)/) {
                print "- VSS ( PIN clk ) ( ctrl/_34_ A ) "
                in_vss_special_net = 1
                next
            }
            if (in_vss_special_net && $0 ~ /^  \+ USE GROUND[[:space:]]*$/) {
                print
                print "  + ROUTED + RECT met1 ( 11000 11000 ) ( 13000 14000 )"
                next
            }
            if (in_vss_special_net && $0 ~ /^;$/) {
                in_vss_special_net = 0
            }
            if ($0 ~ /^- ctrl\$a_mux_sel\[0\]/) {
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

    assert_def_equivalent "$direct_def" "$edadb_def" "$case_dir/direct_vs_edadb.diff"
    case "$check_mode" in
        default)
            check_default_sql "$name" "$edadb_db" "$case_dir/def2edadb.log" "$case_dir/edadb2def.log"
            ;;
        aux)
            check_default_sql "$name" "$edadb_db" "$case_dir/def2edadb.log" "$case_dir/edadb2def.log"
            check_aux_optional_sql "$name" "$edadb_db"
            ;;
        routed)
            check_routed_sql "$name" "$edadb_db" "$case_dir/def2edadb.log" "$case_dir/edadb2def.log"
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
    require_file "$NORMALIZE_DEF_FOR_DIFF"
    command -v sqlite3 >/dev/null
    command -v python3 >/dev/null

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

    echo "All demo EDADB subset roundtrip regression tests passed."
}

main "$@"
