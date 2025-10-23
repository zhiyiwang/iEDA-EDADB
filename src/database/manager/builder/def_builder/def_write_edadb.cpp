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


bool DefWriteEdadb::writeDbToEdadb(const char* edadb_path)
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

    std::cout << "DEADB: Def write to EDADB database : " << edadb_path << std::endl;

#if 0
   switch (_type) {
     case DefWriteType::kChip: {
//       writeChip();
       break;
     }
     case DefWriteType::kSynthesis: {
//       writeDbSynthesis();
       break;
     }
     case DefWriteType::kFloorplan:
     case DefWriteType::kGlobalPlace:
     case DefWriteType::kDetailPlace:
     case DefWriteType::kGlobalRouting:
     case DefWriteType::kDetailRouting: {
//       writeChip();
       break;
     }
     case DefWriteType::kLef: {
//       writeLef();
       break;
     }
 
     default: {
//       writeChip();
       break;
     }
   }
#endif
    return true;
} // writeDbToEdadb



}  // namespace idb
