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

namespace idb {

class DefReadEdadb : public DefRead
{
public:
    DefReadEdadb(IdbDefService* def_service);
    virtual ~DefReadEdadb() = default;
  
    bool createDbFromEdadb(const char* edadb_path);

private: // test functions
    bool test2Read(const char* edadb_path);  

    bool test2ReadIdbDesign(void);
    bool test2ReadIdbUnits(void);
    bool test2ReadIdbPort(void);
//    bool test2ReadIdbTerm(void);
}; // class DefReadEdadb

} // namespace idb
