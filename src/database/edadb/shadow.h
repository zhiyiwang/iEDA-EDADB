/**
 * @file shadow.h
 * @brief This file contains the definition of the IdbShadow class for representing shadow areas in the design.
 * @author Zhiyi Wang 
 */

#pragma once

#include "../../third_party/edadb/include/edadb.h"


#include "../data/design/db_design/IdbBusBitChars.h"
template<>
class edadb::Shadow<idb::IdbBusBitChars> {
public:
    Shadow() = default;
    ~Shadow() = default;

public:
    void fromShadow(idb::IdbBusBitChars* obj) {
        obj->setLeftDelimiter(_left_delim_sd);
        obj->setRightDelimter(_right_delim_sd);
    }

    void toShadow(idb::IdbBusBitChars* obj) {
        _left_delim_sd = obj->getLeftDelimiter();
        _right_delim_sd = obj->getRightDelimiter();
    }

public:
    char _left_delim_sd = '[';
    char _right_delim_sd = ']';
};


#include "../data/design/db_layout/IdbUnits.h"
template<>
class edadb::Shadow<idb::IdbUnits> {
public:
    Shadow() = default;
    ~Shadow() = default;

public:
    void fromShadow(idb::IdbUnits* obj) {
        obj->set_nanoseconds(_nanoseconds_sd);
        obj->set_picofarads(_picofarads_sd);
        obj->set_ohms(_ohms_sd);
        obj->set_milliwatts(_milliwatts_sd);
        obj->set_milliamps(_milliamps_sd);
        obj->set_volts(_volts_sd);
        obj->set_microns_dbu(_micron_dbu_sd);
        obj->set_megahertz(_megahertz_sd);
    }

    void toShadow(idb::IdbUnits* obj) {
        _nanoseconds_sd = obj->get_nanoseconds();
        _picofarads_sd = obj->get_picofarads();
        _ohms_sd = obj->get_ohms();
        _milliwatts_sd = obj->get_milliwatts();
        _milliamps_sd = obj->get_milliamps();
        _volts_sd = obj->get_volts();
        _micron_dbu_sd = obj->get_micron_dbu();
        _megahertz_sd = obj->get_megahertz();
    }
public:
    int32_t _nanoseconds_sd;
    int32_t _picofarads_sd;
    int32_t _ohms_sd;
    int32_t _milliwatts_sd;  //毫瓦
    int32_t _milliamps_sd;   //毫安
    int32_t _volts_sd;       //伏特
    int32_t _micron_dbu_sd;  //微米_dbu
    int32_t _megahertz_sd;
};  


#include "../data/design/IdbDesign.h"
template<>
class edadb::Shadow<idb::IdbDesign> {
public:
    Shadow() = default;
    ~Shadow() = default;
public:
    void fromShadow(idb::IdbDesign* obj) {
        obj->set_design_name(_design_name_sd);
        obj->set_version(_version_sd);
        _units_sd.fromShadow(obj->get_units());
        _bus_bit_chars_sd.fromShadow(obj->get_bus_bit_chars());
    }
    void toShadow(idb::IdbDesign* obj) {
        _design_name_sd = obj->get_design_name();
        _version_sd =  obj->get_version();
        // only access by pointer, no need to copy
        _units_sd.toShadow(obj->get_units());
        _bus_bit_chars_sd.toShadow(obj->get_bus_bit_chars());
    }
public:
    std::string _design_name_sd;
    std::string _version_sd;
    // private members
    edadb::Shadow<idb::IdbUnits> _units_sd; 
    edadb::Shadow<idb::IdbBusBitChars> _bus_bit_chars_sd;
};


#include "../basic/geometry/IdbGeometry.h"
template <>
class edadb::Shadow<idb::IdbCoordinate<int32_t>> {
public:
    Shadow() = default;
    ~Shadow() = default;

public:
    void fromShadow(idb::IdbCoordinate<int32_t>* obj) {
        obj->set_x(_x_sd);
        obj->set_y(_y_sd);
    }

    void toShadow(idb::IdbCoordinate<int32_t>* obj) {
        _x_sd = obj->get_x();
        _y_sd = obj->get_y();
    }

    void clear() {
        _x_sd = 0;
        _y_sd = 0;
    }

public:
    int32_t _x_sd;
    int32_t _y_sd;
};


#include "../data/design/db_layout/IdbDie.h"
namespace edadb {
// no need to define shadow class for IdbDie, 
// directly store vector<IdbCoordinate<int32_t>*> _points instead.
// So here we use edadb::Shadow<IdbCoordinate<int32_t>*> to store each point in the vector.
// 
// DO NOT need define the following typedef again, use 
//   typedef edadb::Shadow<idb::IdbCoordinate<int32_t>> IdbDieShadow;
}
