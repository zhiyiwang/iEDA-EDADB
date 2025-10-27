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
    defrSetBlockageCbk(blockageCallback);
    defrSetComponentCbk(componentsCallback);
    defrSetComponentStartCbk(componentNumberCallback);
    defrSetComponentEndCbk(componentEndCallback);
    defrSetFillStartCbk(fillsCallback);
    defrSetFillCbk(fillCallback);
    defrSetGcellGridCbk(gcellGridCallback);
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

#if DEBUG_EDADB_OUTPUT
    std::cout << "DEADB: Def read to EDADB database : " << edadb_path << std::endl;
#endif

    // read IdbDesign from edadb database
    if (!readIdbDesign()) {
        std::cerr << "DefReadEdadb::createDbByEdadb failed to read IdbDesign!" << std::endl;
        return false;
    }

    // read IdbDie from edadb database
    if (!readIdbDie()) {
        std::cerr << "DefReadEdadb::createDbByEdadb failed to read IdbDie!" << std::endl;
        return false;
    }

    return true;
} // createDbByEdadb



bool DefReadEdadb::readIdbDesign() {
    edadb::DbMap<edadb::Shadow<idb::IdbDesign>> design_map;
    design_map.init();

    edadb::Shadow<idb::IdbDesign> got_shadow;
    edadb::DbMapReader<edadb::Shadow<idb::IdbDesign>>* rd = nullptr;
    if (edadb::read2Scan<edadb::Shadow<idb::IdbDesign>>(rd, design_map, &got_shadow) <= 0) {
        std::cout << "DefReadEdadb::readIdbDesign failed to read!" << std::endl;
        return false;
    } // if 

    IdbDesign got;
    got_shadow.fromShadow(&got);

    IdbDesign* design = _def_service->get_design();
    design->set_design_name(got.get_design_name());
    design->set_version(got.get_version());

    // swap pointers 
    idb::IdbUnits* du = design->get_units();
    idb::IdbUnits* ru = got.get_units();
    design->set_units(ru);
    got.set_units(du);

    idb::IdbBusBitChars* dbc = design->get_bus_bit_chars();
    idb::IdbBusBitChars* rbc = got.get_bus_bit_chars();
    design->set_bus_bit_chars(rbc);
    got.set_bus_bit_chars(dbc);

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

    edadb::DbMap< edadb::Shadow<idb::IdbCoordinate<int32_t>> > die_map;
    die_map.init();
    edadb::DbMapReader< edadb::Shadow<idb::IdbCoordinate<int32_t>> >* rd = nullptr;
    edadb::Shadow<idb::IdbCoordinate<int32_t>> ps;
    int got = 0;
    while ((got = edadb::read2Scan(rd, die_map, &ps)) > 0) {
        idb::IdbCoordinate<int32_t> *coord = new idb::IdbCoordinate<int32_t>();
        ps.fromShadow(coord);
        die->get_points().push_back(coord);
        ps.clear();
    }

    return true;
} // readIdbDie

} // namespace idb
