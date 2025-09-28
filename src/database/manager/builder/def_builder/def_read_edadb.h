/**
 * @file def_read_edadb.h
 * @author Zhiyi Wang (zhiyiwang@aliyun.com)
 * @brief  def read from edadb database
 * @version 0.1
 */

#pragma once

#include "def_read.h"
#include "edadb.h"
#include "../../../edadb/macro.h"
#include "../../../edadb/test_edadb.h"

namespace idb {

class DefReadEdadb : public DefRead
{
public:
    DefReadEdadb(IdbDefService* def_service);
    virtual ~DefReadEdadb() = default;
  
    bool createDbFromEdadb(const char* edadb_path);

private: // test functions
    bool test2Read(const char* edadb_path);  

    template <typename T>
    bool test2Read(void);

}; // class DefReadEdadb


extern template bool DefReadEdadb::test2Read<IdbUnits> (void);
extern template bool DefReadEdadb::test2Read<IdbPort>  (void);
extern template bool DefReadEdadb::test2Read<IdbTerm>  (void);

extern template bool DefReadEdadb::test2Read<IdbLayer> (void);

extern template bool DefReadEdadb::test2Read<IdbDesign>(void);



} // namespace idb