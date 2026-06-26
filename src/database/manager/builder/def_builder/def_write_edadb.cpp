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

#if EDADB_OUTPUT_DEBUG
#define EDADB_IDB_DEBUG_STREAM std::cout
#else
#define EDADB_IDB_DEBUG_STREAM if (true) {} else std::cout
#endif

DefWriteEdadb::DefWriteEdadb(IdbDefService* def_service, DefWriteType type) : DefWrite(def_service, type)
{}


bool DefWriteEdadb::writeDb2Edadb(const char* edadb_path) {
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] DefWriteEdadb::writeDb2Edadb edadb_path="
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
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] DefWriteEdadb::writeDb2Edadb completed" << std::endl;
    return true;
} // writeDbToEdadb


bool DefWriteEdadb::writeChip2Edadb() {
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] writeChip2Edadb Design/Die/Row/TrackGrid/GCell/Via/Instance/Pin/Blockage/Region/Slot/Group/Fill/SpecialNet/Net enabled"
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
    if (writeSpecialNet() != kDbSuccess) {
        return false;
    }
    if (writeIdbNet() != kDbSuccess) {
        return false;
    }

    return true;
} // writeChip2Edadb


bool DefWriteEdadb::writeDbSynthesis2Edadb() {
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] writeDbSynthesis2Edadb Design enabled"
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
        std::cerr << "writeIdbDesign failed: design is nullptr" << std::endl;
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

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] writeIdbDesign insert name="
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

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] writeIdbDie insert point_count="
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
    vector<edadb::Shadow<idb::IdbRow>> row_sd_vec;
    row_sd_vec.reserve(row_vec.size());
    for (uint32_t row_idx = 0; row_idx < row_vec.size(); ++row_idx) {
        row_sd_vec.emplace_back();
        row_sd_vec.back().toShadow(row_vec[row_idx], &row_idx);
    }

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] writeIdbRow insert row_count="
              << row_sd_vec.size() << std::endl;

    if (!edadb::insertVector<edadb::Shadow<idb::IdbRow>>(row_sd_vec)) {
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
    for (uint32_t track_grid_idx = 0; track_grid_idx < track_grid_vec.size(); ++track_grid_idx) {
        track_grid_sd_vec.emplace_back();
        track_grid_sd_vec.back().toShadow(track_grid_vec[track_grid_idx], &track_grid_idx);
    }

    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] writeIdbTrackGrid insert track_grid_count="
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
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] writeIdbGCellGrid insert gcell_grid_count="
              << gcell_grid_vec.size() << std::endl;

    if (gcell_grid_vec.empty()) {
        return kDbSuccess;
    }

    vector<edadb::Shadow<idb::IdbGCellGrid>> gcell_grid_sd_vec;
    gcell_grid_sd_vec.reserve(gcell_grid_vec.size());
    for (uint32_t gcell_grid_idx = 0; gcell_grid_idx < gcell_grid_vec.size(); ++gcell_grid_idx) {
        gcell_grid_sd_vec.emplace_back();
        gcell_grid_sd_vec.back().toShadow(gcell_grid_vec[gcell_grid_idx], &gcell_grid_idx);
    }

    if (!edadb::insertVector<edadb::Shadow<idb::IdbGCellGrid>>(gcell_grid_sd_vec)) {
        std::cerr << "DefWriteEdadb::writeIdbGCellGrid failed to insertVector" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
}

