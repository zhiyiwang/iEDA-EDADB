/**
 * @file def_read_edadb.h
 * @author Zhiyi Wang (zhiyiwang@ict.ac.cn)
*/

#include "def_read_edadb.h"

#include "edadb.h"
#include "edadb_idb_schema.h"

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

    if (!edadb_adapter::EdadbIdbHelper::setIdbDefService(_def_service)) {
        std::cerr << "Error: DefReadEdadb::createDbFromEdadb failed to set IdbDefService!" << std::endl;
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

    defrSetNetStartCbk(netBeginCallback);
    defrSetNetCbk(netCallback);
    defrSetNetEndCbk(netEndCallback);
    defrSetSNetCbk(netFallbackSpecialNetCallback);
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
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] createDbByEdadb Design/Die/Row/TrackGrid/GCell/Via/Region/Instance/Pin/Blockage/Slot/Group/Fill/SpecialNet enabled; Net uses DEF fallback path="
              << edadb_path << std::endl;

    CHECK_READ(readIdbDesign(), "DefReadEdadb::createDbByEdadb failed to read IdbDesign!");
    CHECK_READ(readIdbDie(), "DefReadEdadb::createDbByEdadb failed to read IdbDie!");
    CHECK_READ(readIdbRow(), "DefReadEdadb::createDbByEdadb failed to read IdbRow!");
    CHECK_READ(readIdbTrackGrid(), "DefReadEdadb::createDbByEdadb failed to read readIdbTrackGrid!");
    CHECK_READ(readIdbGCellGrid(), "DefReadEdadb::createDbByEdadb failed to read IdbGCellGrid!");
    CHECK_READ(readIdbVia(), "DefReadEdadb::createDbByEdadb failed to read IdbVia!");
    CHECK_READ(readIdbRegion(), "DefReadEdadb::createDbByEdadb failed to read IdbRegion!");
    CHECK_READ(readIdbInstance(), "DefReadEdadb::createDbByEdadb failed to read IdbInstance!");
    CHECK_READ(readIdbPin(), "DefReadEdadb::createDbByEdadb failed to read IdbPin!");
    CHECK_READ(readIdbBlockage(), "DefReadEdadb::createDbByEdadb failed to read IdbBlockage!");
    CHECK_READ(readIdbSlot(), "DefReadEdadb::createDbByEdadb failed to read IdbSlot!");
    CHECK_READ(readIdbGroup(), "DefReadEdadb::createDbByEdadb failed to read IdbGroup!");
    CHECK_READ(readIdbFill(), "DefReadEdadb::createDbByEdadb failed to read IdbFill!");
    CHECK_READ(readIdbSpecialNet(), "DefReadEdadb::createDbByEdadb failed to read IdbSpecialNet!");



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

    auto die_reader = edadb::makeReadAllOp<edadb::Shadow<idb::IdbDie>>();
    edadb::Shadow<idb::IdbDie> die_sd;
    const int read_count = edadb::readNext<edadb::Shadow<idb::IdbDie>>(die_reader, &die_sd);
    if (read_count <= 0) {
        std::cout << "DefReadEdadb::readIdbDie failed to read!" << std::endl;
        return false;
    }

    die->reset();
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
    IdbRows* rows = layout->get_rows();
    if (rows == nullptr) {
        std::cerr << "DefReadEdadb::readIdbRow failed, rows is nullptr!" << std::endl;
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
        if (!row_sd.fromShadow(row)) {
            delete row;
            std::cerr << "DefReadEdadb::readIdbRow failed to restore row shadow: "
                      << row_sd._name_sd << std::endl;
            return false;
        }

        rows->add_row_list(row);
        ++row_count;
    }

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbRow restored row_count="
              << row_count << std::endl;
    return true;
}

