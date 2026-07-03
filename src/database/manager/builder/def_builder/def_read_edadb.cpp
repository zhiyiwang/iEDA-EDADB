/**
 * @file def_read_edadb.h
 * @author Zhiyi Wang (zhiyiwang@ict.ac.cn)
*/

#include "def_read_edadb.h"

#include "edadb.h"
#include "edadb_idb_schema.h"

#include <algorithm>

namespace idb {

#if EDADB_OUTPUT_DEBUG
#define EDADB_IDB_DEBUG_STREAM std::cout
#else
#define EDADB_IDB_DEBUG_STREAM if (true) {} else std::cout
#endif

DefReadEdadb::DefReadEdadb(IdbDefService* def_service) : DefRead(def_service)
{}



bool DefReadEdadb::createDbFromEdadb(const char* edadb_path, const char* path)
{
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] DefReadEdadb::createDbFromEdadb edadb_path="
              << edadb_path << " def_path=" << path << std::endl;
    if (_def_service == nullptr) {
        std::cerr << "Error: DefReadEdadb::_def_service is nullptr" << std::endl;
        return false;
    }

    if (edadb_adapter::initReadDb(edadb_path) < 0) {
        std::cerr << "Error: DefReadEdadb::createDbFromEdadb failed to initReadDb!" << std::endl;
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

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] DefReadEdadb::createDbFromEdadb completed" << std::endl;
  
    return true;
} // createDbFromEdadb



bool DefReadEdadb::createDbByDef(const char* path) {
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] createDbByDef restore iDB from DEF text path="
              << path << std::endl;
    FILE* f = fopen(path, "r");
    if (f == NULL) {
      std::cerr << "Open def file failed..." << std::endl;
      return false;
    }

    defrInit();
    defrReset();

    defrInitSession();

    // Completed EDADB-restored object families stay disabled here:
    // Design/Units/BusBitChars/Die/Row/TrackGrid/GCellGrid/Via/Region/Slot.
    // Unfinished families are still rebuilt from DEF text for this demo branch.
    defrSetBlockageCbk(blockageCallback);
    defrSetComponentCbk(componentsCallback);
    defrSetComponentStartCbk(componentNumberCallback);
    defrSetComponentEndCbk(componentEndCallback);
    defrSetFillStartCbk(fillsCallback);
    defrSetFillCbk(fillCallback);
    defrSetGroupCbk(groupCallback);
    defrSetGroupMemberCbk(groupMemberCallback);
    defrSetGroupNameCbk(groupNameCallback);
    defrSetNetStartCbk(netBeginCallback);
    defrSetNetCbk(netCallback);
    defrSetNetEndCbk(netEndCallback);
    defrSetPinCbk(pinCallback);
    defrSetPinEndCbk(pinsEndCallback);
    defrSetStartPinsCbk(pinsBeginCallback);
    defrSetSNetStartCbk(specialNetBeginCallback);
    defrSetSNetCbk(specialNetCallback);
    defrSetSNetEndCbk(specialNetEndCallback);

    defrSetAddPathToNet();

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
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] createDbByEdadb Design/Die/Row/TrackGrid/GCell/Via/Region/Slot enabled path="
              << edadb_path << std::endl;

    CHECK_READ(readIdbDesign(), "DefReadEdadb::createDbByEdadb failed to read IdbDesign!");
    CHECK_READ(readIdbDie(), "DefReadEdadb::createDbByEdadb failed to read IdbDie!");
    CHECK_READ(readIdbRow(), "DefReadEdadb::createDbByEdadb failed to read IdbRow!");
    CHECK_READ(readIdbTrackGrid(), "DefReadEdadb::createDbByEdadb failed to read readIdbTrackGrid!");
    CHECK_READ(readIdbGCellGrid(), "DefReadEdadb::createDbByEdadb failed to read IdbGCellGrid!");
    CHECK_READ(readIdbVia(), "DefReadEdadb::createDbByEdadb failed to read IdbVia!");
    CHECK_READ(readIdbRegion(), "DefReadEdadb::createDbByEdadb failed to read IdbRegion!");
    CHECK_READ(readIdbSlot(), "DefReadEdadb::createDbByEdadb failed to read IdbSlot!");



#if EDADB_OUTPUT_DEBUG
    std::cout << "DefReadEdadb::createDbByEdadb successfully read def data from EDADB database!" << std::endl;
