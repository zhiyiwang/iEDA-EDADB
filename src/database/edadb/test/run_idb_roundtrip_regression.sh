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

check_def_fallback_storage() {
    local name="$1"
    local edadb_db="$2"
    local def2edadb_log="$3"
    local edadb2def_log="$4"

    assert_eq "$(sql_value "$edadb_db" "select count(*) from sqlite_master where type='table' and (name='iFillSD' or name='iSpecNetSD' or name='iNetSD');")" \
        "0" "$name Fill/SpecialNet/Net EDADB tables are absent"
    assert_not_contains "$def2edadb_log" "writeIdbFill" "$name Fill write uses DEF fallback"
    assert_not_contains "$def2edadb_log" "writeIdbSpecialNet" "$name SpecialNet write uses DEF fallback"
    assert_not_contains "$def2edadb_log" "writeIdbNet" "$name Net write uses DEF fallback"
    assert_not_contains "$edadb2def_log" "readIdbFill" "$name Fill read uses DEF fallback"
    assert_not_contains "$edadb2def_log" "readIdbSpecialNet" "$name SpecialNet read uses DEF fallback"
    assert_not_contains "$edadb2def_log" "readIdbNet" "$name Net read uses DEF fallback"
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
    assert_eq "$(sql_value "$edadb_db" "select (select count(*) from iDesign) || '|' || (select count(*) from iDieSD) || '|' || (select count(*) from iRow) || '|' || (select count(*) from iTrackGridSD) || '|' || (select count(*) from iGCellGrid) || '|' || (select count(*) from iVia) || '|' || (select count(*) from iInstSD) || '|' || (select count(*) from iPinSD);")" \
        "1|1|39|12|0|4|1458|56" "$name core object counts"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_x_sd || ',' || _y_sd, ';') from (select _x_sd, _y_sd from iDieSD_points_sd_iCoordSD order by _vec_idx);")" \
        "0,0;149960,150128" "$name die points"
    assert_eq "$(sql_value "$edadb_db" "select _name_sd || '|' || _site_name_sd || '|' || _origin_x_sd || ',' || _origin_y_sd || '|' || _row_num_x_sd || '|' || _row_num_y_sd || '|' || _step_x_sd || '|' || _step_y_sd from iRow where _name_sd='ROW_0';")" \
        "ROW_0|unit|9600,9990|271|1|480|0" "$name row fields"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_order_sd || ':' || _name_sd, ',') from (select _order_sd, _name_sd from iRow order by _order_sd limit 5);")" \
        "0:ROW_0,1:ROW_1,2:ROW_2,3:ROW_3,4:ROW_4" "$name row order prefix"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_order_sd, ',') from (select _order_sd from iRow order by rowid limit 5);")" \
        "38,37,36,35,34" "$name row table physical order was perturbed"
    assert_eq "$(sql_value "$edadb_db" "select pk from pragma_table_info('iRow') where name='_name_sd';")" \
        "1" "$name row name is primary key"
    assert_eq "$(sql_value "$edadb_db" "select pk from pragma_table_info('iRow') where name='_order_sd';")" \
        "0" "$name row order is not primary key"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from sqlite_master where type='table' and name='iSite';")" \
        "0" "$name row does not persist an iSite table"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from pragma_table_info('iTrackGridSD') where name='_order_sd';")" \
        "0" "$name track grid has no root order column"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_track_sd__direction || ':' || _track_sd__start || ':' || _track_num_sd || ':' || _track_sd__pitch || ':' || value, ';') from (select g._track_sd__direction, g._track_sd__start, g._track_num_sd, g._track_sd__pitch, v.value from iTrackGridSD g join iTrackGridSD__layer_name_vec_sd___edadb_primitive_vector v on v.iTrackGridSD_primary_key=g.primary_key and v.__edadb_vec_idx=0 order by g._track_sd__direction, g._track_sd__start, g._track_num_sd, g._track_sd__pitch, v.value);")" \
        "1:185:44:3330:met5;1:185:404:370:met1;1:240:311:480:li1;1:240:311:480:met2;1:370:202:740:met3;1:480:155:960:met4;2:185:45:3330:met5;2:185:405:370:li1;2:185:405:370:met1;2:240:312:480:met2;2:370:202:740:met3;2:480:155:960:met4" "$name track fields"
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
    assert_eq "$(sql_value "$edadb_db" "select _order_sd from iInstSD order by rowid limit 1;")" \
        "1457" "$name instance table physical order was perturbed"
    assert_eq "$(sql_value "$edadb_db" "select _pin_name_sd || '|' || _net_name_sd || '|' || _io_term_sd__direction_sd || '|' || _io_term_sd__type_sd || '|' || _io_term_sd__has_port_sd || '|' || _no_port_placement_status_sd || '|' || _no_port_location_sd__x_sd || ',' || _no_port_location_sd__y_sd from iPinSD where _pin_name_sd='req_msg[0]';")" \
        "req_msg[0]|req_msg[0]|1|1|0|3|1000,18645" "$name pin DEF source fields"
    assert_eq "$(sql_value "$edadb_db" "select (select count(*) from pragma_table_info('iPinSD') where name in ('_average_coordinate_sd__x_sd','_average_coordinate_sd__y_sd','_is_io_pin_sd','_is_special_net_sd','_layer_num_sd','_io_term_sd__name_sd','_io_term_sd__shape_sd','_io_term_sd__placement_status_sd','_io_term_sd__is_instance_sd')) || '|' || (select count(*) from pragma_table_info('iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD') where name='_class_sd');")" \
        "0|0" "$name excludes derived pin term port columns"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_order_sd || ':' || _pin_name_sd, ',') from (select _order_sd, _pin_name_sd from iPinSD order by _order_sd limit 5);")" \
        "0:clk,1:req_msg[0],2:req_msg[1],3:req_msg[2],4:req_msg[3]" "$name pin order prefix"
    assert_eq "$(sql_value "$edadb_db" "select (select count(*) from iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD) || '|' || (select count(*) from iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD__layer_shape_list_sd_iLayerShapeSD) || '|' || (select count(*) from iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD__layer_shape_list_sd_iLayerShapeSD__rect_list_sd_IdbRectSD);")" \
        "56|56|56" "$name pin port/layer/rect child counts"

    assert_contains "$def2edadb_log" "[EDADB-IDB] writeIdbInstance insert instance_count=1458" "$name write instance log"
    assert_contains "$def2edadb_log" "[EDADB-IDB] writeIdbPin insert pin_count=56" "$name write pin log"
    assert_contains "$edadb2def_log" "[EDADB-IDB] readIdbInstance restored instance_count=1458" "$name read instance log"
    assert_contains "$edadb2def_log" "[EDADB-IDB] readIdbPin restored pin_count=56" "$name read pin log"
    check_def_fallback_storage "$name" "$edadb_db" "$def2edadb_log" "$edadb2def_log"
}

