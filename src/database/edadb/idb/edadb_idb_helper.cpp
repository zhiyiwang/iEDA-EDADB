/**
 * @file edadb_idb_helper.h
 * @brief This file contains helper functions for Idb and EDADB database operations.
 * @author Zhiyi Wang
 */

#include "edadb_idb_helper.h"

namespace edadb {
    idb::IdbDefService* EdadbIdbHelper::s_def_service = nullptr;
} // namespace edadb