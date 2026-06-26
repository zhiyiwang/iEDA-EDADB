/**
 * @file shadow_idb_blockage.h
 * @brief This file contains shadow class definition for IdbBlockage
 * @author Zhiyi Wang
 */

#pragma once

#include "edadb.h"
#include "database/data/design/db_design/IdbBlockages.h"

namespace edadb {
/**
 * NOTE: 
 * In src/database/data/design/db_design/IdbBlockages.h,
 * the IdbBlockage class has two derived classes,
 *   which are IdbRoutingBlockage and IdbPlacementBlockage. 
 * We use IdbBlockageType _type to complement the polymorphism behavior.
 */
template <> 
class Shadow<idb::IdbBlockage> {

public:
    Shadow<idb::IdbBlockage> (void): primary_key(next_primary_key++) {}
    ~Shadow<idb::IdbBlockage>() = default;

    Shadow<idb::IdbBlockage>(const Shadow& other) = delete;
    Shadow<idb::IdbBlockage>& operator=(const Shadow& other) = delete;

public:
    void toShadow(idb::IdbBlockage* obj) {
        // members from IdbBlockage
        _instance_name_sd = obj->get_instance_name();
        _is_pushdown_sd = obj->is_pushdown();
        _type_sd = obj->get_type();

        assert(_rect_list_sd.empty());
        for (auto rect : obj->get_rect_list()) {
            _rect_list_sd.push_back(*rect);
        }

        if (_type_sd == idb::IdbBlockage::IdbBlockageType::kRoutingBlockage) {
            idb::IdbRoutingBlockage* routing_blockage = dynamic_cast<idb::IdbRoutingBlockage*>(obj);
            _layer_name_sd = routing_blockage->get_layer_name();
            _is_except_pgnet_sd = routing_blockage->is_except_pgnet();
        }
    } // toShadow

    void fromShadow(idb::IdbBlockage* obj) {
        // members from IdbBlockage
        obj->set_instance_name(_instance_name_sd);
        obj->set_pushdown(_is_pushdown_sd);
        // _type_sd is not settable

        assert(obj->get_rect_list().empty());
        for (auto& rect_sd : _rect_list_sd) {
            idb::IdbRect* rect = obj->add_rect();
            *rect = rect_sd;
        }

        if (_type_sd == idb::IdbBlockage::IdbBlockageType::kRoutingBlockage) {
            idb::IdbRoutingBlockage* routing_blockage = dynamic_cast<idb::IdbRoutingBlockage*>(obj);
            routing_blockage->set_layer_name(_layer_name_sd);
            routing_blockage->set_except_pgnet(_is_except_pgnet_sd);
        }
    } // fromShadow

public:
    uint64_t primary_key = 0; // primary key
    int32_t _order_sd = 0;

// members from IdbBlockage
    std::string _instance_name_sd;
    bool _is_pushdown_sd = false;
    idb::IdbBlockage::IdbBlockageType _type_sd = idb::IdbBlockage::IdbBlockageType::kNone;
    std::vector<idb::IdbRect> _rect_list_sd;

// members from IdbRoutingBlockage
    std::string _layer_name_sd;
    bool _is_except_pgnet_sd = false;

private:
    static inline uint64_t next_primary_key = 1;
}; // Shadow<idb::IdbBlockage>

} // namespace