#endif

    return true;
} // createDbByEdadb



bool DefReadEdadb::readIdbDesign() {
    auto design_reader = edadb::makeReadAllOp<idb::IdbDesign>();

    idb::IdbDesign got;
    const int read_count = edadb::readNext<idb::IdbDesign>(design_reader, &got);
    if (read_count <= 0) {
        std::cout << "DefReadEdadb::readIdbDesign failed to read!" << std::endl;
        return false;
    }

    IdbDesign* design = _def_service->get_design();
    if (design == nullptr) {
        std::cerr << "readIdbDesign failed: design is nullptr" << std::endl;
        return false;
    }

    design->set_design_name(got.get_design_name());
    design->set_version(got.get_version());

    IdbUnits* got_units = got.get_units();
    IdbUnits* units = design->get_units();
    if (got_units == nullptr || units == nullptr) {
        std::cerr << "readIdbDesign failed: units is nullptr" << std::endl;
        return false;
    }

    IdbLayout* layout = _def_service->get_layout();
    IdbUnits* lef_units = layout == nullptr ? nullptr : layout->get_units();
    if (lef_units != nullptr && got_units->get_micron_dbu() != lef_units->get_micron_dbu()) {
        std::cout << "Warning : Def DBU dismatch LEF DBU" << std::endl;
    }
    units->set_microns_dbu(got_units->get_micron_dbu());
    delete got_units;
    got.set_units(nullptr);

    IdbBusBitChars* got_bus_bit_chars = got.get_bus_bit_chars();
    if (got_bus_bit_chars == nullptr) {
        std::cerr << "readIdbDesign failed: bus_bit_chars is nullptr" << std::endl;
        return false;
    }
    delete design->get_bus_bit_chars();
    design->set_bus_bit_chars(got_bus_bit_chars);
    got.set_bus_bit_chars(nullptr);

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbDesign restored name="
              << design->get_design_name()
              << " version=" << design->get_version()
              << " micron_dbu="
              << (design->get_units() == nullptr ? -1 : design->get_units()->get_micron_dbu())
              << std::endl;

    return true;
}

bool DefReadEdadb::readIdbDie(void) {
    IdbLayout* layout = _def_service->get_layout();
    IdbDie* die = layout->get_die();
    if (die == nullptr) {
        std::cerr << "DefReadEdadb::IdbDie failed, die is nullptr!" << std::endl;
        return false;
    }

    die->reset();

    auto die_reader = edadb::makeReadAllOp<edadb::Shadow<idb::IdbDie>>();
    edadb::Shadow<idb::IdbDie> die_sd;
    const int read_count = edadb::readNext<edadb::Shadow<idb::IdbDie>>(die_reader, &die_sd);
    if (read_count <= 0) {
        std::cout << "DefReadEdadb::readIdbDie failed to read!" << std::endl;
        return false;
    }

    if (!die_sd.fromShadow(die)) {
        std::cerr << "DefReadEdadb::readIdbDie failed to restore shadow" << std::endl;
        return false;
    }
    die->set_bounding_box();

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbDie restored point_count="
              << die->get_points().size() << std::endl;
    return true;
}

bool DefReadEdadb::readIdbRow(void) {
    IdbLayout* layout = _def_service->get_layout();
    IdbSites* sites = layout->get_sites();
    IdbRows* rows = layout->get_rows();
    if (sites == nullptr || rows == nullptr) {
        std::cerr << "DefReadEdadb::readIdbRow failed, sites or rows is nullptr!" << std::endl;
        return false;
    }

    rows->reset();

    auto row_reader = edadb::makeGenericQueryOp<edadb::Shadow<idb::IdbRow>>();
    if (row_reader.preparePredicate("ORDER BY \"_order_sd\"") < 0) {
        std::cerr << "DefReadEdadb::readIdbRow failed to prepare ordered row query!" << std::endl;
        return false;
    }

    int32_t row_count = 0;
    while (true) {
        edadb::Shadow<idb::IdbRow> row_sd;
        const int read_count = edadb::readNext<edadb::Shadow<idb::IdbRow>>(row_reader, &row_sd);
        if (read_count == 0) {
            break;
        }
        if (read_count < 0) {
            std::cout << "DefReadEdadb::readIdbRow failed to read!" << std::endl;
            return false;
        }

        IdbRow* row = new IdbRow();
        row_sd.fromShadow(row);

        IdbSite* lef_site = sites->add_site_list(row_sd._site_name_sd);
        if (lef_site == nullptr) {
            delete row;
            std::cerr << "DefReadEdadb::readIdbRow failed, lef site is nullptr: "
                      << row_sd._site_name_sd << std::endl;
            return false;
        }

        IdbSite* site = lef_site->clone();
        site->set_orient(row_sd._site_orient_sd);
        row->set_site(site);
        row->set_orient(row_sd._site_orient_sd);
        row->set_bounding_box();

        rows->add_row_list(row);
        ++row_count;
    }

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbRow restored row_count="
              << row_count << std::endl;
    return true;
}