check_aux_optional_sql() {
    local name="$1"
    local edadb_db="$2"

    assert_eq "$(sql_value "$edadb_db" "select count(*) from iBlockageSD;")" "2" "$name blockage count"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iRegion;")" "1" "$name region count"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iSlotSD;")" "1" "$name slot count"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iGroupSD;")" "1" "$name group count"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_type_sd || '|' || coalesce(_layer_name_sd,'') || '|' || _is_pushdown_sd || '|' || _is_except_pgnet_sd, ';') from (select * from iBlockageSD order by _type_sd, coalesce(_layer_name_sd,''), _is_pushdown_sd, _is_except_pgnet_sd);")" \
        "1|met1|1|1;2||0|0" "$name blockage fields"
    assert_eq "$(sql_value "$edadb_db" "select _name || '|' || _type from iRegion;")" "test_region|1" "$name region fields"
    assert_eq "$(sql_value "$edadb_db" "select _vec_idx || '|' || _lx_sd || '|' || _ly_sd || '|' || _hx_sd || '|' || _hy_sd from iRegion__boudary_list_IdbRectSD;")" \
        "0|1000|1000|10000|10000" "$name region rect"
    assert_eq "$(sql_value "$edadb_db" "select _order_sd || '|' || _layer_name_sd from iSlotSD;")" "0|met1" "$name slot layer"
    assert_eq "$(sql_value "$edadb_db" "select _vec_idx || '|' || _lx_sd || '|' || _ly_sd || '|' || _hx_sd || '|' || _hy_sd from iSlotSD__rect_list_sd_IdbRectSD;")" \
        "0|5000|5000|6000|6000" "$name slot rect"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from pragma_table_info('iGroupSD') where name='_order_sd';")" \
        "0" "$name group has no root order column"
    assert_eq "$(sql_value "$edadb_db" "select _group_name_sd || '|' || _region_name_sd from iGroupSD;")" "test_group|test_region" "$name group region"
    assert_eq "$(sql_value "$edadb_db" "select _weight_sd || '|' || _region_name_sd from iInstSD where _name_sd='ctrl/_34_';")" \
        "13|test_region" "$name instance weight region"
    assert_eq "$(sql_value "$edadb_db" "select _io_term_sd__has_port_sd || '|' || _io_term_sd__is_special_net_sd from iPinSD where _pin_name_sd='clk';")" \
        "1|1" "$name explicit special port term DEF fields"
    assert_eq "$(sql_value "$edadb_db" "select _placement_status_sd || '|' || _coordinate_sd__x_sd || ',' || _coordinate_sd__y_sd || '|' || _orient_sd from iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD where iPinSD__pin_name_sd='clk';")" \
        "3|1000,9990|1" "$name explicit port placement"
    assert_eq "$(sql_value "$edadb_db" "select _layer_name_sd || '|' || _type_sd from iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD__layer_shape_list_sd_iLayerShapeSD where iPinSD__pin_name_sd='clk';")" \
        "met5|1" "$name explicit port layer shape"
    assert_eq "$(sql_value "$edadb_db" "select _vec_idx || '|' || _lx_sd || '|' || _ly_sd || '|' || _hx_sd || '|' || _hy_sd from iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD__layer_shape_list_sd_iLayerShapeSD__rect_list_sd_IdbRectSD where iPinSD__pin_name_sd='clk';")" \
        "0|-1000|-1000|1000|1000" "$name explicit port rect"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(__edadb_vec_idx || ':' || value, ',') from (select __edadb_vec_idx, value from iGroupSD__instance_name_vec_sd___edadb_primitive_vector order by __edadb_vec_idx);")" \
        "0:ctrl/_34_,1:ctrl/_35_" "$name group member order"
}

