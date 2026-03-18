# 调用清单（逐类）

## IdbDesign
IdbDesign *design = _def_service->get_design();
  design->set_version(version);
  design->set_design_name(name);
  design->get_bus_bit_chars();
    [IdbBusBitChars]
  units = design->get_units();
    [IdbUnits]
  design->get_blockage_list();
    [IdbBlockageList]
  design->get_instance_list();
    [IdbInstanceList]
  design->get_region_list();
    [IdbRegionList]
  design->get_net_list();
    [IdbNetList]
  design->get_io_pin_list();
    [IdbPins]
  design->get_via_list();
    [IdbVias]
  design->get_special_net_list();
    [IdbSpecialNetList]
  design->get_slot_list();
    [IdbSlotList]
  design->get_group_list();
    [IdbGroupList]
  design->get_fill_list();
    [IdbFillList]

// 不需要存
## IdbLayout
IdbLayout*  layout = _def_service->get_layout();
string _name;
  layout->get_layers();
    [IdbLayers]
  layout->get_units();
    [IdbUnits]
  layout->get_die();
    [IdbDie]
  layout->get_track_grid_list();
    [IdbTrackGridList]
  layout->get_gcell_grid_list();
    [IdbGCellGridList]
  layout->get_rows();
    [IdbRows]
  layout->get_sites();
    [IdbSites]
  layout->get_via_list();
    [IdbVias]
  layout->get_via_rule_list();
    [IdbViaRuleList]
  layout->set_manufacture_grid(transUnitDB(value));
  layout->set_units(new IdbUnits());
  layout->set_max_via_stack(max_via_stack);
    

## IdbUnits
  units->set_microns_dbu(microns);
  units->set_nanoseconds((int32_t)time);
  units->set_picofarads((int32_t)cap);
  units->set_ohms((int32_t)res);
  units->set_milliwatts((int32_t)pow);
  units->set_milliamps((int32_t)cur);
  units->set_volts((int32_t)volt);
  units->set_megahertz((int32_t)freq);
  layout->get_units()->get_micron_dbu();

## IdbDie
IdbDie* die = layout->get_die();
  die->add_point(x, y);
  die->set_bounding_box();

## IdbLayers
IdbLayers* layers = layout->get_layers();
  layers->find_layer(name);
    [IdbLayer]
  layer = layers->set_layer(name, type);
    [IdbLayer]
  layers->add_cut_layer(layer);
    [IdbLayerCut]
  layers->add_routing_layer(layer);
    [IdbLayerRouting]
  layers->find_layer(leflay->name());
    [IdbLayer]

## IdbLayer
layer->get_name();
layer->get_type();
layer->is_routing();
layer->is_cut();



## IdbLayerRouting : public IdbLayer
IdbLayerRouting *routing_layer = dynamic_cast<IdbLayerRouting*>(layer); [[poly]]
  routing_layer->add_track_grid(track_grid);
  routing_layer->set_width(transUnitDB(lef_layer->width()));
  routing_layer->set_min_width(transUnitDB(lef_layer->minwidth()));
  routing_layer->set_max_width(transUnitDB(lef_layer->maxwidth()));
  routing_layer->set_direction(lef_layer->direction());
  routing_layer->set_pitch(pitch);
  routing_layer->set_offset(offset);
  routing_layer->set_wire_extension(wext);
  routing_layer->set_thickness(transUnitDB(lef_layer->thickness()));
  routing_layer->set_height(transUnitDB(lef_layer->height()));
  routing_layer->set_resistance(lef_layer->resistance());
  routing_layer->set_capacitance(lef_layer->capacitance());
  routing_layer->set_edge_capacitance(lef_layer->edgeCap());
  spacing_list = routing_layer->get_spacing_list();
    [IdbLayerSpacingList]
  layer_spacing = new IdbLayerSpacing();
    [IdbLayerSpacing]
  spacing_list->add_spacing(layer_spacing);
  routing_layer->get_spacing_notchlength().set_notch_length(transUnitDB(...));
  routing_layer->get_spacing_notchlength().set_min_spacing(transUnitDB(...));
  routing_layer->set_parallel_spacing_table(std::shared_ptr<IdbParallelSpacingTable>);
  routing_layer->set_area(transAreaDB(lef_layer->area()));
  routing_layer->set_min_step(std::shared_ptr<IdbMinStep>);
  min_enclose_area = routing_layer->get_min_enclose_area_list();
    [IdbMinEncloseAreaList]
  min_enclose_area->add_min_area(area, width);
  routing_layer->set_min_density(lef_layer->minimumDensity());
  routing_layer->set_max_density(lef_layer->maximumDensity());
  routing_layer->set_density_check_length(lef_layer->densityCheckWindowLength());
  routing_layer->set_density_check_width(lef_layer->densityCheckWindowWidth());
  routing_layer->set_density_check_step(lef_layer->densityCheckStep());

