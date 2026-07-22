/**
 * @file shadow_idb_blockage.h
 * @brief This file contains shadow class definition for IdbBlockage
 * @author Zhiyi Wang
 */

#pragma once

#include "edadb.h"
#include "database/data/design/db_design/IdbBlockages.h"
#include "../edadb_idb_helper.h"

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
    bool toShadow(idb::IdbBlockage* obj, const uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr) {
            return false;
        }

        // members from IdbBlockage
        _instance_name_sd.clear();
        if (obj->get_instance() != nullptr) {
            _instance_name_sd = obj->get_instance_name();
        }
        _type_sd = obj->get_type();
        _is_pushdown_sd = false;
        _layer_name_sd.clear();
        _is_except_pgnet_sd = false;

        _rect_list_sd.clear();
        for (auto rect : obj->get_rect_list()) {
            if (rect == nullptr) {
                return false;
            }
            _rect_list_sd.push_back(*rect);
        }

        if (_type_sd == idb::IdbBlockage::IdbBlockageType::kRoutingBlockage) {
            idb::IdbRoutingBlockage* routing_blockage = dynamic_cast<idb::IdbRoutingBlockage*>(obj);
            if (routing_blockage == nullptr) {
                return false;
            }
            _is_pushdown_sd = routing_blockage->is_pushdown();
            _layer_name_sd = routing_blockage->get_layer_name();
            _is_except_pgnet_sd = routing_blockage->is_except_pgnet();
        } else if (_type_sd != idb::IdbBlockage::IdbBlockageType::kPlacementBlockage) {
            return false;
        }
        return true;
    } // toShadow

    bool fromShadow(idb::IdbBlockage* obj, uint32_t* idx_ptr = nullptr) {
        if (obj == nullptr || obj->get_type() != _type_sd) {
            return false;
        }

        // members from IdbBlockage
        obj->set_instance_name(_instance_name_sd);
        // _type_sd is not settable

        if (_type_sd == idb::IdbBlockage::IdbBlockageType::kRoutingBlockage) {
            idb::IdbRoutingBlockage* routing_blockage = dynamic_cast<idb::IdbRoutingBlockage*>(obj);
            if (routing_blockage == nullptr) {
                return false;
            }

            idb::IdbLayer* layer = idb::edadb_adapter::EdadbIdbHelper::findIdbLayerByName(_layer_name_sd);
            if (layer == nullptr) {
                return false;
            }

            routing_blockage->set_layer_name(_layer_name_sd);
            routing_blockage->set_layer(layer);
            routing_blockage->set_pushdown(_is_pushdown_sd);
            routing_blockage->set_except_pgnet(_is_except_pgnet_sd);
        } else if (_type_sd != idb::IdbBlockage::IdbBlockageType::kPlacementBlockage) {
            return false;
        }

        if (!_instance_name_sd.empty()) {
            idb::IdbInstance* instance = idb::edadb_adapter::EdadbIdbHelper::findIdbInstanceByName(_instance_name_sd);
            if (instance == nullptr) {
                return false;
            }
            obj->set_instance(instance);
        }

        if (!obj->get_rect_list().empty()) {
            return false;
        }
        for (auto& rect_sd : _rect_list_sd) {
            idb::IdbRect* rect = obj->add_rect();
            *rect = rect_sd;
        }
        return true;
    } // fromShadow

public:
    uint64_t primary_key = 0; // primary key

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