check_routed_sql() {
    local name="$1"
    local edadb_db="$2"
    local def2edadb_log="$3"
    local edadb2def_log="$4"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iGCellGrid;")" "6" "$name gcell grid count"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_direction || ':' || _start || ':' || _num || ':' || _space, ';') from (select * from iGCellGrid order by _direction, _start, _num, _space);")" \
        "1:0:2:3600;1:3600:43:3360;1:144720:2:5240;2:0:2:3600;2:3600:43:3360;2:144720:2:5408" "$name gcell grid fields"
    check_def_fallback_storage "$name" "$edadb_db" "$def2edadb_log" "$edadb2def_log"
}

check_grid_branch_sql() {
    local name="$1"
    local edadb_db="$2"
    local def2edadb_log="$3"
    local edadb2def_log="$4"

    assert_eq "$(sql_value "$edadb_db" "select count(*) from iTrackGridSD;")" \
        "12" "$name track grid count"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from pragma_table_info('iTrackGridSD') where name='_order_sd';")" \
        "0" "$name track grid has no root order column"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(primary_key, ',') from (select primary_key from iTrackGridSD order by rowid);")" \
        "12,11,10,9,8,7,6,5,4,3,2,1" "$name perturbed track root fetch order"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(__edadb_vec_idx || ':' || value, ',') from (select __edadb_vec_idx, value from iTrackGridSD__layer_name_vec_sd___edadb_primitive_vector where iTrackGridSD_primary_key=1 order by rowid);")" \
        "1:met4,0:met5" "$name perturbed track layer fetch order"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(__edadb_vec_idx || ':' || value, ',') from (select __edadb_vec_idx, value from iTrackGridSD__layer_name_vec_sd___edadb_primitive_vector where iTrackGridSD_primary_key=1 order by __edadb_vec_idx);")" \
        "0:met5,1:met4" "$name ordered track layer names"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_direction || ':' || _start || ':' || _num || ':' || _space, ';') from (select _direction, _start, _num, _space from iGCellGrid order by rowid);")" \
        "2:144720:2:5408;2:3600:43:3360;2:0:2:3600;1:144720:2:5240;1:3600:43:3360;1:0:2:3600" "$name perturbed gcell root fetch order"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_direction || ':' || _start || ':' || _num || ':' || _space, ';') from (select * from iGCellGrid order by _direction, _start, _num, _space);")" \
        "1:0:2:3600;1:3600:43:3360;1:144720:2:5240;2:0:2:3600;2:3600:43:3360;2:144720:2:5408" "$name gcell grid fields"
    assert_eq "$(sql_value "$edadb_db" "select sum(pk) from pragma_table_info('iGCellGrid');")" \
        "0" "$name gcell grid has no primary key"
    assert_contains "$def2edadb_log" "[EDADB-IDB] writeIdbTrackGrid insert track_grid_count=12" "$name write track grid log"
    assert_contains "$edadb2def_log" "[EDADB-IDB] readIdbTrackGrid restored track_grid_count=12" "$name read track grid log"
    assert_contains "$def2edadb_log" "[EDADB-IDB] writeIdbGCellGrid insert gcell_grid_count=6" "$name write gcell grid log"
    assert_contains "$edadb2def_log" "[EDADB-IDB] readIdbGCellGrid restored gcell_grid_count=6" "$name read gcell grid log"
}

check_via_branch_sql() {
    local name="$1"
    local edadb_db="$2"
    local def2edadb_log="$3"
    local edadb2def_log="$4"
    local shape_table="iVia__master_instance_iViaMasterSD_fixed_layer_shape_list_sd_iLayerShapeSD"
    local rect_table="${shape_table}__rect_list_sd_IdbRectSD"
    local rect_owner="${shape_table}_primary_key"

    assert_eq "$(sql_value "$edadb_db" "select count(*) from iVia;")" \
        "5" "$name generated and fixed via count"
    assert_eq "$(sql_value "$edadb_db" "select _master_instance__master_generate_sd__original_offset_x_sd || ',' || _master_instance__master_generate_sd__original_offset_y_sd || '|' || _master_instance__master_generate_sd__offset_bottom_x_sd || ',' || _master_instance__master_generate_sd__offset_bottom_y_sd || ',' || _master_instance__master_generate_sd__offset_top_x_sd || ',' || _master_instance__master_generate_sd__offset_top_y_sd from iVia where _name='via_1600x480';")" \
        "10,20|1,2,3,4" "$name generated via origin and offset"
    assert_eq "$(sql_value "$edadb_db" "select _master_instance__type_sd from iVia where _name='fixed_test';")" \
        "2" "$name fixed via branch"
    assert_eq "$(sql_value "$edadb_db" "select _name from iVia order by rowid limit 1;")" \
        "fixed_test" "$name via table physical order was perturbed"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_vec_idx || ':' || _layer_name_sd, ',') from (select _vec_idx, _layer_name_sd from \"$shape_table\" where iVia__name='fixed_test' order by rowid);")" \
        "2:met2,1:via,0:met1" "$name fixed layer-shape physical order was perturbed"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_vec_idx || ':' || _lx_sd || ',' || _ly_sd || ',' || _hx_sd || ',' || _hy_sd, ';') from (select _vec_idx, _lx_sd, _ly_sd, _hx_sd, _hy_sd from \"$rect_table\" where \"$rect_owner\"=(select primary_key from \"$shape_table\" where iVia__name='fixed_test' and _layer_name_sd='met1') order by rowid);")" \
        "1:-80,-160,80,160;0:-100,-200,100,200" "$name fixed rect physical order was perturbed"
    assert_contains "$edadb2def_log" \
        "[EDADB-IDB] readIdbVia fixed_geometry=fixed_test|0:met1:0=-100,-200,100,200;1=-80,-160,80,160|1:via:0=-50,-50,50,50|2:met2:0=-120,-220,120,220" \
        "$name rebuilt fixed layer and rect order"
    assert_contains "$def2edadb_log" "[EDADB-IDB] writeIdbVia insert via_count=5" "$name write via log"
    assert_contains "$edadb2def_log" "[EDADB-IDB] readIdbVia restored via_count=5" "$name read via log"
}

