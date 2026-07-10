/**
 * @file def_write_edadb.h
 * @author Zhiyi Wang (zhiyiwang@aliyun.com)
 * @brief  def read from edadb database
 * @version 0.1
 */

#pragma once

#include "def_write.h"
#include "edadb_idb_init.h"

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
    int32_t writeIdbRow(void);
    int32_t writeIdbTrackGrid(void);
    int32_t writeIdbGCellGrid(void);
    int32_t writeIdbVia(void);
    int32_t writeIdbInstance(void); 
    int32_t writeIdbPin(void);
    int32_t writeIdbBlockage(void);
    int32_t writeIdbRegion(void);
    int32_t writeIdbSlot(void);
    int32_t writeIdbGroup(void);
    int32_t writeIdbFill(void);
    int32_t writeIdbSpecialNet(void);
    int32_t writeIdbNet(void);
}; // DefWriteEdadb


}  // namespace idb
