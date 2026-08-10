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
EDADB_TEST_JOBS="${EDADB_TEST_JOBS:-auto}"
EDADB_TEST_PROCESS_MEMORY_GIB="${EDADB_TEST_PROCESS_MEMORY_GIB:-8}"
EDADB_TEST_MEMORY_RESERVE_GIB="${EDADB_TEST_MEMORY_RESERVE_GIB:-16}"

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

resolve_test_jobs() {
    if [[ "$EDADB_TEST_JOBS" != "auto" ]]; then
        return
    fi

    local logical_cpus cpu_jobs
    logical_cpus="$(nproc)"
    cpu_jobs=$((logical_cpus / 5))
    [[ "$cpu_jobs" -ge 1 ]] || cpu_jobs=1
    [[ "$cpu_jobs" -le 8 ]] || cpu_jobs=8

    local available_kib available_gib memory_jobs
    available_kib="$(awk '/^MemAvailable:/ {print $2}' /proc/meminfo)"
    available_gib=$((available_kib / 1024 / 1024))
    if [[ "$available_gib" -le "$EDADB_TEST_MEMORY_RESERVE_GIB" ]]; then
        memory_jobs=1
    else
        memory_jobs=$(((available_gib - EDADB_TEST_MEMORY_RESERVE_GIB) / EDADB_TEST_PROCESS_MEMORY_GIB))
        [[ "$memory_jobs" -ge 1 ]] || memory_jobs=1
    fi

    if [[ "$memory_jobs" -lt "$cpu_jobs" ]]; then
        EDADB_TEST_JOBS="$memory_jobs"
    else
        EDADB_TEST_JOBS="$cpu_jobs"
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
    assert_eq "$(sql_value "$edadb_db" "select _net_name_sd || '|' || _connect_type_sd || '|' || _source_type_sd || '|' || _weight_sd from iSpecNetSD where _net_name_sd='VSS';")" \
        "VSS|4|0|0" "$name special net default fields"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from pragma_table_info('iSpecNetSD') where name='_order_sd';")" \
        "0" "$name special net has no root order column"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_net_name_sd, ',') from (select _net_name_sd from iSpecNetSD order by _net_name_sd);")" \
        "VDD,VSS" "$name special net names"
    if [[ "$name" == "aux_optional" || "$name" == "group_branches" ]]; then
        assert_eq "$(sql_value "$edadb_db" "select (select count(*) from iSpecNetSD__pin_string_list_sd___edadb_primitive_vector) || '|' || (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD) || '|' || (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD) || '|' || (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD__point_list_sd_iCoordSD);")" \
            "3|3|640|697" "$name special net child counts"
        assert_eq "$(sql_value "$edadb_db" "select (select count(*) from iSpecNetSD__pin_string_list_sd___edadb_primitive_vector) || '|' || (select count(*) from iSpecNetSD__io_pin_name_list_sd___edadb_primitive_vector) || '|' || (select count(*) from iSpecNetSD__instance_pin_list_sd_iSpecPinRef);")" \
            "3|1|1" "$name special net explicit refs"
        assert_eq "$(sql_value "$edadb_db" "select (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD where _is_via_sd=1) || '|' || (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD where _is_rect_sd=1) || '|' || (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD where _is_via_sd=0 and _is_rect_sd=0);")" \
            "581|1|58" "$name special net segment dispatch types"
    else
        assert_eq "$(sql_value "$edadb_db" "select (select count(*) from iSpecNetSD__pin_string_list_sd___edadb_primitive_vector) || '|' || (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD) || '|' || (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD) || '|' || (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD__point_list_sd_iCoordSD);")" \
            "6|2|639|697" "$name special net child counts"
        assert_eq "$(sql_value "$edadb_db" "select (select count(*) from iSpecNetSD__pin_string_list_sd___edadb_primitive_vector) || '|' || (select count(*) from iSpecNetSD__io_pin_name_list_sd___edadb_primitive_vector) || '|' || (select count(*) from iSpecNetSD__instance_pin_list_sd_iSpecPinRef);")" \
            "6|0|0" "$name special net pin-string exclusive refs"
        assert_eq "$(sql_value "$edadb_db" "select (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD where _is_via_sd=1) || '|' || (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD where _is_rect_sd=1) || '|' || (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD where _is_via_sd=0 and _is_rect_sd=0);")" \
            "581|0|58" "$name special net segment dispatch types"
    fi
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
    local edadb2def_log="$3"

    assert_eq "$(sql_value "$edadb_db" "select count(*) from iBlockageSD;")" "7" "$name blockage count"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iRegion;")" "1" "$name region count"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iSlotSD;")" "1" "$name slot count"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iGroupSD;")" "1" "$name group count"
    assert_eq "$(sql_value "$edadb_db" "select count(*) || '|' || count(distinct primary_key) from iFillSD;")" "4|4" "$name fill count and root identity"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_layer_name_sd || '|' || _is_slots_sd || '|' || _is_fills_sd || '|' || _is_pushdown_sd || '|' || _is_except_pgnet_sd || '|' || _instance_name_sd || '|' || _min_spacing_sd || '|' || _effective_width_sd, ';') from (select * from iBlockageSD where _type_sd=1 order by _layer_name_sd);")" \
        "met1|1|0|0|0||111|-1;met2|0|1|0|0||-1|222;met3|0|0|1|1|ctrl/_34_|-1|-1" "$name routing blockage parser fields"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_lx_sd || '|' || _is_soft_sd || '|' || printf('%.3f', _max_density_sd) || '|' || _is_pushdown_sd || '|' || _instance_name_sd, ';') from (select r._lx_sd, b._is_soft_sd, b._max_density_sd, b._is_pushdown_sd, b._instance_name_sd from iBlockageSD b join iBlockageSD__rect_list_sd_IdbRectSD r on r.iBlockageSD_primary_key=b.primary_key and r._vec_idx=0 where b._type_sd=2 order by r._lx_sd);")" \
        "5000|1|0.000|0|;6000|0|0.500|0|;7000|0|0.000|0|ctrl/_35_;9000|0|0.000|0|" "$name placement blockage parser fields"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from pragma_table_info('iBlockageSD') where name='_is_partial_sd';")" \
        "0" "$name excludes unset placement partial flag"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from pragma_table_info('iBlockageSD') where name='_mask_sd';")" \
        "0" "$name excludes unsupported blockage mask"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_vec_idx || ':' || _lx_sd || ',' || _ly_sd || ',' || _hx_sd || ',' || _hy_sd, ';') from (select r._vec_idx, r._lx_sd, r._ly_sd, r._hx_sd, r._hy_sd from iBlockageSD b join iBlockageSD__rect_list_sd_IdbRectSD r on r.iBlockageSD_primary_key=b.primary_key where b._layer_name_sd='met1' order by r._vec_idx);")" \
        "0:1000,1000,2000,2000;1:1200,1200,1800,1800" "$name blockage ordered rects"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(r._vec_idx, ',') from iBlockageSD b join iBlockageSD__rect_list_sd_IdbRectSD r on r.iBlockageSD_primary_key=b.primary_key where b._layer_name_sd='met1' order by r.rowid;")" \
        "1,0" "$name blockage rect physical order was perturbed"
    assert_contains "$edadb2def_log" "[EDADB-IDB] readIdbBlockage routing layer=met1 slots=1 fills=0 pushdown=0 except_pgnet=0 instance= min_spacing=111 effective_width=-1" "$name read routing slots spacing state"
    assert_contains "$edadb2def_log" "[EDADB-IDB] readIdbBlockage routing layer=met2 slots=0 fills=1 pushdown=0 except_pgnet=0 instance= min_spacing=-1 effective_width=222" "$name read routing fills width state"
    assert_contains "$edadb2def_log" "[EDADB-IDB] readIdbBlockage placement soft=1 partial=0 max_density=0 pushdown=0 instance=" "$name read placement soft state"
    assert_contains "$edadb2def_log" "[EDADB-IDB] readIdbBlockage placement soft=0 partial=0 max_density=0.5 pushdown=0 instance=" "$name read placement partial density state"
    assert_eq "$(sql_value "$edadb_db" "select _name || '|' || _type from iRegion;")" "test_region|1" "$name region fields"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_vec_idx || '|' || _lx_sd || '|' || _ly_sd || '|' || _hx_sd || '|' || _hy_sd, ';') from (select * from iRegion__boudary_list_IdbRectSD order by _vec_idx);")" \
        "0|1000|1000|10000|10000;1|12000|12000|14000|15000" "$name region ordered rects"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_vec_idx, ',') from iRegion__boudary_list_IdbRectSD order by rowid;")" \
        "1,0" "$name region rect physical order was perturbed"
    assert_eq "$(sql_value "$edadb_db" "select _order_sd || '|' || _layer_name_sd from iSlotSD;")" "0|met1" "$name slot layer"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_vec_idx || '|' || _lx_sd || '|' || _ly_sd || '|' || _hx_sd || '|' || _hy_sd, ';') from (select * from iSlotSD__rect_list_sd_IdbRectSD order by _vec_idx);")" \
        "0|5000|5000|6000|6000;1|5100|5200|5800|5900" "$name ordered slot rects"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_vec_idx, ',') from iSlotSD__rect_list_sd_IdbRectSD order by rowid;")" \
        "1,0" "$name slot rect physical order was perturbed"
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
    assert_eq "$(sql_value "$edadb_db" "select group_concat(__edadb_vec_idx || ':' || value, ',') from iGroupSD__instance_name_vec_sd___edadb_primitive_vector order by rowid;")" \
        "1:ctrl/_35_,0:ctrl/_34_" "$name group member physical order was perturbed"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from pragma_table_info('iFillSD') where name='_order_sd';")" \
        "0" "$name fill has no root order column"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_type_sd || '|' || coalesce(_layer_name_sd,'') || '|' || coalesce(_via_name_sd,''), ';') from (select * from iFillSD order by _type_sd, coalesce(_layer_name_sd,''), coalesce(_via_name_sd,''));")" \
        "1|met1|;1|met1|;2||via_1600x480;2||via_1600x480" "$name repeated fill typed rows"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(primary_key, ',') from (select primary_key from iFillSD order by rowid);")" \
        "4,3,2,1" "$name fill root physical order was perturbed"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iFillSD where (_type_sd=1 and coalesce(_via_name_sd,'')<>'') or (_type_sd=2 and coalesce(_layer_name_sd,'')<>'');")" \
        "0" "$name fill inactive reference fields are empty"
    assert_eq "$(sql_value "$edadb_db" "select (select count(*) from iFillSD__coordinate_list_sd_iCoordSD c join iFillSD f on f.primary_key=c.iFillSD_primary_key where f._type_sd=1) || '|' || (select count(*) from iFillSD__rect_list_sd_IdbRectSD r join iFillSD f on f.primary_key=r.iFillSD_primary_key where f._type_sd=2);")" \
        "0|0" "$name fill child rows follow type branch"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(cnt, ',') from (select count(*) cnt from iFillSD__rect_list_sd_IdbRectSD group by iFillSD_primary_key order by cnt);")" \
        "1,2" "$name repeated layer fills keep separate rect owners"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_vec_idx || ':' || _lx_sd || ',' || _ly_sd || ',' || _hx_sd || ',' || _hy_sd, ';') from (select * from iFillSD__rect_list_sd_IdbRectSD where iFillSD_primary_key=(select iFillSD_primary_key from iFillSD__rect_list_sd_IdbRectSD group by iFillSD_primary_key having count(*)=2) order by _vec_idx);")" \
        "0:7000,7000,8000,8000;1:7100,7200,7800,7900" "$name ordered fill rects"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_vec_idx, ',') from iFillSD__rect_list_sd_IdbRectSD where iFillSD_primary_key=(select iFillSD_primary_key from iFillSD__rect_list_sd_IdbRectSD group by iFillSD_primary_key having count(*)=2) order by rowid;")" \
        "1,0" "$name fill rect physical order was perturbed"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(cnt, ',') from (select count(*) cnt from iFillSD__coordinate_list_sd_iCoordSD group by iFillSD_primary_key order by cnt);")" \
        "1,2" "$name repeated via fills keep separate coordinate owners"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_vec_idx || ':' || _x_sd || ',' || _y_sd, ';') from (select * from iFillSD__coordinate_list_sd_iCoordSD where iFillSD_primary_key=(select iFillSD_primary_key from iFillSD__coordinate_list_sd_iCoordSD group by iFillSD_primary_key having count(*)=2) order by _vec_idx);")" \
        "0:9000,9000;1:9200,9300" "$name ordered fill via coordinates"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_vec_idx, ',') from iFillSD__coordinate_list_sd_iCoordSD where iFillSD_primary_key=(select iFillSD_primary_key from iFillSD__coordinate_list_sd_iCoordSD group by iFillSD_primary_key having count(*)=2) order by rowid;")" \
        "1,0" "$name fill coordinate physical order was perturbed"
    assert_eq "$(sql_value "$edadb_db" "select _original_net_name_sd || '|' || _source_type_sd || '|' || _weight_sd from iSpecNetSD where _net_name_sd='VDD';")" \
        "orig_vdd_net|1|5" "$name special net optional fields"
    assert_eq "$(sql_value "$edadb_db" "select (select count(*) from iSpecNetSD__pin_string_list_sd___edadb_primitive_vector) || '|' || (select count(*) from iSpecNetSD__io_pin_name_list_sd___edadb_primitive_vector) || '|' || (select count(*) from iSpecNetSD__instance_pin_list_sd_iSpecPinRef);")" \
        "3|1|1" "$name special net explicit pin refs"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(value, ',') from iSpecNetSD__io_pin_name_list_sd___edadb_primitive_vector where iSpecNetSD__net_name_sd='VSS';")" \
        "clk" "$name special net io pin ref"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_order_sd || ':' || instance_name || ':' || pin_name, ',') from (select _order_sd, instance_name, pin_name from iSpecNetSD__instance_pin_list_sd_iSpecPinRef where iSpecNetSD__net_name_sd='VSS' order by _order_sd);")" \
        "0:ctrl/_34_:A" "$name special net instance pin ref"
    assert_eq "$(sql_value "$edadb_db" "select sum(pk) from pragma_table_info('iSpecNetSD__instance_pin_list_sd_iSpecPinRef');")" \
        "0" "$name special-net pin-ref order is not a primary key"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from pragma_table_info('iSpecNetSD__wire_list_sd_iSpecWireSD') where name='_vec_idx';")" \
        "1" "$name special-wire vector index column"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from pragma_table_info('iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD') where name='_vec_idx';")" \
        "1" "$name special-segment vector index column"
    assert_eq "$(sql_value "$edadb_db" "select count(*) > 0 from iSpecNetSD__wire_list_sd_iSpecWireSD a join iSpecNetSD__wire_list_sd_iSpecWireSD b on a.iSpecNetSD__net_name_sd=b.iSpecNetSD__net_name_sd and a.rowid < b.rowid and a._vec_idx > b._vec_idx;")" \
        "1" "$name special-wire physical order was perturbed"
    assert_eq "$(sql_value "$edadb_db" "select count(*) > 0 from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD a join iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD b on a.iSpecNetSD__net_name_sd=b.iSpecNetSD__net_name_sd and a.iSpecNetSD__wire_list_sd_iSpecWireSD_primary_key=b.iSpecNetSD__wire_list_sd_iSpecWireSD_primary_key and a.rowid < b.rowid and a._vec_idx > b._vec_idx;")" \
        "1" "$name special-segment physical order was perturbed"
    assert_eq "$(sql_value "$edadb_db" "select (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD where _is_via_sd=1) || '|' || (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD where _is_rect_sd=1) || '|' || (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD where _is_via_sd=0 and _is_rect_sd=0);")" \
        "581|1|58" "$name special net segment dispatch with rect"
    assert_eq "$(sql_value "$edadb_db" "select _layer_name_sd || '|' || _delta_rect_sd__lx_sd || ',' || _delta_rect_sd__ly_sd || ',' || _delta_rect_sd__hx_sd || ',' || _delta_rect_sd__hy_sd from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD where iSpecNetSD__net_name_sd='VSS' and _is_rect_sd=1;")" \
        "met1|11000,11000,13000,14000" "$name special net rect segment"
    assert_eq "$(sql_value "$edadb_db" "select _original_net_name_sd || '|' || _source_type_sd || '|' || _weight_sd || '|' || _xtalk_sd || '|' || _fix_bump_sd || '|' || _frequency_sd from iNetSD where _net_name_sd='ctrl\$a_mux_sel[0]';")" \
        "orig_ctrl_net|3|7|11|1|250.0" "$name regular net optional fields"
}

