/**
 * @file macro.h
 * @brief This file contains macros to ORM from iEDA using the edadb library.
 * @author Zhiyi Wang
 */

#include "macro.h"


edadb::Cpp2SqlTypeTrait<idb::IdbRect>::setHasPrimKey(false); // no primary key

edadb::Cpp2SqlTypeTrait<idb::IdbCoordinate<int32_t>>::setHasPrimKey(false); // no primary key