check_instance_branch_sql() {
    local name="$1"
    local edadb_db="$2"
    local def2edadb_log="$3"
    local edadb2def_log="$4"

    assert_eq "$(sql_value "$edadb_db" "select _weight_sd || '|' || _region_name_sd from iInstSD where _name_sd='ctrl/_34_';")" \
        "13|test_region" "$name instance weight region"
    assert_eq "$(sql_value "$edadb_db" "select _halo_sd__is_soft || '|' || _halo_sd__extend_left || ',' || _halo_sd__extend_bottom || ',' || _halo_sd__extend_right || ',' || _halo_sd__extend_top from iInstSD where _name_sd='ctrl/_34_';")" \
        "1|10,20,30,40" "$name instance halo"
    assert_eq "$(sql_value "$edadb_db" "select _route_halo_sd__route_distance_sd || '|' || _route_halo_sd__layer_bottom_name_sd || '|' || _route_halo_sd__layer_top_name_sd from iInstSD where _name_sd='ctrl/_34_';")" \
        "100|met1|met3" "$name instance route halo"
    assert_eq "$(sql_value "$edadb_db" "select _order_sd from iInstSD order by rowid limit 1;")" \
        "1457" "$name instance table physical order was perturbed"
    assert_contains "$def2edadb_log" "[EDADB-IDB] writeIdbInstance insert instance_count=1458" "$name write instance log"
    assert_contains "$edadb2def_log" "[EDADB-IDB] readIdbInstance restored instance_count=1458" "$name read instance log"
}

check_net_branch_sql() {
    local name="$1"
    local edadb_db="$2"
    local def2edadb_log="$3"
    local edadb2def_log="$4"

    check_routed_sql "$name" "$edadb_db" "$def2edadb_log" "$edadb2def_log"
}

check_design_sql() {
    local name="$1"
    local edadb_db="$2"
    local expected_fields="$3"

    assert_eq "$(sql_value "$edadb_db" "select _design_name || '|' || _version || '|' || _units__micron_dbu || '|' || char(_bus_bit_chars__left_delimiter) || '|' || char(_bus_bit_chars__right_delimiter) from iDesign;")" \
        "$expected_fields" "$name canonical design fields"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from pragma_table_info('iDesign') where name in ('_units__nanoseconds','_units__picofarads','_units__ohms','_units__milliwatts','_units__milliamps','_units__volts','_units__megahertz');")" \
        "0" "$name excludes non-DEF IdbUnits fields"
}

check_die_polygon_sql() {
    local name="$1"
    local edadb_db="$2"

    assert_eq "$(sql_value "$edadb_db" "select primary_key from iDieSD;")" \
        "1" "$name singleton die owner key"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_vec_idx) from iDieSD_points_sd_iCoordSD;")" \
        "5,4,3,2,1,0" "$name perturbed unordered die-point fetch order"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_vec_idx || ':' || _x_sd || ',' || _y_sd, ';') from (select _vec_idx, _x_sd, _y_sd from iDieSD_points_sd_iCoordSD order by _vec_idx);")" \
        "0:0,0;1:149960,0;2:149960,75064;3:75000,75064;4:75000,150128;5:0,150128" "$name ordered polygon die points"
    assert_eq "$(sql_value "$edadb_db" "select pk from pragma_table_info('iDieSD_points_sd_iCoordSD') where name='_vec_idx';")" \
        "0" "$name die-point vector index is not primary key"
}

generate_design_fields_fixture() {
    local input="$1"
    local output="$2"
    awk '
        /^VERSION / { print "VERSION 5.7 ;"; next }
        /^BUSBITCHARS / { print "BUSBITCHARS \"{}\" ;"; next }
        /^DESIGN / { print "DESIGN gcd_design ;"; next }
        /^UNITS DISTANCE MICRONS / { print "UNITS DISTANCE MICRONS 2000 ;"; next }
        { print }
    ' "$input" >"$output"
}

generate_design_fallback_fixture() {
    local input="$1"
    local output="$2"
    awk '
        !/^VERSION / && !/^BUSBITCHARS / && !/^UNITS DISTANCE MICRONS / { print }
    ' "$input" >"$output"
}

