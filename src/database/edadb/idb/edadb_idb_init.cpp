/**
 * @file macro.h
 * @brief This file contains macros to ORM from iEDA using the edadb library.
 * @author Zhiyi Wang
 */

#include "edadb_idb_init.h"
#include "edadb.h"
#include "edadb_idb_schema.h"
#include "edadb_idb_shadow.h"


namespace idb::edadb_adapter {

#if EDADB_OUTPUT_DEBUG
#define EDADB_IDB_DEBUG_STREAM std::cout
#else
#define EDADB_IDB_DEBUG_STREAM if (true) {} else std::cout
#endif

void initPrimKeys(void) {
    edadb::Cpp2SqlTypeTrait<idb::IdbUnits>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<idb::IdbBusBitChars>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<idb::IdbTrack>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<idb::IdbGCellGrid>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<edadb::Shadow<idb::IdbCoordinate<int32_t>>>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<edadb::Shadow<idb::IdbRect>>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<edadb::Shadow<idb::IdbViaMasterGenerate>>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<idb::IdbHalo>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<edadb::Shadow<idb::IdbRouteHalo>>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<idb::edadb_adapter::SpecialNetPinRef>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<idb::edadb_adapter::NetPinRef>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<idb::edadb_adapter::RegularWireViaRef>::hasPrimKey = false;
} // initPrimKeys


template <typename T>
int initTable(bool crt_tab, bool self_txn = true) {
    const std::string& table_name = edadb::TypeMetaData<edadb::StoreTypeOf<T>>::table_name();
#if EDADB_OUTPUT_DEBUG
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] schema table=" << table_name
              << " create=" << (crt_tab ? "true" : "false") << std::endl;
#endif

    if (!crt_tab) {
        const edadb::DbTableDefBase* table_def = edadb::getTableDef<T>();
        if (table_def == nullptr) {
            std::cerr << "failed to map table definition: " << table_name << std::endl;
            return -1;
        }
    } else {
        if (!edadb::createTable<T>(self_txn)) {
            std::cerr << "failed to create table: " << table_name << std::endl;
            return -1;
        }
    } // if-else

    return 0;
} // initTable


#define EDADB_INIT_TABLE(T, CTR_TBL, SELF_TXN)                       \
    do {                                                              \
        if (initTable<T>(CTR_TBL, SELF_TXN) < 0) {                    \
            std::fprintf(stderr, "[EDADB] createTable failed: %s\n",  \
                         typeid(T).name());                           \
            return -1;                                                \
        }                                                             \
    } while (0)


int initAllTables(bool crt_tab, bool self_txn = true) {
#if EDADB_OUTPUT_DEBUG
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] initAllTables create=" << (crt_tab ? "true" : "false") << std::endl;
#endif
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] initAllTables register Design/Die/Row/TrackGrid/GCell/Via/Region/Instance/Pin/Blockage/Slot/Group/Fill/SpecialNet/Net groups"
              << std::endl;

    EDADB_INIT_TABLE(idb::IdbDesign, crt_tab, self_txn);
    EDADB_INIT_TABLE(edadb::Shadow<idb::IdbDie>, crt_tab, self_txn);
    EDADB_INIT_TABLE(edadb::Shadow<idb::IdbRow>, crt_tab, self_txn);
    EDADB_INIT_TABLE(edadb::Shadow<idb::IdbTrackGrid>, crt_tab, self_txn);
    EDADB_INIT_TABLE(idb::IdbGCellGrid, crt_tab, self_txn);
    EDADB_INIT_TABLE(idb::IdbVia, crt_tab, self_txn);
    EDADB_INIT_TABLE(edadb::Shadow<idb::IdbInstance>, crt_tab, self_txn);
    EDADB_INIT_TABLE(edadb::Shadow<idb::IdbPin>, crt_tab, self_txn);
    EDADB_INIT_TABLE(edadb::Shadow<idb::IdbBlockage>, crt_tab, self_txn);
    EDADB_INIT_TABLE(idb::IdbRegion, crt_tab, self_txn);
    EDADB_INIT_TABLE(edadb::Shadow<idb::IdbSlot>, crt_tab, self_txn);
    EDADB_INIT_TABLE(edadb::Shadow<idb::IdbGroup>, crt_tab, self_txn);
    EDADB_INIT_TABLE(edadb::Shadow<idb::IdbFill>, crt_tab, self_txn);
    EDADB_INIT_TABLE(edadb::Shadow<idb::IdbSpecialNet>, crt_tab, self_txn);
    EDADB_INIT_TABLE(edadb::Shadow<idb::IdbNet>, crt_tab, self_txn);

    return 0;
} // createAllTables



int initReadDb(const char* edadb_path) {
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] initReadDb path=" << edadb_path << std::endl;
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] core api=DbTableOp primitive-vector target=3077132" << std::endl;
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
} // initReadDb


int initWriteDb(const char* edadb_path) {
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] initWriteDb path=" << edadb_path << std::endl;
    EDADB_IDB_DEBUG_STREAM << "[EDADB-IDB] core api=DbTableOp primitive-vector target=3077132" << std::endl;
    if (!edadb::initDatabase(edadb_path)) {
        std::cerr << "Error: failed to init database from " << edadb_path << std::endl;
        return -1;
    }

    initPrimKeys();

    if (!edadb::beginTransaction()) {
        std::cerr << "Error: failed to begin schema transaction" << std::endl;
        return -1;
    }

    if (initAllTables(true, false) < 0) {
        if (!edadb::rollbackTransaction()) {
            std::cerr << "Error: failed to rollback schema transaction" << std::endl;
        }
        std::cerr << "Error: failed to initAllTables in edadb database" << std::endl;
        return -1;
    }

    if (!edadb::commitTransaction()) {
        std::cerr << "Error: failed to commit schema transaction" << std::endl;
        if (!edadb::rollbackTransaction()) {
            std::cerr << "Error: failed to rollback schema transaction" << std::endl;
        }
        return -1;
    }

    return 0;
} // initWriteDb

#undef EDADB_IDB_DEBUG_STREAM

} // namespace idb::edadb_adapter
