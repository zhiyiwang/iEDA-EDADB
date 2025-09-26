/**
 * @file macro.h
 * @brief This file contains macros to ORM from iEDA using the edadb library.
 * @author Zhiyi Wang
 */

#include "macro.h"


namespace {

// use static object to set primary key config
struct EdadbPrimaryKeyConfig {
  EdadbPrimaryKeyConfig() {
    edadb::Cpp2SqlTypeTrait<idb::IdbRect>::hasPrimKey = false;
    edadb::Cpp2SqlTypeTrait<idb::IdbCoordinate<int32_t>>::hasPrimKey = false;
  }
};
// global object to set primary key config
const EdadbPrimaryKeyConfig kEdadbPrimaryKeyConfig{};
} // namespace





// macro to compare the original and read values
#define CALL_COMPARE_MACRO(org, got, what)   \
    do {                                     \
        return org == got ? true : \
            (std::cerr << "EDADB Error: " what " mismatch " << std::endl, false); \
    } while (0)



namespace test_edadb {

void init(idb::IdbUnits* u)
{
    if (u == nullptr) return;

    // define global object and set values for test
    u->_nanoseconds = 1; 
    u->_picofarads = 2;
    u->_ohms = 3;
    u->_milliwatts = 4;
    u->_milliamps = 5;
    u->_volts = 6;
    u->_micron_dbu = 2000; // 1 micron = 2000 dbu
    u->_megahertz = 1000;  // 1 megahertz = 1000 hertz
} // 


void init(idb::IdbPort* p)
{
    if (p == nullptr) return;

    // define global object and set values for test
    p->_class = idb::IdbPortClass::kCore;
    p->_coordinate->set_xy(100, 200);
    p->_io_average_coordinate->set_xy(150, 250);
    p->_io_bounding_box->set_rect(50, 150, 250, 350);
    p->_orient = idb::IdbOrient::kN_R0;
    p->_placement_status = idb::IdbPlacementStatus::kFixed;
} // 


void init(idb::IdbTerm *t)
{
    if (t == nullptr) return;

    // define global object and set values for test
    t->_name = "TEST_TERM";
    t->_direction = idb::IdbConnectDirection::kInput;
    t->_type = idb::IdbConnectType::kSignal;
    t->_shape = idb::IdbTermShape::kAbutment;
    t->_placement_status = idb::IdbPlacementStatus::kFixed;
    t->_has_port = true;
    t->_is_special_net = false;
    t->_is_instance = false;
    idb::IdbPort* port = new idb::IdbPort();
    init(port);
    t->add_port(port);
} // initTerm


void init(idb::IdbDesign* d)
{
    if (d == nullptr) return;   

    // define global object and set values for test
    d->_version = 5.8;
    d->_design_name = "TEST_DESIGN";
    d->_units = new idb::IdbUnits();
    init(d->_units);
} // initDesign



bool verifyEqual(const idb::IdbUnits* org, const idb::IdbUnits* got)
{
    if (org == nullptr || got == nullptr) return false;

    CALL_COMPARE_MACRO(org->_nanoseconds, got->_nanoseconds, "nanoseconds");
    CALL_COMPARE_MACRO(org->_picofarads, got->_picofarads, "picofarads");
    CALL_COMPARE_MACRO(org->_ohms, got->_ohms, "ohms");
    CALL_COMPARE_MACRO(org->_milliwatts, got->_milliwatts, "milliwatts");
    CALL_COMPARE_MACRO(org->_milliamps, got->_milliamps, "milliamps");
    CALL_COMPARE_MACRO(org->_volts, got->_volts, "volts");
    CALL_COMPARE_MACRO(org->_micron_dbu, got->_micron_dbu, "micron_dbu");
    CALL_COMPARE_MACRO(org->_megahertz, got->_megahertz, "megahertz");
    return true;
} 


bool verifyEqual(const idb::IdbPort* org, const idb::IdbPort* got)
{
    if (org == nullptr || got == nullptr) return false;

    CALL_COMPARE_MACRO(org->_class, got->_class, "class");
    CALL_COMPARE_MACRO(org->_coordinate->get_x(), got->_coordinate->get_x(), "coordinate x");
    CALL_COMPARE_MACRO(org->_coordinate->get_y(), got->_coordinate->get_y(), "coordinate y");
    CALL_COMPARE_MACRO(org->_io_average_coordinate->get_x(), got->_io_average_coordinate->get_x(), "io_average_coordinate x");
    CALL_COMPARE_MACRO(org->_io_average_coordinate->get_y(), got->_io_average_coordinate->get_y(), "io_average_coordinate y");
    CALL_COMPARE_MACRO(org->_io_bounding_box->get_low_x(), got->_io_bounding_box->get_low_x(), "io_bounding_box low x");
    CALL_COMPARE_MACRO(org->_io_bounding_box->get_low_y(), got->_io_bounding_box->get_low_y(), "io_bounding_box low y");
    CALL_COMPARE_MACRO(org->_io_bounding_box->get_high_x(), got->_io_bounding_box->get_high_x(), "io_bounding_box high x");
    CALL_COMPARE_MACRO(org->_io_bounding_box->get_high_y(), got->_io_bounding_box->get_high_y(), "io_bounding_box high y");
    CALL_COMPARE_MACRO(org->_orient, got->_orient, "orient");
    CALL_COMPARE_MACRO(org->_placement_status, got->_placement_status, "placement_status");
    return true;
}


bool verifyEqual(const idb::IdbTerm* org, const idb::IdbTerm* got)
{
    if (org == nullptr || got == nullptr) return false;

    CALL_COMPARE_MACRO(org->_name, got->_name, "name");
    CALL_COMPARE_MACRO(org->_direction, got->_direction, "direction");
    CALL_COMPARE_MACRO(org->_type, got->_type, "type");
    CALL_COMPARE_MACRO(org->_shape, got->_shape, "shape");
    CALL_COMPARE_MACRO(org->_placement_status, got->_placement_status, "placement_status");
    CALL_COMPARE_MACRO(org->_has_port, got->_has_port, "has_port");
    CALL_COMPARE_MACRO(org->_is_special_net, got->_is_special_net, "is_special_net");
    CALL_COMPARE_MACRO(org->_is_instance, got->_is_instance, "is_instance");

    // compare port list
    size_t org_port_size = org->_port_list.size();
    size_t got_port_size = got->_port_list.size();
    CALL_COMPARE_MACRO(org_port_size, got_port_size, "port_list size");
    for (size_t i = 0; i < org_port_size; ++i) {
        if (!verifyEqual(org->_port_list[i], got->_port_list[i])) {
            std::cerr << "Error: port_list[" << i << "] mismatch" << std::endl;
            return false;
        }
    }

    return true;
}


bool verifyEqual(const idb::IdbDesign* org, const idb::IdbDesign* got)
{
    if (org == nullptr || got == nullptr) return false;

    CALL_COMPARE_MACRO(org->_version, got->_version, "version");
    CALL_COMPARE_MACRO(org->_design_name, got->_design_name, "design_name");
    if (!verifyEqual(org->_units, got->_units)) {
        std::cerr << "Error: units mismatch" << std::endl;
        return false;
    }
    return true;
}

} // namespace test_edadb