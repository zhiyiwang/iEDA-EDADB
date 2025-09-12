/**
 * @file def_read_edadb.h
 * @author Zhiyi Wang (zhiyiwang@ict.ac.cn)
*/

#include "def_read_edadb.h"

namespace idb {

DefReadEdadb::DefReadEdadb(IdbDefService* def_service) : DefRead(def_service)
{
}


bool DefReadEdadb::createDbFromEdadb(const char* edadb_path, int edadb_idx)
{
  if (_def_service == nullptr) {
    std::cerr << "Error: DefReadEdadb::_def_service is nullptr" << std::endl;
    return false;
  }

  IdbDesign* design = _def_service->get_design();
  if (design == nullptr) {
    std::cerr << "Error: DefReadEdadb::design is nullptr" << std::endl;
    return false;
  }

  std::cout << "==================================================" << std::endl;
  std::cout << "[DEF] read DEF using EDADB backend: " << edadb_path << std::endl;
  std::cout << "==================================================" << std::endl;

  // init database
  if (!edadb::initDatabase(edadb_path)) {
    std::cerr << "Error: failed to init database from " << edadb_path << std::endl;
    return false;
  }
  std::cout << "Info: succeeded to init database from " << edadb_path << std::endl;


  // TODO: use iEDA member instead local variable here
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

  std::cout << "==================================================" << std::endl;
  std::cout << "[DEF] read DEF using EDADB backend finished." << std::endl;
  std::cout << "==================================================" << std::endl;
  std::cout << std::endl;


  // update _def_service->design and layout using got read from edadb
  // then save to def file to compare with original def file
  design->set_version(got.get_version());


  return true;
} // createDbFromEdadb


} // namespace idb