## IdbLayerCut : public IdbLayer
IdbLayerCut* layer_cut = dynamic_cast<IdbLayerCut*>(layer); [[poly]]
  IdbLayerCut* layer_cut = dynamic_cast<IdbLayerCut*>(layer_list->find_layer(layer_cut_name));
  layer_cut->set_via_rule(via_rule);
  layer_cut->set_width(transUnitDB(lef_layer->width()));
  IdbLayerCut* layer_cut = dynamic_cast<IdbLayerCut*>(layer_list->find_layer(layer_cut_name));
  layer_cut->set_via_rule(via_rule);
  spacing = new IdbLayerCutSpacing(cut_spacing);
    [IdbLayerCutSpacing]
  spacing->set_adjacent_cuts(IdbLayerCutSpacing::AdjacentCuts(adj_cuts, transUnitDB(cut_within)));
  layer_cut->add_spacing(spacing);
  encl_above = layer_cut->get_enclosure_above();
    [IdbLayerCutEnclosure]
  encl_above->set_overhang_1(transUnitDB(...));
  encl_above->set_overhang_2(transUnitDB(...));
  encl_below = layer_cut->get_enclosure_below();
  [IdbLayerCutEnclosure]
  encl_below->set_overhang_1(transUnitDB(...));
  encl_below->set_overhang_1(transUnitDB(...));
  array_spacing = layer_cut->get_array_spacing();
  [IdbLayerCutArraySpacing]
  array_spacing->set_long_array(true);
  array_spacing->set_cut_spacing(transUnitDB(lef_layer->cutSpacing()));
  array_spacing->set_array_cut_num(lef_layer->numArrayCuts());
  array_spacing->set_array_value(i, cuts, transUnitDB(spacing));
  CutLayerParser(_lef_service).parse(propName, propValue, layer_cut);
  [CutLayerParser]

## IdbLayerMasterslice : public IdbLayer
IdbLayerMasterslice* layer_master = dynamic_cast<IdbLayerMasterslice*>(layer); [[poly]]

MastersliceLayerParser(_lef_service).parse(propName, propValue, layer_master);
[MastersliceLayerParser]

## IdbLayerOverlap : public IdbLayer
IdbLayerOverlap* layer_overlap = dynamic_cast<IdbLayerOverlap*>(layer); [[poly]]

## IdbLayerImplant : public IdbLayer
IdbLayerImplant* layer_implant = dynamic_cast<IdbLayerImplant*>(layer); [[poly]]
  min_spacing_list = layer_implant->get_min_spacing_list();
    [IdbLayerImplantMinSpacingList]
  min_spacing = min_spacing_list->add_min_spacing();
    [IdbLayerImplantMinSpacing]
  min_spacing->set_min_spacing(transUnitDB(lef_layer->spacing(i)));
  layers = _lef_service->get_layout()->get_layers();
    [IdbLayers]
  layer_2nd = layers->find_layer(lef_layer->spacingName(i));
    [IdbLayer]
  min_spacing->set_layer_2nd(layer_2nd);
  layer_implant->set_min_width(transUnitDB(lef_layer->width()));



## IdbTrackGridList
vector<IdbTrackGrid*> _track_grid_list;

IdbTrackGrid* track_grid = track_grid_list->add_track_grid(nullptr);

## IdbTrackGrid
IdbTrack* track = track_grid->get_track();
  track_grid->set_track_number(def_track->xNum());
  track_grid->add_layer_list(layer);
    [IdbLayer]

