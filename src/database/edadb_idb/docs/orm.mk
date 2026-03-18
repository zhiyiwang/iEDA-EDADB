# Object Relational Mapping (ORM) for EDA Database

This document describes the Object Relational Mapping (ORM) definitions for the EDA database. 
For each class defined and used in def files, there is a corresponding table in the edadb and defined by TABLE4CLASS macro.

The mapping is defined in the file:
	`src/third_party/edadb/include/edadb/Table4Class.h`
	possibly definitions are:
	TABLE4CLASS(ClassName, TableName, (scalar fields))
	TABLE4CLASS_WVEC(ClassName, TableName, (scalar fields), (vector fields))

According to def_read .h/cpp source files, the following classes are defined and useds with TABLE4CLASS macro defined mappings. 


Here are the mappings:

## IdbDesign
IdbDesign* design = _def_service->get_design();
  design->set_version(version);
  design->set_design_name(name);
  design->set_bus_bit_chars(bus_bit_chars);
  IdbBlockageList* blockage_list = design->get_blockage_list();
    [IdbBlockageList]
  IdbInstanceList* instance_list = design->get_instance_list();
    [IdbInstanceList]
  IdbRegionList* region_list = design->get_region_list();
    [IdbRegionList]
  IdbFillList* fill_list = design->get_fill_list();
    [IdbFillList]
  IdbVias* via_list_def = design->get_via_list();
    [IdbVias]
  IdbGroupList* group_list = design->get_group_list();
    [IdbGroupList]
  IdbPins* io_pin_list = design->get_io_pin_list();
    [IdbPins]


## IdbLayout
IdbLayout* layout = _def_service->get_layout();  // Lef
  IdbLayers* layer_list = layout->get_layers();
    [IdbLayers]
  IdbCellMasterList* master_list = layout->get_cell_master_list();
    [IdbCellMasterList]
  IdbVias* via_list_lef = layout->get_via_list();
    [IdbVias]

## IdbCellMaster
IdbCellMaster* _cur_cell_master;
  _cur_cell_master->get_name()

## IdbLayers
vector<IdbLayer*> _layers;

## IdbLayer
IdbLayer* layer->get_name();
  
## IdbBlockageList
std::vector<IdbBlockage*> _blockage_list
  .emplace_back(IdbRoutingBlockage*);
  .emplace_back(IdbPlacementBlockage*);

## IdbRoutingBlockage: public IdbBlockage
IdbRoutingBlockage* routing_Blockage->set_layer_name(layer_name);
  routing_blockage->set_layer_name(layer_name);
  routing_blockage->set_layer();
    [IdbLayer]
  routing_blockage->set_slots(true);
  routing_blockage->set_fills(true);
  routing_blockage->set_pushdown(true);
  routing_blockage->set_except_pgnet(true);
  routing_blockage->set_instance_name(string );
  routing_blockage->set_instance(ins);
    [IdbInstance]
  routing_blockage->set_min_spacing(def_blockage->minSpacing());
  routing_blockage->set_effective_width(def_blockage->designRuleWidth());
  routing_blockage->add_rect(def_blockage->xl(i), yl(i), xh(i), yh(i));
dynamic_cast<IdbBlockage*> pBlockage->set_type_routing();

## IdbPlacementBlockage: public IdbBlockage
IdbPlacementBlockage* placement_blockage
  placement_blockage->set_soft(true);
  placement_blockage->set_max_density();
  placement_blockage->set_instance_name();
  placement_blockage->set_instance(instance);
    [IdbInstance]
  placement_blockage->add_rect(def_blockage->xl(i), yl(i), xh(i), yh(i));
dynamic_cast<IdbBlockage*> pBlockage->set_type_placement();


## IdbLayer
IdbLayer* layer->get_name();


## IdbInstanceList
std::vector<IdbInstance*> _instance_list

## IdbInstance
IdbInstance* instance->get_name();
  instance->set_name(name);
  instance->set_cell_master(_cur_cell_master);
  instance->set_status_by_def_enum(def_component->placementStatus());
  instance->set_orient_by_enum(def_component->placementOrient());
  instance->set_type(def_component->source());
  instance->set_weight(def_component->weight());
  instance->set_region(region);
	[IdbRegion]
  instance->set_halo(halo);
    [IdbHalo]
  instance->set_route_halo(route_halo);
    [IdbRouteHalo]
  instance->set_coordinate(x, y);
    [IdbCoordinate<int32_t>]


## IdbRegionList
std::vector<IdbRegion*> _region_list

## IdbRegion
IdbRegion* region->get_name();

