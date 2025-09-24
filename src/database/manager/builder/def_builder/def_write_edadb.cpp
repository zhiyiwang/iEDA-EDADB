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
    CALL_TEST_MACRO(test2WriteIdbUnits, "IdbUnits");
    CALL_TEST_MACRO(test2WriteIdbPort, "IdbPort");
//    CALL_TEST_MACRO(test2WriteIdbTerm, "IdbTerm");


    // test nested tables
    CALL_TEST_MACRO(test2WriteIdbDesign, "IdbDesign");


    std::cout << "=======================================================" << std::endl;
    std::cout << "[DefWriteEdadb] write DEF using EDADB backend finished." << std::endl;
    std::cout << "=======================================================" << std::endl;

    return true;
} // test2Write

    

bool DefWriteEdadb::test2WriteIdbDesign(void)
{
    IdbDesign* design = _def_service->get_design();
    if (design == nullptr) {
        std::cerr << "Error: DefWriteEdadb::design is nullptr" << std::endl;
        return false;
    }

    // create table IdbDesign
    edadb::DbMap<idb::IdbDesign> idb_design_dbmap;
    if (!edadb::createTable(idb_design_dbmap)) {
        std::cerr << "Error: failed to create table IdbDesign" << std::endl;
        return false;
    }
    std::cout << "Info: succeeded to create table IdbDesign" << std::endl;
  
    // insert design
    if (!edadb::insertObject(idb_design_dbmap, design)) {
        std::cerr << "Error: failed to insert IdbDesign" << std::endl;
        return false;
    }
    std::cout << "Info: succeeded to insert IdbDesign" << std::endl;
    std::cout << "===================================================" << std::endl;
  
    return true;
} // test2WriteIdbDesign


bool DefWriteEdadb::test2WriteIdbUnits(void)
{
    IdbDesign* design = _def_service->get_design();
    if (design == nullptr) {
        std::cerr << "Error: DefWriteEdadb::design is nullptr" << std::endl;
        return false;
    }

    IdbUnits* units = design->get_units();
    if (units == nullptr) {
        std::cerr << "Error: DefWriteEdadb::units is nullptr" << std::endl;
        return false;
    }

    // create table IdbUnits
    edadb::DbMap<idb::IdbUnits> idb_units_dbmap;
    if (!edadb::createTable(idb_units_dbmap)) {
        std::cerr << "Error: failed to create table IdbUnits" << std::endl;
        return false;
    }
    std::cout << "Info: succeeded to create table IdbUnits" << std::endl;
  
    // insert units
    if (!edadb::insertObject(idb_units_dbmap, units)) {
        std::cerr << "Error: failed to insert IdbUnits" << std::endl;
        return false;
    }
    std::cout << "Info: succeeded to insert IdbUnits" << std::endl;
    std::cout << "===================================================" << std::endl;
  
    return true;
} // test2WriteIdbUnits



bool DefWriteEdadb::test2WriteIdbPort(void)
{
    // use global object to test
    idb::IdbPort port;
    test_edadb::initPort(&port);

    // create table IdbPort
    edadb::DbMap<idb::IdbPort> idb_port_dbmap;
    if (!edadb::createTable(idb_port_dbmap)) {
        std::cerr << "Error: failed to create table IdbPort" << std::endl;
        return false;
    }
    std::cout << "Info: succeeded to create table IdbPort" << std::endl;

    // insert port
    if (!edadb::insertObject(idb_port_dbmap, &port)) {
        std::cerr << "Error: failed to insert IdbPort" << std::endl;
        return false;
    }
    std::cout << "Info: succeeded to insert IdbPort" << std::endl;
    std::cout << "===================================================" << std::endl;

    return true;
} // test2WriteIdbPort



#if 0
bool DefWriteEdadb::test2WriteIdbTerm(void)
{
    // use global object to test
    idb::IdbTerm term;
    test_edadb::initTerm(&term);

    // create table IdbTerm
    edadb::DbMap<idb::IdbTerm> idb_term_dbmap;
    if (!edadb::createTable(idb_term_dbmap)) {
        std::cerr << "Error: failed to create table IdbTerm" << std::endl;
        return false;
    }
    std::cout << "Info: succeeded to create table IdbTerm" << std::endl;

    // insert term
    if (!edadb::insertObject(idb_term_dbmap, &term)) {
        std::cerr << "Error: failed to insert IdbTerm" << std::endl;
        return false;
    }
    std::cout << "Info: succeeded to insert IdbTerm" << std::endl;
    std::cout << "===================================================" << std::endl;

    return true;
} // test2WriteIdbTerm
#endif


}  // namespace idb
