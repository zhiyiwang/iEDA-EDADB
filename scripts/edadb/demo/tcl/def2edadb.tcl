#### scripts/design/sky130_gcd/script/iNO_script/run_iNO_fix_fanout.tcl

#### prepare env

#===========================================================
##   reset data path
#===========================================================
source $::env(DESIGN_TCL_SCRIPT_DIR)/DB_script/db_path_setting.tcl

##===========================================================
##   read lef
#===========================================================
source $::env(DESIGN_TCL_SCRIPT_DIR)/DB_script/db_init_lef.tcl


#===========================================================
## when read / write edadb, EDADB_DB_PATH must be set
## --- flags: 1 => use EDADB; 0 => use DEF
#===========================================================
set read_edadb  [expr {[info exists ::env(READ_EDADB)]  && $::env(READ_EDADB)}]
set write_edadb [expr {[info exists ::env(WRITE_EDADB)] && $::env(WRITE_EDADB)}]
if {$read_edadb || $write_edadb} {
    if {![info exists ::env(EDADB_DB_PATH)] || ![string length $::env(EDADB_DB_PATH)]} {
        error "READ_EDADB=1 or WRITE_EDADB=1, but EDADB_DB_PATH is empty."
    }
    set db_path $::env(EDADB_DB_PATH)
}



#===========================================================
##   read def
#===========================================================
if {[info exists ::env(READ_DEF)] && $::env(READ_DEF)} {
    puts "==> READ_DEF enabled, reading DEF file..."
    def_init -path $::env(INPUT_DEF)
} else {
    puts "==> READ_DEF disabled, skip reading DEF."
}


#===========================================================
##   write edadb 
#===========================================================
if {$write_edadb} {
    file mkdir [file dirname $db_path]
    puts "==> WRITE_EDADB=1: writing EDADB to $db_path"
    edadb_write -edadb_db_path $db_path
} else {
    puts "==> WRITE_EDADB disabled, skip writing to edadb."
}

## #===========================================================
## ##   read edadb
## #===========================================================
## if {$read_edadb} {
##     puts "==> READ_EDADB=1: reading EDADB from $db_path"
##     edadb_read -edadb_db_path $db_path -path $::env(INPUT_DEF)
## } else {
##     puts "==> READ_EDADB disabled, skip reading edadb."
## }


#===========================================================
##   Write something to check running status
#===========================================================
set fh [open "$::env(RESULT_DIR)/.flow_ok" w]
puts $fh "ok"
close $fh



#===========================================================
##   Exit 
#===========================================================
flow_exit