## IdbHalo
IdbHalo* halo = instance->get_halo();
  halo->set_is_soft(true/false);
  halo->set_extend_left();
  halo->set_extend_bottom();
  halo->set_extend_right();
  halo->set_extend_top();

## IdbRouteHalo
IdbRouteHalo* route_halo = instance->set_route_halo();
  route_halo->set_route_distance();
  route_halo->set_layer_bottom();
  route_halo->set_layer_top();

## IdbFillList
IdbFillList* fill_list = design->get_fill_list();
  fill_list->get_num_fill();
  fill_list->get_fill_list();

## IdbFillLayer
IdbFillLayer* fill_layer = fill_list->add_fill_layer(layer);
  fill_layer->set_layer(layer);
    [IdbLayer]
  fill_layer->add_rect(def_fill->xl(i), yl(i), xh(i), yh(i));


## IdbFill
  fill->set_type_layer();
  fill->set_type_via();

## IdbVias
  size_t _num_vias;
  vector<IdbVia*> _via_list;

## IdbVia
  string _name;

## IdbFillVia
  fill_via->set_via(via);
  IdbCoordinate<int32_t>* _coordinate_list;
  
## IdbGCellGridList
  vector<IdbGCellGrid*> _gcelll_grid_list;

## IdbGCellGrid
IdbGCellGrid* gcell_grid 
  gcell_grid->set_direction(IdbTrackDirection::kDirectionX);
  gcell_grid->set_num(def_grid->xNum());
  gcell_grid->set_start(def_grid->x());
  gcell_grid->set_space(def_grid->xStep());

## IdbGroupList
  int32_t _num;
  vector<IdbGroup*> _group_list;

## IdbGroup
  std::string _group_name;
  IdbRegion* _region;

## IdbNetList
  int32_t _num;
  vector<IdbNet*> _net_list;

## IdbNet
  IdbNet* pNet = new IdbNet();
  pNet->set_net_name(name);
  pNet->set_connect_type(type);
  net->set_connect_type(def_net->use());
  net->set_weight(def_net->weight());
  net->set_xtalk(def_net->XTalk());
  net->set_frequency(def_net->frequency());
  net->set_original_net_name(def_net->original());
? net->add_io_pin(pin);
?   [IdbPin]
  net->get_instance_list()
    [IdbInstanceList]
? net->add_instance_pin(pin);
?   [IdbPin]

## IdbPins
  int32_t _pin_num;
  vector<IdbPin*> _pin_list;

## IdbPin
  IdbPin* pin
  pin->get_pin_name();
  pin->get_instance();
  pin->set_net(net);

## IdbRegularWireList
vector<IdbRegularWire*> _wire_list;

## IdbRegularWire
IdbRegularWire* wire
  wire->set_wire_state(def_wire->wireType());
  wire->get_wire_statement()
  wire->set_shield_name(def_wire->wireShieldNetName());
  vector<IdbRegularWireSegment*> _segment_list;

## IdbRegularWireSegment
IdbRegularWireSegment* segment
  segment->set_layer_name(def_path->getLayer());
  segment->set_layer(layer_list->find_layer(def_path->getLayer()));
  segment->set_is_via(true);
  IdbVia* via_new = segment->copy_via(via);
    [IdbVia]
  segment->add_point(x, y);
  segment->add_virtual_point(x, y);
  segment->set_is_rect(true);
  segment->set_delta_rect(ll_x, ll_y, ur_x, ur_y);
??DefRead::parse_net
?? int32_t path_id = def_path->next())




- TABLE4CLASS(IdbUnits,(_micro_dbu))
- TABLE4CLASS(IdbDesign, (_version, _design_name, _bus_bit_chars,  _units, _layout, _io_pin_list, _via_list, _instance_list),)
	（_blockage_list，_special_net_list，_net_list，_slot_list，_group_list，_fill_list，_region_list），（）
- TABLE4CLASS(IdbBusBitChars,(_left_delimiter, _right_delimiter))
- TABLE4CLASS(IdbDie, (), (IdbCoordinate<int32_t>* _points))
- TABLE4CLASS(IdbLayout, (_units, _die, _rows, _track_grid_list, _gcell_grid_list), ())
	，（_layers），（）
