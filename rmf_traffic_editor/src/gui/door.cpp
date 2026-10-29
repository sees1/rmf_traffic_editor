#include "door.h"
#include <stdexcept>

Door::Door() : Edge(-1, -1, Edge::DOOR) {}
Door::Door(const Edge& edge) : Edge(edge)
{
  if (type != Edge::DOOR)
    throw std::invalid_argument("Door requires a door edge");
}
void Door::fromYaml(const YAML::Node& data)
{
  params.clear();
  Edge::fromYaml(data, Edge::DOOR);
}
namespace {
std::optional<Eigen::Vector2d> positionOf(
  const std::map<std::string, Param>& params, const std::string& side)
{
  const auto x = params.find(side + "_vertex_x");
  const auto y = params.find(side + "_vertex_y");
  if (x == params.end() || y == params.end() ||
    x->second.type != Param::DOUBLE || y->second.type != Param::DOUBLE)
    return std::nullopt;
  return Eigen::Vector2d(x->second.value_double, y->second.value_double);
}
}
std::optional<Eigen::Vector2d> Door::frontPosition() const
{
  return frontVertex() >= 0 ? positionOf(params, "front") : std::nullopt;
}
std::optional<Eigen::Vector2d> Door::backPosition() const
{
  return backVertex() >= 0 ? positionOf(params, "back") : std::nullopt;
}
void Door::setApproachPositions(const Eigen::Vector2d& front,
  const Eigen::Vector2d& back)
{
  if (frontVertex() < 0 || backVertex() < 0)
    return;
  params["front_vertex_x"] = Param(front.x());
  params["front_vertex_y"] = Param(front.y());
  params["back_vertex_x"] = Param(back.x());
  params["back_vertex_y"] = Param(back.y());
}
YAML::Node Door::toYaml() const 
{
  return Edge::toYaml();
}

nlohmann::json Door::toJson() const
{
  using Json = nlohmann::json;

  Json json_door, json_geometry, json_properties;
  
  json_door["type"] = "Feature";

  std::optional<Eigen::Vector2d> front_pos_opt = frontPosition();
  std::optional<Eigen::Vector2d> back_pos_opt = backPosition();

  if (!front_pos_opt.has_value() || !back_pos_opt.has_value() || !front_pos_opt->allFinite() || !back_pos_opt->allFinite())
    throw std::runtime_error("Door::toJson: don't have propriate pos of navigation vertices!");

  json_geometry["type"] = "LineString";
  json_geometry["coordinates"] = {{front_pos_opt->x(), front_pos_opt->y()}, {back_pos_opt->x(), back_pos_opt->y()}};

  json_properties["id"] = start_idx;
  json_properties["startid"] = start_idx;
  json_properties["endid"] = end_idx;
  json_properties["frame"] = "map";
  json_properties["cost"] = 10;
  json_properties["overridable"] = false;
  
  Json open_door_op;
  open_door_op["type"] = "open_door";
  open_door_op["trigger"] = "ON_ENTER";
  open_door_op["metadata"] = {};

  Json close_door_op;
  close_door_op["type"] = "close_door";
  close_door_op["trigger"] = "ON_EXIT";
  close_door_op["metadata"] = {};

  Json switch_floor_to_door_controller_op;
  switch_floor_to_door_controller_op["type"] = "switch_floor_to_door_controller";
  switch_floor_to_door_controller_op["trigger"] = "ON_ENTER";
  switch_floor_to_door_controller_op["metadata"] = {};

  Json switch_door_to_floor_controller_op;
  switch_door_to_floor_controller_op["type"] = "switch_door_to_floor_controller";
  switch_door_to_floor_controller_op["trigger"] = "ON_EXIT";
  switch_door_to_floor_controller_op["metadata"] = {};

  json_properties["operations"].push_back(open_door_op);
  json_properties["operations"].push_back(close_door_op);
  json_properties["operations"].push_back(switch_floor_to_door_controller_op);
  json_properties["operations"].push_back(switch_door_to_floor_controller_op);

  json_door["geometry"] = json_geometry;
  json_door["properties"] = json_properties;

  return json_door;
}
int Door::frontVertex() const
{
  const auto it = params.find("front_vertex");
  return it != params.end() && it->second.type == Param::INT ?
    it->second.value_int : -1;
}
int Door::backVertex() const
{
  const auto it = params.find("back_vertex");
  return it != params.end() && it->second.type == Param::INT ?
    it->second.value_int : -1;
}
void Door::setApproachVertices(int front, int back)
{
  params.erase("front_vertex_x");
  params.erase("front_vertex_y");
  params.erase("back_vertex_x");
  params.erase("back_vertex_y");
  params.erase("front_vertex");
  params.erase("back_vertex");
  if (front >= 0 && back >= 0)
  {
    params["front_vertex"] = Param(front);
    params["back_vertex"] = Param(back);
  }
}