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
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] writeChip2Edadb Design/Die/Row/TrackGrid/GCell/Via/Region/Slot enabled"
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
    if (writeIdbRegion() != kDbSuccess) {
        return false;
    }
    if (writeIdbSlot() != kDbSuccess) {
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

    if (!edadb::insertVector<idb::IdbGCellGrid>(gcell_grid_vec)) {
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

    if (!edadb::insertVector<idb::IdbRegion>(region_vec)) {
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

#undef EDADB_IDB_DEBUG_STREAM

} // namespace idb
