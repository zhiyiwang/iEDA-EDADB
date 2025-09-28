/**
 * @file macro.h
 * @brief This file contains macros to ORM from iEDA using the edadb library.
 * @author Zhiyi Wang
 */

#include "macro.h"


namespace {

// use static object to set primary key config
struct EdadbPrimaryKeyConfig {
  EdadbPrimaryKeyConfig() {
    edadb::Cpp2SqlTypeTrait<idb::IdbRect>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<idb::IdbCoordinate<int32_t>>::hasPrimKey = false;
  }
};
// global object to set primary key config
const EdadbPrimaryKeyConfig kEdadbPrimaryKeyConfig{};
} // namespace






