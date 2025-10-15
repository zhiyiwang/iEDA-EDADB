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

#if 0
private: // test functions
    bool test2Read(const char* edadb_path);  

    template <typename T>
    bool test2Read(void);
#endif 
}; // class DefReadEdadb


#if 0
extern template bool DefReadEdadb::test2Read<IdbUnits> (void);
//extern template bool DefReadEdadb::test2Read<IdbPort>  (void);
//extern template bool DefReadEdadb::test2Read<IdbTerm>  (void);
//
//extern template bool DefReadEdadb::test2Read<IdbLayer> (void);
//extern template bool DefReadEdadb::test2Read<IdbLayerShape>(void);

extern template bool DefReadEdadb::test2Read<IdbDesign>(void);
#endif 


} // namespace idb