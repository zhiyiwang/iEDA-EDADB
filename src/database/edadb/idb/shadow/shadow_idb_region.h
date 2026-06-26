/**
 * @file shadow_idb_region.h
 * @brief This file contains shadow class definition for IdbRegion
 * @author Zhiyi Wang
 */

#pragma once

#include "edadb.h"
#include "database/data/design/db_design/IdbRegion.h"

namespace edadb {

template<>
class Shadow<idb::IdbRegion> {
 public:
  bool toShadow(idb::IdbRegion* obj, const uint32_t* idx_ptr = nullptr)
  {
    assert(obj != nullptr);
    assert(idx_ptr != nullptr);

    _name_sd = obj->get_name();
    _order_sd = *idx_ptr;
    _type_sd = obj->get_type();
    _boundary_list_sd = obj->get_boundary();

    return true;
  }

  bool fromShadow(idb::IdbRegion* obj, uint32_t* idx_ptr = nullptr)
  {
    assert(obj != nullptr);
    if (idx_ptr != nullptr) {
      *idx_ptr = static_cast<uint32_t>(_order_sd);
    }

    obj->set_name(_name_sd);
    obj->set_type(_type_sd);
    for (idb::IdbRect* rect : _boundary_list_sd) {
      obj->add_boundary(rect->get_low_x(), rect->get_low_y(), rect->get_high_x(), rect->get_high_y());
    }

    return true;
  }

 public:
  std::string _name_sd;
  uint64_t _order_sd = 0;
  idb::IdbRegionType _type_sd = idb::IdbRegionType::kNone;
  std::vector<idb::IdbRect*> _boundary_list_sd;
};

}  // namespace edadb