## IdbTrack
  track->set_direction(kDirectionX|kDirectionY);
  track->set_start(def_track->x());
  track->set_pitch(def_track->xStep());

## IdbGCellGridList
vector<IdbGCellGrid*> _gcelll_grid_list;

## IdbGCellGrid
gcell_grid->set_direction(kDirectionX|kDirectionY);
gcell_grid->set_num(def_grid->xNum());
gcell_grid->set_start(def_grid->x());
gcell_grid->set_space(def_grid->xStep());

## IdbRows
vector<IdbRow*> _row_list;

## IdbRow
row->set_name(def_row->name());
row->set_original_coordinate(def_row->x(), def_row->y());
lef_site = sites->add_site_list(def_row->macro());
  [IdbSite]
row_site = lef_site->clone();
  [IdbSite]
row->set_site(row_site);
row->set_orient(row_site->get_orient());
row->set_row_num_x(def_row->xNum());
row->set_row_num_y(def_row->yNum());
row->set_step_x(def_row->xStep());
row->set_step_y(def_row->yStep());
row->set_bounding_box();

## IdbSites
vector<IdbSite*> _site_list;

IdbSite* site = sites->add_site_list(site_name);
sites->find_site(lef_macro->siteName());

## IdbSite
row_site->set_orient_by_enum(def_row->orient());
site->set_symmetry(IdbSymmetry::kX|kY|kR90|kNone);
site->set_width(transUnitDB(lef_site->sizeX()));
site->set_height(transUnitDB(lef_site->sizeY()));
site->set_class(lef_site->siteClass());
idb_site->_name = _name;
idb_site->_b_overlap = _b_overlap;
idb_site->_site_class = _site_class;
idb_site->_symmetry = _symmetry;
idb_site->_orient = _orient;
idb_site->_width = _width;
idb_site->_heigtht = _heigtht;


## IdbCellMasterList
  vector<IdbCellMaster*> _master_List;

## IdbCellMaster
cell_master->set_type(lef_macro->macroClass());
cell_master->set_symmetry_x(true);
cell_master->set_symmetry_y(true);
cell_master->set_symmetry_R90(true);
cell_master->set_origin_x(transUnitDB(lef_macro->originX()));
cell_master->set_origin_y(transUnitDB(lef_macro->originY()));
cell_master->set_width(transUnitDB(lef_macro->sizeX()));
cell_master->set_height(transUnitDB(lef_macro->sizeY()));
cell_master->set_site(site);
  [IdbSite]
term = cell_master->add_term(lef_pin->name());
  [IdbTerm]
obs = cell_master->add_obs(nullptr);
  [IdbObs]

## IdbLayerShape
IdbLayerShape* layer_shape = obs_layer->get_shape();
layer_shape->set_layer(layer);
  [IdbLayer]
layer_shape->set_type_rect();
layer_shape->add_rect(llx, lly, urx, ury);

## IdbTerm
IdbTerm* term = cell_master->add_term(lef_pin->name());
term->set_name(pin->get_pin_name());
term->set_special(true);
term->set_as_instance_pin();
term->set_direction(lef_pin->direction());
term->set_type(lef_pin->use());
term->set_shape(lef_pin->shape());
term->set_has_port(true|false);
term->set_average_position(x, y);
term->set_bounding_box(llx, lly, urx, ury);
port = term->add_port();
  [IdbPort]
port = term->add_port(nullptr);
  [IdbPort]
term->set_placement_status_place();
term->set_placement_status_cover();
term->set_placement_status_fix();
term->set_placement_status_unplace();
io_term->get_average_position()
  [IdbCoordinate<int32_t>]

## IdbPort
port->set_orient_by_enum(def_port->orient());
port->set_port_class(lef_geometry->getClass(j));
shape = port->find_layer_shape(layer_name);
  [IdbLayerShape]
shape = port->add_layer_shape();
  [IdbLayerShape]
port->add_via(macro_via);
  [IdbVia]
port->set_placement_status_place();
port->set_placement_status_cover();
port->set_placement_status_fix();
port->set_placement_status_unplace();
port->set_coordinate(placementX(), placementY());

## IdbPins
std::vector<IdbPin*> _pin_list;

