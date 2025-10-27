/**
 * @file def_read_edadb.h
 * @author Zhiyi Wang (zhiyiwang@aliyun.com)
 * @brief  def read from edadb database
 * @version 0.1
 */

#include "def_write_edadb.h"


namespace idb {

DefWriteEdadb::DefWriteEdadb(IdbDefService* def_service, DefWriteType type) : DefWrite(def_service, type)
{
}


bool DefWriteEdadb::writeDb2Edadb(const char* edadb_path)
{
    if (_def_service == nullptr) {
        std::cerr << "Error: DefWriteEdadb::_def_service is nullptr" << std::endl;
        return false;
    }

    // init database
    if (!edadb::initDatabase(edadb_path)) {
        std::cerr << "Error: failed to init database from " << edadb_path << std::endl;
        return false;
    }

#if DEBUG_EDADB_OUTPUT
    std::cout << "EDADB: Def write to EDADB database : " << edadb_path << std::endl;
#endif 

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


    edadb::DbMap<edadb::Shadow<idb::IdbDesign>> design_map;
    edadb::Shadow<idb::IdbDesign> design_shadow;
    design_shadow.toShadow(design);

    // create table in edadb
#if DEBUG_EDADB_OUTPUT
    std::cout << "[DefWriteEdadb] write IdbDesign to edadb database" << std::endl;
#endif 
    if (!edadb::createTable(design_map)) {
        std::cerr << "DefWriteEdadb::writeIdbDesign failed to createTable" << std::endl;
        return kDbFail; 
    }

    // insert object
#if DEBUG_EDADB_OUTPUT
    std::cout << "[DefWriteEdadb] insert IdbDesign object to edadb database" << std::endl;
#endif 
    if (!edadb::insertObject(design_map, &design_shadow)) {
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

    edadb::DbMap< edadb::Shadow<idb::IdbCoordinate<int32_t>> > die_map;

    // create table in edadb
#if DEBUG_EDADB_OUTPUT
    std::cout << "[DefWriteEdadb] write IdbDie to edadb database" << std::endl;
#endif
    if (!edadb::createTable(die_map)) {
        std::cerr << "DefWriteEdadb::writeIdbDie failed to createTable" << std::endl;
        return kDbFail; 
    }

    // insert object
#if DEBUG_EDADB_OUTPUT
    std::cout << "[DefWriteEdadb] insert IdbDie object to edadb database" << std::endl;
#endif
    std::vector< edadb::Shadow<idb::IdbCoordinate<int32_t>>* > ps_vec;
    for (IdbCoordinate<int32_t>* point : die->get_points()) {
        edadb::Shadow<idb::IdbCoordinate<int32_t>>* ps = new edadb::Shadow<idb::IdbCoordinate<int32_t>>();
        ps->toShadow(point);
        ps_vec.push_back(ps);
    }

    if (!edadb::insertVector(die_map, ps_vec)) {
        std::cerr << "DefWriteEdadb::writeIdbDie failed to insertObject" << std::endl;
        return kDbFail;
    }

    for (edadb::Shadow<idb::IdbCoordinate<int32_t>>* ps : ps_vec) {
        delete ps;
    }

    return kDbSuccess;
} // writeIdbDie



}  // namespace idb
