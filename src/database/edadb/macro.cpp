/**
 * @file macro.h
 * @brief This file contains macros to ORM from iEDA using the edadb library.
 * @author Zhiyi Wang
 */

#include "macro.h"


namespace edadb {

int init2read(const char* edadb_path) {
#if EDADB_OUTPUT_DEBUG
    std::cout << "EDADB: Def read to EDADB database : " << edadb_path << std::endl;
#endif 
    if (!edadb::initDatabase(edadb_path)) {
        std::cerr << "Error: failed to init database from " << edadb_path << std::endl;
        return -1; 
    }

    initPrimKeys();

    return 0;
} // init2read


int init2write(const char* edadb_path) {
#if EDADB_OUTPUT_DEBUG
    std::cout << "EDADB: Def write to EDADB database : " << edadb_path << std::endl;
#endif
    if (!edadb::initDatabase(edadb_path)) {
        std::cerr << "Error: failed to init database from " << edadb_path << std::endl;
        return -1;
    }

    initPrimKeys();

    if (createAllTables() < 0) {
        std::cerr << "Error: failed to createAllTables in edadb database" << std::endl;
        return -1;
    }

    return 0;
} // init2write


void initPrimKeys(void) {
    edadb::Cpp2SqlTypeTrait<edadb::CppStrings>::hasPrimKey = false;

    edadb::Cpp2SqlTypeTrait<idb::IdbUnits>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<idb::IdbBusBitChars>::hasPrimKey = false;

    edadb::Cpp2SqlTypeTrait<idb::IdbCoordinate<int32_t>>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<idb::IdbRect>::hasPrimKey = false;

    edadb::Cpp2SqlTypeTrait<idb::IdbTrack>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<idb::IdbGCellGrid>::hasPrimKey = false;

    edadb::Cpp2SqlTypeTrait<idb::IdbViaMasterGenerate>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<idb::IdbViaMaster>::hasPrimKey = false;

//    edadb::Cpp2SqlTypeTrait<idb::IdbSpecialNetEdgeSegment>::hasPrimKey = false;

} // initPrimKeys



template <typename T>
int createTable(void) {
#if EDADB_OUTPUT_DEBUG
    std::cout << "[EDADB]: create "<< T::static_table_name << " table in edadb database" << std::endl;
#endif
    edadb::DbMap<T> table_map;
    table_map.init();

    if (!edadb::createTable(table_map)) {
        std::cerr << "DefWriteEdadb::writeIdbDesign failed to createTable" << std::endl;
        return -1;
    }

    return 0;
} // createTable


int createAllTables(void) {
    if (createTable<idb::IdbDesign>() < 0) {
        return -1;
    }

    if (createTable<edadb::Shadow<idb::IdbDie>>() < 0) {
        return -1;
    }

    if (createTable<idb::IdbRow>() < 0) {
        return -1;
    }

    if (createTable<idb::IdbRegion>() < 0) {
        return -1;
    }

    if (createTable<idb::IdbSlot>() < 0) {
        return -1;
    }

    if (createTable< edadb::Shadow<idb::IdbTrackGrid> >() < 0) {
        return -1;
    }

    if (createTable<idb::IdbGCellGrid>() < 0) {
        return -1;
    }

    if (createTable< edadb::Shadow<idb::IdbVia> >() < 0) {
        return -1;
    }

    return 0;
} // createAllTables



} // namespace edadb