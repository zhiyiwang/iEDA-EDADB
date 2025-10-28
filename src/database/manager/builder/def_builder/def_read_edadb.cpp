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

    // init database
    if (!edadb::initDatabase(edadb_path)) {
         std::cerr << "Error: failed to init database from " << edadb_path << std::endl;
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
    defrSetSNetStartCbk(specialNetBeginCallback);
    defrSetSNetCbk(specialNetCallback);
    defrSetSNetEndCbk(specialNetEndCallback);
    defrSetViaCbk(viaCallback);
    defrSetViaStartCbk(viaBeginCallback);
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

} // namespace idb
