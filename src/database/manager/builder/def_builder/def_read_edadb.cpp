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

    // DEF callbacks for EDADB-restored object families are intentionally not registered.

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
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] createDbByEdadb Design/Die/Row/TrackGrid/GCell/Via/Region/Instance/Pin/Blockage/Slot/Group/Fill/SpecialNet/Net enabled path="
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
    CHECK_READ(readIdbNet(), "DefReadEdadb::createDbByEdadb failed to read IdbNet!");



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

    auto track_grid_reader = edadb::makeReadAllOp<edadb::Shadow<idb::IdbTrackGrid>>();

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

bool DefReadEdadb::readIdbGroup(void) {
    IdbDesign* design = _def_service->get_design();  // def
    if (design == nullptr) {
        std::cerr << "DefReadEdadb::readIdbGroup failed, design is nullptr!" << std::endl;
        return false;
    }

    IdbRegionList* region_list = design->get_region_list();
    IdbInstanceList* instance_list = design->get_instance_list();
    IdbGroupList* group_list = design->get_group_list();
    if (region_list == nullptr || instance_list == nullptr || group_list == nullptr) {
        std::cerr << "DefReadEdadb::readIdbGroup failed, required list is nullptr!" << std::endl;
        return false;
    }

    auto group_reader = edadb::makeReadAllOp<edadb::Shadow<idb::IdbGroup>>();

    int32_t group_count = 0;
    while (true) {
        auto* group_sd = new edadb::Shadow<idb::IdbGroup>();
        const int read_count = edadb::readNext<edadb::Shadow<idb::IdbGroup>>(group_reader, group_sd);
        if (read_count == 0) {
            delete group_sd;
            break;
        }
        if (read_count < 0) {
            delete group_sd;
            std::cout << "DefReadEdadb::readIdbGroup failed to read!" << std::endl;
            return false;
        }

        IdbGroup* group = group_list->add_group(group_sd->_group_name_sd);
        group_sd->fromShadow(group);
        if (!group_sd->_region_name_sd.empty()) {
            group->set_region(region_list->find_region(group_sd->_region_name_sd));
        }

        for (auto& instance_name_sd : group_sd->_instance_name_vec_sd) {
            IdbInstance* instance = instance_list->find_instance(instance_name_sd);
            if (instance != nullptr) {
                group->add_instance(instance);
            }
        }

        delete group_sd;
        ++group_count;
    }

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbGroup restored group_count="
              << group_count << std::endl;
    return true;
}

bool DefReadEdadb::readIdbFill(void) {
    IdbDesign* design = _def_service->get_design();  // def
    IdbLayout* layout = _def_service->get_layout();  // lef
    if (design == nullptr || layout == nullptr) {
        std::cerr << "DefReadEdadb::readIdbFill failed, design or layout is nullptr!" << std::endl;
        return false;
    }

    IdbLayers* layer_list = layout->get_layers();
    IdbVias* via_list_def = design->get_via_list();
    IdbVias* via_list_lef = layout->get_via_list();
    IdbFillList* fill_list = design->get_fill_list();
    if (layer_list == nullptr || via_list_def == nullptr || via_list_lef == nullptr || fill_list == nullptr) {
        std::cerr << "DefReadEdadb::readIdbFill failed, required list is nullptr!" << std::endl;
        return false;
    }

    auto fill_reader = edadb::makeReadAllOp<edadb::Shadow<idb::IdbFill>>();
    int32_t fill_count = 0;
    while (true) {
        auto* fill_sd = new edadb::Shadow<idb::IdbFill>();
        const int read_count = edadb::readNext<edadb::Shadow<idb::IdbFill>>(fill_reader, fill_sd);
        if (read_count == 0) {
            delete fill_sd;
            break;
        }
        if (read_count < 0) {
            delete fill_sd;
            std::cout << "DefReadEdadb::readIdbFill failed to read!" << std::endl;
            return false;
        }

        if (fill_sd->_type_sd == IdbFill::IdbFillType::kLayer) {
            IdbLayer* layer = layer_list->find_layer(fill_sd->_layer_name_sd);
            if (layer == nullptr) {
                std::cerr << "DefReadEdadb::readIdbFill failed to find layer: "
                          << fill_sd->_layer_name_sd << std::endl;
                delete fill_sd;
                return false;
            }

            IdbFillLayer* fill_layer = fill_list->add_fill_layer(layer);
            for (auto& rect_sd : fill_sd->_rect_list_sd) {
                fill_layer->add_rect(rect_sd->get_low_x(), rect_sd->get_low_y(), rect_sd->get_high_x(), rect_sd->get_high_y());
            }
        } else if (fill_sd->_type_sd == IdbFill::IdbFillType::kVia) {
            IdbVia* via = via_list_def->find_via(fill_sd->_via_name_sd);
            if (via == nullptr) {
                via = via_list_lef->find_via(fill_sd->_via_name_sd);
            }
            if (via == nullptr) {
                std::cerr << "DefReadEdadb::readIdbFill failed to find via: "
                          << fill_sd->_via_name_sd << std::endl;
                delete fill_sd;
                return false;
            }

            IdbVia* via_new = via->clone();
            IdbFillVia* fill_via = fill_list->add_fill_via(via_new);
            for (auto& coordinate_sd : fill_sd->_coordinate_list_sd) {
                fill_via->add_coordinate(coordinate_sd->get_x(), coordinate_sd->get_y());
            }
        }

        delete fill_sd;
        ++fill_count;
    }

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbFill restored fill_count="
              << fill_count << std::endl;
    return true;
}

