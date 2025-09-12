/**
 * @file macro.h
 * @brief This file contains macros to ORM from iEDA using the edadb library.
 * @author Zhiyi Wang
 */

#pragma once

#include "../data/design/IdbDesign.h"

#include "../../third_party/edadb/include/edadb.h"



TABLE4CLASS(idb::IdbDesign, "IdbDesign", (_version));