bool DefReadEdadb::readIdbTrackGrid(void) {
    IdbLayout* layout = _def_service->get_layout();
    IdbTrackGridList* track_grid_list = layout->get_track_grid_list();
    if (track_grid_list == nullptr) {
        std::cerr << "DefReadEdadb::readIdbTrackGrid failed, track_grid_list is nullptr!" << std::endl;
        return false;
    }

    track_grid_list->reset();

    auto track_grid_reader = edadb::makeReadAllOp<edadb::Shadow<idb::IdbTrackGrid>>();

    int32_t track_grid_count = 0;
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

        IdbTrackGrid* track_grid = new IdbTrackGrid();
        if (!track_grid_sd.fromShadow(track_grid)) {
            delete track_grid;
            std::cerr << "DefReadEdadb::readIdbTrackGrid failed to restore track grid shadow" << std::endl;
            return false;
        }

        track_grid_list->add_track_grid(track_grid);
        ++track_grid_count;
    }

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbTrackGrid restored track_grid_count="
              << track_grid_count << std::endl;
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

#if EDADB_OUTPUT_DEBUG
        IdbViaMaster* via_master = via_inst->get_instance();
        if (via_master->is_fix()) {
            std::string fixed_geometry = via_inst->get_name();
            uint32_t fixed_idx = 0;
            for (IdbViaMasterFixed* fixed : via_master->get_master_fixed_list()) {
                fixed_geometry += "|" + std::to_string(fixed_idx++) + ":" + fixed->get_layer()->get_name() + ":";
                uint32_t rect_idx = 0;
                for (IdbRect* rect : fixed->get_rect_list()) {
                    if (rect_idx > 0) {
                        fixed_geometry += ";";
                    }
                    fixed_geometry += std::to_string(rect_idx++) + "="
                                      + std::to_string(rect->get_low_x()) + ","
                                      + std::to_string(rect->get_low_y()) + ","
                                      + std::to_string(rect->get_high_x()) + ","
                                      + std::to_string(rect->get_high_y());
                }
            }
            EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbVia fixed_geometry="
                                   << fixed_geometry << std::endl;
        }
#endif

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

    region_list->reset();

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
            region_list->reset();
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

    slot_list->reset();

    auto slot_reader = edadb::makeGenericQueryOp<edadb::Shadow<idb::IdbSlot>>();
    if (slot_reader.preparePredicate("ORDER BY \"_order_sd\"") < 0) {
        std::cerr << "DefReadEdadb::readIdbSlot failed to prepare ordered slot query!" << std::endl;
        return false;
    }

    int32_t slot_count = 0;
    while (true) {
        edadb::Shadow<idb::IdbSlot> slot_sd;
        const int read_count = edadb::readNext<edadb::Shadow<idb::IdbSlot>>(slot_reader, &slot_sd);
        if (read_count == 0) {
            break;
        }
        if (read_count < 0) {
            slot_list->reset();
            std::cout << "DefReadEdadb::readIdbSlot failed to read!" << std::endl;
            return false;
        }

        IdbSlot* slot = slot_list->add_slot();
        if (!slot_sd.fromShadow(slot)) {
            slot_list->reset();
            std::cerr << "DefReadEdadb::readIdbSlot failed to restore slot shadow" << std::endl;
            return false;
        }
        ++slot_count;
    }

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbSlot restored slot_count="
              << slot_count << std::endl;
    return true;
}

bool DefReadEdadb::readIdbGroup(void) {
    IdbDesign* design = _def_service->get_design();  // def
    if (design == nullptr) {
        std::cerr << "DefReadEdadb::readIdbGroup failed, design is nullptr!" << std::endl;
        return false;
    }

    IdbGroupList* group_list = design->get_group_list();
    if (group_list == nullptr) {
        std::cerr << "DefReadEdadb::readIdbGroup failed, group_list is nullptr!" << std::endl;
        return false;
    }

    group_list->reset();

    auto group_reader = edadb::makeReadAllOp<edadb::Shadow<idb::IdbGroup>>();

    int32_t group_count = 0;
    while (true) {
        edadb::Shadow<idb::IdbGroup> group_sd;
        const int read_count = edadb::readNext<edadb::Shadow<idb::IdbGroup>>(group_reader, &group_sd);
        if (read_count == 0) {
            break;
        }
        if (read_count < 0) {
            group_list->reset();
            std::cout << "DefReadEdadb::readIdbGroup failed to read!" << std::endl;
            return false;
        }

        IdbGroup* group = group_list->add_group(group_sd._group_name_sd);
        if (!group_sd.fromShadow(group)) {
            group_list->reset();
            std::cerr << "DefReadEdadb::readIdbGroup failed to restore group shadow" << std::endl;
            return false;
        }
        ++group_count;
    }

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbGroup restored group_count="
              << group_count << std::endl;
    return true;
}

