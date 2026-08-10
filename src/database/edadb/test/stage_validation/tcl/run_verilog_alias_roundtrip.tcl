flow_init -config $::env(IEDA_CONFIG_DIR)/flow_config.json
db_init -config $::env(IEDA_CONFIG_DIR)/db_default_config.json -output_dir_path $::env(RESULT_DIR)

source $::env(IEDA_TCL_SCRIPT_DIR)/DB_script/db_path_setting.tcl
source $::env(IEDA_TCL_SCRIPT_DIR)/DB_script/db_init_lef.tcl

verilog_init -path $::env(INPUT_VERILOG) -top io_port_alias
netlist_save -path $::env(OUTPUT_VERILOG) -exclude_cell_names {}
def_save -path $::env(OUTPUT_DEF)

flow_exit