check_special_net_branch_sql() {
    local name="$1"
    local edadb_db="$2"
    local input_def="$3"
    local direct_def="$4"
    local edadb2def_log="$5"

    assert_eq "$(sql_value "$edadb_db" "select count(*) from pragma_table_info('iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD') where name='_style_sd';")" \
        "1" "$name special segment style column"
    assert_eq "$(sql_value "$edadb_db" "select count(*) || '|' || min(_style_sd) || '|' || max(_style_sd) from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD where _style_sd >= 0;")" \
        "1|7|7" "$name parser-only STYLE state"
    assert_eq "$(sql_value "$edadb_db" "select count(*) || '|' || group_concat(_shield_name_sd, ',') from iSpecNetSD__wire_list_sd_iSpecWireSD where _wire_state_sd=5;")" \
        "1|VDD" "$name parser-only SHIELD state"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD s where iSpecNetSD__net_name_sd='VSS' and _is_via_sd=1 and _via_name_sd='via_1600x480' and (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD__point_list_sd_iCoordSD p where p.iSpecNetSD__net_name_sd=s.iSpecNetSD__net_name_sd and p.iSpecNetSD__wire_list_sd_iSpecWireSD_primary_key=s.iSpecNetSD__wire_list_sd_iSpecWireSD_primary_key and p.iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD_primary_key=s.primary_key)=2;")" \
        "1" "$name two-point via branch"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_vec_idx || ':' || _x_sd || ',' || _y_sd, ';') from (select p._vec_idx, p._x_sd, p._y_sd from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD s join iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD__point_list_sd_iCoordSD p on p.iSpecNetSD__net_name_sd=s.iSpecNetSD__net_name_sd and p.iSpecNetSD__wire_list_sd_iSpecWireSD_primary_key=s.iSpecNetSD__wire_list_sd_iSpecWireSD_primary_key and p.iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD_primary_key=s.primary_key where s.iSpecNetSD__net_name_sd='VSS' and s._is_via_sd=0 and s._is_rect_sd=0 and s._layer_name_sd='met1' and s._route_width_sd=480 and (select count(*) from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD__point_list_sd_iCoordSD points where points.iSpecNetSD__net_name_sd=s.iSpecNetSD__net_name_sd and points.iSpecNetSD__wire_list_sd_iSpecWireSD_primary_key=s.iSpecNetSD__wire_list_sd_iSpecWireSD_primary_key and points.iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD_primary_key=s.primary_key)=3 order by p._vec_idx);")" \
        "0:15000,15000;1:25000,15000;2:30000,15000" "$name ordered three-point branch"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(__edadb_vec_idx || ':' || value, ',') from (select __edadb_vec_idx, value from iSpecNetSD__pin_string_list_sd___edadb_primitive_vector where iSpecNetSD__net_name_sd='VDD' order by __edadb_vec_idx);")" \
        "0:VPWR,1:VPB,2:vdd" "$name ordered pin-string connections"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(__edadb_vec_idx || ':' || value, ',') from (select __edadb_vec_idx, value from iSpecNetSD__io_pin_name_list_sd___edadb_primitive_vector where iSpecNetSD__net_name_sd='VSS' order by __edadb_vec_idx);")" \
        "0:clk,1:req_msg[0]" "$name ordered IO-pin connections"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_order_sd || ':' || instance_name || ':' || pin_name, ',') from (select _order_sd, instance_name, pin_name from iSpecNetSD__instance_pin_list_sd_iSpecPinRef where iSpecNetSD__net_name_sd='VSS' order by _order_sd);")" \
        "0:ctrl/_34_:A,1:ctrl/_35_:A" "$name ordered instance-pin connections"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_net_name_sd, ',') from (select _net_name_sd from iSpecNetSD order by rowid);")" \
        "VSS,VDD" "$name special-net root physical order was perturbed"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(value, ',') from (select value from iSpecNetSD__pin_string_list_sd___edadb_primitive_vector where iSpecNetSD__net_name_sd='VDD' order by rowid);")" \
        "vdd,VPB,VPWR" "$name pin-string physical order was perturbed"
    assert_eq "$(sql_value "$edadb_db" "select count(*) > 0 from iSpecNetSD__wire_list_sd_iSpecWireSD a join iSpecNetSD__wire_list_sd_iSpecWireSD b on a.iSpecNetSD__net_name_sd=b.iSpecNetSD__net_name_sd and a.rowid < b.rowid and a._vec_idx > b._vec_idx;")" \
        "1" "$name special-wire physical order was perturbed"
    assert_eq "$(sql_value "$edadb_db" "select count(*) > 0 from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD a join iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD b on a.iSpecNetSD__net_name_sd=b.iSpecNetSD__net_name_sd and a.iSpecNetSD__wire_list_sd_iSpecWireSD_primary_key=b.iSpecNetSD__wire_list_sd_iSpecWireSD_primary_key and a.rowid < b.rowid and a._vec_idx > b._vec_idx;")" \
        "1" "$name special-segment physical order was perturbed"
    assert_eq "$(sql_value "$edadb_db" "select count(*) > 0 from iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD__point_list_sd_iCoordSD a join iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD__point_list_sd_iCoordSD b on a.iSpecNetSD__net_name_sd=b.iSpecNetSD__net_name_sd and a.iSpecNetSD__wire_list_sd_iSpecWireSD_primary_key=b.iSpecNetSD__wire_list_sd_iSpecWireSD_primary_key and a.iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD_primary_key=b.iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD_primary_key and a.rowid < b.rowid and a._vec_idx > b._vec_idx;")" \
        "1" "$name special-point physical order was perturbed"
    assert_contains "$input_def" "+ STYLE 7" "$name input STYLE token"
    assert_not_contains "$direct_def" "+ STYLE 7" "$name native writer omits STYLE"
    assert_contains "$input_def" "+ SHIELD VDD" "$name input SHIELD wire"
    assert_not_contains "$direct_def" "+ SHIELD VDD" "$name native writer omits SHIELD wire"
    assert_contains "$input_def" "( 15000 15000 ) ( 25000 * ) ( 30000 * )" "$name input three-point path"
    assert_not_contains "$direct_def" "( 15000 15000 ) ( 25000 * ) ( 30000 * )" "$name native writer truncates after second point"
    assert_contains "$edadb2def_log" "styled_segment_count=1 shield_wire_count=1" "$name restored parser-only segment state"
}

