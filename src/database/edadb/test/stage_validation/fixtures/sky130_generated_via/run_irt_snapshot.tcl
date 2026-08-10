proc require_env {name} {
    if {![info exists ::env($name)] || ![string length $::env($name)]} {
        error "$name is not set"
    }
}

foreach name {
    CONFIG_DIR
    TCL_SCRIPT_DIR
    RESULT_DIR
    INPUT_DEF
    INPUT_MODE
} {
    require_env $name
}

if {$::env(INPUT_MODE) ni {native edadb}} {
    error "INPUT_MODE must be native or edadb: $::env(INPUT_MODE)"
}
if {$::env(INPUT_MODE) eq "edadb"} {
    require_env EDADB_DB_PATH
}

file mkdir $::env(RESULT_DIR)
file mkdir $::env(RESULT_DIR)/rt

flow_init -config $::env(CONFIG_DIR)/flow_config.json
db_init -config $::env(CONFIG_DIR)/db_default_config.json -output_dir_path $::env(RESULT_DIR)
source $::env(TCL_SCRIPT_DIR)/DB_script/db_path_setting.tcl
source $::env(TCL_SCRIPT_DIR)/DB_script/db_init_lef.tcl

if {$::env(INPUT_MODE) eq "native"} {
    def_init -path $::env(INPUT_DEF)
} else {
    edadb_read -edadb_db_path $::env(EDADB_DB_PATH) -path $::env(INPUT_DEF)
}

def_save -path $::env(RESULT_DIR)/pre_tool.def
report_db -path $::env(RESULT_DIR)/pre_tool_db.rpt

init_rt -temp_directory_path $::env(RESULT_DIR)/rt/ \
        -bottom_routing_layer met1 \
        -top_routing_layer met4 \
        -thread_number 1 \
        -enable_notification 1
destroy_rt

flow_exit
