/**
 * @file test_edadb.h
 * @brief This file contains test functions to test edadb read/write.
 * @author Zhiyi Wang
 */

#pragma once

#include "macro.h"

#if 0

//////// global object to test edadb read/write //////////////////////////////////
namespace test_edadb {


//////// init iEDA object funcs /////////////////////////////////
template <typename T>
void init(T*) = delete;

void init(idb::IdbUnits*);
//void init(idb::IdbPort*);
//void init(idb::IdbTerm*);
//
//void init(idb::IdbLayer*);
////void init(idb::IdbLayerShape*);

void init(idb::IdbDesign*);

//////// verify equal funcs /////////////////////////////////
template <typename T>
bool verifyEqual(const T*, const T*) = delete;

bool verifyEqual(const idb::IdbUnits*,  const idb::IdbUnits*);
//bool verifyEqual(const idb::IdbPort*,   const idb::IdbPort*);
//bool verifyEqual(const idb::IdbTerm*,   const idb::IdbTerm*);
//
//bool verifyEqual(const idb::IdbLayer*,  const idb::IdbLayer*);
////bool verifyEqual(const idb::IdbLayerShape*,  const idb::IdbLayerShape*);

bool verifyEqual(const idb::IdbDesign*, const idb::IdbDesign*);

}  // namespace test_edadb

#endif 
