/**
 * @file shadow_idb_gcell_grid.h
 * @brief This file contains shadow class definition for IdbGCellGrid
 * @author Zhiyi Wang
 */

#pragma once

#include "edadb.h"
#include "database/data/design/db_layout/IdbGCellGrid.h"

namespace edadb {

// EDADB_TODO: IdbGCellGrid now uses direct TABLE4CLASS mapping because DEF
// GCELLGRID only needs four scalar fields and has no child ownership or
// non-owning pointer references. Keep this old shadow dormant unless those
// requirements change.
#if 0
template<>
class Shadow<idb::IdbGCellGrid> {
 public:
  Shadow() : primary_key(next_primary_key++) {}

  bool toShadow(idb::IdbGCellGrid* obj, const uint32_t* idx_ptr = nullptr)
  {
    assert(obj != nullptr);
    assert(idx_ptr != nullptr);

    _order_sd = *idx_ptr;
    _direction_sd = obj->get_direction();
    _start_sd = obj->get_start();
    _num_sd = obj->get_num();
    _space_sd = obj->get_space();

    return true;
  }

  bool fromShadow(idb::IdbGCellGrid* obj, uint32_t* idx_ptr = nullptr)
  {
    assert(obj != nullptr);
    if (idx_ptr != nullptr) {
      *idx_ptr = static_cast<uint32_t>(_order_sd);
    }

    obj->set_direction(_direction_sd);
    obj->set_start(_start_sd);
    obj->set_num(_num_sd);
    obj->set_space(_space_sd);

    return true;
  }

 public:
  uint64_t primary_key = 0;
  uint64_t _order_sd = 0;
  idb::IdbTrackDirection _direction_sd = idb::IdbTrackDirection::kNone;
  int32_t _start_sd = -1;
  int32_t _num_sd = -1;
  int32_t _space_sd = -1;

 private:
  static inline uint64_t next_primary_key = 1;
};
#endif

}  // namespace edadb