bool DefReadEdadb::readIdbInstance(void) {
    IdbDesign* design = _def_service->get_design();  // Def
    IdbLayout* layout = _def_service->get_layout();  // Lef
    if (design == nullptr || layout == nullptr) {
        std::cerr << "DefReadEdadb::readIdbInstance failed, design or layout is nullptr!" << std::endl;
        return false;
    }

    IdbLayers* layer_list = layout->get_layers();
    IdbRegionList* region_list = design->get_region_list();
    IdbInstanceList* instance_list = design->get_instance_list();
    IdbCellMasterList* master_list = layout->get_cell_master_list();
    if (layer_list == nullptr || region_list == nullptr || instance_list == nullptr || master_list == nullptr) {
        std::cerr << "DefReadEdadb::readIdbInstance failed, required list is nullptr!" << std::endl;
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
        if (nullptr == _cur_cell_master || _cur_cell_master->get_name() != inst_sd._cell_master_name_sd) {
            _cur_cell_master = master_list->find_cell_master(inst_sd._cell_master_name_sd);
        }
        if (_cur_cell_master == nullptr) {
            delete inst;
            std::cerr << "DefReadEdadb::readIdbInstance failed to find cell master: "
                      << inst_sd._cell_master_name_sd << std::endl;
            return false;
        }
        inst->set_cell_master(_cur_cell_master);

        inst_sd.fromShadow(inst);

        if (!inst_sd._region_name_sd.empty()) {
            IdbRegion* region = region_list->find_region(inst_sd._region_name_sd);
            if (region != nullptr) {
                inst->set_region(region);
                region->add_instance(inst);
            }
        }

        if (inst_sd._route_halo_sd != nullptr) {
            IdbRouteHalo* route_halo = inst->set_route_halo(nullptr);
            inst_sd._route_halo_sd->fromShadow(route_halo);
            route_halo->set_layer_bottom(layer_list->find_layer(inst_sd._route_halo_sd->_layer_bottom_name_sd));
            route_halo->set_layer_top(layer_list->find_layer(inst_sd._route_halo_sd->_layer_top_name_sd));
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
            std::cout << "DefReadEdadb::readIdbPin failed to read!" << std::endl;
            return false;
        }

        edadb::Shadow<idb::IdbTerm>* term_sd = pin_sd._io_term_sd;
        if (term_sd == nullptr) {
            std::cerr << "DefReadEdadb::readIdbPin failed, term shadow is nullptr!" << std::endl;
            return false;
        }

        IdbPin* pin = pin_list->add_pin_list(nullptr);
        pin_sd.fromShadow(pin);
        IdbTerm* term = pin->get_term();

        int32_t bounding_box_ll_x = INT_MAX;
        int32_t bounding_box_ll_y = INT_MAX;
        int32_t bounding_box_ur_x = INT_MIN;
        int32_t bounding_box_ur_y = INT_MIN;
        int32_t coordinate_x = 0;
        int32_t coordinate_y = 0;
        int32_t layer_num = 0;

        for (edadb::Shadow<idb::IdbPort>* port_sd : term_sd->_port_list_sd) {
            IdbPort* port = term->add_port(nullptr);
            port_sd->fromShadow(port);

            for (auto& layer_shape_sd : port_sd->_layer_shape_list_sd) {
                IdbLayerShape* layer_shape = port->add_layer_shape();
                if (!layer_shape_sd->fromShadow(layer_shape)) {
                    std::cerr << "DefReadEdadb::readIdbPin failed to restore layer shape" << std::endl;
                    return false;
                }

                if (!term_sd->_has_port_sd) {
                    for (IdbRect* rect : layer_shape->get_rect_list()) {
                        bounding_box_ll_x = std::min(bounding_box_ll_x, rect->get_low_x());
                        bounding_box_ll_y = std::min(bounding_box_ll_y, rect->get_low_y());
                        bounding_box_ur_x = std::max(bounding_box_ur_x, rect->get_high_x());
                        bounding_box_ur_y = std::max(bounding_box_ur_y, rect->get_high_y());
                        coordinate_x += rect->get_low_x() + rect->get_high_x();
                        coordinate_y += rect->get_low_y() + rect->get_high_y();
                        ++layer_num;
                    }
                }
            }

            if (!port->get_layer_shape().empty()) {
                port->set_io_bounding_box();
            }
        }

        if (term_sd->_has_port_sd) {
            pin->set_port_layer_shape();
        } else if (layer_num > 0) {
            term->set_average_position(coordinate_x / (layer_num * 2),
                                       coordinate_y / (layer_num * 2));
            term->set_bounding_box(bounding_box_ll_x, bounding_box_ll_y,
                                   bounding_box_ur_x, bounding_box_ur_y);
            pin->set_bounding_box();
        }

        ++pin_count;
    }

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbPin restored pin_count="
              << pin_count << std::endl;
    return true;
}

