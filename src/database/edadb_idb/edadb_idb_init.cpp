/**
 * @file macro.h
 * @brief This file contains macros to ORM from iEDA using the edadb library.
 * @author Zhiyi Wang
 */

#include "edadb_idb_init.h"
#include "edadb_idb_schema.h"
#include "edadb_idb_shadow.h"


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

    if (initAllTables(false) < 0) {
        std::cerr << "Error: failed to initAllTables in edadb database" << std::endl;
        return -1; 
    }

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

    if (initAllTables(true) < 0) {
        std::cerr << "Error: failed to initAllTables in edadb database" << std::endl;
        return -1;
    }

    return 0;
} // init2write


void initPrimKeys(void) {
    edadb::Cpp2SqlTypeTrait<edadb::CppStrings>::hasPrimKey = false;

    edadb::Cpp2SqlTypeTrait<idb::IdbUnits>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<idb::IdbBusBitChars>::hasPrimKey = false;

    edadb::Cpp2SqlTypeTrait<edadb::Shadow<idb::IdbCoordinate<int32_t>>>::hasPrimKey = false;

    edadb::Cpp2SqlTypeTrait<idb::IdbTrack>::hasPrimKey = false;

    edadb::Cpp2SqlTypeTrait<idb::IdbGCellGrid>::hasPrimKey = false;

    edadb::Cpp2SqlTypeTrait<idb::IdbRect>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<idb::IdbViaMasterGenerate>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<idb::IdbViaMaster>::hasPrimKey = false;

#if 0
    edadb::Cpp2SqlTypeTrait<idb::IdbHalo>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<edadb::Shadow<idb::IdbRouteHalo>>::hasPrimKey = false;
#endif 

//-//    edadb::Cpp2SqlTypeTrait<idb::IdbSpecialNetEdgeSegment>::hasPrimKey = false;

} // initPrimKeys


template <typename T>
int createTable(bool crt_tab) {
#if EDADB_OUTPUT_DEBUG
    std::cout << "[EDADB]: create "<< T::static_table_name << " table in edadb database" << std::endl;
#endif
    edadb::DbMap<T> table_map;

    if (!crt_tab) {
        table_map.init();
    } else {
        if (!edadb::createTable(table_map)) {
            std::cerr << "DefWriteEdadb::writeIdbDesign failed to createTable" << std::endl;
            return -1;
        }
    } // if-else

    return 0;
} // createTable


#define EDADB_INIT_TABLE(T, CTR_TBL)          \
    do {                                      \
        if (edadb::createTable<T>(CTR_TBL) < 0){                      \
            std::fprintf(stderr, "[EDADB] createTable failed: %s\n",  \
                         typeid(T).name());                           \
            return -1;                                                \
        }                                                             \
    } while (0)


int initAllTables(bool crt_tab) {
    EDADB_INIT_TABLE(idb::IdbDesign, crt_tab);
    EDADB_INIT_TABLE(edadb::Shadow<idb::IdbDie>, crt_tab);
    EDADB_INIT_TABLE(idb::IdbRow, crt_tab);
    EDADB_INIT_TABLE(edadb::Shadow<idb::IdbTrackGrid>, crt_tab);
    EDADB_INIT_TABLE(idb::IdbGCellGrid, crt_tab);
    EDADB_INIT_TABLE(idb::IdbVia, crt_tab);

#if 0
    EDADB_INIT_TABLE(edadb::Shadow<idb::IdbInstance>, crt_tab);
    EDADB_INIT_TABLE(edadb::Shadow<idb::IdbPin>, crt_tab);
    EDADB_INIT_TABLE(edadb::Shadow<idb::IdbBlockage>, crt_tab);
    EDADB_INIT_TABLE(idb::IdbRegion, crt_tab);
    EDADB_INIT_TABLE(idb::IdbSlot, crt_tab);
    EDADB_INIT_TABLE(edadb::Shadow<idb::IdbGroup>, crt_tab);
    EDADB_INIT_TABLE(edadb::Shadow<idb::IdbFillLayer>, crt_tab);
#endif

    return 0;
} // createAllTables



} // namespace edadb