int32_t DefWriteEdadb::writeIdbVia(void) {
    IdbDesign* design = _def_service->get_design();  // Def
    if (design == nullptr) {
        std::cerr << "writeIdbVia failed: design is nullptr" << std::endl;
        return kDbFail;
    }

    IdbVias* via_list = design->get_via_list();
    if (via_list == nullptr) {
      std::cout << "Write VIAS error" << std::endl;
      return kDbFail;
    }

    vector<IdbVia*>& via_vec = via_list->get_via_list();
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] writeIdbVia insert via_count="
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
        std::cerr << "writeIdbInstance failed: design is nullptr" << std::endl;
        return kDbFail;
    }

    IdbInstanceList* instance_list = design->get_instance_list();
    if (instance_list == nullptr) {
      std::cout << "Write COMPONENTS error..." << std::endl;
      return kDbFail;
    }

    vector<IdbInstance*>& inst_vec = instance_list->get_instance_list();
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] writeIdbInstance insert instance_count="
              << inst_vec.size() << std::endl;

    if (inst_vec.empty()) {
      return kDbSuccess;
    }

    vector<edadb::Shadow<idb::IdbInstance>*> inst_sd_vec;
    inst_sd_vec.reserve(inst_vec.size());
    for (uint32_t inst_idx = 0; inst_idx < inst_vec.size(); ++inst_idx) {
        auto* inst_sd = new edadb::Shadow<idb::IdbInstance>();
        inst_sd->toShadow(inst_vec[inst_idx], &inst_idx);
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
        std::cerr << "writeIdbPin failed: design is nullptr" << std::endl;
        return kDbFail;
    }

    IdbPins* pin_list = design->get_io_pin_list();
    if (pin_list == nullptr) {
      std::cout << "Write PINS error..." << std::endl;
      return kDbFail;
    }

    vector<IdbPin*>& pin_vec = pin_list->get_pin_list();
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] writeIdbPin insert pin_count="
              << pin_vec.size() << std::endl;

    if (pin_vec.empty()) {
      return kDbSuccess;
    }

    vector<edadb::Shadow<idb::IdbPin>*> pin_sd_vec;
    pin_sd_vec.reserve(pin_vec.size());
    for (uint32_t pin_idx = 0; pin_idx < pin_vec.size(); ++pin_idx) {
        auto* pin_sd = new edadb::Shadow<idb::IdbPin>();
        pin_sd->toShadow(pin_vec[pin_idx], &pin_idx);
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
        std::cerr << "writeIdbBlockage failed: design is nullptr" << std::endl;
        return kDbFail;
    }

    IdbBlockageList* blockage_list = design->get_blockage_list();
    if (blockage_list == nullptr) {
      std::cout << "Write BLOCKAGES error..." << std::endl;
      return kDbFail;
    }

    vector<IdbBlockage*> blockage_vec = blockage_list->get_blockage_list();
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] writeIdbBlockage insert blockage_count="
              << blockage_vec.size() << std::endl;

    if (blockage_vec.empty()) {
      return kDbSuccess;
    }

    vector<edadb::Shadow<idb::IdbBlockage>*> blockage_sd_vec;
    blockage_sd_vec.reserve(blockage_vec.size());
    int32_t blockage_order = 0;
    for (auto& blockage : blockage_vec) {
        auto* blockage_sd = new edadb::Shadow<idb::IdbBlockage>();
        blockage_sd->toShadow(blockage);
        blockage_sd->_order_sd = blockage_order++;
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
        std::cerr << "writeIdbRegion failed: design is nullptr" << std::endl;
        return kDbFail;
    }

    IdbRegionList* region_list = design->get_region_list();
    if (region_list == nullptr) {
      std::cout << "Write REGIONS error..." << std::endl;
      return kDbFail;
    }

    vector<IdbRegion*>& region_vec = region_list->get_region_list();
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] writeIdbRegion insert region_count="
              << region_vec.size() << std::endl;

    if (region_vec.empty()) {
      return kDbSuccess;
    }

    vector<edadb::Shadow<idb::IdbRegion>> region_sd_vec;
    region_sd_vec.reserve(region_vec.size());
    for (uint32_t region_idx = 0; region_idx < region_vec.size(); ++region_idx) {
        region_sd_vec.emplace_back();
        region_sd_vec.back().toShadow(region_vec[region_idx], &region_idx);
    }

    if (!edadb::insertVector<edadb::Shadow<idb::IdbRegion>>(region_sd_vec)) {
        std::cerr << "DefWriteEdadb::writeIdbRegion failed to insertVector" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
}