check_routed_sql() {
    local name="$1"
    local edadb_db="$2"
    local def2edadb_log="$3"
    local edadb2def_log="$4"
    local expected_virtual_count="${5:-0}"
    local expected_net_count="${6:-677}"
    local expected_point_count="${7:-14256}"
    local expected_via_count="${8:-3716}"
    local expected_order_prefix="${9:-0:ctrl\$a_mux_sel[0],1:ctrl\$a_mux_sel[1],2:ctrl\$a_reg_en,3:ctrl\$b_mux_sel,4:ctrl\$b_reg_en}"

    assert_eq "$(sql_value "$edadb_db" "select count(*) from iGCellGrid;")" "6" "$name gcell grid count"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_direction || ':' || _start || ':' || _num || ':' || _space, ';') from (select * from iGCellGrid order by _direction, _start, _num, _space);")" \
        "1:0:2:3600;1:3600:43:3360;1:144720:2:5240;2:0:2:3600;2:3600:43:3360;2:144720:2:5408" "$name gcell grid fields"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iNetSD;")" "$expected_net_count" "$name net count"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_order_sd || ':' || _net_name_sd, ',') from (select _order_sd, _net_name_sd from iNetSD order by _order_sd limit 5);")" \
        "$expected_order_prefix" "$name routed net order prefix"
    assert_eq "$(sql_value "$edadb_db" "select _order_sd from iNetSD order by rowid limit 1;")" \
        "$((expected_net_count - 1))" "$name net root physical order was perturbed"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iNetSD__wire_list_sd_iRegWireSD;")" "677" "$name regular wire count"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD;")" "8997" "$name regular wire segment count"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__point_list_sd_iCoordSD;")" \
        "$expected_point_count" "$name regular wire point count"
    assert_eq "$(sql_value "$edadb_db" "select (select count(*) from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD where _is_via_sd=1) || '|' || (select count(*) from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD where _is_rect_sd=1) || '|' || (select count(*) from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__virtual_point_index_list_sd___edadb_primitive_vector) || '|' || (select count(*) from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__via_ref_list_sd_iRegViaRef);")" \
        "3716|22|${expected_virtual_count}|${expected_via_count}" "$name regular wire segment types"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from pragma_table_info('iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD') where name in ('_via_name_sd','_is_second_point_virtual_sd');")" \
        "0" "$name old reduced segment columns removed"
    assert_eq "$(sql_value "$edadb_db" "select sum(pk) from pragma_table_info('iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__via_ref_list_sd_iRegViaRef');")" \
        "0" "$name via-ref order is not a primary key"
    assert_eq "$(sql_value "$edadb_db" "select min(_order_sd) || '|' || max(_order_sd) || '|' || count(*) from iNetSD__instance_pin_list_sd_iNetPinRef where iNetSD__net_name_sd='clk_0';")" \
        "0|18|19" "$name clk_0 ordered pin refs"
    assert_eq "$(sql_value "$edadb_db" "select sum(pk) from pragma_table_info('iNetSD__instance_pin_list_sd_iNetPinRef');")" \
        "0" "$name net pin-ref order is not a primary key"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_order_sd || ':' || instance_name || ':' || pin_name, ',') from (select _order_sd, instance_name, pin_name from iNetSD__instance_pin_list_sd_iNetPinRef where iNetSD__net_name_sd='clk_0' order by _order_sd limit 5);")" \
        "0:clk_0_buf:X,1:dpath/b_reg/_140_:CLK,2:dpath/b_reg/_139_:CLK,3:dpath/b_reg/_138_:CLK,4:dpath/b_reg/_137_:CLK" "$name clk_0 pin order prefix"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(iNetSD__net_name_sd || ':' || cnt, ',') from (select iNetSD__net_name_sd, count(*) as cnt from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD group by iNetSD__net_name_sd order by cnt desc, iNetSD__net_name_sd limit 3);")" \
        "clk_0:138,clk_1:137,dpath/a_mux/_066_:103" "$name largest routed segment nets"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from pragma_table_info('iNetSD__wire_list_sd_iRegWireSD') where name='_vec_idx';")" \
        "1" "$name regular-wire vector index column"
    assert_eq "$(sql_value "$edadb_db" "select count(*) from pragma_table_info('iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD') where name='_vec_idx';")" \
        "1" "$name regular-segment vector index column"
    assert_eq "$(sql_value "$edadb_db" "select count(*) > 0 from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD a join iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD b on a.iNetSD__net_name_sd=b.iNetSD__net_name_sd and a.iNetSD__wire_list_sd_iRegWireSD_primary_key=b.iNetSD__wire_list_sd_iRegWireSD_primary_key and a.rowid < b.rowid and a._vec_idx > b._vec_idx;")" \
        "1" "$name regular-segment physical order was perturbed"
    assert_eq "$(sql_value "$edadb_db" "select count(*) > 0 from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__point_list_sd_iCoordSD a join iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__point_list_sd_iCoordSD b on a.iNetSD__net_name_sd=b.iNetSD__net_name_sd and a.iNetSD__wire_list_sd_iRegWireSD_primary_key=b.iNetSD__wire_list_sd_iRegWireSD_primary_key and a.iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD_primary_key=b.iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD_primary_key and a.rowid < b.rowid and a._vec_idx > b._vec_idx;")" \
        "1" "$name regular-point physical order was perturbed"

    assert_contains "$def2edadb_log" "[EDADB-IDB] writeIdbNet insert net_count=${expected_net_count}" "$name write routed net log"
    assert_contains "$edadb2def_log" "[EDADB-IDB] readIdbNet restored net_count=${expected_net_count}" "$name read routed net log"
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
    local input_def="$5"
    local direct_def="$6"

    check_routed_sql "$name" "$edadb_db" "$def2edadb_log" "$edadb2def_log" \
        "2" "678" "14257" "3717" \
        "0:special_signal,1:ctrl\$a_mux_sel[0],2:ctrl\$a_mux_sel[1],3:ctrl\$a_reg_en,4:ctrl\$b_mux_sel"
    assert_eq "$(sql_value "$edadb_db" "select _wire_state_sd from iNetSD__wire_list_sd_iRegWireSD where iNetSD__net_name_sd='ctrl\$a_mux_sel[0]' order by primary_key limit 1;")" \
        "2" "$name fixed wire state"
    assert_eq "$(sql_value "$edadb_db" "select _wire_state_sd from iNetSD__wire_list_sd_iRegWireSD where iNetSD__net_name_sd='ctrl\$a_mux_sel[1]' order by primary_key limit 1;")" \
        "1" "$name cover wire state"
    assert_eq "$(sql_value "$edadb_db" "select _wire_state_sd from iNetSD__wire_list_sd_iRegWireSD where iNetSD__net_name_sd='ctrl\$a_reg_en' order by primary_key limit 1;")" \
        "4" "$name no-shield wire state"
    assert_eq "$(sql_value "$edadb_db" "select _connect_type_sd || '|' || _order_sd from iNetSD where _net_name_sd='special_signal';")" \
        "1|0" "$name SPECIALNETS SIGNAL dispatched to IdbNet"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_order_sd || ':' || _via_name_sd || '@' || _point_index_sd, ',') from (select v._order_sd, v._via_name_sd, v._point_index_sd from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD s join iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__via_ref_list_sd_iRegViaRef v on v.iNetSD__net_name_sd=s.iNetSD__net_name_sd and v.iNetSD__wire_list_sd_iRegWireSD_primary_key=s.iNetSD__wire_list_sd_iRegWireSD_primary_key and v.iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD_primary_key=s.primary_key where s.iNetSD__net_name_sd='ctrl\$a_mux_sel[0]' and s._vec_idx=1 order by v._order_sd);")" \
        "0:L1M1_PR@0,1:M1M2_PR@0" "$name ordered multi-via references"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(v._order_sd, ',') from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD s join iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__via_ref_list_sd_iRegViaRef v on v.iNetSD__net_name_sd=s.iNetSD__net_name_sd and v.iNetSD__wire_list_sd_iRegWireSD_primary_key=s.iNetSD__wire_list_sd_iRegWireSD_primary_key and v.iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD_primary_key=s.primary_key where s.iNetSD__net_name_sd='ctrl\$a_mux_sel[0]' and s._vec_idx=1 order by v.rowid;")" \
        "1,0" "$name multi-via physical order was perturbed"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(value, ',') from (select value from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__virtual_point_index_list_sd___edadb_primitive_vector where iNetSD__net_name_sd='ctrl\$a_mux_sel[0]' order by value);")" \
        "1,2" "$name all virtual-point indices"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(value, ',') from (select value from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__virtual_point_index_list_sd___edadb_primitive_vector where iNetSD__net_name_sd='ctrl\$a_mux_sel[0]' order by rowid);")" \
        "2,1" "$name virtual-point physical order was perturbed"
    assert_eq "$(sql_value "$edadb_db" "select group_concat(_vec_idx || ':' || _x_sd || ',' || _y_sd, ';') from (select p._vec_idx, p._x_sd, p._y_sd from iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD s join iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__point_list_sd_iCoordSD p on p.iNetSD__net_name_sd=s.iNetSD__net_name_sd and p.iNetSD__wire_list_sd_iRegWireSD_primary_key=s.iNetSD__wire_list_sd_iRegWireSD_primary_key and p.iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD_primary_key=s.primary_key where s.iNetSD__net_name_sd='ctrl\$a_mux_sel[0]' and s._vec_idx=0 order by p._vec_idx);")" \
        "0:67320,85535;1:68160,85535;2:69000,85535" "$name complete three-point storage view"
    assert_contains "$input_def" "VIRTUAL ( 68160 85535 ) VIRTUAL ( 69000 85535 )" "$name multi-VIRTUAL input"
    assert_not_contains "$direct_def" "VIRTUAL ( 69000 85535 )" "$name native writer omits third point"
    assert_contains "$input_def" "L1M1_PR M1M2_PR" "$name multi-Via input"
    assert_not_contains "$direct_def" "L1M1_PR M1M2_PR" "$name native writer emits first Via only"
    assert_contains "$edadb2def_log" "via_count=3717 virtual_point_count=2 multi_via_segment_count=1" \
        "$name restored parser-only regular-wire state"
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
                print "    - test_region ( 1000 1000 ) ( 10000 10000 ) ( 12000 12000 ) ( 14000 15000 ) + TYPE FENCE ;"
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
            if (in_vss_special_net && $0 ~ /^[[:space:]]*;[[:space:]]*$/) {
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
                print "BLOCKAGES 7 ;"
                print "    - LAYER met1 + SLOTS + SPACING 111 RECT ( 1000 1000 ) ( 2000 2000 ) RECT ( 1200 1200 ) ( 1800 1800 ) ;"
                print "    - LAYER met2 + FILLS + DESIGNRULEWIDTH 222 RECT ( 3000 3000 ) ( 4000 4000 ) ;"
                print "    - LAYER met3 + PUSHDOWN + EXCEPTPGNET + COMPONENT ctrl/_34_ RECT ( 4000 4000 ) ( 4500 4500 ) ;"
                print "    - PLACEMENT + SOFT RECT ( 5000 5000 ) ( 5500 5500 ) ;"
                print "    - PLACEMENT + PARTIAL 0.5 RECT ( 6000 6000 ) ( 6500 6500 ) ;"
                print "    - PLACEMENT + COMPONENT ctrl/_35_ RECT ( 7000 7000 ) ( 8000 8000 ) RECT ( 7200 7200 ) ( 7800 7800 ) ;"
                print "    - PLACEMENT + PUSHDOWN RECT ( 9000 9000 ) ( 9500 9500 ) ;"
                print "END BLOCKAGES"
                print "SLOTS 1 ;"
                print "    - LAYER met1 RECT ( 5000 5000 ) ( 6000 6000 ) RECT ( 5100 5200 ) ( 5800 5900 ) ;"
                print "END SLOTS"
                print ""
                print "GROUPS 1 ;"
                print "    - test_group ctrl/_34_ ctrl/_35_ + REGION test_region ;"
                print "END GROUPS"
                print ""
                print "FILLS 4 ;"
                print "    - LAYER met1 RECT ( 7000 7000 ) ( 8000 8000 ) RECT ( 7100 7200 ) ( 7800 7900 ) ;"
                print "    - LAYER met1 RECT ( 8100 8200 ) ( 8800 8900 ) ;"
                print "    - VIA via_1600x480 ( 9000 9000 ) ( 9200 9300 ) ;"
                print "    - VIA via_1600x480 ( 9400 9500 ) ;"
                print "END FILLS"
            }
        }
    ' "$input" >"$output"
}

