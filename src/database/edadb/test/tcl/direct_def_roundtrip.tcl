proc require_env {name} {
    if {![info exists ::env($name)] || ![string length $::env($name)]} {
        error "$name is not set"
    }
}

foreach name {
    DESIGN_TCL_SCRIPT_DIR
    INPUT_DEF
    OUTPUT_DEF
} {
    require_env $name
}

source $::env(DESIGN_TCL_SCRIPT_DIR)/DB_script/db_path_setting.tcl
source $::env(DESIGN_TCL_SCRIPT_DIR)/DB_script/db_init_lef.tcl

puts "==> direct DEF roundtrip input: $::env(INPUT_DEF)"
puts "==> direct DEF roundtrip output: $::env(OUTPUT_DEF)"
def_init -path $::env(INPUT_DEF)
def_save -path $::env(OUTPUT_DEF)

flow_exit
