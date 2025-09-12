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


bool DefWriteEdadb::writeDbToEdadb(const char* edadb_path, DefWriteType type)
{
  if (_def_service == nullptr) {
    std::cerr << "Error: DefWriteEdadb::_def_service is nullptr" << std::endl;
    return false;
  }

  IdbDesign* design = _def_service->get_design();
  if (design == nullptr) {
    std::cerr << "Error: DefWriteEdadb::design is nullptr" << std::endl;
    return false;
  }


  std::cout << "=============================================" << std::endl;
  std::cout << "[iDM] write DEF using EDADB backend: " << edadb_path << std::endl;
  std::cout << "        with type: " << static_cast<int>(type) << std::endl;
  std::cout << "=============================================" << std::endl;

  // init database
  if (!edadb::initDatabase(edadb_path)) {
    std::cerr << "Error: failed to init database from " << edadb_path << std::endl;
    return false;
  }
  std::cout << "Info: succeeded to init database from " << edadb_path << std::endl;

  // create table IdbDesign
  edadb::DbMap<idb::IdbDesign> idb_design_dbmap;
  if (!edadb::createTable(idb_design_dbmap)) {
    std::cerr << "Error: failed to create table IdbDesign" << std::endl;
    return false;
  }
  std::cout << "Info: succeeded to create table IdbDesign" << std::endl;

  // insert design
  if (!edadb::insertObject(idb_design_dbmap, design)) {
    std::cerr << "Error: failed to insert IdbDesign" << std::endl;
    return false;
  }
  std::cout << "Info: succeeded to insert IdbDesign" << std::endl;
  std::cout << "===================================================" << std::endl;


  return true;
} // writeDbToEdadb

}  // namespace idb
