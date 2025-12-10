/**
 * @file def_read_edadb.h
 * @author Zhiyi Wang (zhiyiwang@ict.ac.cn)
*/

#include "def_read_edadb.h"


namespace idb {

DefReadEdadb::DefReadEdadb(IdbDefService* def_service) : DefRead(def_service)
{
}


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

    if (!createDbByDef(path)) {
        std::cerr << "Error: DefReadEdadb::createDbFromEdadb failed!" << std::endl; 
        return false;
    }

    if (!createDbByEdadb(edadb_path)) {
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

//--    defrSetVersionStrCbk(versionCallback);
//--    defrSetDesignCbk(designCallback);
//--    defrSetBusBitCbk(busBitCharsCallBack);
//--    defrSetUnitsCbk(unitsCallback);
//--    defrSetDieAreaCbk(dieAreaCallback);
//--    defrSetGcellGridCbk(gcellGridCallback);

//--    defrSetViaStartCbk(viaBeginCallback);
//--    defrSetViaCbk(viaCallback);

    defrSetSNetStartCbk(specialNetBeginCallback);
    defrSetSNetCbk(specialNetCallback);
    defrSetSNetEndCbk(specialNetEndCallback);

    defrSetBlockageCbk(blockageCallback);
    defrSetComponentCbk(componentsCallback);
    defrSetComponentStartCbk(componentNumberCallback);
    defrSetComponentEndCbk(componentEndCallback);
    defrSetFillStartCbk(fillsCallback);
    defrSetFillCbk(fillCallback);
    defrSetGroupCbk(groupCallback);
    defrSetNetStartCbk(netBeginCallback);
    defrSetNetCbk(netCallback);
    defrSetNetEndCbk(netEndCallback);
    defrSetPinCbk(pinCallback);
    defrSetPinEndCbk(pinsEndCallback);
    defrSetStartPinsCbk(pinsBeginCallback);
    defrSetRegionCbk(regionCallback);
    defrSetRowCbk(rowCallback);
    defrSetSlotCbk(slotsCallback);
    defrSetAddPathToNet();
    defrSetTrackCbk(trackGridCallback);
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



bool DefReadEdadb::createDbByEdadb(const char* edadb_path) {
#if EDADB_OUTPUT_DEBUG
    std::cout << "DEADB: Def read to EDADB database : " << edadb_path << std::endl;
#endif

    //////// read iEDA Idb classes from edadb database ////////////////////////

    if (!readIdbDesign()) {
        std::cerr << "DefReadEdadb::createDbByEdadb failed to read IdbDesign!" << std::endl;
        return false;
    }

    if (!readIdbDie()) {
        std::cerr << "DefReadEdadb::createDbByEdadb failed to read IdbDie!" << std::endl;
        return false;
    }

    if (!readIdbGCellGridList()) {
        std::cerr << "DefReadEdadb::createDbByEdadb failed to read IdbGCellGridList!" << std::endl;
        return false;
    }

    if (!readIdbVia()) {
        std::cerr << "DefReadEdadb::createDbByEdadb failed to read IdbVia!" << std::endl;
        return false;
    }

//    if (!readSpecialNet()) {
//        std::cerr << "DefReadEdadb::createDbByEdadb failed to read IdbSpecialNet!" << std::endl;
//        return false;
//    }

    return true;
} // createDbByEdadb



bool DefReadEdadb::readIdbDesign() {
    edadb::DbMap<idb::IdbDesign> design_map;
    design_map.init();

    idb::IdbDesign got;
    edadb::DbMapReader<idb::IdbDesign>* rd = nullptr;
    if (edadb::read2Scan<idb::IdbDesign>(rd, design_map, &got) <= 0) {
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
    if ((edadb::read2Scan<edadb::Shadow<idb::IdbDie>>(die_sd_rd, die_sd_map, &die_sd)) <= 0) {
        std::cout << "DefReadEdadb::readIdbDie failed to read!" << std::endl;
        return false;
    }
    die_sd.fromShadow(die);

    return true;
} // readIdbDie



bool DefReadEdadb::readIdbGCellGridList(void) {
    edadb::DbMap<idb::IdbGCellGrid> gcell_grid_map;
    gcell_grid_map.init();

    // check if gcell grid table exists
    const std::string table_name = gcell_grid_map.getTableName();
    if (!edadb::tableExists(table_name)) {
        // no gcell grid table, return true
        // TODO: output when debug
        std::cout << "EDADB DefReadEdadb::readIdbGCellGridList: no table " << table_name << " exists." << std::endl;
        return true;
    }

    IdbLayout* layout = _def_service->get_layout();  // Lef
    IdbGCellGridList* gcell_grid_list = layout->get_gcell_grid_list();

    int got = 0;
    edadb::DbMapReader<idb::IdbGCellGrid>* rd = nullptr;
    while (true) {
        IdbGCellGrid* gcell_grid = new IdbGCellGrid();
        got = edadb::read2Scan<idb::IdbGCellGrid>(rd, gcell_grid_map, gcell_grid);
        if (got == 0) {
            delete gcell_grid;
            break;
        }
        else if (got < 0) {
            std::cout << "DefReadEdadb::readIdbGCellGridList failed to read!" << std::endl;
            return false;
        }
        gcell_grid_list->add_gcell_grid(gcell_grid);
    } // while

    if (got < 0) {
        std::cout << "DefReadEdadb::readIdbGCellGridList failed to read!" << std::endl;
        return false;
    }

    return true;
} // readIdbGCellGridList


bool DefReadEdadb::readIdbVia(void) {
    IdbDesign* design = _def_service->get_design();  // Def
    IdbLayout* layout = _def_service->get_layout();  // Lef
    IdbViaRuleList* _rule_list = layout->get_via_rule_list();
    IdbLayers* layer_list = layout->get_layers();

    IdbVias* via_list = design->get_via_list();

    edadb::DbMap< edadb::Shadow<idb::IdbVia> > via_map;
    via_map.init();

    int got = 0;
    edadb::DbMapReader< edadb::Shadow<idb::IdbVia> >* rd_sd = nullptr;
    while (true) {
        edadb::Shadow<idb::IdbVia> via_sd;
        got = edadb::read2Scan<edadb::Shadow<idb::IdbVia>>(rd_sd, via_map, &via_sd);
        if (got < 0) {
            std::cout << "DefReadEdadb::readIdbVia failed to read!" << std::endl;
            return false;
        }
        else if (got == 0) {
            break;
        }

        // create IdbVia instance from shadow and add to via_list
        IdbVia* via_instance = new IdbVia();
        via_sd.fromShadow(via_instance);
        via_list->add_via(via_instance);

        //  get layer and pattern names from idb::IdbViaMasterGenerate shadow
        edadb::Shadow<idb::IdbViaMaster> &mst_sd = via_sd._master_instance_sd;
        edadb::Shadow<idb::IdbViaMasterGenerate>
            &mst_gen_sd = via_sd._master_generate_sd;
        IdbViaMaster* master_instance = via_instance->get_instance();
        if (mst_sd._type_sd == idb::IdbViaMaster::IdbViaMasterType::kViaRule)
        {
            IdbViaMasterGenerate* master_generate = master_instance->get_master_generate();
            
            IdbViaRuleGenerate* via_rule =
                _rule_list->find_via_rule_generate(mst_gen_sd._rule_name_sd);
            master_generate->set_rule_generate(via_rule);

            IdbLayer* layer_bottom =
                layer_list->find_layer(mst_gen_sd._layer_bottom_name_sd.c_str());
            master_generate->set_layer_bottom(dynamic_cast<IdbLayerRouting*>(layer_bottom));

            IdbLayerCut* layer_cut = dynamic_cast<IdbLayerCut*>(
                layer_list->find_layer(mst_gen_sd._layer_cut_name_sd.c_str()));
            layer_cut->set_via_rule(via_rule);
            master_generate->set_layer_cut(layer_cut);

            IdbLayer* layer_top = layer_list->find_layer(mst_gen_sd._layer_top_name_sd);
            master_generate->set_layer_top(dynamic_cast<IdbLayerRouting*>(layer_top));

            if (!mst_gen_sd._pattern_name_sd.empty()) {
                master_generate->set_patttern(mst_gen_sd._pattern_name_sd);
            }


            // build core cut shape
            vector<IdbRect*> cut_rect_list = master_generate->get_cut_rect_list();
            int32_t num_rows = mst_gen_sd._num_cut_rows_sd;;
            int32_t num_cols = mst_gen_sd._num_cut_cols_sd;
            int32_t cutsize_x = mst_gen_sd._cut_size_x_sd;
            int32_t cutsize_y = mst_gen_sd._cut_size_y_sd;
            int32_t cut_spacing_x = mst_gen_sd._cut_spacing_x_sd;
            int32_t cut_spacing_y = mst_gen_sd._cut_spacing_y_sd;
            int32_t original_offset_x = mst_gen_sd._original_offset_x_sd;
            int32_t original_offset_y = mst_gen_sd._original_offset_y_sd;
              
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
            // Fixed via
            int32_t min_x = INT_MAX;
            int32_t min_y = INT_MAX;
            int32_t max_x = INT_MIN;
            int32_t max_y = INT_MIN;
            
            // Shadow<idb::IdbLayerShape>* in Shadow<idb::IdbViaMasterFixed> int std::vector<> 
            auto& fixed_layer_shapes = via_sd._master_instance_sd.fixed_layer_shape_list_sd;
            for (auto& fls : fixed_layer_shapes) {
                std::string& layer_name = fls->_layer_name_sd;
                IdbViaMasterFixed* master_fixed = master_instance->add_fixed(layer_name);
                IdbLayer* layer = layer_list->find_layer(layer_name);
                if (layer == nullptr) {
                  return kDbFail;
                }
                master_fixed->set_layer(layer);

                std::vector<idb::IdbRect*> &rect_list = fls->_rect_list_sd;
                assert(rect_list.size() == 1);
                idb::IdbRect* rect = rect_list.at(0);
                int32_t ll_x = rect->get_low_x();
                int32_t ll_y = rect->get_low_y();
                int32_t ur_x = rect->get_high_x();
                int32_t ur_y = rect->get_high_y();
                master_fixed->add_rect(ll_x, ll_y, ur_x, ur_y);

                // record the core area of cut
                if (layer->get_type() == IdbLayerType::kLayerCut) {
                  min_x = std::min(min_x, ll_x);
                  min_y = std::min(min_y, ll_y);
                  max_x = std::max(max_x, ur_x);
                  max_y = std::max(max_y, ur_y);
                }

                master_instance->set_cut_rect(min_x, min_y, max_x, max_y);
                master_instance->set_via_shape();
            } // for 
        } // if IdbViaMasterGenerate or Shadow<idb::IdbViaMasterFixed>
    } // while

    if (got < 0) {
        std::cout << "DefReadEdadb::readIdbVia failed to read!" << std::endl;
        return false;
    }

    return true;
} // readIdbVia


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
        got = edadb::read2Scan<SpecialNetShadw>(rd_sd, special_net_map, &special_net_sd);
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