generate_die_polygon_fixture() {
    local input="$1"
    local output="$2"
    awk '
        /^DIEAREA / {
            print "DIEAREA ( 0 0 ) ( 149960 0 ) ( 149960 75064 ) ( 75000 75064 ) ( 75000 150128 ) ( 0 150128 ) ;"
            next
        }
        { print }
    ' "$input" >"$output"
}

generate_grid_branch_fixture() {
    local input="$1"
    local output="$2"
    awk '
        !replaced && /^TRACKS / {
            sub(/LAYER [^;]*;/, "LAYER met5 met4 ;")
            replaced = 1
        }
        { print }
        END {
            if (!replaced) {
                exit 1
            }
        }
    ' "$input" >"$output"
}

generate_via_branch_fixture() {
    local input="$1"
    local output="$2"
    awk '
        /^VIAS 4 ;$/ {
            print "VIAS 5 ;"
            next
        }
        /^- via_1600x480 / {
            print $0 " + ORIGIN 10 20 + OFFSET 1 2 3 4"
            next
        }
        /^END VIAS$/ {
            print "- fixed_test"
            print "  + RECT met1 ( -100 -200 ) ( 100 200 )"
            print "  + RECT met1 ( -80 -160 ) ( 80 160 )"
            print "  + RECT via ( -50 -50 ) ( 50 50 )"
            print "  + RECT met2 ( -120 -220 ) ( 120 220 )"
            print " ;"
            print
            next
        }
        { print }
    ' "$input" >"$output"
}

generate_instance_branch_fixture() {
    local input="$1"
    local output="$2"
    awk '
        { print }
        /^      \+ REGION test_region$/ {
            print "      + HALO SOFT 10 20 30 40"
            print "      + ROUTEHALO 100 met1 met3"
        }
    ' "$input" >"$output"
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

generate_net_branch_fixture() {
    local input="$1"
    local output="$2"
    awk '
        /^NETS / {
            in_nets = 1
        }
        in_nets && /^END NETS$/ {
            in_nets = 0
        }
        in_nets && /^  \+ ROUTED/ {
            ++wire_index
            if (wire_index == 1) {
                sub(/\+ ROUTED[[:space:]]+/, "+ FIXED ")
            } else if (wire_index == 2) {
                sub(/\+ ROUTED[[:space:]]+/, "+ COVER ")
            } else if (wire_index == 3) {
                sub(/\+ ROUTED[[:space:]]+/, "+ NOSHIELD ")
            } else if (!virtual_done && $0 ~ /\)[[:space:]]+\(/) {
                sub(/\)[[:space:]]+\(/, ") VIRTUAL (")
                virtual_done = 1
            }
        }
        { print }
    ' "$input" >"$output"
}

generate_pin_derived_fixture() {
    local input="$1"
    local output="$2"
    awk '
        /^ - req_msg\[0\] / {
            in_target_pin = 1
            print
            next
        }
        in_target_pin && /^ \+ LAYER met5 / {
            print " + LAYER met5 ( 0 0 ) ( 1000 2000 ) + PLACED ( 1000 18645 ) W"
            in_target_pin = 0
            next
        }
        { print }
    ' "$input" >"$output"
}

generate_pin_writer_fixture() {
    local input="$1"
    local output="$2"
    awk '
        /^- VSS \( PIN clk \) \( ctrl\/_34_ A \)/ {
            print "- VSS ( PIN clk ) ( PIN req_msg[0] ) ( ctrl/_34_ A ) "
            next
        }
        { print }
    ' "$input" >"$output"
}

generate_pin_branches_fixture() {
    local input="$1"
    local output="$2"
    awk '
        /^ - clk / {
            print " - clk + NET clk + SPECIAL + DIRECTION INPUT  + USE SIGNAL"
            print "  + PORT"
            print "   + LAYER met5 ( -1000 -1000 ) ( 1000 1000 )"
            print "   + LAYER met5 ( -500 -500 ) ( 500 500 )"
            print "  + PORT"
            print "   + LAYER met3 ( -300 -300 ) ( 300 300 ) + PLACED ( 1000 9990 ) N"
            print "  + PORT"
            print "   + LAYER met2 ( -200 -200 ) ( 200 200 ) + FIXED ( 2000 9990 ) S"
            print ";"
            skip_pin = 1
            next
        }
        skip_pin {
            if ($0 ~ /^;$/) {
                skip_pin = 0
            }
            next
        }
        /^ - req_msg\[1\] / { pin_status = "FIXED" }
        /^ - req_msg\[2\] / { pin_status = "COVER" }
        /^ - req_msg\[3\] / { pin_status = "NONE" }
        pin_status != "" && /^ \+ LAYER / {
            if (pin_status == "NONE") {
                sub(/ \+ PLACED \( [^)]* \) [A-Z]+/, "")
            } else {
                sub(/\+ PLACED/, "+ " pin_status)
            }
            pin_status = ""
        }
        { print }
    ' "$input" >"$output"
}

