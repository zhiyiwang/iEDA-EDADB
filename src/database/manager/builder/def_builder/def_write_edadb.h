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
  
    // writer
    bool writeDbToEdadb(const char* edadb_path);

protected: // 


}; // class DefWriteEdadb


}  // namespace idb