bool DefReadEdadb::readIdbFill(void) {
    IdbDesign* design = _def_service->get_design();  // def
    if (design == nullptr) {
        std::cerr << "DefReadEdadb::readIdbFill failed, design is nullptr!" << std::endl;
        return false;
    }

    IdbFillList* fill_list = design->get_fill_list();
    if (fill_list == nullptr) {
        std::cerr << "DefReadEdadb::readIdbFill failed, fill_list is nullptr!" << std::endl;
        return false;
    }

    fill_list->reset();

    auto fill_reader = edadb::makeReadAllOp<edadb::Shadow<idb::IdbFill>>();
    int32_t fill_count = 0;
    while (true) {
        edadb::Shadow<idb::IdbFill> fill_sd;
        const int read_count = edadb::readNext<edadb::Shadow<idb::IdbFill>>(fill_reader, &fill_sd);
        if (read_count == 0) {
            break;
        }
        if (read_count < 0) {
            fill_list->reset();
            std::cout << "DefReadEdadb::readIdbFill failed to read!" << std::endl;
            return false;
        }

        IdbFill* fill = new IdbFill();
        if (!fill_sd.fromShadow(fill)) {
            delete fill;
            fill_list->reset();
            std::cerr << "DefReadEdadb::readIdbFill failed to restore fill shadow" << std::endl;
            return false;
        }
        fill_list->add_fill(fill);
        ++fill_count;
    }

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbFill restored fill_count="
              << fill_count << std::endl;
    return true;
}

bool DefReadEdadb::readIdbInstance(void) {
    IdbDesign* design = _def_service->get_design();  // Def
    if (design == nullptr) {
        std::cerr << "DefReadEdadb::readIdbInstance failed, design is nullptr!" << std::endl;
        return false;
    }

    IdbInstanceList* instance_list = design->get_instance_list();
    if (instance_list == nullptr) {
        std::cerr << "DefReadEdadb::readIdbInstance failed, instance_list is nullptr!" << std::endl;
        return false;
    }

    instance_list->reset();

    auto inst_reader = edadb::makeGenericQueryOp<edadb::Shadow<idb::IdbInstance>>();
    if (inst_reader.preparePredicate("ORDER BY \"_order_sd\"") < 0) {
        std::cerr << "DefReadEdadb::readIdbInstance failed to prepare ordered instance query!" << std::endl;
        return false;
    }

    int32_t instance_count = 0;
    while (true) {
        edadb::Shadow<idb::IdbInstance> inst_sd;
        const int read_count = edadb::readNext<edadb::Shadow<idb::IdbInstance>>(inst_reader, &inst_sd);
        if (read_count == 0) {
            break;
        }
        if (read_count < 0) {
            std::cout << "DefReadEdadb::readIdbInstance failed to read!" << std::endl;
            return false;
        }

        IdbInstance* inst = new IdbInstance();
        if (!inst_sd.fromShadow(inst)) {
            delete inst;
            std::cerr << "DefReadEdadb::readIdbInstance failed to restore instance shadow" << std::endl;
            return false;
        }

        instance_list->add_instance(inst);
        ++instance_count;

        if (instance_list->get_num() % 1000 == 0) {
            std::cout << "-" << std::flush;
            if (instance_list->get_num() % 100000 == 0) {
                std::cout << std::endl;
            }
        }
    }

    if (instance_count >= 1000) {
        std::cout << std::endl;
    }
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbInstance restored instance_count="
              << instance_count << std::endl;
    return true;
}