bool DefReadEdadb::readIdbTrackGrid(void) {
    IdbLayout* layout = _def_service->get_layout();
    IdbLayers* layers = layout->get_layers();
    IdbTrackGridList* track_grid_list = layout->get_track_grid_list();
    if (layers == nullptr || track_grid_list == nullptr) {
        std::cerr << "DefReadEdadb::readIdbTrackGrid failed, layers or track_grid_list is nullptr!" << std::endl;
        return false;
    }

    track_grid_list->reset();

    auto track_grid_reader = edadb::makeGenericQueryOp<edadb::Shadow<idb::IdbTrackGrid>>();
    if (track_grid_reader.preparePredicate("ORDER BY \"_order_sd\"") < 0) {
        std::cerr << "DefReadEdadb::readIdbTrackGrid failed to prepare ordered track grid query!" << std::endl;
        return false;
    }

    int32_t track_grid_count = 0;
    int32_t layer_ref_count = 0;
    while (true) {
        edadb::Shadow<idb::IdbTrackGrid> track_grid_sd;
        const int read_count = edadb::readNext<edadb::Shadow<idb::IdbTrackGrid>>(track_grid_reader, &track_grid_sd);
        if (read_count == 0) {
            break;
        }
        if (read_count < 0) {
            std::cout << "DefReadEdadb::readIdbTrackGrid failed to read!" << std::endl;
            return false;
        }

        IdbTrackGrid* track_grid = track_grid_list->add_track_grid(nullptr);
        track_grid_sd.fromShadow(track_grid);

        for (auto& layer_name_sd : track_grid_sd._layer_name_vec_sd) {
            IdbLayer* layer = layers->find_layer(layer_name_sd);
            if (layer == nullptr) {
                std::cout << "Track Grid Error : no layer exist..." << std::endl;
                continue;
            }

            track_grid->add_layer_list(layer);
            if (layer->is_routing()) {
                IdbLayerRouting* routing_layer = dynamic_cast<IdbLayerRouting*>(layer);
                routing_layer->add_track_grid(track_grid);
            }
            ++layer_ref_count;
        }

        ++track_grid_count;
    }

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbTrackGrid restored track_grid_count="
              << track_grid_count
              << " layer_ref_count=" << layer_ref_count << std::endl;
    return true;
}

bool DefReadEdadb::readIdbGCellGrid(void) {
    IdbLayout* layout = _def_service->get_layout();  // Lef
    IdbGCellGridList* gcell_grid_list = layout->get_gcell_grid_list();
    if (gcell_grid_list == nullptr) {
        std::cerr << "DefReadEdadb::readIdbGCellGrid failed, gcell_grid_list is nullptr!" << std::endl;
        return false;
    }

    gcell_grid_list->clear();

    auto gcell_grid_reader = edadb::makeReadAllOp<idb::IdbGCellGrid>();

    int32_t gcell_grid_count = 0;
    while (true) {
        IdbGCellGrid* gcell_grid = new IdbGCellGrid();
        const int read_count = edadb::readNext<idb::IdbGCellGrid>(gcell_grid_reader, gcell_grid);
        if (read_count == 0) {
            delete gcell_grid;
            break;
        }
        if (read_count < 0) {
            delete gcell_grid;
            std::cout << "DefReadEdadb::readIdbGCellGrid failed to read!" << std::endl;
            return false;
        }

        gcell_grid_list->add_gcell_grid(gcell_grid);
        ++gcell_grid_count;
    }

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbGCellGrid restored gcell_grid_count="
              << gcell_grid_count << std::endl;
    return true;
}

