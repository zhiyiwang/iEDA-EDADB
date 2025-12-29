/**
 * @file edadb_core.h
 * @brief This file contains the core functions for EDADB database operations.
 * @author Zhiyi Wang
 */

#pragma once

//#include "../../third_party/edadb/include/edadb.h"
#include "edadb.h"

//////// global init function ////////////////////////////////////////
namespace edadb {

/**
 * @brief Initialize the edadb database to read data.
 * @return 0 success; <0 fail
 */
int init2read(const char* edadb_path);

/**
 * @brief Initialize the edadb database to write data.
 * @return 0 success; <0 fail
 */
int init2write(const char* edadb_path);


/**
 * @brief Initialize primary key settings for iEDA classes in edadb.
 */
void initPrimKeys(void);

/**
 * @brief Create a database table for the specified iEDA class type.
 * @return 0 success; <0 fail
 */
template <typename T>
int createTable(void);

/**
 * @brief Create a database table for the specified iEDA class type.
 * @return 0 success; <0 fail
 */
int createAllTables(void);

} // namespace edadb