## IdbPin
std::string _pin_name; // zhiyi:set by idbinstance
pin->set_net_name(new_net_name);
pin->set_orient_by_enum(def_pin->orient());
pin->set_as_io();
io_term = pin->set_term(nullptr);
  [IdbTerm]
pin->set_location(x, y);
pin->set_average_coordinate(x, y);
pin->set_bounding_box();
pin->set_port_layer_shape();
pin->get_net();
pin->set_net(net);
  [IdbNet]
pin->set_special_net(snet);
  [IdbSpecialNet]

## IdbRegionList
std::vector<IdbRegion*> _region_list;

## IdbRegion
std::string _name;
region->set_type(def_region->type());
region->add_boundary(xl, yl, xh, yh);
region->add_instance(instance);
  [IdbInstance]
region->get_name();

## IdbInstanceList
std::vector<IdbInstance*> _instance_list;

instance_list->init(def_component_num);
instance = instance_list->add_instance(new_inst_name);
  [IdbInstance]
instance_list->find_instance(io_name);
  [IdbInstance]
instance_list->get_pin_list_by_names(io_name_array, net->get_instance_pin_list(), net->get_instance_list());

## IdbInstance
std::string _name;
instance->set_cell_master(_cur_cell_master);
  [IdbCellMaster]
instance->set_status_by_def_enum(def_component->placementStatus());
instance->set_orient_by_enum(def_component->placementOrient());
instance->set_type(def_component->source());
instance->set_weight(def_component->weight());
instance->set_region(region);
  [IdbRegion]
halo = instance->set_halo();
  [IdbHalo]
route_halo = instance->set_route_halo();
  [IdbRouteHalo]
instance->set_coodinate(def_component->placementX(), def_component->placementY());
pin = instance->get_pin_by_term(def_net->pin(i));
  [IdbPin]

## IdbHalo
halo->set_soft(def_component->hasHaloSoft());
halo->set_extend_lef(extend_left);
halo->set_extend_right(extend_right);
halo->set_extend_bottom(extend_bottom);
halo->set_extend_top(extend_top);

## IdbRouteHalo
route_halo->set_route_distance(def_component->haloDist());
route_halo->set_layer_bottom(layers->find_layer(def_component->minLayer()));
  [IdbLayer]
route_halo->set_layer_top(layers->find_layer(def_component->maxLayer()));
  [IdbLayer]

## IdbNetList
std::vector<IdbNet*> _net_list;

net_list->init(def_net_num);
IdbNet* net = net_list->add_net(new_net_name);

## IdbNet
std::string _net_name;
net->set_connect_type(def_net->use());
net->set_source_type(def_net->source());
net->set_weight(def_net->weight());
net->set_xtalk(def_net->XTalk());
net->set_frequency(def_net->frequency());
net->set_original_net_name(def_net->original());
net->add_io_pin(pin);
  [IdbPin]
net->add_instance_pin(pin);
  [IdbPin]
net->get_instance_list()->add_instance(instance);
  [IdbInstanceList]
wire_list = net->get_wire_list();
  [IdbRegularWireList]

## IdbRegularWireList
vector<IdbRegularWire*> _wire_list;

IdbRegularWire* wire = wire_list->add_wire(nullptr);

## IdbRegularWire
wire->set_wire_state(def_wire->wireType());
wire->set_shield_name(def_wire->wireShieldNetName());
IdbRegularWireSegment* segment = wire->add_segment(nullptr);

## IdbRegularWireSegment
segment->set_layer_name(def_path->getLayer());
segment->set_layer(layers->find_layer(def_path->getLayer()));
  [IdbLayer]
segment->set_is_via(true);
sseg->get_point_start()
coord = segment->get_point_end();
  [IdbCoordinate<int32_t>]
segment->add_point(x, y);
segment->add_virtual_point(x, y);
segment->set_is_rect(true);
segment->set_delta_rect(llx, lly, urx, ury);

## IdbSpecialNetList
vector<IdbSpecialNet*> _net_list;

## IdbSpecialNet
string _net_name;

