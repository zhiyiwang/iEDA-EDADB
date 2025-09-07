/**
 * @File Name: idm_edadb.cpp
 * @Brief :
 * @Author : zhiyi wang
 * @Version : 1.0
 */

#include "idm_edadb.h"
#include "macro.h"


namespace idm {

bool DataManagerEdadb::writeDefToEdadb(string path) {
  std::cout << "============================================" << std::endl;
  std::cout << "[iDM] write DEF using EDADB backend: " << path << std::endl;
  std::cout << "============================================" << std::endl;

  // init database
  if (!edadb::initDatabase(path)) {
    std::cerr << "Error: failed to init database from " << path << std::endl;
    return false;
  }
  std::cout << "Info: succeeded to init database from " << path << std::endl;

  // create table IdbDesign
  edadb::DbMap<idb::IdbDesign> idb_design_dbmap;
  if (!edadb::createTable(idb_design_dbmap)) {
    std::cerr << "Error: failed to create table IdbDesign" << std::endl;
    return false;
  }
  std::cout << "Info: succeeded to create table IdbDesign" << std::endl;

//TODO: _design is nullptr, since it is builded from def_path
//    _idb_def_service = _idb_builder->buildDef(def_path);
//    _design = get_idb_design();
//   IdbDesign* get_idb_design() { return _idb_def_service != nullptr ? _idb_def_service->get_design() : nullptr; }


  // insert _design
  if (!edadb::insertObject(idb_design_dbmap, _design)) {
    std::cerr << "Error: failed to insert IdbDesign" << std::endl;
    return false;
  }
  std::cout << "Info: succeeded to insert IdbDesign" << std::endl;
  std::cout << "===================================================" << std::endl;

  return true;
}



bool DataManagerEdadb::readDefFromEdadb(string path) {
  std::cout << "============================================" << std::endl;
  std::cout << "[iDM] read DEF using EDADB backend: " << path << std::endl;
  std::cout << "============================================" << std::endl;

  // init database
  if (!edadb::initDatabase(path)) {
    std::cerr << "Error: failed to init database from " << path << std::endl;
    return false;
  }
  std::cout << "Info: succeeded to init database from " << path << std::endl;

  // create table IdbDesign
  edadb::DbMap<idb::IdbDesign> idb_design_dbmap;
  edadb::DbMapReader<idb::IdbDesign> *idb_design_dbmap_reader = nullptr;
  idb::IdbDesign got;
  if (edadb::read2Scan(idb_design_dbmap_reader, idb_design_dbmap, &got) != 1) {
    std::cerr << "Error: failed to read IdbDesign" << std::endl;
    return false;
  }
  if (edadb::read2Scan(idb_design_dbmap_reader, idb_design_dbmap, &got) != 0) {
    std::cerr << "Error: more than one IdbDesign found" << std::endl;
    return false;
  }

  // compare _design and got are the same
  std::cout << "Info: succeeded to read IdbDesign" << std::endl;
  std::cout << "===================================================" << std::endl;


  // compare _design and got are the same
  assert(got._version == _design->_version);
  std::cout << "Comparing IdbDesign" << got._version << " with original IdbDesign" << _design->_version << std::endl;

  return true;
}


} // namespace idm