bool DefReadEdadb::readIdbBlockage(void) {
    IdbDesign* design = _def_service->get_design();  // Def
    IdbLayout* layout = _def_service->get_layout();  // Lef
    if (design == nullptr || layout == nullptr) {
        std::cerr << "DefReadEdadb::readIdbBlockage failed, design or layout is nullptr!" << std::endl;
        return false;
    }

    IdbBlockageList* blockage_list = design->get_blockage_list();
    IdbInstanceList* instance_list = design->get_instance_list();
    IdbLayers* layer_list = layout->get_layers();
    if (blockage_list == nullptr || instance_list == nullptr || layer_list == nullptr) {
        std::cerr << "DefReadEdadb::readIdbBlockage failed, required list is nullptr!" << std::endl;
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
            std::cout << "DefReadEdadb::readIdbBlockage failed to read!" << std::endl;
            return false;
        }

        IdbBlockage* blockage = nullptr;
        if (blockage_sd._type_sd == idb::IdbBlockage::IdbBlockageType::kRoutingBlockage) {
            IdbRoutingBlockage* routing_blockage = blockage_list->add_blockage_routing(blockage_sd._layer_name_sd);
            blockage_sd.fromShadow(routing_blockage);
            routing_blockage->set_layer(layer_list->find_layer(blockage_sd._layer_name_sd));
            blockage = routing_blockage;
        } else if (blockage_sd._type_sd == idb::IdbBlockage::IdbBlockageType::kPlacementBlockage) {
            IdbPlacementBlockage* placement_blockage = blockage_list->add_blockage_placement();
            blockage_sd.fromShadow(placement_blockage);
            blockage = placement_blockage;
        } else {
            std::cerr << "DefReadEdadb::readIdbBlockage failed, unknown blockage type" << std::endl;
            return false;
        }

        if (!blockage_sd._instance_name_sd.empty()) {
            IdbInstance* inst = instance_list->find_instance(blockage_sd._instance_name_sd);
            if (inst == nullptr) {
                std::cerr << "DefReadEdadb::readIdbBlockage failed to find instance: "
                          << blockage_sd._instance_name_sd << std::endl;
                return false;
            }
            blockage->set_instance(inst);
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

    auto special_net_reader = edadb::makeReadAllOp<edadb::Shadow<idb::IdbSpecialNet>>();
    int32_t special_net_count = 0;
    int32_t segment_count = 0;
    while (true) {
        auto* special_net_sd = new edadb::Shadow<idb::IdbSpecialNet>();
        const int read_count = edadb::readNext<edadb::Shadow<idb::IdbSpecialNet>>(special_net_reader, special_net_sd);
        if (read_count == 0) {
            delete special_net_sd;
            break;
        }
        if (read_count < 0) {
            delete special_net_sd;
            std::cout << "DefReadEdadb::readIdbSpecialNet failed to read!" << std::endl;
            return false;
        }

        IdbSpecialNet* special_net = net_list->add_net(special_net_sd->_net_name_sd);
        if (special_net == nullptr) {
            std::cerr << "DefReadEdadb::readIdbSpecialNet failed to create special net: "
                      << special_net_sd->_net_name_sd << std::endl;
            delete special_net_sd;
            return false;
        }
        if (!special_net_sd->fromShadow(special_net)) {
            delete special_net_sd;
            return false;
        }
        segment_count += special_net_sd->getSegmentCount();

        delete special_net_sd;
        ++special_net_count;
    }

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbSpecialNet restored special_net_count="
              << special_net_count << " segment_count=" << segment_count << std::endl;
    return true;
} // readIdbSpecialNet

bool DefReadEdadb::readIdbNet(void) {
    IdbDesign* design = _def_service->get_design();  // Def
    IdbLayout* layout = _def_service->get_layout();  // Lef
    if (design == nullptr || layout == nullptr) {
        std::cerr << "DefReadEdadb::readIdbNet failed, design or layout is nullptr!" << std::endl;
        return false;
    }

    IdbLayers* layer_list = layout->get_layers();
    IdbVias* via_list_def = design->get_via_list();
    IdbVias* via_list_lef = layout->get_via_list();
    IdbPins* io_pin_list = design->get_io_pin_list();
    IdbInstanceList* instance_list = design->get_instance_list();
    IdbNetList* net_list = design->get_net_list();
    if (layer_list == nullptr || via_list_def == nullptr || via_list_lef == nullptr || io_pin_list == nullptr || instance_list == nullptr || net_list == nullptr) {
        std::cerr << "DefReadEdadb::readIdbNet failed, required list is nullptr!" << std::endl;
        return false;
    }

    auto net_reader = edadb::makeGenericQueryOp<edadb::Shadow<idb::IdbNet>>();
    net_reader.preparePredicate("ORDER BY \"_order_sd\"");
    int32_t net_count = 0;
    int32_t segment_count = 0;
    while (true) {
        auto* net_sd = new edadb::Shadow<idb::IdbNet>();
        const int read_count = edadb::readNext<edadb::Shadow<idb::IdbNet>>(net_reader, net_sd);
        if (read_count == 0) {
            delete net_sd;
            break;
        }
        if (read_count < 0) {
            delete net_sd;
            std::cout << "DefReadEdadb::readIdbNet failed to read!" << std::endl;
            return false;
        }

        IdbNet* net = net_list->add_net(net_sd->_net_name_sd);
        if (net == nullptr) {
            std::cout << "Create Net Error..." << std::endl;
            delete net_sd;
            return false;
        }

        net->set_original_net_name(net_sd->_original_net_name_sd);
        net->set_connect_type(net_sd->_connect_type_sd);
        net->set_source_type(net_sd->_source_type_sd);
        net->set_weight(net_sd->_weight_sd);
        net->set_xtalk(net_sd->_xtalk_sd);
        net->set_fix_bump(net_sd->_fix_bump_sd);
        net->set_frequency(net_sd->_frequency_sd);

        const int32_t num_connections = net_sd->_io_pin_name_list_sd.size() + net_sd->_instance_pin_list_sd.size();
        auto setPinNet = [net, num_connections](IdbPin* pin) {
            if (num_connections < 2) {
                if (pin->get_net() == nullptr) {
                    pin->set_net(net);
                }
            } else {
                pin->set_net(net);
            }
        };

        for (auto& pin_name_sd : net_sd->_io_pin_name_list_sd) {
            IdbPin* pin = io_pin_list->find_pin(pin_name_sd);
            if (pin != nullptr) {
                net->add_io_pin(pin);
                setPinNet(pin);
            }
        }

        std::sort(net_sd->_instance_pin_list_sd.begin(), net_sd->_instance_pin_list_sd.end(),
                  [](const auto& lhs, const auto& rhs) { return lhs._order_sd < rhs._order_sd; });
        for (auto& pin_ref_sd : net_sd->_instance_pin_list_sd) {
            IdbInstance* instance = instance_list->find_instance(pin_ref_sd.instance_name);
            if (instance != nullptr) {
                net->get_instance_list()->add_instance(instance);
                IdbPin* pin = instance->get_pin_by_term(pin_ref_sd.pin_name);
                if (pin != nullptr) {
                    net->add_instance_pin(pin);
                    setPinNet(pin);
                }
            }
        }

        IdbRegularWireList* wire_list = net->get_wire_list();
        for (auto wire_sd : net_sd->_wire_list_sd) {
            IdbRegularWire* wire = wire_list->add_wire(nullptr);
            wire->set_wire_state(wire_sd->_wire_state_sd);
            wire->set_shield_name(wire_sd->_shield_name_sd);
            wire->init(wire_sd->_segment_list_sd.size());

            for (auto segment_sd : wire_sd->_segment_list_sd) {
                IdbRegularWireSegment* segment = wire->add_segment(nullptr);
                segment->set_is_via(segment_sd->_is_via_sd);
                segment->set_is_rect(segment_sd->_is_rect_sd);

                if (!segment_sd->_layer_name_sd.empty()) {
                    segment->set_layer_name(segment_sd->_layer_name_sd);
                    IdbLayer* layer = layer_list->find_layer(segment_sd->_layer_name_sd);
                    if (layer == nullptr) {
                        std::cerr << "DefReadEdadb::readIdbNet failed to find layer: "
                                  << segment_sd->_layer_name_sd << std::endl;
                        delete net_sd;
                        return false;
                    }
                    segment->set_layer(layer);
                }

                for (size_t point_idx = 0; point_idx < segment_sd->_point_list_sd.size(); ++point_idx) {
                    auto point_sd = segment_sd->_point_list_sd.at(point_idx);
                    if (point_idx == _POINT_SECOND_ && segment_sd->_is_second_point_virtual_sd) {
                        segment->add_virtual_point(point_sd->get_x(), point_sd->get_y());
                    } else {
                        segment->add_point(point_sd->get_x(), point_sd->get_y());
                    }
                }

                if (segment_sd->_delta_rect_sd != nullptr) {
                    segment->set_delta_rect(segment_sd->_delta_rect_sd->get_low_x(), segment_sd->_delta_rect_sd->get_low_y(),
                                            segment_sd->_delta_rect_sd->get_high_x(), segment_sd->_delta_rect_sd->get_high_y());
                }

                if (segment_sd->_is_via_sd) {
                    IdbVia* via = via_list_def->find_via(segment_sd->_via_name_sd);
                    if (via == nullptr) {
                        via = via_list_lef->find_via(segment_sd->_via_name_sd);
                    }
                    if (via == nullptr) {
                        std::cerr << "DefReadEdadb::readIdbNet failed to find via: "
                                  << segment_sd->_via_name_sd << std::endl;
                        delete net_sd;
                        return false;
                    }

                    IdbVia* via_new = segment->copy_via(via);
                    if (via_new != nullptr) {
                        via_new->set_coordinate(segment->get_point_end());
                    }
                }

                ++segment_count;
            }
        }

        delete net_sd;
        ++net_count;
    }

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] readIdbNet restored net_count="
              << net_count << " segment_count=" << segment_count << std::endl;
    return true;
} // readIdbNet

#undef EDADB_IDB_DEBUG_STREAM

} // namespace idb
