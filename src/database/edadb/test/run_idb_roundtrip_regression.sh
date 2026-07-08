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
        echo "PASS: DEF semantic match with D-level root order differences: $actual"
        echo "raw diff saved to: $diff_file"
        return
    fi

    echo "FAIL: DEF mismatch: $expected vs $actual" >&2
    echo "raw diff saved to: $diff_file" >&2
    echo "normalized diff saved to: $norm_diff" >&2
    exit 1
}

check_default_sql() {
    local name="$1"
    local edadb_db="$2"
    local def2edadb_log="$3"
    local edadb2def_log="$4"

    assert_eq "$(sql_value "$edadb_db" "select _design_name || '|' || _version || '|' || _units__micron_dbu || '|' || char(_bus_bit_chars__left_delimiter) || '|' || char(_bus_bit_chars__right_delimiter) from iDesign;")" \
        "gcd|5.8|1000|[|]" "$name design fields"
    assert_eq "$(sql_value "$edadb_db" "select (select count(*) from iDesign) || '|' || (select count(*) from iDieSD) || '|' || (select count(*) from iRow) || '|' || (select count(*) from iTrackGridSD) || '|' || (select count(*) from iGCellGrid) || '|' || (select count(*) from iVia) || '|' || (select count(*) from iInstSD) || '|' || (select count(*) from iPinSD) || '|' || (select count(*) from iSpecNetSD) || '|' || (select count(*) from iNetSD);")" \
        "1|1|39|12|0|4|1458|56|2|675" "$name core object counts"
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
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_name, ',') from (select _name from iVia order by _name);")" \
        "via2_1600x480,via3_1600x480,via4_1600x1600,via_1600x480" "$name via names"
    assert_eq "$(sql_value "$edadb_db" "select _name || '|' || _master_instance__type_sd || '|' || _master_instance__master_generate_sd__rule_name_sd || '|' || _master_instance__master_generate_sd__cut_size_x_sd || '|' || _master_instance__master_generate_sd__cut_size_y_sd || '|' || _master_instance__master_generate_sd__num_cut_rows_sd || '|' || _master_instance__master_generate_sd__num_cut_cols_sd || '|' || _master_instance__master_generate_sd__layer_bottom_name_sd || '|' || _master_instance__master_generate_sd__layer_cut_name_sd || '|' || _master_instance__master_generate_sd__layer_top_name_sd from iVia where _name='via_1600x480';")" \
        "via_1600x480|1|M1M2_PR_C|150|150|2|5|met1|via|met2" "$name via generate fields"
    assert_contains "$def2edadb_log" "[EDADB-IDB] writeIdbVia insert via_count=4" "$name write via log"
    assert_contains "$edadb2def_log" "[EDADB-IDB] readIdbVia restored via_count=4" "$name read via log"
    assert_eq "$(sql_value "$edadb_db" "select _name_sd || '|' || _cell_master_name_sd || '|' || _status_sd || '|' || _orient_sd || '|' || _coordinate_sd__x_sd || ',' || _coordinate_sd__y_sd from iInstSD where _name_sd='ENDCAP_0';")" \
        "ENDCAP_0|sky130_fd_sc_hs__fill_1|1|7|9600,9990" "$name instance fields"
    assert_eq "$(sql_value "$edadb_db" "select _weight_sd || '|' || _region_name_sd from iInstSD where _name_sd='ENDCAP_0';")" \
        "-1|" "$name instance default weight region"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_order_sd || ':' || _name_sd, ',') from (select _order_sd, _name_sd from iInstSD order by _order_sd limit 5);")" \
        "0:ctrl/_17_,1:ctrl/_18_,2:ctrl/_19_,3:ctrl/_20_,4:ctrl/_21_" "$name instance order prefix"
    assert_eq "$(sql_value "$edadb_db" "select _pin_name_sd || '|' || _net_name_sd || '|' || _io_term_sd__direction_sd || '|' || _io_term_sd__type_sd || '|' || _io_term_sd__has_port_sd || '|' || _location_sd__x_sd || ',' || _location_sd__y_sd || '|' || _layer_num_sd from iPinSD where _pin_name_sd='req_msg[0]';")" \
        "req_msg[0]|req_msg[0]|1|1|0|1000,18645|1" "$name pin fields"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_order_sd || ':' || _pin_name_sd, ',') from (select _order_sd, _pin_name_sd from iPinSD order by _order_sd limit 5);")" \
        "0:clk,1:req_msg[0],2:req_msg[1],3:req_msg[2],4:req_msg[3]" "$name pin order prefix"
    assert_eq "$(sql_value "$edadb_db" "select (select count(*) from iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD) || '|' || (select count(*) from iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD__layer_shape_list_sd_iLayerShapeSD) || '|' || (select count(*) from iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD__layer_shape_list_sd_iLayerShapeSD__rect_list_sd_IdbRectSD);")" \
        "56|56|56" "$name pin port/layer/rect child counts"
    assert_eq "$(sql_value "$edadb_db" "select _net_name_sd || '|' || _connect_type_sd || '|' || _source_type_sd || '|' || _weight_sd from iSpecNetSD where _net_name_sd='VSS';")" \
        "VSS|4|0|0" "$name special net default fields"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_order_sd || ':' || _net_name_sd, ',') from (select _order_sd, _net_name_sd from iSpecNetSD order by _order_sd);")" \
        "0:VDD,1:VSS" "$name special net order"
    assert_eq "$(sql_value "$edadb_db" "select (select count(*) from iSpecNetSD__pin_string_list_sd___edadb_primitive_vector) || '|' || (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD) || '|' || (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD) || '|' || (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD__point_list_sd_iCoordSD);")" \
        "6|2|639|697" "$name special net child counts"
    assert_eq "$(sql_value "$edadb_db" "select (select count(*) from iSpecNetSD__pin_string_list_sd___edadb_primitive_vector) || '|' || (select count(*) from iSpecNetSD__io_pin_name_list_sd___edadb_primitive_vector) || '|' || (select count(*) from iSpecNetSD__instance_pin_list_sd_iSpecPinRef);")" \
        "6|0|0" "$name special net pin-string exclusive refs"
    assert_eq "$(sql_value "$edadb_db" "select (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD where _is_via_sd=1) || '|' || (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD where _is_rect_sd=1) || '|' || (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD where _is_via_sd=0 and _is_rect_sd=0);")" \
        "581|0|58" "$name special net segment dispatch types"
    assert_eq "$(sql_value "$edadb_db" "select _net_name_sd || '|' || _connect_type_sd || '|' || _source_type_sd || '|' || _weight_sd || '|' || _xtalk_sd || '|' || _fix_bump_sd || '|' || _frequency_sd from iNetSD where _net_name_sd='clk';")" \
        "clk|5|0|0|0|0|-1.0" "$name regular net default fields"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_order_sd || ':' || _net_name_sd, ',') from (select _order_sd, _net_name_sd from iNetSD order by _order_sd limit 5);")" \
        "0:ctrl\$a_mux_sel[0],1:ctrl\$a_mux_sel[1],2:ctrl\$a_reg_en,3:ctrl\$b_mux_sel,4:ctrl\$b_reg_en" "$name regular net order prefix"

    assert_contains "$def2edadb_log" "[EDADB-IDB] writeIdbInstance insert instance_count=1458" "$name write instance log"
    assert_contains "$def2edadb_log" "[EDADB-IDB] writeIdbPin insert pin_count=56" "$name write pin log"
    assert_contains "$edadb2def_log" "[EDADB-IDB] readIdbInstance restored instance_count=1458" "$name read instance log"
    assert_contains "$edadb2def_log" "[EDADB-IDB] readIdbPin restored pin_count=56" "$name read pin log"
}