generate_special_net_branch_fixture() {
    local input="$1"
    local output="$2"

    awk '
        {
            if (!style_added && $0 ~ /^[[:space:]]+\+ ROUTED met1 480 \+ SHAPE FOLLOWPIN/) {
                sub(/\+ SHAPE FOLLOWPIN/, "+ SHAPE FOLLOWPIN + STYLE 7")
                style_added = 1
            }
            if ($0 ~ /^- VSS \( PIN clk \) \( ctrl\/_34_ A \)/) {
                print "- VSS ( PIN clk ) ( PIN req_msg[0] ) ( ctrl/_34_ A ) ( ctrl/_35_ A ) "
                in_vss_special_net = 1
                next
            }
            if (in_vss_special_net && $0 ~ /^[[:space:]]*;[[:space:]]*$/) {
                print "  + ROUTED met2 0 + SHAPE STRIPE ( 10000 10000 ) ( * 12000 ) via_1600x480"
                print "  + ROUTED met1 480 + SHAPE STRIPE ( 15000 15000 ) ( 25000 * ) ( 30000 * )"
                print "  + SHIELD VDD met1 480 + SHAPE STRIPE ( 20000 20000 ) ( 30000 * )"
                in_vss_special_net = 0
            }
            print
        }
    ' "$input" >"$output"
}