int32_t DefWriteEdadb::writeIdbSlot(void) {
    IdbDesign* design = _def_service->get_design();  // def
    if (design == nullptr) {
        std::cerr << "writeIdbSlot failed: design is nullptr" << std::endl;
        return kDbFail;
    }

    IdbSlotList* slot_list = design->get_slot_list();
    if (slot_list == nullptr) {
      std::cout << "Write SLOTS error..." << std::endl;
      return kDbFail;
    }

    vector<IdbSlot*>& slot_vec = slot_list->get_slot_list();
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] writeIdbSlot insert slot_count="
              << slot_vec.size() << std::endl;

    if (slot_vec.empty()) {
      return kDbSuccess;
    }

    vector<edadb::Shadow<idb::IdbSlot>*> slot_sd_vec;
    slot_sd_vec.reserve(slot_vec.size());
    for (uint32_t slot_idx = 0; slot_idx < slot_vec.size(); ++slot_idx) {
        auto* slot_sd = new edadb::Shadow<idb::IdbSlot>();
        slot_sd->toShadow(slot_vec[slot_idx], &slot_idx);
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
        std::cerr << "writeIdbGroup failed: design is nullptr" << std::endl;
        return kDbFail;
    }

    IdbGroupList* group_list = design->get_group_list();
    if (group_list == nullptr) {
      std::cout << "Write GROUPS error..." << std::endl;
      return kDbFail;
    }

    vector<IdbGroup*>& group_vec = group_list->get_group_list();
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] writeIdbGroup insert group_count="
              << group_vec.size() << std::endl;

    if (group_vec.empty()) {
      return kDbSuccess;
    }

    vector<edadb::Shadow<idb::IdbGroup>*> group_sd_vec;
    group_sd_vec.reserve(group_vec.size());
    for (uint32_t group_idx = 0; group_idx < group_vec.size(); ++group_idx) {
        auto* group_sd = new edadb::Shadow<idb::IdbGroup>();
        group_sd->toShadow(group_vec[group_idx], &group_idx);
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
        std::cerr << "writeIdbFill failed: design is nullptr" << std::endl;
        return kDbFail;
    }

    IdbFillList* fill_list = design->get_fill_list();
    if (fill_list == nullptr) {
      std::cout << "Write FILLS error..." << std::endl;
      return kDbFail;
    }

    vector<IdbFill*>& fill_vec = fill_list->get_fill_list();
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] writeIdbFill insert fill_count="
              << fill_vec.size() << std::endl;

    if (fill_vec.empty()) {
      return kDbSuccess;
    }

    vector<edadb::Shadow<idb::IdbFill>*> fill_sd_vec;
    fill_sd_vec.reserve(fill_vec.size());
    int32_t fill_order = 0;
    for (auto& fill : fill_vec) {
        auto* fill_sd = new edadb::Shadow<idb::IdbFill>();
        fill_sd->toShadow(fill);
        fill_sd->_order_sd = fill_order++;
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

int32_t DefWriteEdadb::writeSpecialNet(void) {
    IdbDesign* design = _def_service->get_design();  // def
    if (design == nullptr) {
        std::cerr << "writeSpecialNet failed: design is nullptr" << std::endl;
        return kDbFail;
    }

    IdbSpecialNetList* special_net_list = design->get_special_net_list();
    if (special_net_list == nullptr) {
      std::cout << "Write SPECIALNETS error..." << std::endl;
      return kDbFail;
    }

    vector<idb::IdbSpecialNet*>& special_net_vec = special_net_list->get_net_list();
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] writeSpecialNet insert special_net_count="
              << special_net_vec.size() << " segment_count="
              << special_net_list->get_segment_num() << std::endl;

    if (special_net_vec.empty()) {
      return kDbSuccess;
    }

    vector<edadb::Shadow<idb::IdbSpecialNet>*> special_net_sd_vec;
    special_net_sd_vec.reserve(special_net_vec.size());
    for (auto& special_net : special_net_vec) {
        auto* special_net_sd = new edadb::Shadow<idb::IdbSpecialNet>();
        special_net_sd->toShadow(special_net);
        special_net_sd_vec.emplace_back(special_net_sd);
    }

    bool ok = edadb::insertVector<edadb::Shadow<idb::IdbSpecialNet>>(special_net_sd_vec);
    for (auto& special_net_sd : special_net_sd_vec) {
        delete special_net_sd;
        special_net_sd = nullptr;
    }

    if (!ok) {
        std::cerr << "DefWriteEdadb::writeSpecialNet failed to insertVector" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
}

int32_t DefWriteEdadb::writeIdbNet(void) {
    IdbDesign* design = _def_service->get_design();  // def
    if (design == nullptr) {
        std::cerr << "writeIdbNet failed: design is nullptr" << std::endl;
        return kDbFail;
    }

    IdbNetList* net_list = design->get_net_list();
    if (net_list == nullptr) {
      std::cout << "Write NETS error..." << std::endl;
      return kDbFail;
    }

    vector<idb::IdbNet*>& net_vec = net_list->get_net_list();
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] writeIdbNet insert net_count="
              << net_vec.size() << " segment_count="
              << net_list->get_segment_num() << std::endl;

    if (net_vec.empty()) {
      return kDbSuccess;
    }

    vector<edadb::Shadow<idb::IdbNet>*> net_sd_vec;
    net_sd_vec.reserve(net_vec.size());
    for (auto& net : net_vec) {
        auto* net_sd = new edadb::Shadow<idb::IdbNet>();
        net_sd->toShadow(net);
        net_sd_vec.emplace_back(net_sd);
    }

    bool ok = edadb::insertVector<edadb::Shadow<idb::IdbNet>>(net_sd_vec);
    for (auto& net_sd : net_sd_vec) {
        delete net_sd;
        net_sd = nullptr;
    }

    if (!ok) {
        std::cerr << "DefWriteEdadb::writeIdbNet failed to insertVector" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
}


#undef EDADB_IDB_DEBUG_STREAM

}  // namespace idb