bool DefReadEdadb::readIdbVia(void) {
    idb::IdbDefService* idb_def_service = edadb_adapter::EdadbIdbHelper::getIdbDefService();
    if (idb_def_service == nullptr) {
        edadb_adapter::EdadbIdbHelper::setIdbDefService(_def_service);
    } else if (edadb_adapter::EdadbIdbHelper::getIdbDefService() != _def_service) {
        std::cerr << "DefReadEdadb::readIdbVia failed, IdbDefService not consistent!" << std::endl;
        return false;
    }

    IdbDesign* design = _def_service->get_design();  // Def
    if (design == nullptr) {
        std::cerr << "DefReadEdadb::readIdbVia failed, design is nullptr!" << std::endl;
        return false;
    }

    IdbVias* via_list = design->get_via_list();
    if (via_list == nullptr) {
        std::cerr << "DefReadEdadb::readIdbVia failed, via_list is nullptr!" << std::endl;
        return false;
    }

    auto via_reader = edadb::makeReadAllOp<idb::IdbVia>();
    int32_t via_count = 0;
    while (true) {
        IdbVia* via_inst = new IdbVia();
        const int read_count = edadb::readNext<idb::IdbVia>(via_reader, via_inst);
        if (read_count == 0) {
            delete via_inst;
            break;
        }
        if (read_count < 0) {
            delete via_inst;
            std::cout << "DefReadEdadb::readIdbVia failed to read!" << std::endl;
            return false;
        }

        via_list->add_via(via_inst);
        ++via_count;
    }

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbVia restored via_count="
              << via_count << std::endl;
    return true;
}

bool DefReadEdadb::readIdbRegion(void) {
    IdbDesign* design = _def_service->get_design();  // def
    if (design == nullptr) {
        std::cerr << "DefReadEdadb::readIdbRegion failed, design is nullptr!" << std::endl;
        return false;
    }

    IdbRegionList* region_list = design->get_region_list();
    if (region_list == nullptr) {
        std::cerr << "DefReadEdadb::readIdbRegion failed, region_list is nullptr!" << std::endl;
        return false;
    }

    auto region_reader = edadb::makeReadAllOp<idb::IdbRegion>();

    int32_t region_count = 0;
    while (true) {
        IdbRegion* region = new IdbRegion();
        const int read_count = edadb::readNext<idb::IdbRegion>(region_reader, region);
        if (read_count == 0) {
            delete region;
            break;
        }
        if (read_count < 0) {
            delete region;
            std::cout << "DefReadEdadb::readIdbRegion failed to read!" << std::endl;
            return false;
        }

        region_list->add_region(region);
        ++region_count;
    }

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbRegion restored region_count="
              << region_count << std::endl;
    return true;
}

bool DefReadEdadb::readIdbSlot(void) {
    IdbDesign* design = _def_service->get_design();  // def
    if (design == nullptr) {
        std::cerr << "DefReadEdadb::readIdbSlot failed, design is nullptr!" << std::endl;
        return false;
    }

    IdbSlotList* slot_list = design->get_slot_list();
    if (slot_list == nullptr) {
        std::cerr << "DefReadEdadb::readIdbSlot failed, slot_list is nullptr!" << std::endl;
        return false;
    }

    auto slot_reader = edadb::makeGenericQueryOp<edadb::Shadow<idb::IdbSlot>>();
    if (slot_reader.preparePredicate("ORDER BY \"_order_sd\"") < 0) {
        std::cerr << "DefReadEdadb::readIdbSlot failed to prepare ordered slot query!" << std::endl;
        return false;
    }

    int32_t slot_count = 0;
    while (true) {
        auto* slot_sd = new edadb::Shadow<idb::IdbSlot>();
        const int read_count = edadb::readNext<edadb::Shadow<idb::IdbSlot>>(slot_reader, slot_sd);
        if (read_count == 0) {
            delete slot_sd;
            break;
        }
        if (read_count < 0) {
            delete slot_sd;
            std::cout << "DefReadEdadb::readIdbSlot failed to read!" << std::endl;
            return false;
        }

        IdbSlot* slot = slot_list->add_slot();
        slot_sd->fromShadow(slot);
        delete slot_sd;
        ++slot_count;
    }

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbSlot restored slot_count="
              << slot_count << std::endl;
    return true;
}

#undef EDADB_IDB_DEBUG_STREAM

} // namespace idb
