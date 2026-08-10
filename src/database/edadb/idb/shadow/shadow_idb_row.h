/**
 * @file shadow_idb_row.h
 * @brief This file contains shadow class definition for IdbRow
 * @author Zhiyi Wang
*/

#pragma once

#include "edadb.h"
#include "database/data/design/db_layout/IdbRow.h"
#include "../edadb_idb_helper.h"

namespace edadb {

template<>
class Shadow<idb::IdbRow> {
 public:
  bool toShadow(idb::IdbRow* obj, const uint32_t* = nullptr)
  {
    if (obj == nullptr || obj->get_site() == nullptr
        || obj->get_original_coordinate() == nullptr) {
      return false;
    }

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

  bool fromShadow(idb::IdbRow* obj, uint32_t* = nullptr)
  {
    if (obj == nullptr) {
      return false;
    }

    idb::IdbLayout* layout = idb::edadb_adapter::EdadbIdbHelper::getIdbLayout();
    idb::IdbSites* sites = layout == nullptr ? nullptr : layout->get_sites();
    idb::IdbSite* lef_site = sites == nullptr ? nullptr : sites->add_site_list(_site_name_sd);
    if (lef_site == nullptr) {
      return false;
    }

    idb::IdbSite* row_site = lef_site->clone();
    if (row_site == nullptr) {
      return false;
    }

    obj->set_name(_name_sd);
    obj->set_original_coordinate(_origin_x_sd, _origin_y_sd);
    row_site->set_orient(_site_orient_sd);
    obj->set_site(row_site);
    obj->set_orient(row_site->get_orient());
    obj->set_row_num_x(_row_num_x_sd);
    obj->set_row_num_y(_row_num_y_sd);
    obj->set_step_x(_step_x_sd);
    obj->set_step_y(_step_y_sd);

    return obj->set_bounding_box();
  }

 public:
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
