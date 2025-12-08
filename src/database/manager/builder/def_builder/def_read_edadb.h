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
  
    /**
     * @brief build def from edadb database:
     *   Currently, use path to call createDbByDef to debug;
     *   Finally, will directly build def from edadb database.
     * @param edadb_path edadb database path
     * @param path def file path
     * @return true if success, false otherwise
     */
    bool createDbFromEdadb(const char* edadb_path, const char* path);

protected:
    /**
     * @brief create database by def file
     * @param path def file path
     * @return true if success, false otherwise
     */
    bool createDbByDef(const char* path);

    /**
     * @brief create database by edadb database
     * @param edadb_path edadb database path
     * @return true if success, false otherwise
     */
    bool createDbByEdadb(const char* edadb_path);

protected:
    bool readIdbDesign(void);
    bool readIdbDie(void);
    bool readIdbGCellGridList(void);
    bool readIdbVia(void);
    

}; // class DefReadEdadb



} // namespace idb