check_aux_optional_sql() {
    local name="$1"
    local edadb_db="$2"

    assert_eq "$(sql_value "$edadb_db" "select count(*) from iBlockageSD;")" "2" "$name blockage count"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iRegion;")" "1" "$name region count"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iSlotSD;")" "1" "$name slot count"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iGroupSD;")" "1" "$name group count"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iFillSD;")" "2" "$name fill count"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_type_sd || '|' || coalesce(_layer_name_sd,'') || '|' || _is_pushdown_sd || '|' || _is_except_pgnet_sd, ';') from (select * from iBlockageSD order by _type_sd, coalesce(_layer_name_sd,''), _is_pushdown_sd, _is_except_pgnet_sd);")" \
        "1|met1|1|1;2||0|0" "$name blockage fields"
    assert_eq "$(sql_value "$edadb_db" "select _name || '|' || _type from iRegion;")" "test_region|1" "$name region fields"
    assert_eq "$(sql_value "$edadb_db" "select _vec_idx || '|' || _lx_sd || '|' || _ly_sd || '|' || _hx_sd || '|' || _hy_sd from iRegion__boudary_list_IdbRectSD;")" \
        "0|1000|1000|10000|10000" "$name region rect"
    assert_eq "$(sql_value "$edadb_db" "select _order_sd || '|' || _layer_name_sd from iSlotSD;")" "0|met1" "$name slot layer"
    assert_eq "$(sql_value "$edadb_db" "select _vec_idx || '|' || _lx_sd || '|' || _ly_sd || '|' || _hx_sd || '|' || _hy_sd from iSlotSD__rect_list_sd_IdbRectSD;")" \
        "0|5000|5000|6000|6000" "$name slot rect"
    assert_eq "$(sql_value "$edadb_db" "select _group_name_sd || '|' || _order_sd || '|' || _region_name_sd from iGroupSD;")" "test_group|0|test_region" "$name group region"
    assert_eq "$(sql_value "$edadb_db" "select _weight_sd || '|' || _region_name_sd from iInstSD where _name_sd='ctrl/_34_';")" \
        "13|test_region" "$name instance weight region"
    assert_eq "$(sql_value "$edadb_db" "select _io_term_sd__has_port_sd || '|' || _io_term_sd__is_special_net_sd || '|' || _io_term_sd__placement_status_sd from iPinSD where _pin_name_sd='clk';")" \
        "1|1|3" "$name explicit special port term fields"
    assert_eq "$(sql_value "$edadb_db" "select _placement_status_sd || '|' || _coordinate_sd__x_sd || ',' || _coordinate_sd__y_sd || '|' || _orient_sd from iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD where iPinSD__pin_name_sd='clk';")" \
        "3|1000,9990|1" "$name explicit port placement"
    assert_eq "$(sql_value "$edadb_db" "select _layer_name_sd || '|' || _type_sd from iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD__layer_shape_list_sd_iLayerShapeSD where iPinSD__pin_name_sd='clk';")" \
        "met5|1" "$name explicit port layer shape"
    assert_eq "$(sql_value "$edadb_db" "select _vec_idx || '|' || _lx_sd || '|' || _ly_sd || '|' || _hx_sd || '|' || _hy_sd from iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD__layer_shape_list_sd_iLayerShapeSD__rect_list_sd_IdbRectSD where iPinSD__pin_name_sd='clk';")" \
        "0|-1000|-1000|1000|1000" "$name explicit port rect"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(__edadb_vec_idx || ':' || value, ',') from (select __edadb_vec_idx, value from iGroupSD__instance_name_vec_sd___edadb_primitive_vector order by __edadb_vec_idx);")" \
        "0:ctrl/_34_,1:ctrl/_35_" "$name group member order"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_order_sd || '|' || _type_sd || '|' || coalesce(_layer_name_sd,'') || '|' || coalesce(_via_name_sd,''), ';') from (select * from iFillSD order by _order_sd);")" \
        "0|1|met1|;1|2||via_1600x480" "$name fill typed rows"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iFillSD__rect_list_sd_IdbRectSD;")" "1" "$name fill rect count"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iFillSD__coordinate_list_sd_iCoordSD;")" "1" "$name fill via coordinate count"
    assert_eq "$(sql_value "$edadb_db" "select _original_net_name_sd || '|' || _source_type_sd || '|' || _weight_sd from iSpecNetSD where _net_name_sd='VDD';")" \
        "orig_vdd_net|1|5" "$name special net optional fields"
    assert_eq "$(sql_value "$edadb_db" "select _original_net_name_sd || '|' || _source_type_sd || '|' || _weight_sd || '|' || _xtalk_sd || '|' || _fix_bump_sd || '|' || _frequency_sd from iNetSD where _net_name_sd='ctrl\$a_mux_sel[0]';")" \
        "orig_ctrl_net|3|7|11|1|250.0" "$name regular net optional fields"
}

