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

// macro to compare the original and read values
#define CALL_COMPARE_MACRO(org, got, what)   \
    do {                                     \
        return org == got ? true : (std::cerr << "Error: " what " mismatch " << std::endl, false); \
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
bool readread_exactly_one(edadb::DbMapReader<T>* &reader, edadb::DbMap<T> &dbmap, T* obj) { 
    return edadb::read2Scan(reader, dbmap, obj) == 1;
}



bool DefReadEdadb::test2Read(const char* edadb_path)
{
    std::cout << "========================================================" << std::endl;
    std::cout << "[DefReadEdadb] Read from EDADB database : " << edadb_path << std::endl;
    std::cout << "========================================================" << std::endl;
  
    // test non-nested tables
    CALL_TEST_MACRO(test2ReadIdbUnits, "IdbUnits");
    CALL_TEST_MACRO(test2ReadIdbPort, "IdbPort");
//    CALL_TEST_MACRO(test2ReadIdbTerm, "IdbTerm");


    // test nested tables
    CALL_TEST_MACRO(test2ReadIdbDesign, "IdbDesign");


    std::cout << "=====================================================" << std::endl;
    std::cout << "[DefReadEdadb] read DEF using EDADB backend finished." << std::endl;
    std::cout << "=====================================================" << std::endl;
    std::cout << std::endl;
  
    return true;
} // test2Read



bool DefReadEdadb::test2ReadIdbDesign(void)
{
    //// get design in data_manager from iEDA
    IdbDesign* design = _def_service->get_design();
    if (design == nullptr) {
         std::cerr << "Error: DefReadEdadb::design is nullptr" << std::endl;
         return false;
    }
  

    //// read design from edadb database
    // TODO: use iEDA member instead local variable here
    idb::IdbDesign got;
    edadb::DbMap<idb::IdbDesign> idb_design_dbmap;
    edadb::DbMapReader<idb::IdbDesign> *idb_design_dbmap_reader = nullptr;
    // only one design in database
    if (edadb::read2Scan(idb_design_dbmap_reader, idb_design_dbmap, &got) != 1) {
      std::cerr << "Error: failed to read IdbDesign" << std::endl;
      return false;
    }
    if (edadb::read2Scan(idb_design_dbmap_reader, idb_design_dbmap, &got) != 0) {
      std::cerr << "Error: more than one IdbDesign found" << std::endl;
      return false;
    }
  
 
    //// update design from data_manager using read from edadb database 
    //// we will compare the data by comparing the def file to original def file
    design->set_version(got.get_version());

 
    return true;
} // test2ReadIdbDesign



bool DefReadEdadb::test2ReadIdbUnits(void)
{
    //// get design in data_manager from iEDA
    IdbDesign* design = _def_service->get_design();
    if (design == nullptr) {
         std::cerr << "Error: DefReadEdadb::design is nullptr" << std::endl;
         return false;
    }

  
    //// read units from edadb database
    edadb::DbMap<idb::IdbUnits> idb_units_dbmap;
    edadb::DbMapReader<idb::IdbUnits> *idb_units_dbmap_reader = nullptr;

    idb::IdbUnits *units1 = new idb::IdbUnits();
    if (!readread_exactly_one(idb_units_dbmap_reader, idb_units_dbmap, units1)) {
      std::cerr << "Error: failed to read IdbUnits" << std::endl;
      return false;
    }

    idb::IdbUnits *units2 = new idb::IdbUnits();
    if (readread_exactly_one(idb_units_dbmap_reader, idb_units_dbmap, units2)) {
      std::cerr << "Error: more than one IdbUnits found" << std::endl;
      return false;
    }
    delete units2; units2 = nullptr; // only one units in database


    //// update units from data_manager using read from edadb database
    idb::IdbUnits *units = design->get_units();
    delete units; // delete old units
    design->set_units(units1);


    return true;
} // test2ReadIdbUnits



bool DefReadEdadb::test2ReadIdbPort(void)
{
    //// read port from edadb database
    edadb::DbMap<idb::IdbPort> idb_port_dbmap;
    edadb::DbMapReader<idb::IdbPort> *idb_port_dbmap_reader = nullptr;

    idb::IdbPort *port1 = new idb::IdbPort();
    if (!readread_exactly_one(idb_port_dbmap_reader, idb_port_dbmap, port1)) {
      std::cerr << "Error: failed to read IdbPort" << std::endl;
      return false;
    }

    idb::IdbPort *port2 = new idb::IdbPort();
    if (readread_exactly_one(idb_port_dbmap_reader, idb_port_dbmap, port2)) {
      std::cerr << "Error: more than one IdbPort found" << std::endl;
      return false;
    }
    delete port2; port2 = nullptr; // only one port in database


    //// update port from data_manager using read from edadb database
    idb::IdbPort &got = *port1;


    // compare got with global object
    idb::IdbPort port;
    test_edadb::initPort(&port);
    CALL_COMPARE_MACRO(port.get_port_class(), got.get_port_class(), "IdbPort::_class");
    CALL_COMPARE_MACRO(port.get_coordinate()->get_x(), got.get_coordinate()->get_x(), "IdbPort::_coordinate._x");
    CALL_COMPARE_MACRO(port.get_coordinate()->get_y(), got.get_coordinate()->get_y(), "IdbPort::_coordinate._y");
    CALL_COMPARE_MACRO(port.get_io_average_coordinate()->get_x(), got.get_io_average_coordinate()->get_x(), "IdbPort::_io_average_coordinate._x");
    CALL_COMPARE_MACRO(port.get_io_average_coordinate()->get_y(), got.get_io_average_coordinate()->get_y(), "IdbPort::_io_average_coordinate._y");
    CALL_COMPARE_MACRO(port.get_io_bounding_box()->get_low_x(), got.get_io_bounding_box()->get_low_x(), "IdbPort::_io_bounding_box._lx");
    CALL_COMPARE_MACRO(port.get_io_bounding_box()->get_low_y(), got.get_io_bounding_box()->get_low_y(), "IdbPort::_io_bounding_box._ly");
    CALL_COMPARE_MACRO(port.get_io_bounding_box()->get_high_x(), got.get_io_bounding_box()->get_high_x(), "IdbPort::_io_bounding_box._hx");
    CALL_COMPARE_MACRO(port.get_io_bounding_box()->get_high_y(), got.get_io_bounding_box()->get_high_y(), "IdbPort::_io_bounding_box._hy");
    CALL_COMPARE_MACRO(port.get_orient(), got.get_orient(), "IdbPort::_orient");
    CALL_COMPARE_MACRO(port.get_placement_status(), got.get_placement_status(), "IdbPort::_placement_status");

    std::cout << "[DefReadEdadb]: IdbPort read from edadb database matches the original." << std::endl;

    return true;
} // test2ReadIdbPort




#if 0
bool DefReadEdadb::test2ReadIdbTerm(void)
{
    // use global object to test
    idb::IdbTerm got;
    edadb::DbMap<idb::IdbTerm> idb_term_dbmap;
    edadb::DbMapReader<idb::IdbTerm> *idb_term_dbmap_reader = nullptr;
    // only one term in database
    if (edadb::read2Scan(idb_term_dbmap_reader, idb_term_dbmap, &got) != 1) {
      std::cerr << "Error: failed to read IdbTerm" << std::endl;
      return false;
    }  
    if (edadb::read2Scan(idb_term_dbmap_reader, idb_term_dbmap, &got) != 0) {
      std::cerr << "Error: more than one IdbTerm found" << std::endl;
      return false;
    }

    // compare got with global object
    idb::IdbTerm& term = test_edadb::gTerm;
    CALL_COMPARE_MACRO(term.get_name(), got.get_name(), "IdbTerm::_name");
    CALL_COMPARE_MACRO(term.get_direction(), got.get_direction(), "IdbTerm::_direction");
    CALL_COMPARE_MACRO(term.get_type(), got.get_type(), "IdbTerm::_type");
    CALL_COMPARE_MACRO(term.get_shape(), got.get_shape(), "IdbTerm::_shape");
    CALL_COMPARE_MACRO(term.get_placement_status(), got.get_placement_status(), "IdbTerm::_placement_status");
    CALL_COMPARE_MACRO(term.is_port_exist(), got.is_port_exist(), "IdbTerm::_has_port");
    CALL_COMPARE_MACRO(term.is_special_net(), got.is_special_net(), "IdbTerm::_is_special_net");
    CALL_COMPARE_MACRO(term.is_instance_pin(), got.is_instance_pin(), "IdbTerm::_is_instance");

    std::cout << "[DefReadEdadb]: IdbTerm read from edadb database matches the original." << std::endl;

    return true;
} // test2ReadIdbTerm
#endif



} // namespace idb