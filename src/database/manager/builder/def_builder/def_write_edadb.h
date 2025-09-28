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
    bool writeDbToEdadb(const char* edadb_path, DefWriteType type);

private: // test functions
    bool test2Write(const char* edadb_path, DefWriteType type);

    template <typename T>
    bool test2Write(void);
}; // class DefWriteEdadb


extern template bool DefWriteEdadb::test2Write<IdbUnits> (void);
extern template bool DefWriteEdadb::test2Write<IdbPort>  (void);
extern template bool DefWriteEdadb::test2Write<IdbTerm>  (void);

extern template bool DefWriteEdadb::test2Write<IdbLayer> (void);

extern template bool DefWriteEdadb::test2Write<IdbDesign>(void);

}  // namespace idb
