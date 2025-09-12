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

  bool createDbFromEdadb(const char* edadb_path, int edadb_idx);
}; // class DefReadEdadb

} // namespace idb
