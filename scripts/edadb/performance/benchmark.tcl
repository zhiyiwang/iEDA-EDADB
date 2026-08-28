# run.sh starts a fresh iEDA process three times and sets PERF_MODE to:
#   native: time DEF read and DEF write.
#   write:  build iDB from DEF, then time only EDADB write.
#   read:   time EDADB read, then write a DEF only for correctness checking.
# This file executes exactly one of those modes per iEDA process.

# Stop immediately when run.sh did not provide a required environment variable.
proc require_env {name} {
  if {![info exists ::env($name)] || $::env($name) eq ""} {
    error "$name is not set"
  }
}

# Time one synchronous iEDA Tcl command and print one machine-readable record:
#   EDADB_PERF<TAB>phase<TAB>elapsed_microseconds
# uplevel executes the command list in the caller's Tcl scope. If the command
# fails, the error naturally propagates and run.sh rejects the sample.
proc time_command {phase command} {
  # Start before uplevel so all synchronous iEDA work is included.
  set start_us [clock microseconds]
  # Execute the command in the caller's scope and stop timing after it returns.
  set result [uplevel 1 $command]
  set elapsed_us [expr {[clock microseconds] - $start_us}]
  puts "EDADB_PERF\t$phase\t$elapsed_us"
  flush stdout
  return $result
}

# These three variables are shared by all modes. The mode-specific paths are
# checked inside their own switch branch below.
foreach name {PERF_MODE DESIGN_TCL_SCRIPT_DIR INPUT_DEF} {
  require_env $name
}

# Load technology/cell LEF before starting any timer. These source commands call
# tech_lef_init/lef_init directly, so LEF file parsing is excluded from all four
# reported DEF/EDADB metrics. Later layer/site/master lookups against the loaded
# LEF objects remain part of the corresponding DEF/EDADB command.
source $::env(DESIGN_TCL_SCRIPT_DIR)/DB_script/db_path_setting.tcl
source $::env(DESIGN_TCL_SCRIPT_DIR)/DB_script/db_init_lef.tcl

switch -- $::env(PERF_MODE) {
  native {
    # Native baseline:
    #   INPUT_DEF --def_init--> active iDB --def_save--> native.def
    # Both DEF commands are timed independently.
    require_env OUTPUT_DEF
    # [] evaluates list first; list safely builds the command passed to time_command.
    time_command native_def_read [list def_init -path $::env(INPUT_DEF)]
    time_command native_def_write [list def_save -path $::env(OUTPUT_DEF)]
  }
  write {
    # EDADB write:
    #   INPUT_DEF --def_init--> active iDB --edadb_write--> edadb.db
    # def_init only prepares the same in-memory iDB used by both writers and is
    # deliberately outside time_command; only edadb_write is measured.
    require_env EDADB_DB_PATH
    def_init -path $::env(INPUT_DEF)
    time_command edadb_write [list edadb_write -edadb_db_path $::env(EDADB_DB_PATH)]
  }
  read {
    # EDADB read:
    #   edadb.db --edadb_read--> active iDB --def_save--> edadb.def
    # Only edadb_read is measured. def_save produces the file that run.sh
    # compares with native.def and is deliberately outside time_command.
    require_env EDADB_DB_PATH
    require_env OUTPUT_DEF
    time_command edadb_read [list edadb_read \
      -edadb_db_path $::env(EDADB_DB_PATH) \
      -path $::env(INPUT_DEF)]
    def_save -path $::env(OUTPUT_DEF)
  }
  default {
    error "unknown PERF_MODE: $::env(PERF_MODE)"
  }
}

# End this iEDA process after its selected mode has completed.
flow_exit