check_routed_sql() {
    local name="$1"
    local edadb_db="$2"
    local def2edadb_log="$3"
    local edadb2def_log="$4"

    assert_eq "$(sql_value "$edadb_db" "select count(*) from iGCellGrid;")" "6" "$name gcell grid count"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_direction || ':' || _start || ':' || _num || ':' || _space, ';') from (select * from iGCellGrid order by _direction, _start, _num, _space);")" \
        "1:0:2:3600;1:3600:43:3360;1:144720:2:5240;2:0:2:3600;2:3600:43:3360;2:144720:2:5408" "$name gcell grid fields"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iNetSD;")" "677" "$name net count"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_order_sd || ':' || _net_name_sd, ',') from (select _order_sd, _net_name_sd from iNetSD order by _order_sd limit 5);")" \
        "0:ctrl\$a_mux_sel[0],1:ctrl\$a_mux_sel[1],2:ctrl\$a_reg_en,3:ctrl\$b_mux_sel,4:ctrl\$b_reg_en" "$name routed net order prefix"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iNetSD__wire_list_sd_iRegWireSD;")" "677" "$name regular wire count"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD;")" "8997" "$name regular wire segment count"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__point_list_sd_iCoordSD;")" "14256" "$name regular wire point count"
    assert_eq "$(sql_value "$edadb_db" "select (select count(*) from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD where _is_via_sd=1) || '|' || (select count(*) from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD where _is_rect_sd=1) || '|' || (select count(*) from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD where _is_second_point_virtual_sd=1) || '|' || (select count(*) from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD where _via_name_sd is not null and _via_name_sd != '');")" \
        "3716|22|0|3716" "$name regular wire segment types"
    assert_eq "$(sql_value "$edadb_db" "select min(_order_sd) || '|' || max(_order_sd) || '|' || count(*) from iNetSD__instance_pin_list_sd_iNetPinRef where iNetSD__net_name_sd='clk_0';")" \
        "0|18|19" "$name clk_0 ordered pin refs"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_order_sd || ':' || instance_name || ':' || pin_name, ',') from (select _order_sd, instance_name, pin_name from iNetSD__instance_pin_list_sd_iNetPinRef where iNetSD__net_name_sd='clk_0' order by _order_sd limit 5);")" \
        "0:clk_0_buf:X,1:dpath/b_reg/_140_:CLK,2:dpath/b_reg/_139_:CLK,3:dpath/b_reg/_138_:CLK,4:dpath/b_reg/_137_:CLK" "$name clk_0 pin order prefix"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(iNetSD__net_name_sd || ':' || cnt, ',') from (select iNetSD__net_name_sd, count(*) as cnt from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD group by iNetSD__net_name_sd order by cnt desc, iNetSD__net_name_sd limit 3);")" \
        "clk_0:138,clk_1:137,dpath/a_mux/_066_:103" "$name largest routed segment nets"

    assert_contains "$def2edadb_log" "[EDADB-IDB] writeIdbNet insert net_count=677" "$name write routed net log"
    assert_contains "$edadb2def_log" "[EDADB-IDB] readIdbNet restored net_count=677" "$name read routed net log"
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

    echo "All EDADB iDB roundtrip regression tests passed."
}

main "$@"
