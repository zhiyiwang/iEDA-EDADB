/**
 * @file def_read_edadb.h
 * @author Zhiyi Wang (zhiyiwang@ict.ac.cn)
*/

#include "def_read_edadb.h"

// macros to call test functions and handle errors
#define CALL_TEST_MACRO(fn, what)                         \
    do {                                                  \
        if (!(fn())) {                                    \
            std::cerr << "Error: failed to read " what    \
                      << " from database " << edadb_path  \
                      << std::endl;                       \
            return false;                                 \
        }                                                 \
        else {                                            \
            std::cout << "[DefReadEdadb]: succeeded to read " what   \
                      << " from edadb by calling " #fn << std::endl  \
                      << std::endl << std::endl << std::flush;       \
        }                                                            \
    } while (0)



namespace idb {

DefReadEdadb::DefReadEdadb(IdbDefService* def_service) : DefRead(def_service)
{
}



bool DefReadEdadb::createDbFromEdadb(const char* edadb_path)
{
    if (_def_service == nullptr) {
        std::cerr << "Error: DefReadEdadb::_def_service is nullptr" << std::endl;
        return false;
    }
  
    // init database
    if (!edadb::initDatabase(edadb_path)) {
         std::cerr << "Error: failed to init database from " << edadb_path << std::endl;
         return false;
    }
    std::cout << "Info: succeeded to init database from " << edadb_path << std::endl;
  
    // TODO: read design rather than read to test
    if (!test2Read(edadb_path)) {
         std::cerr << "Error: failed to read from database " << edadb_path << std::endl;
         return false;
    }
  
    return true;
} // createDbFromEdadb



template <typename T>
int read_exactly_one(edadb::DbMapReader<T>* &reader, edadb::DbMap<T> &dbmap, T* obj) { 
    return edadb::read2Scan(reader, dbmap, obj);
}



bool DefReadEdadb::test2Read(const char* edadb_path)
{
    std::cout << "========================================================" << std::endl;
    std::cout << "[DefReadEdadb] Read from EDADB database : " << edadb_path << std::endl;
    std::cout << "========================================================" << std::endl;
  
    // test non-nested tables
    CALL_TEST_MACRO(test2Read<IdbUnits>, "IdbUnits");
    CALL_TEST_MACRO(test2Read<IdbPort>, "IdbPort");
    CALL_TEST_MACRO(test2Read<IdbTerm>, "IdbTerm");

    CALL_TEST_MACRO(test2Read<IdbLayer>, "IdbLayer");


    // test nested tables
    CALL_TEST_MACRO(test2Read<IdbDesign>, "IdbDesign");


    std::cout << "=====================================================" << std::endl;
    std::cout << "[DefReadEdadb] read DEF using EDADB backend finished." << std::endl;
    std::cout << "=====================================================" << std::endl;
    std::cout << std::endl;
  
    return true;
} // test2Read



template <typename T>
bool DefReadEdadb::test2Read(void)
{
    //// read T from edadb database
    edadb::DbMap<T> dbmap;
    dbmap.init();
    edadb::DbMapReader<T>* reader = nullptr;

    T* obj1 = new T();
    if (read_exactly_one(reader, dbmap, obj1) != 1) {
        std::cerr << "Error: failed to read " << dbmap.getTableName() << std::endl;
        return false;
    }

    T* obj2 = new T();
    if (read_exactly_one(reader, dbmap, obj2) != 0) {
        std::cerr << "Error: failed to read " << dbmap.getTableName() << std::endl;
        return false;
    }
    delete obj2; obj2 = nullptr;  // only one in database


    // got from edadb 
    T org; 
    test_edadb::init(&org); // init org to compare with got from database
    if (!test_edadb::verifyEqual(&org, obj1)) {
        std::cerr << "Error: read " << dbmap.getTableName() << " from database does not match the original" << std::endl;
        delete obj1; obj1 = nullptr;
        return false;
    }

    return true;
} // test2Read


template bool DefReadEdadb::test2Read<idb::IdbUnits> (void);
template bool DefReadEdadb::test2Read<idb::IdbPort>  (void);
template bool DefReadEdadb::test2Read<idb::IdbTerm>  (void);

template bool DefReadEdadb::test2Read<idb::IdbLayer> (void);

template bool DefReadEdadb::test2Read<idb::IdbDesign>(void);



} // namespace idb
