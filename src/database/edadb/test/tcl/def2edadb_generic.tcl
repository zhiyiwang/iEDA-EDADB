proc require_env {name} {
    if {![info exists ::env($name)] || ![string length $::env($name)]} {
        error "$name is not set"
    }
}

foreach name {
    DESIGN_TCL_SCRIPT_DIR
    INPUT_DEF
    EDADB_DB_PATH
} {
    require_env $name
}

source $::env(DESIGN_TCL_SCRIPT_DIR)/DB_script/db_path_setting.tcl
source $::env(DESIGN_TCL_SCRIPT_DIR)/DB_script/db_init_lef.tcl

puts "==> EDADB write input DEF: $::env(INPUT_DEF)"
puts "==> EDADB write DB: $::env(EDADB_DB_PATH)"
file mkdir [file dirname $::env(EDADB_DB_PATH)]
def_init -path $::env(INPUT_DEF)
edadb_write -edadb_db_path $::env(EDADB_DB_PATH)

flow_exit
