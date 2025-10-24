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

//    if (!createDbByDef(path)) {
//        std::cerr << "Error: DefReadEdadb::createDbFromEdadb failed!" << std::endl; 
//        return false;
//    }

    if (!createDbByEdadb(edadb_path)) {
        std::cerr << "Error: DefReadEdadb::createDbFromEdadb failed!" << std::endl;
        return false;
    }
  
    return true;
} // createDbFromEdadb



bool DefReadEdadb::createDbByDef(const char* path) {
    return DefRead::createDb(path);
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
    std::cout << "EDADB DefReadEdadb::readIdbDesign read design name: " << got.get_design_name() << std::endl;
    std::cout << "EDADB DefReadEdadb::readIdbDesign read design version: " << got.get_version() << std::endl;

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



} // namespace idb
