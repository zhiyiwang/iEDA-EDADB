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
    int32_t writeIdbDesign(void);
    int32_t writeIdbDie(void);

}; // class DefWriteEdadb


}  // namespace idb
