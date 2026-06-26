/**
 * @file shadow_idb_row.h
 * @brief This file contains shadow class definition for IdbRow
 * @author Zhiyi Wang
*/

#pragma once

#include "edadb.h"
#include "database/data/design/db_layout/IdbRow.h"

namespace edadb {

template<>
class Shadow<idb::IdbRow> {
 public:
  bool toShadow(idb::IdbRow* obj, const uint32_t* idx_ptr = nullptr)
  {
    assert(obj != nullptr);
    assert(idx_ptr != nullptr);
    assert(obj->get_site() != nullptr);
    assert(obj->get_original_coordinate() != nullptr);

    _order_sd = *idx_ptr;
    _name_sd = obj->get_name();
    _site_name_sd = obj->get_site()->get_name();
    _site_orient_sd = obj->get_site()->get_orient();
    _origin_x_sd = obj->get_original_coordinate()->get_x();
    _origin_y_sd = obj->get_original_coordinate()->get_y();
    _row_num_x_sd = obj->get_row_num_x();
    _row_num_y_sd = obj->get_row_num_y();
    _step_x_sd = obj->get_step_x();
    _step_y_sd = obj->get_step_y();

    return true;
  }

  bool fromShadow(idb::IdbRow* obj, uint32_t* idx_ptr = nullptr)
  {
    assert(obj != nullptr);
    if (idx_ptr != nullptr) {
      *idx_ptr = static_cast<uint32_t>(_order_sd);
    }

    obj->set_name(_name_sd);
    obj->set_original_coordinate(_origin_x_sd, _origin_y_sd);
    obj->set_row_num_x(_row_num_x_sd);
    obj->set_row_num_y(_row_num_y_sd);
    obj->set_step_x(_step_x_sd);
    obj->set_step_y(_step_y_sd);
    obj->set_orient(_site_orient_sd);

    return true;
  }

 public:
  uint64_t _order_sd = 0;
  std::string _name_sd;
  std::string _site_name_sd;
  idb::IdbOrient _site_orient_sd = idb::IdbOrient::kNone;
  int32_t _origin_x_sd = 0;
  int32_t _origin_y_sd = 0;
  int32_t _row_num_x_sd = 0;
  int32_t _row_num_y_sd = 0;
  int32_t _step_x_sd = 0;
  int32_t _step_y_sd = 0;
};

}  // namespace edadb
