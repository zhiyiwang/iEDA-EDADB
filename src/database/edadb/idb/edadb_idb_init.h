/**
 * @file edadb_core.h
 * @brief This file contains the core functions for EDADB database operations.
 * @author Zhiyi Wang
 */

#pragma once

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