bool DefReadEdadb::readIdbPin(void) {
    IdbDesign* design = _def_service->get_design();  // Def
    if (design == nullptr) {
        std::cerr << "DefReadEdadb::readIdbPin failed, design is nullptr!" << std::endl;
        return false;
    }

    IdbPins* pin_list = design->get_io_pin_list();
    if (pin_list == nullptr) {
        std::cerr << "DefReadEdadb::readIdbPin failed, pin_list is nullptr!" << std::endl;
        return false;
    }

    pin_list->reset();

    auto pin_reader = edadb::makeGenericQueryOp<edadb::Shadow<idb::IdbPin>>();
    if (pin_reader.preparePredicate("ORDER BY \"_order_sd\"") < 0) {
        std::cerr << "DefReadEdadb::readIdbPin failed to prepare ordered pin query!" << std::endl;
        return false;
    }

    int32_t pin_count = 0;
    while (true) {
        edadb::Shadow<idb::IdbPin> pin_sd;
        const int read_count = edadb::readNext<edadb::Shadow<idb::IdbPin>>(pin_reader, &pin_sd);
        if (read_count == 0) {
            break;
        }
        if (read_count < 0) {
            pin_list->reset();
            std::cout << "DefReadEdadb::readIdbPin failed to read!" << std::endl;
            return false;
        }

        IdbPin* pin = new IdbPin();
        if (!pin_sd.fromShadow(pin)) {
            delete pin;
            pin_list->reset();
            std::cerr << "DefReadEdadb::readIdbPin failed to restore pin" << std::endl;
            return false;
        }
        pin_list->add_pin_list(pin);

        ++pin_count;
    }

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbPin restored pin_count="
              << pin_count << std::endl;
    return true;
}

bool DefReadEdadb::readIdbBlockage(void) {
    IdbDesign* design = _def_service->get_design();  // Def
    if (design == nullptr) {
        std::cerr << "DefReadEdadb::readIdbBlockage failed, design is nullptr!" << std::endl;
        return false;
    }

    IdbBlockageList* blockage_list = design->get_blockage_list();
    if (blockage_list == nullptr) {
        std::cerr << "DefReadEdadb::readIdbBlockage failed, blockage_list is nullptr!" << std::endl;
        return false;
    }

    blockage_list->reset();

    auto blockage_reader = edadb::makeReadAllOp<edadb::Shadow<idb::IdbBlockage>>();
    int32_t blockage_count = 0;
    while (true) {
        edadb::Shadow<idb::IdbBlockage> blockage_sd;
        const int read_count = edadb::readNext<edadb::Shadow<idb::IdbBlockage>>(blockage_reader, &blockage_sd);
        if (read_count == 0) {
            break;
        }
        if (read_count < 0) {
            blockage_list->reset();
            std::cout << "DefReadEdadb::readIdbBlockage failed to read!" << std::endl;
            return false;
        }

        IdbBlockage* blockage = nullptr;
        if (blockage_sd._type_sd == idb::IdbBlockage::IdbBlockageType::kRoutingBlockage) {
            blockage = blockage_list->add_blockage_routing(blockage_sd._layer_name_sd);
        } else if (blockage_sd._type_sd == idb::IdbBlockage::IdbBlockageType::kPlacementBlockage) {
            blockage = blockage_list->add_blockage_placement();
        } else {
            blockage_list->reset();
            std::cerr << "DefReadEdadb::readIdbBlockage failed, unknown blockage type" << std::endl;
            return false;
        }

        if (!blockage_sd.fromShadow(blockage)) {
            blockage_list->reset();
            std::cerr << "DefReadEdadb::readIdbBlockage failed to restore blockage shadow" << std::endl;
            return false;
        }

        if (blockage->get_type() == idb::IdbBlockage::IdbBlockageType::kRoutingBlockage) {
            auto* routing_blockage = dynamic_cast<idb::IdbRoutingBlockage*>(blockage);
            EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbBlockage routing layer=" << routing_blockage->get_layer_name()
                                   << " slots=" << routing_blockage->is_slots()
                                   << " fills=" << routing_blockage->is_fills()
                                   << " pushdown=" << routing_blockage->is_pushdown()
                                   << " except_pgnet=" << routing_blockage->is_except_pgnet()
                                   << " instance=" << routing_blockage->get_instance_name()
                                   << " min_spacing=" << routing_blockage->get_min_spacing()
                                   << " effective_width=" << routing_blockage->get_effective_width() << std::endl;
        } else {
            auto* placement_blockage = dynamic_cast<idb::IdbPlacementBlockage*>(blockage);
            EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbBlockage placement soft=" << placement_blockage->is_soft()
                                   << " partial=" << placement_blockage->is_partial()
                                   << " max_density=" << placement_blockage->get_max_density()
                                   << " pushdown=" << placement_blockage->is_pushdown()
                                   << " instance=" << placement_blockage->get_instance_name() << std::endl;
        }

        ++blockage_count;
    }

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbBlockage restored blockage_count="
              << blockage_count << std::endl;
    return true;
}