snet->set_connect_type(def_net->use());
snet->set_source_type(def_net->source());
snet->set_weight(def_net->weight());
snet->set_original_net_name(def_net->original());
snet->add_pin_string(def_net->pin(i));
snet->add_io_pin(pin);
  [IdbPin]
snet->add_instance(instance);
  [IdbInstance]
snet->add_instance_pin(pin);
  [IdbPin]
snet->get_pin_string_list();
wire_list = snet->get_wire_list();
  [IdbSpecialWireList]

## IdbSpecialWireList
vector<IdbSpecialWire*> _wire_list;

## IdbSpecialWire
wire->set_wire_state(def_wire->wireType());
wire->set_shield_name(def_wire->wireShieldNetName());
seg = swire->add_segment(nullptr);
  [IdbSpecialWireSegment]

## IdbSpecialWireSegment
seg->set_layer_as_new();
seg->set_layer(layers->find_layer(def_path->getLayer()));
  [IdbLayer]
seg->set_is_via(true);
seg->get_point_start()
seg->set_route_width(def_path->getWidth());
seg->add_point(x, y);
seg->set_shape_type(def_path->getShape());
seg->set_style(def_path->getStyle());
seg->set_is_rect(true);
seg->set_delta_rect(llx, lly, urx, ury);
seg->set_bounding_box();

## IdbBlockageList
  int32_t _num;
  std::vector<IdbBlockage*> _blockage_list;
    [IdbRoutingBlockage]
    [IdbPlacementBlockage]

## IdbRoutingBlockage : public IdbBlockage
routing_Blockage->set_layer_name(layer_name);
routing_blockage->set_layer(layers->find_layer(def_blockage->layerName()));
  [IdbLayer]
routing_blockage->set_type_routing();
routing_blockage->set_slots(true);
routing_blockage->set_fills(true);
routing_blockage->set_pushdown(true);
routing_blockage->set_except_pgnet(true);
routing_blockage->set_instance_name(def_blockage->layerComponentName());
routing_blockage->set_instance(instance_list->find_instance(def_blockage->layerComponentName()));
  [IdbInstance]
routing_blockage->set_min_spacing(def_blockage->minSpacing());
routing_blockage->set_effective_width(def_blockage->designRuleWidth());
routing_blockage->add_rect(xl, yl, xh, yh);

## IdbPlacementBlockage : public IdbBlockage
placement_Blockage->set_type_placement();
placement_blockage->set_soft(true);
placement_blockage->set_max_density(def_blockage->placementMaxDensity());
placement_blockage->set_instance_name(def_blockage->layerComponentName());
placement_blockage->set_instance(instance_list->find_instance(def_blockage->layerComponentName()));
  [IdbInstance]
placement_blockage->add_rect(xl, yl, xh, yh);

## IdbSlotList
vector<IdbSlot*> _slot_list;

## IdbSlot
slot->set_layer_name(def_slot->layerName());
slot->add_rect(xl, yl, xh, yh);

## IdbGroupList
std::vector<IdbGroup*> _group_list;

IdbGroup* group = group_list->add_group(def_group->name());

## IdbGroup
std::string _group_name;
IdbRegion* _region;

group->set_region(region_list->find_region(def_group->regionName()));
  [IdbRegion]

## IdbFillList
std::vector<IdbFill*> _fill_list;

fill_layer = fill_list->add_fill_layer(layer);
  [IdbFillLayer]
fill_via = fill_list->add_fill_via(via_new);
  [IdbFillVia]

## IdbFill
fill->set_type_layer();
fill->set_type_via();
fill_layer = fill->get_layer();
  [IdbFillLayer]
fill_via = fill->get_via();
  [IdbFillVia]

## IdbFillLayer
fill_layer->set_layer(layer);
  [IdbLayer]
fill_layer->add_rect(xl, yl, xh, yh);

## IdbFillVia
fill_via->add_coordinate(x, y);

## IdbVias
vector<IdbVia*> _via_list;

via_list->find_via(name);
  [IdbVia]
via_instance = via_list->add_via(lef_via->name());
  [IdbVia]

## IdbVia : public IdbObject
string _name;
IdbViaMaster* _master_instance;
bool _b_master_clone = false;
IdbCoordinate<int32_t>* _coordinate;

via->clone();
via->set_coordinate(coord);
  [IdbCoordinate<int32_t>]
