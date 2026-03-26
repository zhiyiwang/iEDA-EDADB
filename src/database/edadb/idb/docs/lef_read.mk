# edadb read lef call flow

## tcl file
DESIGN_TCL_SCRIPT_DIR = scripts/design/sky130_gcd/script/iNO_script

### reset data path
`source $::env(DESIGN_TCL_SCRIPT_DIR)/DB_script/db_path_setting.tcl`

### read lef
`source $::env(DESIGN_TCL_SCRIPT_DIR)/DB_script/db_init_lef.tcl`

## tcl commands

### tech_lef_init
`tech_lef_init -path $TECH_LEF_PATH`
- 定义 src/interface/tcl/tcl_idb/tcl_register_idb.h
- 实现 src/interface/tcl/tcl_idb/tcl_db_file.cpp
	- 调用 `dmInst->readLef(path_list, true);`: read lef files
- src/platform/data_manager/idm.cpp
	- `idm::DataManager::readLef`
	- `bool DataManager::initLef(vector<string> lef_path, bool b_techlef)`
- src/database/manager/builder/builder.cpp
	- `IdbLefService* IdbBuilder::buildLef(vector<string>& files, bool b_techfile)`
- src/database/manager/service/lef_service/lef_service.cpp
	- `IdbLefServiceResult IdbLefService::LefFileInit(vector<string> lef_files)`
- src/database/manager/builder/lef_builder/lef_read.cpp
	- `bool LefRead::createDb(const char* file_name)`
		- `int LefRead::parse_layer(lefiLayer* lef_layer)` -> [IdbLayer] and the child classes
		- `int LefRead::parse_macro_new(const char* macro_name)` -> [IdbCellMaster] set macro name
		- `int LefRead::parse_macro(lefiMacro* lef_macro)` -> lef_macro to set _this_cell_master
		- `int LefRead::parse_manufacture_grid(double value)` -> [IdbLayer] set manufacturing grid
		- `int LefRead::parse_max_stack_via(lefiMaxStackVia* maxStack)` -> [IdbLayer] set max stack via
		- `int LefRead::parse_obs(lefiObstruction* lef_obs)` -> [IdbObstruction] set obstruction
			- lefiGeometries*, IdbObsLayer*, IdbLayer*, IdbLayerShape, IdbObs
		- `int LefRead::parse_pin(lefiPin* lef_pin)` -> [IdbPin], [IdbTerm]
		- `int LefRead::parse_property_definition(lefiProp* prop)` 
		- `bool LefRead::createDb(const char* file_name)` -> [IdbSites]
		- `int LefRead::parse_units(lefiUnits* lef_units)` -> [IdbUnits]
		- `int LefRead::parse_via(lefiVia* lef_via)` -> [IdbVia]
		- `int LefRead::parse_via_rule(lefiViaRule* lef_via_rule)` -> [IdbViaRule]


### lef_init
`lef_init -path $LEF_PATH`
- 定义 `src/interface/tcl/tcl_idb/tcl_register_idb.h`
- 实现 `src/interface/tcl/tcl_idb/tcl_db_file.cpp`
	- 调用 `iplf::tmInst->idbStart(data_config);`: initialize lef parser
- src/platform/tool_manager/tool_manager.cpp
	- `iplf::ToolManager::idbStart(std::string config_path)`
- src/platform/data_manager/idm.cpp
	- `bool DataManager::init(string config_path)`


## lef read class
- IdbLayer and child classes
- IdbCellMaster
- IdbPins
- IdbTerm