- TABLE4CLASS(IdbRows, (_row_num), (_row_list))
- TABLE4CLASS(IdbRow, (_name，_site, _original_coordinate, _row_num_x, _row_num_y, _step_x, _step_y), ())
- TABLE4CLASS(IdbSite, (_orient, _name), ())
- TABLE4CLASS(IdbCoordinate<int32_t>, (_x, _y), ())
- TABLE4CLASS(IdbTrackGridList, (_track_grid_num), (_track_grid_list))
- TABLE4CLASS(IdbTrackGrid, (_track, _track_num), (_layer_list))
- TABLE4CLASS(IdbTrack, (_direction, _start, _pitch), ())
- TABLE4CLASS(IdbGCellGridList, (), (_gcelll_grid_list))
- TABLE4CLASS(IdbGCellGrid, (_direction, _start, _num, _space), ())
- TABLE4CLASS(IdbVias, (_num_via), (_via_list))
- TABLE4CLASS(IdbVia, (_name, _master_instance))
- TABLE4CLASS(IdbViaMaster, (_name, _type, _master_generate), ())
- TABLE4CLASS(IdbViaMasterGenerate, (_rule_name, _cut_size_x, _cut_size_y, _cut_spacing_x, _cut_spacing_y, _enclosure_bottom_x, _cut_enclosure_bottom_y, _enclosure_top_x, _enclosure_top_y, _num_cut_rows, _num_cut_cols, _patttern))
- TABLE4CLASS(IdbLayerRouting, (IdbLayer::_name, _width, ) )
	（get_spacing()使用）），（）
- TABLE4CLASS(IdbLayerCut, (IdbLayer::_name))
- TABLE4CLASS(IdbViaMasterRulePattern, (_patttern))
- TABLE4CLASS(IdbInstanceList, (), (_instance_list))
- TABLE4CLASS(IdbInstance, (_name, _type, _status, _orient, _cell_master, _coordinate, _halo, _route_halo), 
- TABLE4CLASS(IdbCellMaster, (_name), ())
- TABLE4CLASS(IdbHalo, (_is_soft, _extend_left, _extend_bottom, _extend_right, _extend_top), ())
- TABLE4CLASS(IdbRouteHalo,(_route_distance, _layer_bottom, _layer_top), ())
- TABLE4CLASS(IdbLayers, (_name, _type, _layer_id, _layer_order), ())



// todo
write_pin();




IdbLayers，（ ， ）（仅接口：find_layer_by_order()/get_layers_num()），（）

IdbLayer，（_name，_order），（）












IdbPins，（_pin_num），（IdbPin* _pin_list）

IdbPin，（_pin_name，_net_name，_is_special_net_pin，_term(IdbTerm*)，_location(IdbCoordinate<int32_t>*)，_orient），（）

IdbTerm，（_direction，_type，_is_special_net，_is_port_exist，_is_placed，_placement_status），（IdbPort* _port_list）

IdbPort，（_placement_status，_orient），（IdbLayerShape* _layer_shape / _port_box_list）

IdbLayerShape，（_layer(IdbLayer*)），（IdbRect* _rect_list）

IdbRect，（_lx,_ly,_hx,_hy），（）

IdbBlockageList，（_num），（IdbBlockage* _blockage_list）

IdbRoutingBlockage，（_layer_name，_pushdown，_except_pgnet，_instance/name），（IdbRect* _rect_list）

IdbPlacementBlockage，（_pushdown，_instance/name），（IdbRect* _rect_list）

IdbSpecialNetList，（_num），（IdbSpecialNet* _net_list）

IdbSpecialNet，（_net_name，_connect_type，_pin_string_list: vector<string>），（IdbPin*（io_pin_list/instance_pin_list中的 pin），IdbSpecialWire* _wire_list）

IdbSpecialWire，（_wire_state），（IdbSpecialWireSegment* _segment_list）

IdbSpecialWireSegment，（_shape_type/_route_width，_layer(IdbLayer*)，_delta_rect(IdbRect*)，_via(IdbVia*)，_points(start/second)），（）

IdbNetList，（_num），（IdbNet* _net_list）

IdbNet，（_net_name，_connect_type，_io_pins(IdbPins*)，_instance_pin_list(IdbPins*)），（IdbRegularWire* _wire_list）

IdbRegularWire，（_wire_statement，_shiled_name），（IdbRegularWireSegment* _segment_list）

IdbRegularWireSegment，（_layer(IdbLayer*)，_delta_rect(IdbRect*)，_via_list: vector<IdbVia*>，_points(start/second)，_virtual-flag），（IdbVia*）



IdbRegionList，（_num），（IdbRegion* _region_list）

IdbRegion，（_name，_type），（IdbRect* _boundary）

IdbSlotList，（_num），（IdbSlot* _slot_list）

IdbSlot，（_layer_name），（IdbRect* _rect_list）

IdbGroupList，（_num），（IdbGroup* _group_list）

IdbGroup，（_group_name，_region(IdbRegion*)），（IdbInstance* _instance_list）

IdbFillList，（_num_fill），（IdbFill* _fill_list）

IdbFill，（ ），（IdbRect*（经 layer->rect_list），IdbCoordinate<int32_t>*（经 via->coordinate_list））