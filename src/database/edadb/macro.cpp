/**
 * @file macro.h
 * @brief This file contains macros to ORM from iEDA using the edadb library.
 * @author Zhiyi Wang
 */

#include "macro.h"


namespace edadb {

void initPrimKeys(void) {
    edadb::Cpp2SqlTypeTrait<edadb::Shadow<idb::IdbUnits>>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<edadb::Shadow<idb::IdbBusBitChars>>::hasPrimKey = false;

    edadb::Cpp2SqlTypeTrait<edadb::Shadow<idb::IdbCoordinate<int32_t>>>::hasPrimKey = false;
}


} // edadb 