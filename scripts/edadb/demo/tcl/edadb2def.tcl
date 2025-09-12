#### scripts/design/sky130_gcd/script/iNO_script/run_iNO_fix_fanout.tcl

#### prepare env

#===========================================================
##   reset data path 
#===========================================================
source $::env(TCL_SCRIPT_DIR)/DB_script/db_path_setting.tcl

#===========================================================
##   read lef
#===========================================================
source $::env(TCL_SCRIPT_DIR)/DB_script/db_init_lef.tcl


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



#### read from def and edadb

#===========================================================
##   read def:
#===========================================================
set INPUT_DEF $::env(INPUT_DEF)
if {[info exists ::env(READ_DEF)] && $::env(READ_DEF)} {
    puts "==> READ_DEF enabled, reading DEF file..."
    def_init -path $INPUT_DEF
} else {
    puts "==> READ_DEF disabled, skip reading DEF."
}

#===========================================================
##   read edadb: 
#===========================================================
if {[info exists ::env(READ_EDADB)] && $::env(READ_EDADB)} {
    puts "==> READ_EDADB enabled, reading edadb..."
    edadb_read -index 1 -edadb_db_path $db_path
} else {
    puts "==> READ_EDADB disabled, skip reading edadb."
}


#===========================================================
##   save def: output to def_post file to compare with input def
#===========================================================
set base [file rootname $INPUT_DEF]
set ext [file extension $INPUT_DEF]
set EDADB_DEF_POST $::env(EDADB_DEF_POST)
set OUTPUT_DEF "${base}${EDADB_DEF_POST}${ext}"

## output to terminal
puts "==> Saving DEF to $OUTPUT_DEF ..."
def_save -path $OUTPUT_DEF



#===========================================================
##   Exit 
##   [USE_EDADB]: 核心是执行 exit(0);
#===========================================================
flow_exit