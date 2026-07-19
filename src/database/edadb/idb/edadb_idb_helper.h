/*
 * @file edadb_idb_helper.h
 * @brief This file contains helper functions for Idb and EDADB database operations.
 * @author Zhiyi Wang
 */

#pragma once

#include "def_service.h"
#include "IdbViaRule.h"

namespace idb::edadb_adapter {

class EdadbIdbHelper {
private:
    EdadbIdbHelper (void) = default;
    ~EdadbIdbHelper(void) = default;

public:
    // helper function to build def from edadb database
    static bool setIdbDefService(idb::IdbDefService* def_service) {
        if (def_service == nullptr) {
            std::cout << "EdabIdbHelper Error: setIdbDefService input def_service is nullptr";
            std::cout << std::endl;
            return false;
        }

        if (s_def_service == def_service) {
            return true;
        }

        if (s_def_service != nullptr) {
            std::cout << "EdabIdbHelper Error: setIdbDefService s_def_service is already set";
            std::cout << std::endl;
            return false;
        }

        s_def_service = def_service;
        return true;
    }

    static idb::IdbDefService* getIdbDefService() {
        return s_def_service;
    }

    static idb::IdbDesign* getIdbDesign() {
        if (s_def_service == nullptr) {
            std::cout << "EdabIdbHelper Error: getIdbDesign s_def_service is nullptr";
            std::cout << std::endl;
            assert(s_def_service != nullptr);
            return nullptr;
        }
        return s_def_service->get_design();
    }

    static idb::IdbLayout* getIdbLayout() {
        if (s_def_service == nullptr) {
            std::cout << "EdabIdbHelper Error: getIdbLayout s_def_service is nullptr";
            std::cout << std::endl;
            assert(s_def_service != nullptr);
            return nullptr;
        }
        return s_def_service->get_layout();
    }

    static idb::IdbPins* getIdbIoPins() {
        idb::IdbDesign* design = getIdbDesign();
        if (design == nullptr) {
            return nullptr;
        }
        return design->get_io_pin_list();
    }

    static idb::IdbInstanceList* getIdbInstanceList() {
        idb::IdbDesign* design = getIdbDesign();
        if (design == nullptr) {
            return nullptr;
        }
        return design->get_instance_list();
    }

    static idb::IdbRegionList* getIdbRegionList() {
        idb::IdbDesign* design = getIdbDesign();
        if (design == nullptr) {
            return nullptr;
        }
        return design->get_region_list();
    }

    static idb::IdbRegion* findIdbRegionByName(const std::string& region_name) {
        idb::IdbRegionList* region_list = getIdbRegionList();
        if (region_list == nullptr) {
            return nullptr;
        }
        return region_list->find_region(region_name);
    }

    static idb::IdbCellMasterList* getIdbCellMasterList() {
        idb::IdbLayout* layout = getIdbLayout();
        if (layout == nullptr) {
            return nullptr;
        }
        return layout->get_cell_master_list();
    }

    static idb::IdbCellMaster* findIdbCellMasterByName(const std::string& master_name) {
        idb::IdbCellMasterList* master_list = getIdbCellMasterList();
        if (master_list == nullptr) {
            return nullptr;
        }
        return master_list->find_cell_master(master_name);
    }

    static idb::IdbVias* getIdbDefVias() {
        idb::IdbDesign* design = getIdbDesign();
        if (design == nullptr) {
            return nullptr;
        }
        return design->get_via_list();
    }

    static idb::IdbVias* getIdbLefVias() {
        idb::IdbLayout* layout = getIdbLayout();
        if (layout == nullptr) {
            return nullptr;
        }
        return layout->get_via_list();
    }


    static idb::IdbViaRuleList* getIdbViaRuleList() {
        if (s_def_service == nullptr) {
            std::cout << "EdabIdbHelper Error: getIdbViaRuleList s_def_service is nullptr";
            std::cout << std::endl;
            assert(s_def_service != nullptr);
            return nullptr;
        }
        return s_def_service->get_layout()->get_via_rule_list();
    }

    static idb::IdbViaRuleGenerate* findIdbViaRuleGenerateByName(const std::string& rule_name) {
        idb::IdbViaRuleList* via_rule_list = getIdbViaRuleList();
        if (via_rule_list == nullptr) {
            std::cout << "EdabIdbHelper Error: findIdbViaRuleGenerateByName failed to get via_rule_list";
            std::cout << std::endl;
            return nullptr;
        }
        return via_rule_list->find_via_rule_generate(rule_name);
    }


    static idb::IdbLayers* getIdbLayers() {
        if (s_def_service == nullptr) {
            std::cout << "EdabIdbHelper Error: getIdbLayers s_def_service is nullptr";
            std::cout << std::endl;
            assert(s_def_service != nullptr);
            return nullptr;
        }
        return s_def_service->get_layout()->get_layers();
    }

    static idb::IdbLayer* findIdbLayerByName(const std::string& layer_name) {
        idb::IdbLayers* layers = getIdbLayers();
        if (layers == nullptr) {
            std::cout << "EdabIdbHelper Error: findIdbLayerByName failed to get layers";
            std::cout << std::endl;
            return nullptr;
        }
        return layers->find_layer(layer_name);
    }


public:
    // helper function to build def from edadb database
    static idb::IdbDefService* s_def_service;
}; // class EdadbIdbHelper


} // namespace idb::edadb_adapter