generate_net_branch_fixture() {
    local input="$1"
    local output="$2"
    awk '
        /^SPECIALNETS [0-9]+ ;$/ {
            print "SPECIALNETS " ($2 + 1) " ;"
            in_special_nets = 1
            next
        }
        in_special_nets && /^END SPECIALNETS$/ {
            print "- special_signal ( PIN clk )"
            print "  + USE SIGNAL"
            print " ;"
            in_special_nets = 0
        }
        /^NETS / {
            in_nets = 1
        }
        in_nets && /^END NETS$/ {
            in_nets = 0
        }
        in_nets && /^  \+ ROUTED/ {
            ++wire_index
            if (wire_index == 1) {
                print "  + FIXED  met1 ( 67320 85535 ) VIRTUAL ( 68160 85535 ) VIRTUAL ( 69000 85535 )"
                virtual_done = 1
                next
            } else if (wire_index == 2) {
                sub(/\+ ROUTED[[:space:]]+/, "+ COVER ")
            } else if (wire_index == 3) {
                sub(/\+ ROUTED[[:space:]]+/, "+ NOSHIELD ")
            } else if (!virtual_done && $0 ~ /\)[[:space:]]+\(/) {
                sub(/\)[[:space:]]+\(/, ") VIRTUAL (")
                virtual_done = 1
            }
        }
        in_nets && !multi_via_done && /^    NEW  met1 \( 67320 85535 \) L1M1_PR$/ {
            print "    NEW  met1 ( 67320 85535 ) L1M1_PR M1M2_PR"
            multi_via_done = 1
            next
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

generate_group_branches_fixture() {
    local input="$1"
    local output="$2"
    awk '
        /^    - test_group ctrl\/_34_ ctrl\/_35_ \+ REGION test_region ;$/ {
            print "    - test_group ctrl/_3[45]_ ctrl/_34_ + REGION test_region ;"
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

perturb_aux_child_query_order() {
    local edadb_db="$1"

    sqlite3 "$edadb_db" <<'SQL'
PRAGMA foreign_keys = OFF;
CREATE TEMP TABLE blockage_rects_reversed AS
SELECT * FROM iBlockageSD__rect_list_sd_IdbRectSD
ORDER BY iBlockageSD_primary_key, _vec_idx DESC;
DELETE FROM iBlockageSD__rect_list_sd_IdbRectSD;
INSERT INTO iBlockageSD__rect_list_sd_IdbRectSD
SELECT * FROM blockage_rects_reversed;

CREATE TEMP TABLE region_rects_reversed AS
SELECT * FROM iRegion__boudary_list_IdbRectSD
ORDER BY iRegion__name, _vec_idx DESC;
DELETE FROM iRegion__boudary_list_IdbRectSD;
INSERT INTO iRegion__boudary_list_IdbRectSD
SELECT * FROM region_rects_reversed;

CREATE TEMP TABLE slot_rects_reversed AS
SELECT * FROM iSlotSD__rect_list_sd_IdbRectSD
ORDER BY iSlotSD_primary_key, _vec_idx DESC;
DELETE FROM iSlotSD__rect_list_sd_IdbRectSD;
INSERT INTO iSlotSD__rect_list_sd_IdbRectSD
SELECT * FROM slot_rects_reversed;

CREATE TEMP TABLE group_members_reversed AS
SELECT * FROM iGroupSD__instance_name_vec_sd___edadb_primitive_vector
ORDER BY iGroupSD__group_name_sd, __edadb_vec_idx DESC;
DELETE FROM iGroupSD__instance_name_vec_sd___edadb_primitive_vector;
INSERT INTO iGroupSD__instance_name_vec_sd___edadb_primitive_vector
SELECT * FROM group_members_reversed;

CREATE TEMP TABLE fill_rects_reversed AS
SELECT * FROM iFillSD__rect_list_sd_IdbRectSD
ORDER BY iFillSD_primary_key, _vec_idx DESC;
DELETE FROM iFillSD__rect_list_sd_IdbRectSD;
INSERT INTO iFillSD__rect_list_sd_IdbRectSD
SELECT * FROM fill_rects_reversed;

CREATE TEMP TABLE fill_coordinates_reversed AS
SELECT * FROM iFillSD__coordinate_list_sd_iCoordSD
ORDER BY iFillSD_primary_key, _vec_idx DESC;
DELETE FROM iFillSD__coordinate_list_sd_iCoordSD;
INSERT INTO iFillSD__coordinate_list_sd_iCoordSD
SELECT * FROM fill_coordinates_reversed;

CREATE TEMP TABLE fills_reversed AS
SELECT * FROM iFillSD ORDER BY rowid DESC;
DELETE FROM iFillSD;
INSERT INTO iFillSD SELECT * FROM fills_reversed;

CREATE TEMP TABLE special_wires_reversed AS
SELECT * FROM iSpecNetSD__wire_list_sd_iSpecWireSD
ORDER BY iSpecNetSD__net_name_sd, _vec_idx DESC;
DELETE FROM iSpecNetSD__wire_list_sd_iSpecWireSD;
INSERT INTO iSpecNetSD__wire_list_sd_iSpecWireSD
SELECT * FROM special_wires_reversed;

CREATE TEMP TABLE special_segments_reversed AS
SELECT * FROM iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD
ORDER BY iSpecNetSD__net_name_sd, iSpecNetSD__wire_list_sd_iSpecWireSD_primary_key, _vec_idx DESC;
DELETE FROM iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD;
INSERT INTO iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD
SELECT * FROM special_segments_reversed;
SQL
}

perturb_special_net_query_order() {
    local edadb_db="$1"

    sqlite3 "$edadb_db" <<'SQL'
PRAGMA foreign_keys = OFF;
CREATE TEMP TABLE special_points_reversed AS
SELECT * FROM iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD__point_list_sd_iCoordSD
ORDER BY iSpecNetSD__net_name_sd, iSpecNetSD__wire_list_sd_iSpecWireSD_primary_key,
         iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD_primary_key, _vec_idx DESC;
DELETE FROM iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD__point_list_sd_iCoordSD;
INSERT INTO iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD__point_list_sd_iCoordSD
SELECT * FROM special_points_reversed;

CREATE TEMP TABLE special_segments_reversed AS
SELECT * FROM iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD
ORDER BY iSpecNetSD__net_name_sd, iSpecNetSD__wire_list_sd_iSpecWireSD_primary_key, _vec_idx DESC;
DELETE FROM iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD;
INSERT INTO iSpecNetSD__wire_list_sd_iSpecWireSD__segment_list_sd_iSpecWireSegSD
SELECT * FROM special_segments_reversed;

CREATE TEMP TABLE special_wires_reversed AS
SELECT * FROM iSpecNetSD__wire_list_sd_iSpecWireSD
ORDER BY iSpecNetSD__net_name_sd, _vec_idx DESC;
DELETE FROM iSpecNetSD__wire_list_sd_iSpecWireSD;
INSERT INTO iSpecNetSD__wire_list_sd_iSpecWireSD
SELECT * FROM special_wires_reversed;

CREATE TEMP TABLE special_pin_strings_reversed AS
SELECT * FROM iSpecNetSD__pin_string_list_sd___edadb_primitive_vector
ORDER BY iSpecNetSD__net_name_sd, __edadb_vec_idx DESC;
DELETE FROM iSpecNetSD__pin_string_list_sd___edadb_primitive_vector;
INSERT INTO iSpecNetSD__pin_string_list_sd___edadb_primitive_vector
SELECT * FROM special_pin_strings_reversed;

CREATE TEMP TABLE special_io_pins_reversed AS
SELECT * FROM iSpecNetSD__io_pin_name_list_sd___edadb_primitive_vector
ORDER BY iSpecNetSD__net_name_sd, __edadb_vec_idx DESC;
DELETE FROM iSpecNetSD__io_pin_name_list_sd___edadb_primitive_vector;
INSERT INTO iSpecNetSD__io_pin_name_list_sd___edadb_primitive_vector
SELECT * FROM special_io_pins_reversed;

CREATE TEMP TABLE special_instance_pins_reversed AS
SELECT * FROM iSpecNetSD__instance_pin_list_sd_iSpecPinRef
ORDER BY iSpecNetSD__net_name_sd, _order_sd DESC;
DELETE FROM iSpecNetSD__instance_pin_list_sd_iSpecPinRef;
INSERT INTO iSpecNetSD__instance_pin_list_sd_iSpecPinRef
SELECT * FROM special_instance_pins_reversed;

CREATE TEMP TABLE special_nets_reversed AS
SELECT * FROM iSpecNetSD ORDER BY rowid DESC;
DELETE FROM iSpecNetSD;
INSERT INTO iSpecNetSD SELECT * FROM special_nets_reversed;
SQL
}

perturb_net_child_query_order() {
    local edadb_db="$1"

    sqlite3 "$edadb_db" <<'SQL'
PRAGMA foreign_keys = OFF;
CREATE TEMP TABLE regular_points_reversed AS
SELECT * FROM iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__point_list_sd_iCoordSD
ORDER BY iNetSD__net_name_sd, iNetSD__wire_list_sd_iRegWireSD_primary_key,
         iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD_primary_key, _vec_idx DESC;
DELETE FROM iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__point_list_sd_iCoordSD;
INSERT INTO iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__point_list_sd_iCoordSD
SELECT * FROM regular_points_reversed;

CREATE TEMP TABLE regular_via_refs_reversed AS
SELECT * FROM iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__via_ref_list_sd_iRegViaRef
ORDER BY iNetSD__net_name_sd, iNetSD__wire_list_sd_iRegWireSD_primary_key,
         iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD_primary_key, _order_sd DESC;
DELETE FROM iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__via_ref_list_sd_iRegViaRef;
INSERT INTO iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__via_ref_list_sd_iRegViaRef
SELECT * FROM regular_via_refs_reversed;

CREATE TEMP TABLE regular_virtual_points_reversed AS
SELECT * FROM iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__virtual_point_index_list_sd___edadb_primitive_vector
ORDER BY iNetSD__net_name_sd, iNetSD__wire_list_sd_iRegWireSD_primary_key,
         iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD_primary_key, __edadb_vec_idx DESC;
DELETE FROM iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__virtual_point_index_list_sd___edadb_primitive_vector;
INSERT INTO iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD__virtual_point_index_list_sd___edadb_primitive_vector
SELECT * FROM regular_virtual_points_reversed;

CREATE TEMP TABLE regular_wires_reversed AS
SELECT * FROM iNetSD__wire_list_sd_iRegWireSD
ORDER BY iNetSD__net_name_sd, _vec_idx DESC;
DELETE FROM iNetSD__wire_list_sd_iRegWireSD;
INSERT INTO iNetSD__wire_list_sd_iRegWireSD
SELECT * FROM regular_wires_reversed;

CREATE TEMP TABLE regular_segments_reversed AS
SELECT * FROM iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD
ORDER BY iNetSD__net_name_sd, iNetSD__wire_list_sd_iRegWireSD_primary_key, _vec_idx DESC;
DELETE FROM iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD;
INSERT INTO iNetSD__wire_list_sd_iRegWireSD__segment_list_sd_iRegWireSegSD
SELECT * FROM regular_segments_reversed;

CREATE TEMP TABLE regular_io_pins_reversed AS
SELECT * FROM iNetSD__io_pin_name_list_sd___edadb_primitive_vector
ORDER BY iNetSD__net_name_sd, __edadb_vec_idx DESC;
DELETE FROM iNetSD__io_pin_name_list_sd___edadb_primitive_vector;
INSERT INTO iNetSD__io_pin_name_list_sd___edadb_primitive_vector
SELECT * FROM regular_io_pins_reversed;

CREATE TEMP TABLE regular_instance_pins_reversed AS
SELECT * FROM iNetSD__instance_pin_list_sd_iNetPinRef
ORDER BY iNetSD__net_name_sd, _order_sd DESC;
DELETE FROM iNetSD__instance_pin_list_sd_iNetPinRef;
INSERT INTO iNetSD__instance_pin_list_sd_iNetPinRef
SELECT * FROM regular_instance_pins_reversed;

CREATE TEMP TABLE regular_nets_reversed AS
SELECT * FROM iNetSD ORDER BY _order_sd DESC;
DELETE FROM iNetSD;
INSERT INTO iNetSD SELECT * FROM regular_nets_reversed;
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
    if [[ "$check_mode" == "default" || "$check_mode" == "aux" || "$check_mode" == "pin_derived" || "$check_mode" == "instance_branches" || "$check_mode" == "group_branches" ]]; then
        perturb_row_query_order "$edadb_db"
        perturb_instance_query_order "$edadb_db"
    fi
    if [[ "$check_mode" == "aux" || "$check_mode" == "group_branches" ]]; then
        perturb_aux_child_query_order "$edadb_db"
    fi
    if [[ "$check_mode" == "special_net_branches" ]]; then
        perturb_special_net_query_order "$edadb_db"
    fi
    if [[ "$check_mode" == "grid_branches" ]]; then
        perturb_grid_query_order "$edadb_db"
    fi
    if [[ "$check_mode" == "via_branches" ]]; then
        perturb_via_query_order "$edadb_db"
    fi
    if [[ "$check_mode" == "routed" || "$check_mode" == "net_branches" ]]; then
        perturb_net_child_query_order "$edadb_db"
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
            check_aux_optional_sql "$name" "$edadb_db" "$case_dir/edadb2def.log"
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
            check_net_branch_sql "$name" "$edadb_db" "$case_dir/def2edadb.log" "$case_dir/edadb2def.log" \
                "$input_def" "$direct_def"
            ;;
        pin_derived)
            check_default_sql "$name" "$edadb_db" "$case_dir/def2edadb.log" "$case_dir/edadb2def.log"
            assert_eq "$(sql_value "$edadb_db" "select _no_port_orient_sd || '|' || _no_port_placement_status_sd || '|' || _no_port_location_sd__x_sd || ',' || _no_port_location_sd__y_sd from iPinSD where _pin_name_sd='req_msg[0]';")" \
                "2|3|1000,18645" "$name non-R0 no-PORT source fields"
            assert_eq "$(sql_value "$edadb_db" "select _vec_idx || '|' || _lx_sd || '|' || _ly_sd || '|' || _hx_sd || '|' || _hy_sd from iPinSD__io_term_sd_iTermSD__port_list_sd_iPortSD__layer_shape_list_sd_iLayerShapeSD__rect_list_sd_IdbRectSD where iPinSD__pin_name_sd='req_msg[0]';")" \
                "0|0|0|1000|2000" "$name asymmetric no-PORT rect"
            ;;
        pin_writer)
            assert_eq "$(sql_value "$edadb_db" "select _io_term_sd__has_port_sd || '|' || _io_term_sd__is_special_net_sd || '|' || _no_port_orient_sd || '|' || _no_port_placement_status_sd from iPinSD where _pin_name_sd='req_msg[0]';")" \
                "1|1|1|0" "$name stores canonical writer PORT and SPECIAL branches"
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
        group_branches)
            check_default_sql "$name" "$edadb_db" "$case_dir/def2edadb.log" "$case_dir/edadb2def.log"
            check_aux_optional_sql "$name" "$edadb_db" "$case_dir/edadb2def.log"
            assert_contains "$input_def" "test_group ctrl/_3[45]_ ctrl/_34_" "$name regex and duplicate member input"
            assert_contains "$direct_def" "test_group ctrl/_34_ ctrl/_35_ + REGION test_region" "$name parser-expanded deduplicated members"
            local reparsed_def="$case_dir/reparsed.def"
            export INPUT_DEF="$edadb_def"
            export OUTPUT_DEF="$reparsed_def"
            run_ieda "$SCRIPT_DIR/tcl/direct_def_roundtrip.tcl" "$case_dir/reparse.log"
            assert_def_equivalent "$edadb_def" "$reparsed_def" "$case_dir/reparse.diff"
            ;;
        special_net_branches)
            check_special_net_branch_sql "$name" "$edadb_db" "$input_def" "$direct_def" "$case_dir/edadb2def.log"
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

case_is_selected() {
    local case_name="$1"
    shift

    if [[ "$#" -eq 0 ]]; then
        return 0
    fi

    local requested_case
    for requested_case in "$@"; do
        if [[ "$requested_case" == "$case_name" ]]; then
            return 0
        fi
    done
    return 1
}

wait_for_one_case() {
    local -n case_names_ref="$1"
    local -n case_logs_ref="$2"
    local finished_pid
    local status

    if wait -n -p finished_pid "${!case_names_ref[@]}"; then
        status=0
    else
        status=$?
    fi

    local case_name="${case_names_ref[$finished_pid]}"
    local case_log="${case_logs_ref[$finished_pid]}"
    cat "$case_log"
    if [[ "$status" -eq 0 ]]; then
        echo "PASS: completed case $case_name"
    else
        echo "FAIL: case $case_name exited with status $status; log: $case_log" >&2
    fi

    unset 'case_names_ref[$finished_pid]'
    unset 'case_logs_ref[$finished_pid]'
    return "$status"
}

run_cases_parallel() {
    local -a requested_cases=("$@")
    local -a case_specs=(
        "default_ipl|$BASE_DEF|default"
        "design_fields|$OUT_DIR/fixtures/design_fields.def|design_fields"
        "design_fallback|$OUT_DIR/fixtures/design_fallback.def|design_fallback"
        "die_polygon|$OUT_DIR/fixtures/die_polygon.def|die_polygon"
        "aux_optional|$OUT_DIR/fixtures/aux_optional.def|aux"
        "pin_derived|$OUT_DIR/fixtures/pin_derived.def|pin_derived"
        "pin_writer|$OUT_DIR/fixtures/pin_writer.def|pin_writer"
        "pin_branches|$OUT_DIR/fixtures/pin_branches.def|pin_branches"
        "group_branches|$OUT_DIR/fixtures/group_branches.def|group_branches"
        "special_net_branches|$OUT_DIR/fixtures/special_net_branches.def|special_net_branches"
        "grid_branches|$OUT_DIR/fixtures/grid_branches.def|grid_branches"
        "via_branches|$OUT_DIR/fixtures/via_branches.def|via_branches"
        "instance_branches|$OUT_DIR/fixtures/instance_branches.def|instance_branches"
        "routed_irt|$ROUTED_DEF|routed"
        "net_branches|$OUT_DIR/fixtures/net_branches.def|net_branches"
    )
    local -A running_case_names=()
    local -A running_case_logs=()
    local selected_count=0
    local failed=0

    mkdir -p "$OUT_DIR/case-logs"
    local case_spec case_name input_def check_mode case_log case_pid
    for case_spec in "${case_specs[@]}"; do
        IFS='|' read -r case_name input_def check_mode <<<"$case_spec"
        if ! case_is_selected "$case_name" "${requested_cases[@]}"; then
            continue
        fi

        selected_count=$((selected_count + 1))
        case_log="$OUT_DIR/case-logs/$case_name.log"
        (
            run_case "$case_name" "$input_def" "$check_mode"
        ) >"$case_log" 2>&1 &
        case_pid=$!
        running_case_names["$case_pid"]="$case_name"
        running_case_logs["$case_pid"]="$case_log"
        echo "START: case=$case_name pid=$case_pid log=$case_log"

        if [[ "${#running_case_names[@]}" -ge "$EDADB_TEST_JOBS" ]]; then
            if ! wait_for_one_case running_case_names running_case_logs; then
                failed=1
            fi
        fi
    done

    if [[ "$selected_count" -eq 0 ]]; then
        echo "No matching cases selected: ${requested_cases[*]}" >&2
        return 1
    fi

    while [[ "${#running_case_names[@]}" -gt 0 ]]; do
        if ! wait_for_one_case running_case_names running_case_logs; then
            failed=1
        fi
    done

    return "$failed"
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
    if [[ "$EDADB_TEST_JOBS" != "auto" && ! "$EDADB_TEST_JOBS" =~ ^[1-9][0-9]*$ ]]; then
        echo "EDADB_TEST_JOBS must be auto or a positive integer: $EDADB_TEST_JOBS" >&2
        exit 1
    fi
    if [[ ! "$EDADB_TEST_PROCESS_MEMORY_GIB" =~ ^[1-9][0-9]*$ ]]; then
        echo "EDADB_TEST_PROCESS_MEMORY_GIB must be a positive integer" >&2
        exit 1
    fi
    if [[ ! "$EDADB_TEST_MEMORY_RESERVE_GIB" =~ ^[1-9][0-9]*$ ]]; then
        echo "EDADB_TEST_MEMORY_RESERVE_GIB must be a positive integer" >&2
        exit 1
    fi
    resolve_test_jobs

    rm -rf "$OUT_DIR"
    mkdir -p "$OUT_DIR/fixtures"

    local aux_def="$OUT_DIR/fixtures/aux_optional.def"
    local net_branch_def="$OUT_DIR/fixtures/net_branches.def"
    local pin_derived_def="$OUT_DIR/fixtures/pin_derived.def"
    local pin_writer_def="$OUT_DIR/fixtures/pin_writer.def"
    local pin_branches_def="$OUT_DIR/fixtures/pin_branches.def"
    local group_branches_def="$OUT_DIR/fixtures/group_branches.def"
    local special_net_branches_def="$OUT_DIR/fixtures/special_net_branches.def"
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
    generate_group_branches_fixture "$aux_def" "$group_branches_def"
    generate_special_net_branch_fixture "$aux_def" "$special_net_branches_def"
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
    echo "generated fixture: $group_branches_def"
    echo "generated fixture: $special_net_branches_def"
    echo "generated fixture: $design_fields_def"
    echo "generated fixture: $design_fallback_def"
    echo "generated fixture: $die_polygon_def"
    echo "generated fixture: $grid_branch_def"
    echo "generated fixture: $via_branch_def"
    echo "generated fixture: $instance_branch_def"

    echo "parallel jobs: $EDADB_TEST_JOBS"
    if [[ "$#" -gt 0 ]]; then
        echo "selected cases: $*"
    else
        echo "selected cases: all"
    fi
    run_cases_parallel "$@"

    echo "All EDADB iDB roundtrip regression tests passed."
}

main "$@"
