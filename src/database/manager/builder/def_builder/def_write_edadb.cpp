/**
 * @file def_read_edadb.h
 * @author Zhiyi Wang (zhiyiwang@aliyun.com)
 * @brief  def read from edadb database
 * @version 0.1
 */

#include "def_write_edadb.h"

#include "edadb.h"
#include "edadb_idb_schema.h"


namespace idb {

DefWriteEdadb::DefWriteEdadb(IdbDefService* def_service, DefWriteType type) : DefWrite(def_service, type)
{}


bool DefWriteEdadb::writeDb2Edadb(const char* edadb_path) {
    std::cout << "[EDADB-IDB] DefWriteEdadb::writeDb2Edadb edadb_path="
              << edadb_path << " type=" << static_cast<int>(_type) << std::endl;
    if (_def_service == nullptr) {
        std::cerr << "Error: DefWriteEdadb::_def_service is nullptr" << std::endl;
        return false;
    }

    if (edadb_adapter::initWriteDb(edadb_path) < 0) {
        std::cerr << "Error: DefWriteEdadb::writeDb2Edadb failed to initWriteDb!" << std::endl;
        return false;
    } 


    switch (_type) {
      case DefWriteType::kChip: {
        writeChip2Edadb();
        break;
      }
      case DefWriteType::kSynthesis: {
        writeDbSynthesis2Edadb();
        break;
      }
      case DefWriteType::kFloorplan:
      case DefWriteType::kGlobalPlace:
      case DefWriteType::kDetailPlace:
      case DefWriteType::kGlobalRouting:
      case DefWriteType::kDetailRouting: {
        writeChip2Edadb();
        break;
      }
      case DefWriteType::kLef: {
        //EDADB_TODO: implement LEF-to-EDADB persistence when LEF schema is defined.
        writeLef2Edadb();
        break;
      }
  
      default: {
        writeChip2Edadb();
        break;
      }
    }
    std::cout << "[EDADB-IDB] DefWriteEdadb::writeDb2Edadb completed" << std::endl;
    return true;
} // writeDbToEdadb


bool DefWriteEdadb::writeChip2Edadb() {
    std::cout << "[EDADB-IDB] writeChip2Edadb Design/Die/Row/TrackGrid/GCell/Via/Instance/Pin/Blockage/Region/Slot/Group/Fill enabled; other writeIdbXXX disabled"
              << std::endl;
    if (writeIdbDesign() != kDbSuccess) {
        return false;
    }
    if (writeIdbDie() != kDbSuccess) {
        return false;
    }
    if (writeIdbRow() != kDbSuccess) {
        return false;
    }
    if (writeIdbTrackGrid() != kDbSuccess) {
        return false;
    }
    if (writeIdbGCellGrid() != kDbSuccess) {
        return false;
    }
    if (writeIdbVia() != kDbSuccess) {
        return false;
    }
    if (writeIdbInstance() != kDbSuccess) {
        return false;
    }
    if (writeIdbPin() != kDbSuccess) {
        return false;
    }
    if (writeIdbBlockage() != kDbSuccess) {
        return false;
    }
    if (writeIdbRegion() != kDbSuccess) {
        return false;
    }
    if (writeIdbSlot() != kDbSuccess) {
        return false;
    }
    if (writeIdbGroup() != kDbSuccess) {
        return false;
    }
    if (writeIdbFill() != kDbSuccess) {
        return false;
    }

    return true;
} // writeChip2Edadb


bool DefWriteEdadb::writeDbSynthesis2Edadb() {
    std::cout << "[EDADB-IDB] writeDbSynthesis2Edadb Design enabled"
              << std::endl;
    if (writeIdbDesign() != kDbSuccess) {
        return false;
    }

    return true;
} // writeDbSynthesis2Edadb


bool DefWriteEdadb::writeLef2Edadb() {
    return true;
}





int32_t DefWriteEdadb::writeIdbDesign() {
    IdbDesign* design = _def_service->get_design();
    if (design == nullptr) {
        std::cerr << "[EDADB-IDB] writeIdbDesign failed: design is nullptr" << std::endl;
        return kDbFail;
    }

    IdbUnits* def_units = design->get_units();
    IdbUnits* lef_units = design->get_layout() == nullptr ? nullptr : design->get_layout()->get_units();
    if (def_units == nullptr && lef_units == nullptr) {
        std::cout << "Write UNITS error..." << std::endl;
        return kDbFail;
    }

    int32_t micron_dbu = def_units->get_micron_dbu() > 0
                              ? def_units->get_micron_dbu()
                              : lef_units->get_micron_dbu();
    if (micron_dbu <= 0) {
        std::cout << "Write UNITS error..." << std::endl;
        return kDbFail;
    }
    if (def_units->get_micron_dbu() <= 0) {
        def_units->set_microns_dbu(micron_dbu);
    }

    if (design->get_bus_bit_chars() == nullptr) {
        std::cout << "Write BUSBITCHARS error..." << std::endl;
        return kDbFail;
    }

    std::cout << "[EDADB-IDB] writeIdbDesign insert name="
              << design->get_design_name()
              << " version=" << design->get_version()
              << " micron_dbu=" << micron_dbu << std::endl;

    if (!edadb::insertObject<idb::IdbDesign>(design)) {
        std::cerr << "DefWriteEdadb::writeIdbDesign failed to insertObject" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
}

int32_t DefWriteEdadb::writeIdbDie(void) {
    IdbLayout* layout = _def_service->get_layout();
    IdbDie* die = layout->get_die();
    if (die == nullptr) {
       std::cout << "Write DIE error..." << std::endl;
       return kDbFail;
    }

    edadb::Shadow<idb::IdbDie> die_sd;
    die_sd.toShadow(die);

    std::cout << "[EDADB-IDB] writeIdbDie insert point_count="
              << die->get_points().size() << std::endl;

    if (!edadb::insertObject<edadb::Shadow<idb::IdbDie>>(&die_sd)) {
        std::cerr << "DefWriteEdadb::writeIdbDie failed to insertObject" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
}

int32_t DefWriteEdadb::writeIdbRow(void) {
    IdbLayout* layout = _def_service->get_layout();
    IdbRows* rows = layout->get_rows();
    if (rows == nullptr) {
      std::cout << "Write ROWS error..." << std::endl;
      return kDbFail;
    }

    vector<IdbRow*>& row_vec = rows->get_row_list();
    std::cout << "[EDADB-IDB] writeIdbRow insert row_count="
              << row_vec.size() << std::endl;

    if (!edadb::insertVector<idb::IdbRow>(row_vec)) {
        std::cerr << "DefWriteEdadb::writeIdbRow failed to insertVector" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
}

int32_t DefWriteEdadb::writeIdbTrackGrid(void) {
    IdbLayout* layout = _def_service->get_layout();
    IdbTrackGridList* track_grid_list = layout->get_track_grid_list();
    if (track_grid_list == nullptr) {
        std::cout << "Write Track Grid error..." << std::endl;
        return kDbFail;
    }

    vector<edadb::Shadow<idb::IdbTrackGrid>> track_grid_sd_vec;
    vector<IdbTrackGrid*>& track_grid_vec = track_grid_list->get_track_grid_list();
    track_grid_sd_vec.reserve(track_grid_vec.size());
    for (auto& track_grid : track_grid_vec) {
        track_grid_sd_vec.emplace_back();
        track_grid_sd_vec.back().toShadow(track_grid);
    }

    std::cout << "[EDADB-IDB] writeIdbTrackGrid insert track_grid_count="
              << track_grid_sd_vec.size() << std::endl;

    if (!edadb::insertVector<edadb::Shadow<idb::IdbTrackGrid>>(track_grid_sd_vec)) {
        std::cerr << "DefWriteEdadb::writeIdbTrackGrid failed to insertVector" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
}

int32_t DefWriteEdadb::writeIdbGCellGrid(void) {
    IdbLayout* layout = _def_service->get_layout();  // Lef
    IdbGCellGridList* gcell_grid_list = layout->get_gcell_grid_list();
    if (gcell_grid_list == nullptr) {
        std::cout << "Write GCELLGRID error..." << std::endl;
        return kDbFail;
    }

    vector<IdbGCellGrid*>& gcell_grid_vec = gcell_grid_list->get_gcell_grid_list();
    std::cout << "[EDADB-IDB] writeIdbGCellGrid insert gcell_grid_count="
              << gcell_grid_vec.size() << std::endl;

    if (gcell_grid_vec.empty()) {
        return kDbSuccess;
    }

    if (!edadb::insertVector<idb::IdbGCellGrid>(gcell_grid_vec)) {
        std::cerr << "DefWriteEdadb::writeIdbGCellGrid failed to insertVector" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
}

int32_t DefWriteEdadb::writeIdbVia(void) {
    IdbDesign* design = _def_service->get_design();  // Def
    if (design == nullptr) {
        std::cerr << "[EDADB-IDB] writeIdbVia failed: design is nullptr" << std::endl;
        return kDbFail;
    }

    IdbVias* via_list = design->get_via_list();
    if (via_list == nullptr) {
      std::cout << "Write VIAS error" << std::endl;
      return kDbFail;
    }

    vector<IdbVia*>& via_vec = via_list->get_via_list();
    std::cout << "[EDADB-IDB] writeIdbVia insert via_count="
              << via_vec.size() << std::endl;

    if (via_vec.empty()) {
      return kDbSuccess;
    }

    if (!edadb::insertVector<idb::IdbVia>(via_vec)) {
        std::cerr << "DefWriteEdadb::writeIdbVia failed to insertVector" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
}

int32_t DefWriteEdadb::writeIdbInstance(void) {
    IdbDesign* design = _def_service->get_design();  // Def
    if (design == nullptr) {
        std::cerr << "[EDADB-IDB] writeIdbInstance failed: design is nullptr" << std::endl;
        return kDbFail;
    }

    IdbInstanceList* instance_list = design->get_instance_list();
    if (instance_list == nullptr) {
      std::cout << "Write COMPONENTS error..." << std::endl;
      return kDbFail;
    }

    vector<IdbInstance*>& inst_vec = instance_list->get_instance_list();
    std::cout << "[EDADB-IDB] writeIdbInstance insert instance_count="
              << inst_vec.size() << std::endl;

    if (inst_vec.empty()) {
      return kDbSuccess;
    }

    vector<edadb::Shadow<idb::IdbInstance>*> inst_sd_vec;
    inst_sd_vec.reserve(inst_vec.size());
    for (auto& instance : inst_vec) {
        auto* inst_sd = new edadb::Shadow<idb::IdbInstance>();
        inst_sd->toShadow(instance);
        inst_sd_vec.emplace_back(inst_sd);
    }

    bool ok = edadb::insertVector<edadb::Shadow<idb::IdbInstance>>(inst_sd_vec);
    for (auto& inst_sd : inst_sd_vec) {
        delete inst_sd;
        inst_sd = nullptr;
    }

    if (!ok) {
        std::cerr << "DefWriteEdadb::writeIdbInstance failed to insertVector" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
}

int32_t DefWriteEdadb::writeIdbPin(void) {
    IdbDesign* design = _def_service->get_design();
    if (design == nullptr) {
        std::cerr << "[EDADB-IDB] writeIdbPin failed: design is nullptr" << std::endl;
        return kDbFail;
    }

    IdbPins* pin_list = design->get_io_pin_list();
    if (pin_list == nullptr) {
      std::cout << "Write PINS error..." << std::endl;
      return kDbFail;
    }

    vector<IdbPin*>& pin_vec = pin_list->get_pin_list();
    std::cout << "[EDADB-IDB] writeIdbPin insert pin_count="
              << pin_vec.size() << std::endl;

    if (pin_vec.empty()) {
      return kDbSuccess;
    }

    vector<edadb::Shadow<idb::IdbPin>*> pin_sd_vec;
    pin_sd_vec.reserve(pin_vec.size());
    for (auto& pin : pin_vec) {
        auto* pin_sd = new edadb::Shadow<idb::IdbPin>();
        pin_sd->toShadow(pin);
        pin_sd_vec.emplace_back(pin_sd);
    }

    bool ok = edadb::insertVector<edadb::Shadow<idb::IdbPin>>(pin_sd_vec);
    for (auto& pin_sd : pin_sd_vec) {
        delete pin_sd;
        pin_sd = nullptr;
    }

    if (!ok) {
        std::cerr << "DefWriteEdadb::writeIdbPin failed to insertVector" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
}

int32_t DefWriteEdadb::writeIdbBlockage(void) {
    IdbDesign* design = _def_service->get_design();
    if (design == nullptr) {
        std::cerr << "[EDADB-IDB] writeIdbBlockage failed: design is nullptr" << std::endl;
        return kDbFail;
    }

    IdbBlockageList* blockage_list = design->get_blockage_list();
    if (blockage_list == nullptr) {
      std::cout << "Write BLOCKAGES error..." << std::endl;
      return kDbFail;
    }

    vector<IdbBlockage*> blockage_vec = blockage_list->get_blockage_list();
    std::cout << "[EDADB-IDB] writeIdbBlockage insert blockage_count="
              << blockage_vec.size() << std::endl;

    if (blockage_vec.empty()) {
      return kDbSuccess;
    }

    vector<edadb::Shadow<idb::IdbBlockage>*> blockage_sd_vec;
    blockage_sd_vec.reserve(blockage_vec.size());
    for (auto& blockage : blockage_vec) {
        auto* blockage_sd = new edadb::Shadow<idb::IdbBlockage>();
        blockage_sd->toShadow(blockage);
        blockage_sd_vec.emplace_back(blockage_sd);
    }

    bool ok = edadb::insertVector<edadb::Shadow<idb::IdbBlockage>>(blockage_sd_vec);
    for (auto& blockage_sd : blockage_sd_vec) {
        delete blockage_sd;
        blockage_sd = nullptr;
    }

    if (!ok) {
        std::cerr << "DefWriteEdadb::writeIdbBlockage failed to insertVector" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
}

int32_t DefWriteEdadb::writeIdbRegion(void) {
    IdbDesign* design = _def_service->get_design();  // def
    if (design == nullptr) {
        std::cerr << "[EDADB-IDB] writeIdbRegion failed: design is nullptr" << std::endl;
        return kDbFail;
    }

    IdbRegionList* region_list = design->get_region_list();
    if (region_list == nullptr) {
      std::cout << "Write REGIONS error..." << std::endl;
      return kDbFail;
    }

    vector<IdbRegion*>& region_vec = region_list->get_region_list();
    std::cout << "[EDADB-IDB] writeIdbRegion insert region_count="
              << region_vec.size() << std::endl;

    if (region_vec.empty()) {
      return kDbSuccess;
    }

    if (!edadb::insertVector<idb::IdbRegion>(region_vec)) {
        std::cerr << "DefWriteEdadb::writeIdbRegion failed to insertVector" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
}

int32_t DefWriteEdadb::writeIdbSlot(void) {
    IdbDesign* design = _def_service->get_design();  // def
    if (design == nullptr) {
        std::cerr << "[EDADB-IDB] writeIdbSlot failed: design is nullptr" << std::endl;
        return kDbFail;
    }

    IdbSlotList* slot_list = design->get_slot_list();
    if (slot_list == nullptr) {
      std::cout << "Write SLOTS error..." << std::endl;
      return kDbFail;
    }

    vector<IdbSlot*>& slot_vec = slot_list->get_slot_list();
    std::cout << "[EDADB-IDB] writeIdbSlot insert slot_count="
              << slot_vec.size() << std::endl;

    if (slot_vec.empty()) {
      return kDbSuccess;
    }

    vector<edadb::Shadow<idb::IdbSlot>*> slot_sd_vec;
    slot_sd_vec.reserve(slot_vec.size());
    for (auto& slot : slot_vec) {
        auto* slot_sd = new edadb::Shadow<idb::IdbSlot>();
        slot_sd->toShadow(slot);
        slot_sd_vec.emplace_back(slot_sd);
    }

    bool ok = edadb::insertVector<edadb::Shadow<idb::IdbSlot>>(slot_sd_vec);
    for (auto& slot_sd : slot_sd_vec) {
        delete slot_sd;
        slot_sd = nullptr;
    }

    if (!ok) {
        std::cerr << "DefWriteEdadb::writeIdbSlot failed to insertVector" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
}

int32_t DefWriteEdadb::writeIdbGroup(void) {
    IdbDesign* design = _def_service->get_design();  // def
    if (design == nullptr) {
        std::cerr << "[EDADB-IDB] writeIdbGroup failed: design is nullptr" << std::endl;
        return kDbFail;
    }

    IdbGroupList* group_list = design->get_group_list();
    if (group_list == nullptr) {
      std::cout << "Write GROUPS error..." << std::endl;
      return kDbFail;
    }

    vector<IdbGroup*>& group_vec = group_list->get_group_list();
    std::cout << "[EDADB-IDB] writeIdbGroup insert group_count="
              << group_vec.size() << std::endl;

    if (group_vec.empty()) {
      return kDbSuccess;
    }

    vector<edadb::Shadow<idb::IdbGroup>*> group_sd_vec;
    group_sd_vec.reserve(group_vec.size());
    for (auto& group : group_vec) {
        auto* group_sd = new edadb::Shadow<idb::IdbGroup>();
        group_sd->toShadow(group);
        group_sd_vec.emplace_back(group_sd);
    }

    bool ok = edadb::insertVector<edadb::Shadow<idb::IdbGroup>>(group_sd_vec);
    for (auto& group_sd : group_sd_vec) {
        delete group_sd;
        group_sd = nullptr;
    }

    if (!ok) {
        std::cerr << "DefWriteEdadb::writeIdbGroup failed to insertVector" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
}

int32_t DefWriteEdadb::writeIdbFill(void) {
    IdbDesign* design = _def_service->get_design();  // def
    if (design == nullptr) {
        std::cerr << "[EDADB-IDB] writeIdbFill failed: design is nullptr" << std::endl;
        return kDbFail;
    }

    IdbFillList* fill_list = design->get_fill_list();
    if (fill_list == nullptr) {
      std::cout << "Write FILLS error..." << std::endl;
      return kDbFail;
    }

    vector<IdbFill*>& fill_vec = fill_list->get_fill_list();
    std::cout << "[EDADB-IDB] writeIdbFill insert fill_count="
              << fill_vec.size() << std::endl;

    if (fill_vec.empty()) {
      return kDbSuccess;
    }

    vector<edadb::Shadow<idb::IdbFill>*> fill_sd_vec;
    fill_sd_vec.reserve(fill_vec.size());
    for (auto& fill : fill_vec) {
        auto* fill_sd = new edadb::Shadow<idb::IdbFill>();
        fill_sd->toShadow(fill);
        fill_sd_vec.emplace_back(fill_sd);
    }

    bool ok = edadb::insertVector<edadb::Shadow<idb::IdbFill>>(fill_sd_vec);
    for (auto& fill_sd : fill_sd_vec) {
        delete fill_sd;
        fill_sd = nullptr;
    }

    if (!ok) {
        std::cerr << "DefWriteEdadb::writeIdbFill failed to insertVector" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
}

#if 0  //EDADB_TODO: keep the old DbMap-based write implementations for stepwise DbTableOp porting.
int32_t DefWriteEdadb::writeIdbDesign() {
    IdbDesign* design = _def_service->get_design();
    IdbUnits* def_units = design->get_units();
    IdbUnits* lef_units = design->get_layout()->get_units();
    if (def_units == nullptr && lef_units == nullptr) {
      std::cout << "Write UNITS error..." << std::endl;
      return kDbFail;
    }

    uint32_t def_microns = def_units->get_micron_dbu() > 0 ?
        def_units->get_micron_dbu() : lef_units->get_micron_dbu();
    if (def_microns <= 0) {
      std::cout << "Write UNITS error..." << std::endl;
  
      return kDbFail;
    }


    edadb::DbMap<idb::IdbDesign> design_map;
    design_map.init();

    // insert object
#if EDADB_OUTPUT_DEBUG
    std::cout << "[DefWriteEdadb] insert IdbDesign object to edadb database" << std::endl;
#endif 
    if (!edadb::insertObject(design_map, design)) {
        std::cerr << "DefWriteEdadb::writeIdbDesign failed to insertObject" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
} // writeIdbDesign


int32_t DefWriteEdadb::writeIdbDie(void) {
    IdbLayout* layout = _def_service->get_layout();
    IdbDie* die = layout->get_die();
    if (die == nullptr) {
       std::cout << "Write DIE error..." << std::endl;
       return kDbFail;
    }

    edadb::DbMap< edadb::Shadow<idb::IdbDie> > die_sd_map;
    die_sd_map.init();

    // insert object
#if EDADB_OUTPUT_DEBUG
    std::cout << "[DefWriteEdadb] insert IdbDie object to edadb database" << std::endl;
#endif
    edadb::Shadow<idb::IdbDie> die_sd;
    die_sd.toShadow(die);
    if (!edadb::insertObject(die_sd_map, &die_sd)) {
        std::cerr << "DefWriteEdadb::writeIdbDie failed to insertObject" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
} // writeIdbDie


int32_t DefWriteEdadb::writeIdbRow(void) {
    IdbLayout* layout = _def_service->get_layout();
    IdbRows* rows = layout->get_rows();
    if (rows == nullptr) {
      std::cout << "Write ROWS error..." << std::endl;
      return kDbFail;
    }

    edadb::DbMap< idb::IdbRow > row_map;
    row_map.init();

    vector<IdbRow*>& row_vec = rows->get_row_list();
    if (!edadb::insertVector(row_map, row_vec)) {
        std::cerr << "DefWriteEdadb::writeIdbRow failed to insertVector" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
} // writeIdbRow


int32_t DefWriteEdadb::writeIdbTrackGrid(void) {
    IdbLayout* layout = _def_service->get_layout();
    IdbTrackGridList* track_grid_list = layout->get_track_grid_list();
    if (track_grid_list == nullptr) {
        std::cout << "Write Track Grid error..." << std::endl;
        return kDbFail;
    }

    edadb::DbMap< edadb::Shadow<idb::IdbTrackGrid> > track_grid_map;
    track_grid_map.init();
    
    vector<edadb::Shadow<idb::IdbTrackGrid>*> track_grid_sd_vec;
    for (auto& track_grid : track_grid_list->get_track_grid_list()) {
        edadb::Shadow<idb::IdbTrackGrid>* track_grid_sd = new edadb::Shadow<idb::IdbTrackGrid>();
        track_grid_sd->toShadow( track_grid );
        track_grid_sd_vec.emplace_back( track_grid_sd );
    }

    if (!edadb::insertVector(track_grid_map, track_grid_sd_vec)) {
        std::cerr << "DefWriteEdadb::writeIdbTrackGrid failed to insertVector" << std::endl;
        return kDbFail;
    }

    for (auto& track_grid_sd : track_grid_sd_vec) {
        delete track_grid_sd;
        track_grid_sd = nullptr;
    }

    return kDbSuccess;
} // writeIdbTrackGrid


int32_t DefWriteEdadb::writeIdbGCellGrid(void) {
    IdbLayout* layout = _def_service->get_layout();  // Lef
    IdbGCellGridList* gcell_grid_list = layout->get_gcell_grid_list();
    if (gcell_grid_list == nullptr) {
        std::cout << "Write GCELLGRID error..." << std::endl;
        return kDbFail;
    }
  
    if (gcell_grid_list->get_gcell_grid_num() <= 0) {
        std::cout << "No GCELLGRID..." << std::endl;
        return kDbFail;
    }

    edadb::DbMap< idb::IdbGCellGrid > gcell_grid_map;
    gcell_grid_map.init();
    if (!edadb::insertVector(gcell_grid_map, gcell_grid_list->get_gcell_grid_list())) {
        std::cerr << "DefWriteEdadb::writeIdbGCellGrid failed to insertVector" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
} // writeIdbGCellGrid


int32_t DefWriteEdadb::writeIdbVia(void) {
    IdbDesign* design = _def_service->get_design();  // Def
    IdbVias* via_list = design->get_via_list();
    if (via_list == nullptr) {
      std::cout << "Write VIAS error" << std::endl;
      return kDbFail;
    }

    if (via_list->get_num_via() == 0) {
      std::cout << "No VIAS To Write..." << std::endl;
      return kDbFail;
    }


    edadb::DbMap< idb::IdbVia > via_map;
    via_map.init();

    // insert object in vector to edadb
#if EDADB_OUTPUT_DEBUG
    std::cout << "[DefWriteEdadb] insert IdbVia object to edadb database" << std::endl;
#endif

    vector<IdbVia*>& via_vec = via_list->get_via_list();

    if (!edadb::insertVector(via_map, via_vec)) {
        std::cerr << "DefWriteEdadb::writeIdbVia failed to insertVector" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
} // writeIdbVia
#endif


//--int32_t DefWriteEdadb::writeIdbInstance(void) {
//--    edadb::DbMap< edadb::Shadow<idb::IdbInstance> > instance_map;
//--    instance_map.init();
//--
//--#if EDADB_OUTPUT_DEBUG
//--    std::cout << "[DefWriteEdadb] insert IdbInstance object to edadb" << std::endl;
//--#endif 
//--
//--    IdbDesign* design = _def_service->get_design();  // Def
//--    IdbInstanceList* instance_list = design->get_instance_list();
//--    if (instance_list == nullptr) {
//--      std::cout << "Write COMPONENTS error..." << std::endl;
//--      return kDbFail;
//--    }
//--  
//--    if (instance_list->get_num() == 0) {
//--      std::cout << "No COMPONENT To Write..." << std::endl;
//--      return kDbFail;
//--    }
//--
//--    vector<idb::IdbInstance*>& inst_vec = instance_list->get_instance_list();
//--    vector<edadb::Shadow<idb::IdbInstance>*> inst_sd_vec;
//--    inst_sd_vec.reserve(inst_vec.size());
//--    for (auto &instance : inst_vec) {
//--        edadb::Shadow<idb::IdbInstance>* inst_sd = new edadb::Shadow<idb::IdbInstance>();
//--        inst_sd->toShadow( instance );
//--        inst_sd_vec.emplace_back( inst_sd );
//--    }
//--
//--    if (!edadb::insertVector(instance_map, inst_sd_vec)) {
//--        std::cerr << "DefWriteEdadb::writeComponent failed to insertVector" << std::endl;
//--        return kDbFail;
//--    }
//--
//--    for (auto& inst_sd : inst_sd_vec) {
//--        delete inst_sd;
//--        inst_sd = nullptr;
//--    }
//--    inst_sd_vec.clear();
//--
//--    return kDbSuccess;
//--} // writeIdbInstance
//--
//--
//--
//--int32_t DefWriteEdadb::writeIdbPin(void) {
//--    edadb::DbMap< edadb::Shadow<idb::IdbPin> > pin_map;
//--    pin_map.init();
//--
//--    IdbDesign* design = _def_service->get_design();
//--    IdbPins* pin_list = design->get_io_pin_list();
//--    if (pin_list == nullptr) {
//--      std::cout << "Write PINS error..." << std::endl;
//--      return kDbFail;
//--    }
//--
//--    vector<edadb::Shadow<idb::IdbPin>*> pin_sd_vec;
//--    for (auto& pin : pin_list->get_pin_list()) {
//--        edadb::Shadow<idb::IdbPin>* pin_sd = new edadb::Shadow<idb::IdbPin>();
//--        pin_sd->toShadow( pin );
//--        pin_sd_vec.emplace_back( pin_sd );
//--    }
//--
//--    if (!edadb::insertVector(pin_map, pin_sd_vec)) {
//--        std::cerr << "DefWriteEdadb::writeIdbPin failed to insertVector" << std::endl;
//--        return kDbFail;
//--    }
//--
//--    for (auto& pin_sd : pin_sd_vec) {
//--        delete pin_sd;
//--        pin_sd = nullptr;
//--    }
//--
//--    return kDbSuccess;
//--} // writeIdbPin
//--
//--
//--
//--int32_t DefWriteEdadb::writeIdbBlockage(void) {
//--    IdbDesign* design = _def_service->get_design();
//--    IdbBlockageList* blockage_list = design->get_blockage_list();
//--    if (blockage_list == nullptr) {
//--      std::cout << "Write BLOCKAGES error..." << std::endl;
//--      return kDbFail;
//--    }
//--
//--    edadb::DbMap< edadb::Shadow<idb::IdbBlockage> > blockage_map;
//--    blockage_map.init();
//--
//--    vector<edadb::Shadow<idb::IdbBlockage>*> blockage_sd_vec;
//--    for (auto& blockage : blockage_list->get_blockage_list()) {
//--        edadb::Shadow<idb::IdbBlockage>* blockage_sd = new edadb::Shadow<idb::IdbBlockage>();
//--        blockage_sd->toShadow( blockage );
//--        blockage_sd_vec.emplace_back( blockage_sd );
//--    }
//--
//--    if (!edadb::insertVector(blockage_map, blockage_sd_vec)) {
//--        std::cerr << "DefWriteEdadb::writeIdbBlockage failed to insertVector" << std::endl;
//--        return kDbFail;
//--    }
//--
//--    for (auto& blockage_sd : blockage_sd_vec) {
//--        delete blockage_sd;
//--        blockage_sd = nullptr;
//--    }
//--
//--    return kDbSuccess;
//--} // writeIdbBlockage
//--
//--
//--
//--int32_t DefWriteEdadb::writeIdbRegion(void) {
//--    IdbDesign* design = _def_service->get_design();  // def
//--    IdbRegionList* region_list = design->get_region_list();
//--    if (region_list == nullptr) {
//--      std::cout << "Write REGIONS error..." << std::endl;
//--      return kDbFail;
//--    }
//--    if (region_list->get_num() == 0) {
//--      std::cout << "No REGION To Write..." << std::endl;
//--      return kDbFail;
//--    }
//--
//--    edadb::DbMap< idb::IdbRegion > region_map;
//--    region_map.init();
//--    if (!edadb::insertVector(region_map, region_list->get_region_list())) {
//--        std::cerr << "DefWriteEdadb::writeIdbRegion failed to insertVector" << std::endl;
//--        return kDbFail;
//--    }
//--
//--    return kDbSuccess;
//--} // writeIdbRegion
//--
//--
//--
//--int32_t DefWriteEdadb::writeIdbSlot(void) {
//--    IdbDesign* design = _def_service->get_design();  // def
//--    IdbSlotList* slot_list = design->get_slot_list();
//--    if (slot_list == nullptr) {
//--      std::cout << "Write SLOTS error..." << std::endl;
//--      return kDbFail;
//--    }
//--  
//--    if (slot_list->get_num() == 0) {
//--      std::cout << "No SLOT To Write..." << std::endl;
//--      return kDbFail;
//--    }
//--
//--    edadb::DbMap< idb::IdbSlot > slot_map;
//--    slot_map.init();
//--    if (!edadb::insertVector(slot_map, slot_list->get_slot_list())) {
//--        std::cerr << "DefWriteEdadb::writeIdbSlot failed to insertVector" << std::endl;
//--        return kDbFail;
//--    }
//--
//--    return kDbSuccess;
//--} // writeIdbSlot
//--
//--
//--
//--int32_t DefWriteEdadb::writeIdbGroup(void) {
//--    edadb::DbMap< edadb::Shadow<idb::IdbGroup> > group_map;
//--    group_map.init();
//--
//--
//--    IdbDesign* design = _def_service->get_design();  // def
//--    IdbGroupList* group_list = design->get_group_list();
//--    if (group_list == nullptr) {
//--      std::cout << "Write GROUPS error..." << std::endl;
//--      return kDbFail;
//--    }
//--    if (group_list->get_num() == 0) {
//--      std::cout << "No GROUP To Write..." << std::endl;
//--      return kDbFail;
//--    }
//--
//--    vector<edadb::Shadow<idb::IdbGroup>*> group_sd_vec;
//--    for (auto& group : group_list->get_group_list()) {
//--        edadb::Shadow<idb::IdbGroup>* group_sd = new edadb::Shadow<idb::IdbGroup>();
//--        group_sd->toShadow( group );
//--        group_sd_vec.emplace_back( group_sd );
//--    }
//--
//--    if (!edadb::insertVector(group_map, group_sd_vec)) {
//--        std::cerr << "DefWriteEdadb::writeIdbGroup failed to insertVector" << std::endl;
//--        return kDbFail;
//--    }
//--
//--    for (auto& group_sd : group_sd_vec) {
//--        delete group_sd;
//--        group_sd = nullptr;
//--    }
//--
//--    return kDbSuccess;
//--} // writeIdbGroup
//--
//--
//--
//--int32_t DefWriteEdadb::writeIdbFill(void) {
//--    IdbDesign* design = _def_service->get_design();  // def
//--    IdbFillList* fill_list = design->get_fill_list();
//--    if (fill_list == nullptr) {
//--      std::cout << "Write FILLS error..." << std::endl;
//--      return kDbFail;
//--    }
//--
//--    if (fill_list->get_num_fill() == 0) {
//--      std::cout << "No FILL To Write..." << std::endl;
//--      return kDbFail;
//--    }
//--
//--    edadb::DbMap< edadb::Shadow<idb::IdbFill> > fill_map;
//--    fill_map.init();
//--
//--    vector<edadb::Shadow<idb::IdbFill>*> fill_sd_vec; 
//--    for (auto& fill : fill_list->get_fill_list()) {
//--        edadb::Shadow<idb::IdbFill>* fill_sd = new edadb::Shadow<idb::IdbFill>();
//--        fill_sd->toShadow( fill );
//--        fill_sd_vec.emplace_back( fill_sd );
//--    }
//--
//--    if (!edadb::insertVector(fill_map, fill_sd_vec)) {
//--        std::cerr << "DefWriteEdadb::writeIdbFill failed to insertVector" << std::endl;
//--        return kDbFail;
//--    }
//--
//--    for (auto& fill_sd : fill_sd_vec) {
//--        delete fill_sd;
//--        fill_sd = nullptr;
//--    }
//--    fill_sd_vec.clear();
//--
//--    return kDbSuccess;
//--} // writeIdbFill
//--


#if 0  //EDADB_TODO: restore special-net EDADB write when net/special-net persistence is implemented.
int32_t DefWriteEdadb::writeSpecialNet(void) {
    IdbSpecialNetList* special_net_list = _def_service->get_design()->get_special_net_list();
    if (special_net_list == nullptr || special_net_list->get_num() == 0) {
      std::cout << "No SPECIALNETS..." << std::endl;
      return kDbFail;
    }

    edadb::DbMap< edadb::Shadow<idb::IdbSpecialNet> > special_net_map;
    special_net_map.init();

    vector<idb::IdbSpecialNet*>& special_net_vec = special_net_list->get_net_list();
    vector<edadb::Shadow<idb::IdbSpecialNet>*> special_net_sd_vec; 
    for (auto& special_net : special_net_vec) {
        edadb::Shadow<idb::IdbSpecialNet>* special_net_sd = new edadb::Shadow<idb::IdbSpecialNet>();
        special_net_sd->toShadow( special_net );
        special_net_sd_vec.emplace_back( special_net_sd );
    }

    if (!edadb::insertVector(special_net_map, special_net_sd_vec)) {
        std::cerr << "DefWriteEdadb::writeSpecialNet failed to insertVector" << std::endl;
        return kDbFail;
    }

    for (auto& special_net_sd : special_net_sd_vec) {
        delete special_net_sd;
        special_net_sd = nullptr;
    }
    special_net_sd_vec.clear();

    return kDbSuccess;
} // writeSpecialNet
#endif 


}  // namespace idb
