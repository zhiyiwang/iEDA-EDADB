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
    PRE_TOOL_DEF
    PRE_TOOL_REPORT
    STAGE
} {
    require_env $name
}

set stage $::env(STAGE)
set input_mode $::env(INPUT_MODE)
set run_tool [expr {![info exists ::env(RUN_TOOL)] || $::env(RUN_TOOL)}]

if {$input_mode ni {native edadb}} {
    error "INPUT_MODE must be native or edadb: $input_mode"
}
if {$input_mode eq "edadb"} {
    require_env EDADB_DB_PATH
}
if {$run_tool} {
    require_env OUTPUT_DEF
}

file mkdir $::env(RESULT_DIR)
file mkdir $::env(RESULT_DIR)/feature
file mkdir $::env(RESULT_DIR)/report

flow_init -config $::env(CONFIG_DIR)/flow_config.json
db_init -config $::env(CONFIG_DIR)/db_default_config.json -output_dir_path $::env(RESULT_DIR)
source $::env(TCL_SCRIPT_DIR)/DB_script/db_path_setting.tcl

switch -- $stage {
    ito_drv {
        source $::env(TCL_SCRIPT_DIR)/DB_script/db_init_lib_drv.tcl
    }
    ito_hold {
        source $::env(TCL_SCRIPT_DIR)/DB_script/db_init_lib_hold.tcl
    }
    default {
        source $::env(TCL_SCRIPT_DIR)/DB_script/db_init_lib.tcl
    }
}

source $::env(TCL_SCRIPT_DIR)/DB_script/db_init_sdc.tcl
source $::env(TCL_SCRIPT_DIR)/DB_script/db_init_lef.tcl

if {$input_mode eq "native"} {
    puts "==> native DEF read: $::env(INPUT_DEF)"
    def_init -path $::env(INPUT_DEF)
} else {
    puts "==> EDADB read: $::env(EDADB_DB_PATH)"
    puts "==> EDADB reference DEF: $::env(INPUT_DEF)"
    edadb_read -edadb_db_path $::env(EDADB_DB_PATH) -path $::env(INPUT_DEF)
}

def_save -path $::env(PRE_TOOL_DEF)
report_db -path $::env(PRE_TOOL_REPORT)

if {!$run_tool} {
    flow_exit
    return
}

switch -- $stage {
    ipl {
        run_placer -config $::env(CONFIG_DIR)/pl_default_config.json
        feature_tool -path $::env(RESULT_DIR)/feature/tool.json -step place
        set feature_step place
    }
    icts {
        file mkdir $::env(RESULT_DIR)/cts
        run_cts -config $::env(CONFIG_DIR)/cts_default_config.json -work_dir $::env(RESULT_DIR)/cts
        feature_tool -path $::env(RESULT_DIR)/feature/tool.json -step CTS
        cts_report -path $::env(RESULT_DIR)/cts
        set feature_step CTS
    }
    ito_drv {
        run_to_drv -config $::env(CONFIG_DIR)/to_default_config_drv.json
        feature_tool -path $::env(RESULT_DIR)/feature/tool.json -step optDrv
        set feature_step optDrv
    }
    ito_hold {
        run_to_hold -config $::env(CONFIG_DIR)/to_default_config_hold.json
        feature_tool -path $::env(RESULT_DIR)/feature/tool.json -step optHold
        set feature_step optHold
    }
    ipl_lg {
        run_incremental_flow -config $::env(CONFIG_DIR)/pl_default_config.json
        feature_tool -path $::env(RESULT_DIR)/feature/tool.json -step legalization
        set feature_step legalization
    }
    irt {
        set rt_threads [expr {[info exists ::env(RT_THREAD_NUMBER)] ? $::env(RT_THREAD_NUMBER) : 64}]
        set rt_enable_notification [expr {[info exists ::env(RT_ENABLE_NOTIFICATION)] ? $::env(RT_ENABLE_NOTIFICATION) : 0}]
        set rt_snapshot_only [expr {[info exists ::env(RT_SNAPSHOT_ONLY)] ? $::env(RT_SNAPSHOT_ONLY) : 0}]
        set rt_bottom_layer [expr {[info exists ::env(RT_BOTTOM_LAYER)] ? $::env(RT_BOTTOM_LAYER) : "met1"}]
        set rt_top_layer [expr {[info exists ::env(RT_TOP_LAYER)] ? $::env(RT_TOP_LAYER) : "met4"}]
        file mkdir $::env(RESULT_DIR)/rt
        init_rt -temp_directory_path $::env(RESULT_DIR)/rt/ \
                -bottom_routing_layer $rt_bottom_layer \
                -top_routing_layer $rt_top_layer \
                -thread_number $rt_threads \
                -enable_notification $rt_enable_notification
        if {!$rt_snapshot_only} {
            run_rt
            feature_tool -path $::env(RESULT_DIR)/feature/tool.json -step route
        }
        destroy_rt
        set feature_step route
    }
    default {
        error "unsupported STAGE: $stage"
    }
}

def_save -path $::env(OUTPUT_DEF)
netlist_save -path $::env(RESULT_DIR)/post_tool.v -exclude_cell_names {}
report_db -path $::env(RESULT_DIR)/report/post_tool_db.rpt
if {![info exists rt_snapshot_only] || !$rt_snapshot_only} {
    feature_summary -path $::env(RESULT_DIR)/feature/summary.json -step $feature_step
}

flow_exit
