/**
 * @file def_read_edadb.h
 * @author Zhiyi Wang (zhiyiwang@aliyun.com)
 * @brief  def read from edadb database
 * @version 0.1
 */

#include "def_write_edadb.h"


namespace idb {

DefWriteEdadb::DefWriteEdadb(IdbDefService* def_service, DefWriteType type) : DefWrite(def_service, type)
{}


bool DefWriteEdadb::writeDb2Edadb(const char* edadb_path) {
    if (_def_service == nullptr) {
        std::cerr << "Error: DefWriteEdadb::_def_service is nullptr" << std::endl;
        return false;
    }

    if (edadb::init2write(edadb_path) < 0) {
        std::cerr << "Error: DefWriteEdadb::writeDb2Edadb failed to init2write!" << std::endl;
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
        // todo 
        writeLef2Edadb();
        break;
      }
  
      default: {
        writeChip2Edadb();
        break;
      }
    }
    return true;
} // writeDbToEdadb


bool DefWriteEdadb::writeChip2Edadb() {
    writeIdbDesign();
    writeIdbDie();
    writeIdbRow();
    writeIdbTrackGrid();
    writeIdbGCellGrid();
    writeIdbVia();
    writeIdbRegion();
    writeIdbSlot();

    return true;
} // writeChip2Edadb


bool DefWriteEdadb::writeDbSynthesis2Edadb() {
    writeIdbDesign();

    return true;
} // writeDbSynthesis2Edadb


bool DefWriteEdadb::writeLef2Edadb() {
    return true;
}





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

    edadb::DbMap<edadb::Shadow<idb::IdbTrackGrid>> track_grid_map;
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


    edadb::DbMap< edadb::Shadow<idb::IdbVia> > via_map;
    via_map.init();

    // insert object in vector to edadb
#if EDADB_OUTPUT_DEBUG
    std::cout << "[DefWriteEdadb] insert IdbVia object to edadb database" << std::endl;
#endif

    vector<IdbVia*>& via_vec = via_list->get_via_list();
    vector<edadb::Shadow<idb::IdbVia>*> via_sd_vec;
    via_sd_vec.reserve( via_vec.size() );
    for (auto& via : via_vec) {
        edadb::Shadow<idb::IdbVia>* via_sd = new edadb::Shadow<idb::IdbVia>();
        via_sd->toShadow( via );
        via_sd_vec.emplace_back( via_sd );
    }

    if (!edadb::insertVector(via_map, via_sd_vec)) {
        std::cerr << "DefWriteEdadb::writeIdbVia failed to insertVector" << std::endl;
        return kDbFail;
    }

    for (auto& via_sd : via_sd_vec) {
        delete via_sd;
        via_sd = nullptr;
    }
    via_sd_vec.clear();

    return kDbSuccess;
} // writeIdbVia


int32_t DefWriteEdadb::writeIdbRegion(void) {
    IdbDesign* design = _def_service->get_design();  // def
    IdbRegionList* region_list = design->get_region_list();
    if (region_list == nullptr) {
      std::cout << "Write REGIONS error..." << std::endl;
      return kDbFail;
    }
    if (region_list->get_num() == 0) {
      std::cout << "No REGION To Write..." << std::endl;
      return kDbFail;
    }

    edadb::DbMap< idb::IdbRegion > region_map;
    region_map.init();
    if (!edadb::insertVector(region_map, region_list->get_region_list())) {
        std::cerr << "DefWriteEdadb::writeIdbRegion failed to insertVector" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
} // writeIdbRegion


int32_t DefWriteEdadb::writeIdbSlot(void) {
    IdbDesign* design = _def_service->get_design();  // def
    IdbSlotList* slot_list = design->get_slot_list();
    if (slot_list == nullptr) {
      std::cout << "Write SLOTS error..." << std::endl;
      return kDbFail;
    }
  
    if (slot_list->get_num() == 0) {
      std::cout << "No SLOT To Write..." << std::endl;
      return kDbFail;
    }

    edadb::DbMap< idb::IdbSlot > slot_map;
    slot_map.init();
    if (!edadb::insertVector(slot_map, slot_list->get_slot_list())) {
        std::cerr << "DefWriteEdadb::writeIdbSlot failed to insertVector" << std::endl;
        return kDbFail;
    }

    return kDbSuccess;
} // writeIdbSlot




#if 0
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
