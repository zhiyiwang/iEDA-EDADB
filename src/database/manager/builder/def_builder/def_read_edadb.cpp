/**
 * @file def_read_edadb.h
 * @author Zhiyi Wang (zhiyiwang@ict.ac.cn)
*/

#include "def_read_edadb.h"


namespace idb {

DefReadEdadb::DefReadEdadb(IdbDefService* def_service) : DefRead(def_service)
{}



bool DefReadEdadb::createDbFromEdadb(const char* edadb_path, const char* path)
{
    if (_def_service == nullptr) {
        std::cerr << "Error: DefReadEdadb::_def_service is nullptr" << std::endl;
        return false;
    }

    if (edadb::init2read(edadb_path) < 0) {
        std::cerr << "Error: DefReadEdadb::createDbFromEdadb failed to init2read!" << std::endl;
        return false;
    }

    if (!createDbByEdadb(edadb_path)) {
        std::cerr << "Error: DefReadEdadb::createDbFromEdadb failed!" << std::endl;
        return false;
    }

    if (!createDbByDef(path)) {
        std::cerr << "Error: DefReadEdadb::createDbFromEdadb failed!" << std::endl; 
        return false;
    }

  
    return true;
} // createDbFromEdadb



bool DefReadEdadb::createDbByDef(const char* path) {
    FILE* f = fopen(path, "r");
    if (f == NULL) {
      std::cerr << "Open def file failed..." << std::endl;
      return false;
    }

    defrInit();
    defrReset();

    defrInitSession();

//-- done
//    defrSetVersionStrCbk(versionCallback);
//    defrSetBusBitCbk(busBitCharsCallBack);
//    defrSetUnitsCbk(unitsCallback);
//    defrSetDesignCbk(designCallback);
//    defrSetDieAreaCbk(dieAreaCallback);
//    defrSetRowCbk(rowCallback);
//    defrSetTrackCbk(trackGridCallback);
//    defrSetGcellGridCbk(gcellGridCallback);
//    defrSetViaStartCbk(viaBeginCallback);
//    defrSetViaCbk(viaCallback);
    defrSetRegionCbk(regionCallback);
    defrSetSlotCbk(slotsCallback);
    defrSetComponentCbk(componentsCallback);
    defrSetComponentStartCbk(componentNumberCallback);
    defrSetComponentEndCbk(componentEndCallback);
    defrSetPinCbk(pinCallback);
    defrSetPinEndCbk(pinsEndCallback);
    defrSetStartPinsCbk(pinsBeginCallback);
    defrSetBlockageCbk(blockageCallback);
    defrSetGroupCbk(groupCallback);
    defrSetFillStartCbk(fillsCallback);
    defrSetFillCbk(fillCallback);


// todo 
    defrSetNetStartCbk(netBeginCallback);
    defrSetNetCbk(netCallback);
    defrSetNetEndCbk(netEndCallback);
    defrSetAddPathToNet();

//-- working on 
    defrSetSNetStartCbk(specialNetBeginCallback);
    defrSetSNetCbk(specialNetCallback);
    defrSetSNetEndCbk(specialNetEndCallback);

//------------------------------------------------------------------

    //   defrSetPropCbk(propCallback);
    //   defrSetPropDefEndCbk(propEndCallback);
    //   defrSetPropDefStartCbk(propStartCallback);
    //  defrSetBlockageStartCbk(blockageBeginCallback);
    //  defrSetBlockageEndCbk(blockageEndCallback);
    //   defrSetComponentMaskShiftLayerCbk(componentMaskShiftCallback);
    //   defrSetExtensionCbk(extensionCallback);
    //   defrSetGroupMemberCbk(groupMemberCallback);
    //   defrSetGroupNameCbk(groupNameCallback);
    //   defrSetHistoryCbk(historyCallback);
    //   defrSetNonDefaultCbk(nonDefaultRuleCallback);
    //   defrSetPinPropCbk(pinPropCallback);
    //   defrSetScanchainsStartCbk(scanchainsCallback);
    // defrSetStartPinsCbk(pinsStartCallback);
    //   defrSetStylesStartCbk(stylesCallback);
    //   defrSetTechnologyCbk(technologyCallback);
    // void* userData = (void*) 0x01020304;

    int res = defrRead(f, path, (defiUserData) this, /* case sensitive */ 1);

    if (res != 0) {
      return false;
    }

    (void) defrUnsetCallbacks();

    // Unset all the callbacks
    defrUnsetArrayNameCbk();
    defrUnsetAssertionCbk();
    defrUnsetAssertionsStartCbk();
    defrUnsetAssertionsEndCbk();
    defrUnsetBlockageCbk();
    defrUnsetBlockageStartCbk();
    defrUnsetBlockageEndCbk();
    defrUnsetBusBitCbk();
    defrUnsetCannotOccupyCbk();
    defrUnsetCanplaceCbk();
    defrUnsetCaseSensitiveCbk();
    defrUnsetComponentCbk();
    defrUnsetComponentExtCbk();
    defrUnsetComponentStartCbk();
    defrUnsetComponentEndCbk();
    defrUnsetConstraintCbk();
    defrUnsetConstraintsStartCbk();
    defrUnsetConstraintsEndCbk();
    defrUnsetDefaultCapCbk();
    defrUnsetDesignCbk();
    defrUnsetDesignEndCbk();
    defrUnsetDieAreaCbk();
    defrUnsetDividerCbk();
    defrUnsetExtensionCbk();
    defrUnsetFillCbk();
    defrUnsetFillStartCbk();
    defrUnsetFillEndCbk();
    defrUnsetFPCCbk();
    defrUnsetFPCStartCbk();
    defrUnsetFPCEndCbk();
    defrUnsetFloorPlanNameCbk();
    defrUnsetGcellGridCbk();
    defrUnsetGroupCbk();
    defrUnsetGroupExtCbk();
    defrUnsetGroupMemberCbk();
    defrUnsetComponentMaskShiftLayerCbk();
    defrUnsetGroupNameCbk();
    defrUnsetGroupsStartCbk();
    defrUnsetGroupsEndCbk();
    defrUnsetHistoryCbk();
    defrUnsetIOTimingCbk();
    defrUnsetIOTimingsStartCbk();
    defrUnsetIOTimingsEndCbk();
    defrUnsetIOTimingsExtCbk();
    defrUnsetNetCbk();
    defrUnsetNetNameCbk();
    defrUnsetNetNonDefaultRuleCbk();
    defrUnsetNetConnectionExtCbk();
    defrUnsetNetExtCbk();
    defrUnsetNetPartialPathCbk();
    defrUnsetNetSubnetNameCbk();
    defrUnsetNetStartCbk();
    defrUnsetNetEndCbk();
    defrUnsetNonDefaultCbk();
    defrUnsetNonDefaultStartCbk();
    defrUnsetNonDefaultEndCbk();
    defrUnsetPartitionCbk();
    defrUnsetPartitionsExtCbk();
    defrUnsetPartitionsStartCbk();
    defrUnsetPartitionsEndCbk();
    defrUnsetPathCbk();
    defrUnsetPinCapCbk();
    defrUnsetPinCbk();
    defrUnsetPinEndCbk();
    defrUnsetPinExtCbk();
    defrUnsetPinPropCbk();
    defrUnsetPinPropStartCbk();
    defrUnsetPinPropEndCbk();
    defrUnsetPropCbk();
    defrUnsetPropDefEndCbk();
    defrUnsetPropDefStartCbk();
    defrUnsetRegionCbk();
    defrUnsetRegionStartCbk();
    defrUnsetRegionEndCbk();
    defrUnsetRowCbk();
    defrUnsetScanChainExtCbk();
    defrUnsetScanchainCbk();
    defrUnsetScanchainsStartCbk();
    defrUnsetScanchainsEndCbk();
    defrUnsetSiteCbk();
    defrUnsetSlotCbk();
    defrUnsetSlotStartCbk();
    defrUnsetSlotEndCbk();
    defrUnsetSNetWireCbk();
    defrUnsetSNetCbk();
    defrUnsetSNetStartCbk();
    defrUnsetSNetEndCbk();
    defrUnsetSNetPartialPathCbk();
    defrUnsetStartPinsCbk();
    defrUnsetStylesCbk();
    defrUnsetStylesStartCbk();
    defrUnsetStylesEndCbk();
    defrUnsetTechnologyCbk();
    defrUnsetTimingDisableCbk();
    defrUnsetTimingDisablesStartCbk();
    defrUnsetTimingDisablesEndCbk();
    defrUnsetTrackCbk();
    defrUnsetUnitsCbk();
    defrUnsetVersionCbk();
    defrUnsetVersionStrCbk();
    defrUnsetViaCbk();
    defrUnsetViaExtCbk();
    defrUnsetViaStartCbk();
    defrUnsetViaEndCbk();

    defrClear();

    fclose(f);

    return true;
} // createDbByDef



#define CHECK_READ(call, msg)                     \
    do {                                          \
        if (!(call)) {                            \
            std::cerr << (msg) << std::endl;      \
            return false;                         \
        }                                         \
    } while (0)


bool DefReadEdadb::createDbByEdadb(const char* edadb_path) {
#if EDADB_OUTPUT_DEBUG
    std::cout << "DEADB: Def read to EDADB database : " << edadb_path << std::endl;
#endif

    //////// read iEDA Idb classes from edadb database ////////////////////////
    CHECK_READ(readIdbDesign(), "DefReadEdadb::createDbByEdadb failed to read IdbDesign!");
    CHECK_READ(readIdbDie(), "DefReadEdadb::createDbByEdadb failed to read IdbDie!");
    CHECK_READ(readIdbRow(), "DefReadEdadb::createDbByEdadb failed to read IdbRow!");
    CHECK_READ(readIdbTrackGrid(), "DefReadEdadb::createDbByEdadb failed to read readIdbTrackGrid!");
    CHECK_READ(readIdbGCellGrid(), "DefReadEdadb::createDbByEdadb failed to read IdbGCellGrid!");
    CHECK_READ(readIdbVia(), "DefReadEdadb::createDbByEdadb failed to read IdbVia!");
#if 0
    CHECK_READ(readIdbInstance(), "DefReadEdadb::createDbByEdadb failed to read IdbInstance!");
    CHECK_READ(readIdbPin(), "DefReadEdadb::createDbByEdadb failed to read IdbPin!");
    CHECK_READ(readIdbBlockage(), "DefReadEdadb::createDbByEdadb failed to read IdbBlockage!");
    CHECK_READ(readIdbRegion(), "DefReadEdadb::createDbByEdadb failed to read IdbRegion!");
    CHECK_READ(readIdbSlot(), "DefReadEdadb::createDbByEdadb failed to read IdbSlot!");
    CHECK_READ(readIdbGroup(), "DefReadEdadb::createDbByEdadb failed to read IdbGroup!");
    CHECK_READ(readIdbFill(), "DefReadEdadb::createDbByEdadb failed to read IdbFill!");
#endif



//    if (!readSpecialNet()) {
//        std::cerr << "DefReadEdadb::createDbByEdadb failed to read IdbSpecialNet!" << std::endl;
//        return false;
//    }

#if EDADB_OUTPUT_DEBUG
    std::cout << "DefReadEdadb::createDbByEdadb successfully read def data from EDADB database!" << std::endl;
#endif

    return true;
} // createDbByEdadb



bool DefReadEdadb::readIdbDesign() {
    edadb::DbMap<idb::IdbDesign> design_map;
    design_map.init();

    idb::IdbDesign got;
    edadb::DbMapReader<idb::IdbDesign>* rd = nullptr;
    if (edadb::readNext<idb::IdbDesign>(rd, design_map, &got) <= 0) {
        std::cout << "DefReadEdadb::readIdbDesign failed to read!" << std::endl;
        return false;
    } // if 

    IdbDesign* design = _def_service->get_design();
    design->set_design_name(got.get_design_name());
    design->set_version(got.get_version());

    // update the design pointer members
    delete design->get_units();
    design->set_units(got.get_units());
    got.set_units(nullptr);

    delete design->get_bus_bit_chars();
    design->set_bus_bit_chars( got.get_bus_bit_chars() );
    got.set_bus_bit_chars(nullptr);

    return true;
} // readIdbDesign



bool DefReadEdadb::readIdbDie(void) {
    IdbLayout* layout = _def_service->get_layout();
    IdbDie* die = layout->get_die();
    if (die == nullptr) {
        std::cerr << "DefReadEdadb::IdbDie failed, die is nullptr!" << std::endl;
        return false;
    }

    assert(die->get_points().empty());

    edadb::Shadow<idb::IdbDie> die_sd;
    edadb::DbMap< edadb::Shadow<idb::IdbDie> > die_sd_map;
    die_sd_map.init();
    edadb::DbMapReader< edadb::Shadow<idb::IdbDie> >* die_sd_rd = nullptr;
    if ((edadb::readNext<edadb::Shadow<idb::IdbDie>>(die_sd_rd, die_sd_map, &die_sd)) <= 0) {
        std::cout << "DefReadEdadb::readIdbDie failed to read!" << std::endl;
        return false;
    }
    die_sd.fromShadow(die);

    return true;
} // readIdbDie



bool DefReadEdadb::readIdbRow(void) {
    edadb::DbMap<idb::IdbRow> row_map;
    row_map.init();
    if (!row_map.tableExists(row_map.getTableName())) {
        // no row table, return true
        std::cout << "EDADB DefReadEdadb::readIdbRow: no table " << row_map.getTableName() << " exists." << std::endl;
        return true;
    }

    IdbLayout* layout = _def_service->get_layout();  // Lef
    IdbSites* sites = layout->get_sites();
    IdbRows* rows = layout->get_rows();

    int got = 0;
    edadb::DbMapReader<idb::IdbRow>* rd = nullptr;
    while (true) {
        IdbRow *row = new IdbRow();
        got = edadb::readNext<idb::IdbRow>(rd, row_map, row);
        if (got == 0) {
            delete row;
            break;
        }
        else if (got < 0) {
            std::cout << "DefReadEdadb::readIdbRow failed to read!" << std::endl;
            return false;
        }

        row->set_bounding_box();

        // update global sites from reading row
        IdbSite* site = row->get_site();
        std::string site_name = site->get_name();
        IdbSite* lef_site = sites->find_site(site_name);
        if (lef_site == nullptr) {
            // IdbSite in site list only has name 
            lef_site = sites->add_site_list(site_name);
        }

        rows->add_row_list(row);
    } // while 

    return true;
} // readIdbRow



bool DefReadEdadb::readIdbTrackGrid(void) {
    edadb::DbMap< edadb::Shadow<idb::IdbTrackGrid> > track_grid_map;
    track_grid_map.init();

    const std::string table_name = track_grid_map.getTableName();
    if (!edadb::tableExists(table_name)) {
        // no track grid table, return true
        std::cout << "EDADB DefReadEdadb::readIdbTrackGrid: no table " << table_name << " exists." << std::endl;
        return true;
    }

    IdbLayout* layout = _def_service->get_layout();  
    IdbLayers* layers = layout->get_layers();
    IdbTrackGridList* track_grid_list = layout->get_track_grid_list();

    int got = 0;
    edadb::DbMapReader< edadb::Shadow<idb::IdbTrackGrid> >* rd = nullptr;
    while (true) {
        edadb::Shadow<idb::IdbTrackGrid> track_grid_sd;
        got = edadb::readNext< edadb::Shadow<idb::IdbTrackGrid> >(rd, track_grid_map, &track_grid_sd);
        if (got == 0) {
            break;
        }
        else if (got < 0) {
            std::cout << "DefReadEdadb::readIdbTrackGrid failed to read!" << std::endl;
            return false;
        }
        IdbTrackGrid* track_grid = track_grid_list->add_track_grid(nullptr);
        track_grid_sd.fromShadow(track_grid);

        for ( auto& layer_name_sd : track_grid_sd._layer_name_vec_sd ) {
            IdbLayer* layer = layers->find_layer( layer_name_sd.str );
            if ( layer != nullptr ) {
                track_grid->add_layer_list( layer );
                if (layer->is_routing()) {
                    IdbLayerRouting* routing_layer = dynamic_cast<IdbLayerRouting*>(layer);
                    routing_layer->add_track_grid(track_grid);
                }
            }
            else {
                std::cerr << "DefReadEdadb::readIdbTrackGrid failed to find layer: " << layer_name_sd.str << std::endl;
            }
        } // for layer names
    } // while

    return true;
} // readIdbTrackGrid



bool DefReadEdadb::readIdbGCellGrid(void) {
    edadb::DbMap<idb::IdbGCellGrid> gcell_grid_map;
    gcell_grid_map.init();

    // check if gcell grid table exists
    const std::string table_name = gcell_grid_map.getTableName();
    if (!edadb::tableExists(table_name)) {
        // no gcell grid table, return true
        // TODO: output when debug
        std::cout << "EDADB DefReadEdadb::readIdbGCellGrid: no table " << table_name << " exists." << std::endl;
        return true;
    }

    IdbLayout* layout = _def_service->get_layout();  // Lefri
    IdbGCellGridList* gcell_grid_list = layout->get_gcell_grid_list();

    int got = 0;
    edadb::DbMapReader<idb::IdbGCellGrid>* rd = nullptr;
    while (true) {
        IdbGCellGrid* gcell_grid = new IdbGCellGrid();
        got = edadb::readNext<idb::IdbGCellGrid>(rd, gcell_grid_map, gcell_grid);
        if (got == 0) {
            delete gcell_grid;
            break;
        }
        else if (got < 0) {
            std::cout << "DefReadEdadb::readIdbGCellGrid failed to read!" << std::endl;
            return false;
        }
        gcell_grid_list->add_gcell_grid(gcell_grid);
    } // while

    return true;
} // readIdbGCellGrid



bool DefReadEdadb::readIdbVia(void) {
    idb::IdbDefService* idb_def_service = edadb::EdadbIdbHelper::getIdbDefService();
    if (idb_def_service == nullptr) {
        edadb::EdadbIdbHelper::setIdbDefService(_def_service);
    }
    else if (edadb::EdadbIdbHelper::getIdbDefService() != _def_service) {
        std::cerr << "DefReadEdadb::readIdbVia failed, IdbDefService not consistent!" << std::endl;
        return false;
    }

    IdbDesign* design = _def_service->get_design();  // Def
    IdbLayout* layout = _def_service->get_layout();  // Lef
    IdbViaRuleList* _rule_list = layout->get_via_rule_list();
    IdbLayers* layer_list = layout->get_layers();

    IdbVias* via_list = design->get_via_list();

    edadb::DbMap<idb::IdbVia> via_map;
    via_map.init();

    int got = 0;
    edadb::DbMapReader<idb::IdbVia>* inst_rd = nullptr;
    while (true) {
        IdbVia* via_inst = new IdbVia();
        got = edadb::readNext<idb::IdbVia>(inst_rd, via_map, via_inst);
        if (got < 0) {
            std::cout << "DefReadEdadb::readIdbVia failed to read!" << std::endl;
            return false;
        }
        else if (got == 0) {
            break;
        }

        via_list->add_via(via_inst);

        //  get layer and pattern names from idb::IdbViaMasterGenerate shadow

        IdbViaMaster* master_instance = via_inst->get_instance();
        if (master_instance->get_type() == idb::IdbViaMaster::IdbViaMasterType::kViaRule)
        {
            IdbViaMasterGenerate* master_generate = master_instance->get_master_generate();
            
            const std::string rule_name = master_generate->get_rule_name();
            IdbViaRuleGenerate* via_rule = _rule_list->find_via_rule_generate(rule_name);
            assert(via_rule != nullptr);
            master_generate->set_rule_generate(via_rule);

            // build core cut shape
            vector<IdbRect*> cut_rect_list = master_generate->get_cut_rect_list();
            int32_t num_rows = master_generate->get_cut_rows();
            int32_t num_cols = master_generate->get_cut_cols();
            int32_t cutsize_x = master_generate->get_cut_size_x();
            int32_t cutsize_y = master_generate->get_cut_size_y();
            int32_t cut_spacing_x = master_generate->get_cut_spcing_x();
            int32_t cut_spacing_y = master_generate->get_cut_spcing_y();
            int32_t original_offset_x = master_generate->get_original_offset_x();
            int32_t original_offset_y = master_generate->get_original_offset_y();
              
            int32_t cut_width_total = num_cols * cutsize_x + (num_cols - 1) * cut_spacing_x;
            int32_t cut_height_total = num_rows * cutsize_y + (num_rows - 1) * cut_spacing_y;
              
            // copy from 
            //  src/database/manager/builder/def_builder/def_read.cpp @ 1875
            int32_t ll_x_min = (-cut_width_total / 2) + original_offset_x;
            int32_t ll_y_min = (-cut_height_total / 2) + original_offset_y;
            for (int32_t i = 0; i < num_rows; ++i) {
              for (int32_t j = 0; j < num_cols; j++) {
                /// if pattern exist, cut shape must o
                if (nullptr != master_generate->get_patttern() && !master_generate->is_pattern_cut_exist(i, j)) {
                  continue;
                }
                int32_t ll_x = ll_x_min + j * (cutsize_x + cut_spacing_x);
                int32_t ll_y = ll_y_min + i * (cutsize_y + cut_spacing_y);
                int32_t ur_x = ll_x + cutsize_x;
                int32_t ur_y = ll_y + cutsize_y;
                master_generate->add_cut_rect(ll_x, ll_y, ur_x, ur_y);
              }
            }

            master_generate->set_cut_bouding_rect(ll_x_min, ll_y_min, ll_x_min + cut_width_total, ll_y_min + cut_height_total);

            master_instance->set_via_shape();
        }
        else
        {
            master_instance->set_type_fixed();
        } // if IdbViaMasterGenerate or Shadow<idb::IdbViaMasterFixed>
    } // while

    if (got < 0) {
        std::cout << "DefReadEdadb::readIdbVia failed to read!" << std::endl;
        return false;
    }

    return true;
} // readIdbVia



//--bool DefReadEdadb::readIdbInstance(void) {
//--    IdbDesign* design = _def_service->get_design();  // Def
//--    IdbLayout* layout = _def_service->get_layout();  // Lef
//--    IdbLayers* layer_list = layout->get_layers();
//--    IdbRegionList* region_list = design->get_region_list();
//--    IdbInstanceList* instance_list = design->get_instance_list();
//--    IdbCellMasterList* master_list = layout->get_cell_master_list();
//--
//--    edadb::DbMap< edadb::Shadow<idb::IdbInstance> > inst_map;
//--    inst_map.init();
//--
//--#if EDADB_OUTPUT_DEBUG
//--    std::cout << "[DefReadEdadb::readComponent] Start to read IdbInstance from EDADB..." << std::endl;
//--#endif
//--
//--    int got = 0;
//--    edadb::DbMapReader< edadb::Shadow<idb::IdbInstance>>* rd_sd = nullptr;
//--    while (true) {
//--        edadb::Shadow<idb::IdbInstance> inst_sd;
//--        got = edadb::readNext<edadb::Shadow<idb::IdbInstance>>(rd_sd, inst_map, &inst_sd);
//--        if (got < 0) {
//--            std::cout << "DefReadEdadb::readComponent failed to read!" << std::endl;
//--            return false;
//--        }
//--        else if (got == 0) {
//--            break;
//--        }
//--
//--
//--        // create IdbInstance instance from shadow and add to instance_list
//--        IdbInstance* inst = new IdbInstance();
//--
//--         if (nullptr == _cur_cell_master || _cur_cell_master->get_name() != inst_sd._cell_master_name_sd) {
//--            _cur_cell_master = master_list->find_cell_master(inst_sd._cell_master_name_sd);
//--        }
//--        if (nullptr == _cur_cell_master) {
//--            std::cerr << "DefReadEdadb::readComponent failed to find cell master: " << inst_sd._cell_master_name_sd << std::endl;
//--            return false;
//--        }
//--        inst->set_cell_master(_cur_cell_master);
//--
//--        if (!inst_sd._region_name_sd.empty()) {
//--            IdbRegion* region = region_list->find_region(inst_sd._region_name_sd);
//--            if (region != nullptr) {
//--                inst->set_region(region);
//--                region->add_instance(inst);
//--            }   
//--        }
//--
//--        if (inst_sd._route_halo_sd != nullptr) {
//--            idb::IdbRouteHalo* route_halo = inst->set_route_halo(nullptr);
//--            inst_sd._route_halo_sd->fromShadow( route_halo );
//--
//--            route_halo->set_layer_bottom(
//--                dynamic_cast<IdbLayerRouting*>(
//--                    layer_list->find_layer(
//--                        inst_sd._route_halo_sd->_layer_bottom_name_sd))
//--            );
//--            route_halo->set_layer_top(
//--                dynamic_cast<IdbLayerRouting*>(
//--                    layer_list->find_layer(
//--                        inst_sd._route_halo_sd->_layer_top_name_sd))
//--            );  
//--        } // if 
//--
//--        inst_sd.fromShadow(inst);
//--        instance_list->add_instance(inst);
//--
//--        if (instance_list->get_num() % 1000 == 0) {
//--            std::cout << "-" << std::flush;
//--            if (instance_list->get_num() % 100000 == 0) {
//--                std::cout << std::endl;
//--            }
//--        }
//--    } // while 
//--
//--    return true;
//--} // readIdbInstance
//--
//--
//--
//--bool DefReadEdadb::readIdbPin(void) {
//--    edadb::DbMap<edadb::Shadow<idb::IdbPin>> pin_map;
//--    pin_map.init();
//--
//--    const std::string table_name = pin_map.getTableName();
//--    if (!edadb::tableExists(table_name)) {
//--        // no pin table, return true
//--        std::cout << "EDADB DefReadEdadb::readIdbPin: no table " << table_name << " exists." << std::endl;
//--        return false;
//--    }
//--
//--
//--    IdbDesign* design = _def_service->get_design();  // Def
//--    IdbLayout* layout = _def_service->get_layout();  // Lef
//--    IdbLayers* layer_list = layout->get_layers();
//--    // IdbNetList* net_list = design->get_net_list();
//--    IdbPins* pin_list = design->get_io_pin_list();
//--
//--    int got = 0;
//--    edadb::DbMapReader<edadb::Shadow<idb::IdbPin>>* rd_sd = nullptr;
//--    while (true) {
//--        edadb::Shadow<idb::IdbPin> pin_sd;
//--        got = edadb::readNext<edadb::Shadow<idb::IdbPin>>(rd_sd, pin_map, &pin_sd);
//--        if (got < 0) {
//--            std::cout << "DefReadEdadb::readIdbPin failed to read!" << std::endl;
//--            return false;
//--        }
//--        else if (got == 0) {
//--            break;
//--        }
//--
//--        edadb::Shadow<idb::IdbTerm>* term_sd = pin_sd._io_term_sd;
//--        assert(term_sd != nullptr);
//--
//--        // create IdbPin instance from shadow and add to pin_list
//--        IdbPin* pin = pin_list->add_pin_list(nullptr);
//--        pin_sd.fromShadow(pin);
//--
//--        // created by pin_sd.fromShadow
//--        idb::IdbTerm *term = pin->get_term(); 
//--
//--        if (term_sd->_has_port_sd) {
//--            for (edadb::Shadow<idb::IdbPort>* port_sd : term_sd->_port_list_sd) {
//--                IdbPort* port = term->add_port(nullptr);
//--                port_sd->fromShadow(port);
//--
//--                // IdbLayerShape
//--                for (auto& layer_shape_sd : port_sd->_layer_shape_list_sd) {
//--                    IdbLayer* layer = layer_list->find_layer(layer_shape_sd->_layer_name_sd);
//--                    if (layer == nullptr) {
//--                        std::cerr << "DefReadEdadb::readIdbPin failed to find layer: " << layer_shape_sd->_layer_name_sd << std::endl;
//--                        continue;
//--                    }
//--                    IdbLayerShape* layer_shape = port->add_layer_shape();
//--                    layer_shape_sd->fromShadow(layer_shape);
//--                    layer_shape->set_layer(layer);
//--                } // for layer shapes
//--
//--                if (!port->get_layer_shape().empty())
//--                    port->set_io_bounding_box();
//--            } // for ports
//--
//--            pin->set_port_layer_shape();
//--        } else {
//--            int32_t bounding_box_ll_x = INT_MAX;
//--            int32_t bounding_box_ll_y = INT_MAX;
//--            int32_t bounding_box_ur_x = INT_MIN;
//--            int32_t bounding_box_ur_y = INT_MIN;
//--
//--            uint32_t layer_num = pin_sd._layer_num_sd;
//--            if (layer_num > 0) {
//--                IdbPort* port = term->add_port(nullptr);
//--
//--                int32_t coordinate_x = 0;
//--                int32_t coordinate_y = 0;
//--                for (uint32_t i = 0; i < layer_num; ++i) {
//--                    edadb::Shadow<idb::IdbPort>* port_sd = term_sd->_port_list_sd.at(i);
//--                    assert(port_sd->_layer_shape_list_sd.size() == 1);
//--                    edadb::Shadow<idb::IdbLayerShape>* layer_shape_sd = 
//--                            port_sd->_layer_shape_list_sd.at(0);
//--                    IdbLayerShape* layer_shape = port->add_layer_shape();
//--                    layer_shape_sd->fromShadow(layer_shape);
//--
//--                    IdbLayer* layer = layer_list->find_layer(layer_shape_sd->_layer_name_sd);
//--                    if (layer == nullptr) {
//--                        std::cerr << "DefReadEdadb::readIdbPin failed to find layer: " << layer_shape_sd->_layer_name_sd << std::endl;
//--                        continue;
//--                    }
//--                    layer_shape->set_layer(layer);
//--
//--                    // src/database/manager/builder/def_builder/def_read.cpp
//--                    // `shape->add_rect(ll_x, ll_y, ur_x, ur_y);`
//--                    assert(layer_shape->get_rect_list().size() == 1);
//--                    idb::IdbRect* rect = layer_shape->get_rect_list().at(0);
//--                    bounding_box_ll_x = std::min(bounding_box_ll_x, rect->get_low_x());
//--                    bounding_box_ll_y = std::min(bounding_box_ll_y, rect->get_low_y());
//--                    bounding_box_ur_x = std::max(bounding_box_ur_x, rect->get_high_x());
//--                    bounding_box_ur_y = std::max(bounding_box_ur_y, rect->get_high_y());
//--
//--                    int32_t mid_x = (rect->get_low_x() + rect->get_high_x()) / 2;
//--                    int32_t mid_y = (rect->get_low_y() + rect->get_high_y()) / 2;
//--                    coordinate_x += mid_x;
//--                    coordinate_y += mid_y;
//--                } // for layer shapes
//--
//--                if (layer_num > 0) {
//--                    term->set_average_position(
//--                        coordinate_x / (layer_num * 2),
//--                        coordinate_y / (layer_num * 2));
//--                    term->set_bounding_box(
//--                        bounding_box_ll_x, bounding_box_ll_y,
//--                        bounding_box_ur_x, bounding_box_ur_y);
//--                } else {
//--                    return kDbSuccess;
//--                }
//--
//--                // set pin bounding box
//--                pin->set_bounding_box();
//--            } // if 
//--        } // if _has_port
//--    } // while
//--
//--    return true;
//--} // readIdbPin
//--
//--
//--
//--bool DefReadEdadb::readIdbBlockage(void) {
//--    edadb::DbMap<edadb::Shadow<idb::IdbBlockage>> blockage_map;
//--    blockage_map.init();
//--
//--    // check if blockage table exists
//--    const std::string table_name = blockage_map.getTableName();
//--    if (!edadb::tableExists(table_name)) {
//--        // no blockage table, return true
//--        std::cout << "EDADB DefReadEdadb::readIdbBlockage: no table " << table_name << " exists." << std::endl;
//--        return true;
//--    }
//--
//--
//--    IdbDesign* design = _def_service->get_design();  // Def
//--    IdbBlockageList* blockage_list = design->get_blockage_list();
//--    IdbInstanceList* instance_list = design->get_instance_list();
//--    IdbLayout* layout = _def_service->get_layout();  // Lef
//--    IdbLayers* layer_list = layout->get_layers();
//--
//--    int got = 0;
//--    edadb::DbMapReader<edadb::Shadow<idb::IdbBlockage>>* rd_sd = nullptr;
//--    while (true) {
//--        edadb::Shadow<idb::IdbBlockage> blockage_sd;
//--        got = edadb::readNext<edadb::Shadow<idb::IdbBlockage>>(rd_sd, blockage_map, &blockage_sd);
//--        if (got < 0) {
//--            std::cout << "DefReadEdadb::readIdbBlockage failed to read!" << std::endl;
//--            return false;
//--        }
//--        else if (got == 0) {
//--            break;
//--        }
//--
//--        if (blockage_sd._type_sd == idb::IdbBlockage::IdbBlockageType::kRoutingBlockage) {
//--            IdbRoutingBlockage* routing_blockage = blockage_list->add_blockage_routing(blockage_sd._layer_name_sd);
//--            blockage_sd.fromShadow(routing_blockage);
//--
//--            routing_blockage->set_layer(layer_list->find_layer(blockage_sd._layer_name_sd));
//--
//--            if (!blockage_sd._instance_name_sd.empty()) {
//--                IdbInstance* inst = instance_list->find_instance(blockage_sd._instance_name_sd);
//--                if (inst == nullptr) {
//--                    std::cerr << "DefReadEdadb::readIdbBlockage failed to find instance: " << blockage_sd._instance_name_sd << std::endl;
//--                    continue;
//--                }
//--            }
//--
//--            for (auto& rect : blockage_sd._rect_list_sd) {
//--                routing_blockage->add_rect(
//--                    rect.get_low_x (), rect.get_low_y (),
//--                    rect.get_high_x(), rect.get_high_y());
//--            } // for 
//--        }
//--        else {
//--            // placement blockage
//--            IdbPlacementBlockage* placement_blockage = blockage_list->add_blockage_placement();
//--            blockage_sd.fromShadow(placement_blockage);
//--
//--            if (!blockage_sd._instance_name_sd.empty()) {
//--                IdbInstance* inst = instance_list->find_instance(blockage_sd._instance_name_sd);
//--                if (inst == nullptr) {
//--                    std::cerr << "DefReadEdadb::readIdbBlockage failed to find instance: " << blockage_sd._instance_name_sd << std::endl;
//--                    continue;
//--                }
//--            }
//--
//--            for (auto& rect : blockage_sd._rect_list_sd) {
//--                placement_blockage->add_rect(
//--                    rect.get_low_x (), rect.get_low_y (),
//--                    rect.get_high_x(), rect.get_high_y());
//--            } // for
//--        } // if instance or global
//--    } // while
//--
//--    return true;
//--} // readIdbBlockage
//--
//--
//--
//--bool DefReadEdadb::readIdbRegion(void) {
//--    edadb::DbMap<idb::IdbRegion> region_map;
//--    region_map.init();
//--
//--    // check if region table exists
//--    const std::string table_name = region_map.getTableName();
//--    if (!edadb::tableExists(table_name)) {
//--        // no region table, return true
//--        std::cout << "EDADB DefReadEdadb::readIdbRegion: no table " << table_name << " exists." << std::endl;
//--        return true;
//--    }
//--
//--    IdbDesign* design = _def_service->get_design();  // def
//--    IdbRegionList* region_list = design->get_region_list();
//--    
//--    int got = 0;
//--    edadb::DbMapReader<idb::IdbRegion>* rd = nullptr;
//--    while (true) {
//--        idb::IdbRegion* def_region = new idb::IdbRegion();
//--        got = edadb::readNext<idb::IdbRegion>(rd, region_map, def_region);
//--        if (got == 0) {
//--            delete def_region;
//--            break;
//--        }
//--        else if (got < 0) {
//--            std::cout << "DefReadEdadb::readIdbRegion failed to read!" << std::endl;
//--            return false;
//--        }
//--        region_list->add_region(def_region);
//--    } // while
//--
//--    return true;
//--} // readIdbRegion
//--
//--
//--
//--bool DefReadEdadb::readIdbSlot(void) {
//--    edadb::DbMap<idb::IdbSlot> slot_map;
//--    slot_map.init();
//--
//--    // check if slot table exists
//--    const std::string table_name = slot_map.getTableName();
//--    if (!edadb::tableExists(table_name)) {
//--        // no slot table, return true
//--        std::cout << "EDADB DefReadEdadb::readIdbSlot: no table " << table_name << " exists." << std::endl;
//--        return true;
//--    }
//--
//--    int got = 0;
//--    edadb::DbMapReader<idb::IdbSlot>* rd = nullptr;
//--
//--    IdbDesign* design = _def_service->get_design();  // def
//--    IdbSlotList* slot_list = design->get_slot_list();
//--    while (true) {
//--        IdbSlot* slot = new IdbSlot();
//--        got = edadb::readNext<idb::IdbSlot>(rd, slot_map, slot);
//--        if (got == 0) {
//--            delete slot;
//--            break;
//--        }
//--        else if (got < 0) {
//--            std::cout << "DefReadEdadb::readIdbSlot failed to read!" << std::endl;
//--            return false;
//--        }
//--
//--        slot_list->_slot_list.emplace_back(slot);
//--        slot_list->_num++;
//--    }
//--    return true;
//--} // readIdbSlot
//--
//--
//--
//--bool DefReadEdadb::readIdbGroup(void) {
//--    edadb::DbMap<edadb::Shadow<idb::IdbGroup>> group_map;
//--    group_map.init();
//--
//--    // check if group table exists
//--    const std::string table_name = group_map.getTableName();
//--    if (!edadb::tableExists(table_name)) {
//--        // no group table, return true
//--        std::cout << "EDADB DefReadEdadb::readIdbGroup: no table " << table_name << " exists." << std::endl;
//--        return true;
//--    }
//--
//--    IdbDesign* design = _def_service->get_design();  // def
//--    IdbRegionList* region_list = design->get_region_list();
//--//    IdbInstanceList* instance_list = design->get_instance_list();
//--    IdbGroupList* group_list = design->get_group_list();
//--
//--    int got = 0;
//--    edadb::DbMapReader<edadb::Shadow<idb::IdbGroup>>* rd_sd = nullptr;
//--    while (true) {
//--        edadb::Shadow<idb::IdbGroup> group_sd;
//--        got = edadb::readNext<edadb::Shadow<idb::IdbGroup>>(rd_sd, group_map, &group_sd);
//--        if (got < 0) {
//--            std::cout << "DefReadEdadb::readIdbGroup failed to read!" << std::endl;
//--            return false;
//--        }
//--        else if (got == 0) {
//--            break;
//--        }
//--
//--        // create IdbGroup instance from shadow and add to group_list
//--        IdbGroup* group = group_list->add_group(group_sd._group_name_sd);
//--        group_sd.fromShadow(group);
//--
//--        group->set_region(region_list->find_region(group_sd._region_name_sd));
//--    } // while
//--
//--    return true;
//--} // readIdbGroup
//--
//--
//--bool DefReadEdadb::readIdbFill(void) {
//--    edadb::DbMap< edadb::Shadow<idb::IdbFill> > fill_map;
//--    fill_map.init();
//--
//--    // check if group table exists
//--    const std::string table_name = fill_map.getTableName();
//--    if (!edadb::tableExists(table_name)) {
//--        // no group table, return true
//--        std::cout << "EDADB DefReadEdadb::readIdbFill: no table " << table_name << " exists." << std::endl;
//--        return true;
//--    }
//--
//--
//--    IdbDesign* design = _def_service->get_design();  // def
//--    IdbLayout* layout = _def_service->get_layout();  // lef
//--    IdbLayers* layer_list = layout->get_layers();
//--    IdbFillList* fill_list = design->get_fill_list();
//--    
//--    int got = 0;
//--    edadb::DbMapReader< edadb::Shadow<idb::IdbFill> >* rd_sd = nullptr;
//--    while (true) {
//--        edadb::Shadow<idb::IdbFill> fill_sd;
//--        got = edadb::readNext<edadb::Shadow<idb::IdbFill>>(rd_sd, fill_map, &fill_sd);
//--        if (got < 0) {
//--            std::cout << "DefReadEdadb::readIdbFill failed to read!" << std::endl;
//--            return false;
//--        }
//--        else if (got == 0) {
//--            break;
//--        }
//--
//--        if (fill_sd._layer_sd != nullptr) {
//--            IdbLayer* layer = layer_list->find_layer(fill_sd._layer_sd->_layer_name_sd);
//--            if (layer == nullptr) {
//--                std::cerr << "DefReadEdadb::readIdbFill failed to find layer: " << fill_sd._layer_sd->_layer_name_sd << std::endl;
//--                continue;
//--            }
//--
//--            IdbFillLayer* fill_layer = fill_list->add_fill_layer(layer);
//--            fill_sd._layer_sd->fromShadow(fill_layer);
//--        }
//--
//--        if (fill_sd._via_sd != nullptr) {
//--            IdbVias* via_list_def = design->get_via_list();
//--            IdbVia* via = via_list_def->find_via(fill_sd._via_sd->_via_name_sd);
//--            if (via == nullptr) {
//--                IdbVias* via_list_lef = layout->get_via_list();
//--                via = via_list_lef->find_via(fill_sd._via_sd->_via_name_sd);
//--            }
//--
//--            IdbVia* via_new = via->clone();
//--            IdbFillVia* fill_via = fill_list->add_fill_via(via_new);
//--            fill_sd._via_sd->fromShadow(fill_via);
//--        }
//--    } // while
//--    
//--    return kDbSuccess;
//--} // readIdbFill



#if 0
bool DefReadEdadb::readSpecialNet(void) {
    IdbDesign* design = _def_service->get_design();  // Def
    IdbPins* io_pin_list = design->get_io_pin_list();
    IdbInstanceList* instance_list = design->get_instance_list();
    IdbSpecialNetList* net_list = design->get_special_net_list();

    using SpecialNetShadw = edadb::Shadow<idb::IdbSpecialNet>;
    edadb::DbMap<SpecialNetShadw> special_net_map;
    special_net_map.init();

    int got = 0;
    edadb::DbMapReader<SpecialNetShadw>* rd_sd = nullptr;
    while (true) {
        SpecialNetShadw special_net_sd;
        got = edadb::readNext<SpecialNetShadw>(rd_sd, special_net_map, &special_net_sd);
        if (got < 0) {
            std::cout << "DefReadEdadb::readSpecialNet failed to read!" << std::endl;
            return false;
        }
        else if (got == 0) {
            break;
        }

        // create IdbSpecialNet instance from shadow and add to net_list
        IdbSpecialNet* special_net_instance = new IdbSpecialNet();
        special_net_sd.fromShadow(special_net_instance, io_pin_list, instance_list);
        net_list->add_net(special_net_instance);
    } // while

    return true;
} // readSpecialNet
#endif 

} // namespace idb