perturb_pin_child_query_order() {
    local edadb_db="$1"
    local port_table="iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD"
    local layer_table="${port_table}__layer_shape_list_sd_iLayerShapeSD"
    local rect_table="${layer_table}__rect_list_sd_IdbRectSD"
    local port_parent="iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD_primary_key"
    local layer_parent="${layer_table}_primary_key"

    sqlite3 "$edadb_db" <<SQL
UPDATE "$rect_table"
SET "$port_parent" = -"$port_parent", "$layer_parent" = -"$layer_parent"
WHERE iPinSD__pin_name_sd = 'clk';
UPDATE "$layer_table"
SET "$port_parent" = -"$port_parent", primary_key = -primary_key
WHERE iPinSD__pin_name_sd = 'clk';
UPDATE "$port_table"
SET primary_key = -primary_key
WHERE iPinSD__pin_name_sd = 'clk';
SQL
}

perturb_die_point_query_order() {
    local edadb_db="$1"

    sqlite3 "$edadb_db" <<'SQL'
CREATE TEMP TABLE die_points_reversed AS
SELECT * FROM iDieSD_points_sd_iCoordSD ORDER BY _vec_idx DESC;
DELETE FROM iDieSD_points_sd_iCoordSD;
INSERT INTO iDieSD_points_sd_iCoordSD SELECT * FROM die_points_reversed;
SQL
}

perturb_row_query_order() {
    local edadb_db="$1"

    sqlite3 "$edadb_db" <<'SQL'
CREATE TEMP TABLE rows_reversed AS
SELECT * FROM iRow ORDER BY _order_sd DESC;
DELETE FROM iRow;
INSERT INTO iRow SELECT * FROM rows_reversed;
SQL
}

perturb_grid_query_order() {
    local edadb_db="$1"

    sqlite3 "$edadb_db" <<'SQL'
PRAGMA foreign_keys = OFF;
CREATE TEMP TABLE track_grids_reversed AS
SELECT * FROM iTrackGridSD ORDER BY rowid DESC;
DELETE FROM iTrackGridSD;
INSERT INTO iTrackGridSD SELECT * FROM track_grids_reversed;

CREATE TEMP TABLE track_layers_reversed AS
SELECT * FROM iTrackGridSD__layer_name_vec_sd___edadb_primitive_vector
ORDER BY iTrackGridSD_primary_key, __edadb_vec_idx DESC;
DELETE FROM iTrackGridSD__layer_name_vec_sd___edadb_primitive_vector;
INSERT INTO iTrackGridSD__layer_name_vec_sd___edadb_primitive_vector
SELECT * FROM track_layers_reversed;

CREATE TEMP TABLE gcell_grids_reversed AS
SELECT * FROM iGCellGrid ORDER BY rowid DESC;
DELETE FROM iGCellGrid;
INSERT INTO iGCellGrid SELECT * FROM gcell_grids_reversed;
SQL
}

perturb_instance_query_order() {
    local edadb_db="$1"

    sqlite3 "$edadb_db" <<'SQL'
CREATE TEMP TABLE instances_reversed AS
SELECT * FROM iInstSD ORDER BY _order_sd DESC;
DELETE FROM iInstSD;
INSERT INTO iInstSD SELECT * FROM instances_reversed;
SQL
}

