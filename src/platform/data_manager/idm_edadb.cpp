/**
 * @file  idm_edadb.cpp
 * @brief DataManager methods for edadb integration
 * @author Zhiyi Wang
 * @version 1.0
 * @date 2025-09-10
 */

#include "idm.h"

namespace idm {

bool DataManager::readDefFromEdadb(const char* edadb_path, const char* path)
{
    if (_idb_builder == nullptr || _idb_lef_service == nullptr || _layout == nullptr) {
      return false;
    }

    // Similiar to bool DataManager::initDef(string def_path)
    _idb_def_service = _idb_builder->buildDefFromEdadb(edadb_path, path);
    _design = get_idb_design();

    /// make original coordinate on (0,0)
    if (isNeedTransformByDie()) {
      /// transform
      transformByDie();
    }
  
    return _idb_def_service == nullptr ? false : true;
} // readDefFromEdadb


bool DataManager::writeDefToEdadb(const char* edadb_path)
{
    if (_idb_builder == nullptr || _idb_lef_service == nullptr || _layout == nullptr) {
      return false;
    }

    return _idb_builder->writeDefToEdadb(edadb_path);
} // writeDefToEdadb


} // namespace idm