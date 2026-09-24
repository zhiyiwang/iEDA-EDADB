# Expected failure must propagate through the adapter/builder to the Tcl command.
source $::env(DESIGN_TCL_SCRIPT_DIR)/DB_script/db_path_setting.tcl
source $::env(DESIGN_TCL_SCRIPT_DIR)/DB_script/db_init_lef.tcl
def_init -path $::env(INPUT_DEF)
if {![catch {edadb_write -edadb_db_path $::env(EDADB_DB_PATH)} message]} {
    error "injected write unexpectedly succeeded"
}
puts "PASS: adapter error reached Tcl"
# A separate read-only connection checks committed state before any retry.
puts [exec python3 $::env(FAULT_RUNNER) check-empty $::env(EDADB_DB_PATH)]
# Retry in the same iEDA process; the shim injects only once.
edadb_write -edadb_db_path $::env(EDADB_DB_PATH)
puts "PASS: same-process retry succeeded"
flow_exit