perturb_via_query_order() {
    local edadb_db="$1"

    sqlite3 "$edadb_db" <<'SQL'
PRAGMA foreign_keys = OFF;
CREATE TEMP TABLE via_rects_reversed AS
SELECT * FROM iVia__master_instance_iViaMasterSD_fixed_layer_shape_list_sd_iLayerShapeSD__rect_list_sd_IdbRectSD
ORDER BY iVia__name, iVia__master_instance_iViaMasterSD_fixed_layer_shape_list_sd_iLayerShapeSD_primary_key, _vec_idx DESC;
CREATE TEMP TABLE via_shapes_reversed AS
SELECT * FROM iVia__master_instance_iViaMasterSD_fixed_layer_shape_list_sd_iLayerShapeSD
ORDER BY iVia__name, _vec_idx DESC;
CREATE TEMP TABLE vias_reversed AS
SELECT * FROM iVia ORDER BY rowid DESC;
DELETE FROM iVia__master_instance_iViaMasterSD_fixed_layer_shape_list_sd_iLayerShapeSD__rect_list_sd_IdbRectSD;
DELETE FROM iVia__master_instance_iViaMasterSD_fixed_layer_shape_list_sd_iLayerShapeSD;
DELETE FROM iVia;
INSERT INTO iVia SELECT * FROM vias_reversed;
INSERT INTO iVia__master_instance_iViaMasterSD_fixed_layer_shape_list_sd_iLayerShapeSD
SELECT * FROM via_shapes_reversed;
INSERT INTO iVia__master_instance_iViaMasterSD_fixed_layer_shape_list_sd_iLayerShapeSD__rect_list_sd_IdbRectSD
SELECT * FROM via_rects_reversed;
SQL
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

    if [[ "$check_mode" == "pin_branches" ]]; then
        perturb_pin_child_query_order "$edadb_db"
    fi
    if [[ "$check_mode" == "die_polygon" ]]; then
        perturb_die_point_query_order "$edadb_db"
    fi
    if [[ "$check_mode" == "default" || "$check_mode" == "aux" || "$check_mode" == "pin_derived" || "$check_mode" == "instance_branches" ]]; then
        perturb_row_query_order "$edadb_db"
        perturb_instance_query_order "$edadb_db"
    fi
    if [[ "$check_mode" == "grid_branches" ]]; then
        perturb_grid_query_order "$edadb_db"
    fi
    if [[ "$check_mode" == "via_branches" ]]; then
        perturb_via_query_order "$edadb_db"
    fi

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
        grid_branches)
            if [[ ! -s "$case_dir/direct_vs_edadb.diff" ]]; then
                echo "FAIL: $name expected Level-D root order raw diff" >&2
                exit 1
            fi
            echo "PASS: $name raw DEF differs only in normalized Level-D root order"
            check_grid_branch_sql "$name" "$edadb_db" "$case_dir/def2edadb.log" "$case_dir/edadb2def.log"
            ;;
        via_branches)
            if [[ ! -s "$case_dir/direct_vs_edadb.diff" ]]; then
                echo "FAIL: $name expected Level-D via root order raw diff" >&2
                exit 1
            fi
            echo "PASS: $name raw DEF differs only in normalized Level-D via root order"
            check_via_branch_sql "$name" "$edadb_db" "$case_dir/def2edadb.log" "$case_dir/edadb2def.log"
            ;;
        instance_branches)
            check_instance_branch_sql "$name" "$edadb_db" "$case_dir/def2edadb.log" "$case_dir/edadb2def.log"
            ;;
        net_branches)
            check_net_branch_sql "$name" "$edadb_db" "$case_dir/def2edadb.log" "$case_dir/edadb2def.log"
            ;;
        pin_derived)
            check_default_sql "$name" "$edadb_db" "$case_dir/def2edadb.log" "$case_dir/edadb2def.log"
            assert_eq "$(sql_value "$edadb_db" "select _no_port_orient_sd || '|' || _no_port_placement_status_sd || '|' || _no_port_location_sd__x_sd || ',' || _no_port_location_sd__y_sd from iPinSD where _pin_name_sd='req_msg[0]';")" \
                "2|3|1000,18645" "$name non-R0 no-PORT source fields"
            assert_eq "$(sql_value "$edadb_db" "select _vec_idx || '|' || _lx_sd || '|' || _ly_sd || '|' || _hx_sd || '|' || _hy_sd from iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD__layer_shape_list_sd_iLayerShapeSD__rect_list_sd_IdbRectSD where iPinSD__pin_name_sd='req_msg[0]';")" \
                "0|0|0|1000|2000" "$name asymmetric no-PORT rect"
            ;;
        pin_writer)
            assert_eq "$(sql_value "$edadb_db" "select _io_term_sd__has_port_sd || '|' || _no_port_orient_sd || '|' || _no_port_placement_status_sd from iPinSD where _pin_name_sd='req_msg[0]';")" \
                "1|1|0" "$name stores canonical writer PORT branch"
            assert_eq "$(sql_value "$edadb_db" "select _placement_status_sd || '|' || _coordinate_sd__x_sd || ',' || _coordinate_sd__y_sd || '|' || _orient_sd from iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD where iPinSD__pin_name_sd='req_msg[0]';")" \
                "0|0,0|1" "$name writer PORT fields come from implicit port"
            assert_contains "$direct_def" " - req_msg[0] + NET req_msg[0] + SPECIAL" "$name direct writer special pin"
            assert_eq "$(awk '/^ - req_msg\[0\] / { in_pin = 1; next } in_pin && /^  \+ PORT$/ { print "yes"; exit } in_pin && /^;$/ { exit }' "$direct_def")" \
                "yes" "$name direct writer emits PORT for req_msg[0]"
            ;;
        pin_branches)
            assert_eq "$(sql_value "$edadb_db" "select group_concat(_vec_idx) from iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD where iPinSD__pin_name_sd='clk';")" \
                "2,1,0" "$name perturbed unordered port fetch order"
            assert_eq "$(sql_value "$edadb_db" "select group_concat(_vec_idx || '|' || _placement_status_sd || '|' || _coordinate_sd__x_sd || ',' || _coordinate_sd__y_sd || '|' || _orient_sd, ';') from (select _vec_idx, _placement_status_sd, _coordinate_sd__x_sd, _coordinate_sd__y_sd, _orient_sd from iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD where iPinSD__pin_name_sd='clk' order by _vec_idx);")" \
                "0|0|0,0|1;1|3|1000,9990|1;2|1|2000,9990|3" "$name explicit port order and placement branches"
            assert_eq "$(sql_value "$edadb_db" "select group_concat(port_idx || ':' || layer_idx || ':' || layer_name, ',') from (select p._vec_idx port_idx, l._vec_idx layer_idx, l._layer_name_sd layer_name from iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD p join iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD__layer_shape_list_sd_iLayerShapeSD l on l.iPinSD__pin_name_sd=p.iPinSD__pin_name_sd and l.iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD_primary_key=p.primary_key where p.iPinSD__pin_name_sd='clk' order by p._vec_idx,l._vec_idx);")" \
                "0:0:met5,0:1:met5,1:0:met3,2:0:met2" "$name explicit layer-shape order with duplicate layer names"
            assert_eq "$(sql_value "$edadb_db" "select count(*) || '|' || count(distinct l.primary_key) from iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD p join iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD__layer_shape_list_sd_iLayerShapeSD l on l.iPinSD__pin_name_sd=p.iPinSD__pin_name_sd and l.iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD_primary_key=p.primary_key where p.iPinSD__pin_name_sd='clk' and p._vec_idx=0 and l._layer_name_sd='met5';")" \
                "2|2" "$name duplicate layer names retain distinct layer-shape identities"
            assert_eq "$(sql_value "$edadb_db" "select group_concat(_no_port_placement_status_sd || ':' || _pin_name_sd, ',') from (select _pin_name_sd, _no_port_placement_status_sd from iPinSD where _pin_name_sd in ('req_msg[1]','req_msg[2]','req_msg[3]') order by _order_sd);")" \
                "1:req_msg[1],2:req_msg[2],0:req_msg[3]" "$name no-PORT fixed cover and no-placement branches"
            assert_eq "$(sql_value "$edadb_db" "select count(*) from pragma_table_info('iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD') where name='_vec_idx';")" \
                "1" "$name port vector index column"
            assert_eq "$(sql_value "$edadb_db" "select count(*) from pragma_table_info('iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD__layer_shape_list_sd_iLayerShapeSD') where name in ('primary_key','_vec_idx');")" \
                "2" "$name layer-shape identity and vector index columns"
            local reparsed_def="$case_dir/reparsed.def"
            export INPUT_DEF="$edadb_def"
            export OUTPUT_DEF="$reparsed_def"
            run_ieda "$SCRIPT_DIR/tcl/direct_def_roundtrip.tcl" "$case_dir/reparse.log"
            assert_def_equivalent "$edadb_def" "$reparsed_def" "$case_dir/reparse.diff"
            ;;
        design_fields)
            check_design_sql "$name" "$edadb_db" "gcd_design|5.7|2000|{|}"
            ;;
        design_fallback)
            check_design_sql "$name" "$edadb_db" "gcd|5.8|1000|[|]"
            ;;
        die_polygon)
            check_die_polygon_sql "$name" "$edadb_db"
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
    local net_branch_def="$OUT_DIR/fixtures/net_branches.def"
    local pin_derived_def="$OUT_DIR/fixtures/pin_derived.def"
    local pin_writer_def="$OUT_DIR/fixtures/pin_writer.def"
    local pin_branches_def="$OUT_DIR/fixtures/pin_branches.def"
    local design_fields_def="$OUT_DIR/fixtures/design_fields.def"
    local design_fallback_def="$OUT_DIR/fixtures/design_fallback.def"
    local die_polygon_def="$OUT_DIR/fixtures/die_polygon.def"
    local grid_branch_def="$OUT_DIR/fixtures/grid_branches.def"
    local via_branch_def="$OUT_DIR/fixtures/via_branches.def"
    local instance_branch_def="$OUT_DIR/fixtures/instance_branches.def"
    generate_aux_optional_fixture "$BASE_DEF" "$aux_def"
    generate_net_branch_fixture "$ROUTED_DEF" "$net_branch_def"
    generate_pin_derived_fixture "$BASE_DEF" "$pin_derived_def"
    generate_pin_writer_fixture "$aux_def" "$pin_writer_def"
    generate_pin_branches_fixture "$aux_def" "$pin_branches_def"
    generate_design_fields_fixture "$BASE_DEF" "$design_fields_def"
    generate_design_fallback_fixture "$BASE_DEF" "$design_fallback_def"
    generate_die_polygon_fixture "$BASE_DEF" "$die_polygon_def"
    generate_grid_branch_fixture "$ROUTED_DEF" "$grid_branch_def"
    generate_via_branch_fixture "$BASE_DEF" "$via_branch_def"
    generate_instance_branch_fixture "$aux_def" "$instance_branch_def"

    echo "EDADB regression output dir: $OUT_DIR"
    echo "iEDA binary: $IEDA_BIN"
    echo "base fixture: $BASE_DEF"
    echo "generated fixture: $aux_def"
    echo "generated fixture: $net_branch_def"
    echo "generated fixture: $pin_derived_def"
    echo "generated fixture: $pin_writer_def"
    echo "generated fixture: $pin_branches_def"
    echo "generated fixture: $design_fields_def"
    echo "generated fixture: $design_fallback_def"
    echo "generated fixture: $die_polygon_def"
    echo "generated fixture: $grid_branch_def"
    echo "generated fixture: $via_branch_def"
    echo "generated fixture: $instance_branch_def"

    run_case "default_ipl" "$BASE_DEF" "default"
    run_case "design_fields" "$design_fields_def" "design_fields"
    run_case "design_fallback" "$design_fallback_def" "design_fallback"
    run_case "die_polygon" "$die_polygon_def" "die_polygon"
    run_case "aux_optional" "$aux_def" "aux"
    run_case "pin_derived" "$pin_derived_def" "pin_derived"
    run_case "pin_writer" "$pin_writer_def" "pin_writer"
    run_case "pin_branches" "$pin_branches_def" "pin_branches"
    run_case "grid_branches" "$grid_branch_def" "grid_branches"
    run_case "via_branches" "$via_branch_def" "via_branches"
    run_case "instance_branches" "$instance_branch_def" "instance_branches"
    run_case "routed_irt" "$ROUTED_DEF" "routed"
    run_case "net_branches" "$net_branch_def" "net_branches"

    echo "All EDADB iDB roundtrip regression tests passed."
}

main "$@"
