proc require_env {name} {
    if {![info exists ::env($name)] || ![string length $::env($name)]} {
        error "$name is not set"
    }
}

foreach name {DESIGN_TCL_SCRIPT_DIR INPUT_DEF EDADB_DB_PATH OUTPUT_DEF} {
    require_env $name
}

source $::env(DESIGN_TCL_SCRIPT_DIR)/DB_script/db_path_setting.tcl
source $::env(DESIGN_TCL_SCRIPT_DIR)/DB_script/db_init_lef.tcl

puts "==> EDADB read database: $::env(EDADB_DB_PATH)"
puts "==> EDADB reference DEF: $::env(INPUT_DEF)"
puts "==> EDADB output DEF: $::env(OUTPUT_DEF)"
edadb_read -edadb_db_path $::env(EDADB_DB_PATH) -path $::env(INPUT_DEF)
def_save -path $::env(OUTPUT_DEF)

flow_exit
