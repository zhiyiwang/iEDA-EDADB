/**
 * @file macro.h
 * @brief This file contains macros to ORM from iEDA using the edadb library.
 * @author Zhiyi Wang
 */

#include "edadb_idb_init.h"
#include "edadb.h"
#include "edadb_idb_schema.h"
#include "edadb_idb_shadow.h"


namespace edadb {

int init2read(const char* edadb_path) {
    return idb::edadb_adapter::initReadDb(edadb_path);
} // init2read


int init2write(const char* edadb_path) {
    return idb::edadb_adapter::initWriteDb(edadb_path);
} // init2write


void initPrimKeys(void) {
//EDADB_TODO: C currently does not register iDB object schemas; restore these
// primary-key rules together with the matching schema/read/write path.
#if 0
    edadb::Cpp2SqlTypeTrait<edadb::CppStrings>::hasPrimKey = false;

    edadb::Cpp2SqlTypeTrait<idb::IdbUnits>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<idb::IdbBusBitChars>::hasPrimKey = false;

    edadb::Cpp2SqlTypeTrait<edadb::Shadow<idb::IdbCoordinate<int32_t>>>::hasPrimKey = false;

    edadb::Cpp2SqlTypeTrait<idb::IdbTrack>::hasPrimKey = false;

    edadb::Cpp2SqlTypeTrait<idb::IdbGCellGrid>::hasPrimKey = false;

    edadb::Cpp2SqlTypeTrait<idb::IdbRect>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<idb::IdbViaMasterGenerate>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<idb::IdbViaMaster>::hasPrimKey = false;
#endif

#if 0  //EDADB_TODO: restore these primary-key rules when the corresponding shadow classes are enabled.
    edadb::Cpp2SqlTypeTrait<idb::IdbHalo>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<edadb::Shadow<idb::IdbRouteHalo>>::hasPrimKey = false;
#endif 

//-//    edadb::Cpp2SqlTypeTrait<idb::IdbSpecialNetEdgeSegment>::hasPrimKey = false;

} // initPrimKeys


template <typename T>
int initTable(bool crt_tab) {
    const std::string& table_name = edadb::TypeMetaData<edadb::StoreTypeOf<T>>::table_name();
#if EDADB_OUTPUT_DEBUG
    std::cout << "[EDADB-IDB] schema table=" << table_name
              << " create=" << (crt_tab ? "true" : "false") << std::endl;
#endif

    if (!crt_tab) {
        const edadb::DbTableDefBase* table_def = edadb::getTableDef<T>();
        if (table_def == nullptr) {
            std::cerr << "[EDADB-IDB] failed to map table definition: " << table_name << std::endl;
            return -1;
        }
    } else {
        if (!edadb::createTable<T>()) {
            std::cerr << "[EDADB-IDB] failed to create table: " << table_name << std::endl;
            return -1;
        }
    } // if-else

    return 0;
} // initTable


#define EDADB_INIT_TABLE(T, CTR_TBL)          \
    do {                                      \
        if (edadb::initTable<T>(CTR_TBL) < 0){                        \
            std::fprintf(stderr, "[EDADB] createTable failed: %s\n",  \
                         typeid(T).name());                           \
            return -1;                                                \
        }                                                             \
    } while (0)


int initAllTables(bool crt_tab) {
#if EDADB_OUTPUT_DEBUG
    std::cout << "[EDADB-IDB] initAllTables create=" << (crt_tab ? "true" : "false") << std::endl;
#endif
    std::cout << "[EDADB-IDB] initAllTables empty framework; no iDB object tables registered"
              << std::endl;

#if 0  //EDADB_TODO: restore these basic tables when read/write paths are ported to DbTableOp.
    EDADB_INIT_TABLE(idb::IdbDesign, crt_tab);
    EDADB_INIT_TABLE(edadb::Shadow<idb::IdbDie>, crt_tab);
    EDADB_INIT_TABLE(idb::IdbRow, crt_tab);
    EDADB_INIT_TABLE(edadb::Shadow<idb::IdbTrackGrid>, crt_tab);
    EDADB_INIT_TABLE(idb::IdbGCellGrid, crt_tab);
    EDADB_INIT_TABLE(idb::IdbVia, crt_tab);
#endif

#if 0  //EDADB_TODO: enable these tables when instance/pin/blockage/fill persistence is restored.
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


namespace idb::edadb_adapter {

int initReadDb(const char* edadb_path) {
    std::cout << "[EDADB-IDB] initReadDb path=" << edadb_path << std::endl;
    std::cout << "[EDADB-IDB] core api=DbTableOp commit-target=293c162" << std::endl;
    if (!edadb::initDatabase(edadb_path)) {
        std::cerr << "Error: failed to init database from " << edadb_path << std::endl;
        return -1; 
    }

    edadb::initPrimKeys();

    if (edadb::initAllTables(false) < 0) {
        std::cerr << "Error: failed to initAllTables in edadb database" << std::endl;
        return -1; 
    }

    return 0;
} // initReadDb


int initWriteDb(const char* edadb_path) {
    std::cout << "[EDADB-IDB] initWriteDb path=" << edadb_path << std::endl;
    std::cout << "[EDADB-IDB] core api=DbTableOp commit-target=293c162" << std::endl;
    if (!edadb::initDatabase(edadb_path)) {
        std::cerr << "Error: failed to init database from " << edadb_path << std::endl;
        return -1;
    }

    edadb::initPrimKeys();

    if (edadb::initAllTables(true) < 0) {
        std::cerr << "Error: failed to initAllTables in edadb database" << std::endl;
        return -1;
    }

    return 0;
} // initWriteDb

} // namespace idb::edadb_adapter