bool DefReadEdadb::readIdbSpecialNet(void) {
    IdbDesign* design = _def_service->get_design();  // Def
    if (design == nullptr) {
        std::cerr << "DefReadEdadb::readIdbSpecialNet failed, design is nullptr!" << std::endl;
        return false;
    }

    IdbSpecialNetList* net_list = design->get_special_net_list();
    if (net_list == nullptr) {
        std::cerr << "DefReadEdadb::readIdbSpecialNet failed, required list is nullptr!" << std::endl;
        return false;
    }

    net_list->reset();

    auto special_net_reader = edadb::makeReadAllOp<edadb::Shadow<idb::IdbSpecialNet>>();
#if EDADB_OUTPUT_DEBUG
    int32_t special_net_count = 0;
    int32_t segment_count = 0;
    int32_t styled_segment_count = 0;
    int32_t shield_wire_count = 0;
#endif
    while (true) {
        auto* special_net_sd = new edadb::Shadow<idb::IdbSpecialNet>();
        const int read_count = edadb::readNext<edadb::Shadow<idb::IdbSpecialNet>>(special_net_reader, special_net_sd);
        if (read_count == 0) {
            delete special_net_sd;
            break;
        }
        if (read_count < 0) {
            delete special_net_sd;
            net_list->reset();
            std::cout << "DefReadEdadb::readIdbSpecialNet failed to read!" << std::endl;
            return false;
        }

        IdbSpecialNet* special_net = net_list->add_net(special_net_sd->_net_name_sd);
        if (special_net == nullptr) {
            std::cerr << "DefReadEdadb::readIdbSpecialNet failed to create special net: "
                      << special_net_sd->_net_name_sd << std::endl;
            delete special_net_sd;
            net_list->reset();
            return false;
        }
        if (!special_net_sd->fromShadow(special_net)) {
            delete special_net_sd;
            net_list->reset();
            return false;
        }
#if EDADB_OUTPUT_DEBUG
        segment_count += special_net_sd->getSegmentCount();
        for (IdbSpecialWire* wire : special_net->get_wire_list()->get_wire_list()) {
            if (wire->get_wire_state() == IdbWiringStatement::kShield) {
                ++shield_wire_count;
            }
            for (IdbSpecialWireSegment* segment : wire->get_segment_list()) {
                if (segment->get_style() >= 0) {
                    ++styled_segment_count;
                }
            }
        }
#endif

        delete special_net_sd;
#if EDADB_OUTPUT_DEBUG
        ++special_net_count;
#endif
    }

#if EDADB_OUTPUT_DEBUG
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbSpecialNet restored special_net_count="
              << special_net_count << " segment_count=" << segment_count
              << " styled_segment_count=" << styled_segment_count
              << " shield_wire_count=" << shield_wire_count << std::endl;
#endif
    return true;
} // readIdbSpecialNet

int32_t DefReadEdadb::netFallbackSpecialNetCallback(defrCallbackType_e type, defiNet* def_net, defiUserData data) {
    if (def_net == nullptr) {
        return kDbFail;
    }

    auto* def_reader = static_cast<DefReadEdadb*>(data);
    if (!def_reader->check_type(type) || !def_net->hasUse()) {
        return kDbFail;
    }

    auto* connect_property = IdbEnum::GetInstance()->get_connect_property();
    return connect_property->is_net(def_net->use()) ? def_reader->parse_net(def_net) : kDbSuccess;
}

#undef EDADB_IDB_DEBUG_STREAM

} // namespace idb
