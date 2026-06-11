/**
 * @file edadb_core.h
 * @brief This file contains the core functions for EDADB database operations.
 * @author Zhiyi Wang
 */

#pragma once

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
 * @brief init database table for all iEDA class type.
 * @param crt_tab create table or not
 * @return 0 success; <0 fail
 */
int initAllTables(bool crt_tab = true);


/**
 * @brief Create a database table for the specified iEDA class type.
 * @param crt_tab create table or not
 * @return 0 success; <0 fail
 */
template <typename T>
int initTable(bool crt_tab);

} // namespace edadb

namespace idb::edadb_adapter {

/**
 * @brief Stable iEDA adapter entry for opening EDADB in read mode.
 * @return 0 success; <0 fail
 */
int initReadDb(const char* edadb_path);

/**
 * @brief Stable iEDA adapter entry for opening EDADB in write mode.
 * @return 0 success; <0 fail
 */
int initWriteDb(const char* edadb_path);

} // namespace idb::edadb_adapter