via_master = via->get_instance();
  [IdbViaMaster]

## IdbViaMaster
via_master->set_name(lef_via->name());
via_master->set_default(true);
via_master->set_type_generate();
via_master->set_type_fixed();
master_fixed = via_master->add_fixed(layer_name);
  [IdbViaMasterFixed]
via_master->set_cut_rect(min_x, min_y, max_x, max_y);
via_master->set_via_shape();
via_master->set_cut_row_col(lef_via->numCutRows(), lef_via->numCutCols());
master_generate = via_master->get_master_generate();
  [IdbViaMasterGenerate]

## IdbViaMasterFixed
IdbLayerShape* _layer_shape;

master_fixed->set_layer(layer);
  [IdbLayer]
master_fixed->add_rect(llx, lly, urx, ury);

## IdbViaMasterGenerate
master_generate->set_rule_name(rule_name);
master_generate->set_rule_generate(via_rule);
  [IdbViaRuleGenerate]
master_generate->set_cut_size(cutsize_x, cutsize_y);
master_generate->set_layer_bottom(dynamic_cast<IdbLayerRouting*>(layer)); [[poly]]
  [IdbLayerRouting]
master_generate->set_layer_cut(dynamic_cast<IdbLayerCut*>(layer)); [[poly]]
  [IdbLayerCut]
master_generate->set_layer_top(dynamic_cast<IdbLayerRouting*>(layer)); [[poly]]
  [IdbLayerRouting]
master_generate->set_cut_spacing(cut_spacing_x, cut_spacing_y);
master_generate->set_enclosure_bottom(encl_x, encl_y);
master_generate->set_enclosure_top(encl_x, encl_y);
master_generate->set_original(x, y);
master_generate->set_offset_bottom(x, y);
master_generate->set_offset_top(x, y);
master_generate->set_cut_row_col(r, c);
master_generate->set_patttern(pattern);
master_generate->is_pattern_cut_exist(i, j)
master_generate->add_cut_rect(llx, lly, urx, ury);
master_generate->set_cut_bouding_rect(llx, lly, urx, ury);
master_generate->set_spacing(spacing_x, spacing_y);
master_generate->swap_routing_layer();

## IdbViaRuleList
 vector<IdbViaRuleGenerate*> _via_rule_generate_list;

via_rule_generate = via_rule_list->add_via_rule_generate(name);
  [IdbViaRuleGenerate]
via_rule_list->find_via_rule_generate(rule_name);
  [IdbViaRuleGenerate]

## IdbViaRuleGenerate
via_rule_generate->get_enclosure_bottom();
  [IdbLayerCutEnclosure]
via_rule_generate->get_enclosure_top();
  [IdbLayerCutEnclosure]
via_rule_generate->set_layer_cut(cut_layer);
  [IdbLayerCut]
via_rule_generate->set_cut_rect(llx, lly, urx, ury);
via_rule_generate->set_spacing(spacing_x, spacing_y);

## IdbBusBitChars
bus_bit_chars->setLeftDelimiter(bus_bit_chars_str[0]);
bus_bit_chars->setRightDelimter(bus_bit_chars_str[1]);

## IdbMaxViaStack
max_via_stack = new IdbMaxViaStack();
  [IdbMaxViaStack]
max_via_stack->set_stacked_via_num(number);
max_via_stack->set_no_single(true);
max_via_stack->set_layer_bottom(bottom_layer);
max_via_stack->set_layer_top(top_layer);

## IdbCoordinate<int32_t>
segment->get_point_end();
segment->get_point_start();
segment->add_point(x, y);


## CutLayerParser / RoutingLayerParser / MastersliceLayerParser
CutLayerParser(_lef_service).parse(name, value, layer_cut);
[CutLayerParser]
RoutingLayerParser(_lef_service).parse(name, value, layer_routing);
[RoutingLayerParser]
MastersliceLayerParser(_lef_service).parse(name, value, layer_master);
[MastersliceLayerParser]

## IdbObs
IdbObsLayer* obs_layer = obs->add_obs_layer(nullptr);

## IdbObsLayer
IdbLayerShape* layer_shape = obs_layer->get_shape();

