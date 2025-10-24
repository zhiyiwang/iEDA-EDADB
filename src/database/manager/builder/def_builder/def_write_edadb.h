/**
 * @file def_write_edadb.h
 * @author Zhiyi Wang (zhiyiwang@aliyun.com)
 * @brief  def read from edadb database
 * @version 0.1
 */

#pragma once

#include "edadb.h"
#include "def_write.h"
#include "../../../edadb/macro.h"
#include "../../../edadb/test_edadb.h"

namespace idb {

class DefWriteEdadb : public DefWrite
{
public:
    DefWriteEdadb(IdbDefService* def_service, DefWriteType type = DefWriteType::kChip);
    virtual ~DefWriteEdadb() = default;
  
    // operator using edadb 
    bool writeDb2Edadb(const char* edadb_path);

protected: // writer
    bool writeChip2Edadb();
    bool writeDbSynthesis2Edadb();
    bool writeLef2Edadb();
 
protected:
    /**
     * write IdbDesign to edadb database, including DefWrite functions:
     *   write_design, write_busbit_char, write_units, write_version
     */
    int32_t writeIdbDesign();



}; // class DefWriteEdadb


}  // namespace idb
