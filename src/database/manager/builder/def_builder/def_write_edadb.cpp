/**
 * @file def_read_edadb.h
 * @author Zhiyi Wang (zhiyiwang@aliyun.com)
 * @brief  def read from edadb database
 * @version 0.1
 */

#include "def_write_edadb.h"

#define CALL_TEST_MACRO(fn, what)                  \
  do {                                             \
    if (!(fn())) {                                 \
      std::cerr << "Error: failed to write " what  \
                << " to database " << edadb_path   \
                << std::endl;                      \
      return false;                                \
    }                                              \
  } while (0)



namespace idb {

DefWriteEdadb::DefWriteEdadb(IdbDefService* def_service, DefWriteType type) : DefWrite(def_service, type)
{
}



bool DefWriteEdadb::writeDbToEdadb(const char* edadb_path, DefWriteType type)
{
    if (_def_service == nullptr) {
        std::cerr << "Error: DefWriteEdadb::_def_service is nullptr" << std::endl;
        return false;
    }

    // init database
    if (!edadb::initDatabase(edadb_path)) {
        std::cerr << "Error: failed to init database from " << edadb_path << std::endl;
        return false;
    }

    // TODO: write design rather than write to test
    if (!test2Write(edadb_path, type)) {
        std::cerr << "Error: failed to write to database " << edadb_path << std::endl;
        return false;
    }

    return true;
} // writeDbToEdadb



bool DefWriteEdadb::test2Write(const char* edadb_path, DefWriteType type)
{
    std::cout << "========================================================" << std::endl;
    std::cout << "[DefWriteEdadb] Write to EDADB database : " << edadb_path << std::endl;
    std::cout << "        with type: " << static_cast<int>(type) << std::endl;
    std::cout << "========================================================" << std::endl;

    // test non-nested tables
    CALL_TEST_MACRO(test2Write<IdbUnits>, "IdbUnits");
    CALL_TEST_MACRO(test2Write<IdbPort>, "IdbPort");
    CALL_TEST_MACRO(test2Write<IdbTerm>, "IdbTerm");


    // test nested tables
    CALL_TEST_MACRO(test2Write<IdbDesign>, "IdbDesign");


    std::cout << "=======================================================" << std::endl;
    std::cout << "[DefWriteEdadb] write DEF using EDADB backend finished." << std::endl;
    std::cout << "=======================================================" << std::endl;

    return true;
} // test2Write



template <typename T>
bool DefWriteEdadb::test2Write()
{
    // initialize object
    T obj;
    test_edadb::init(&obj);

    // create table
    edadb::DbMap<T> dbmap;
    if (!edadb::createTable(dbmap)) {
        std::cerr << "Error: failed to create table " << dbmap.getTableName() << std::endl;
        return false;
    }
    std::cout << "Info: succeeded to create table " << dbmap.getTableName() << std::endl;

    // insert object
    if (!edadb::insertObject(dbmap, &obj)) {
        std::cerr << "Error: failed to insert " << dbmap.getTableName() << std::endl;
        return false;
    }
    std::cout << "Info: succeeded to insert " << dbmap.getTableName() << std::endl;
    std::cout << "===================================================" << std::endl;

    return true;
} // test2Write
    

template bool DefWriteEdadb::test2Write<IdbUnits> (void);
template bool DefWriteEdadb::test2Write<IdbPort>  (void);
template bool DefWriteEdadb::test2Write<IdbTerm>  (void);
template bool DefWriteEdadb::test2Write<IdbDesign>(void);


////bool DefWriteEdadb::test2WriteIdbDesign(void)
////{
//////    IdbDesign* design = _def_service->get_design();
//////    if (design == nullptr) {
//////        std::cerr << "Error: DefWriteEdadb::design is nullptr" << std::endl;
//////        return false;
//////    }
////
////    return true;
////} // test2WriteIdbDesign




}  // namespace idb
