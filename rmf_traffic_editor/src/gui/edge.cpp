/*
 * Copyright (C) 2019 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include "edge.h"
#include <cmath>
using std::string;


Edge::Edge()
: start_idx(0),
  end_idx(0),
  type(UNDEFINED),
  selected(false)
{
}

Edge::Edge(const int start_id, const int end_id, const Type t)
: start_idx(_start_idx),
  end_idx(_end_idx),
  type(_type),
  selected(false)
{
  createRequiredParameters();
}

Edge(const Edge& other)
: start_idx(other.start_idx),
  end_idx(other.end_idx),
  type(other.type),
  selected(false)
{ }

Edge(Edge&& other)
: Edge(other)
{ }

Edge& Edge::operator=(const Edge& other)
{
  if (*this == other)
    return *this;

  start_idx = other.start_idx;
  end_idx = other.end_idx;
  type = other.type;
  selected = other.selected;
  return *this;
}

Edge& Edge::operator=(Edge&& other)
{
  return operator=(other);
}

Edge::~Edge()
{
}

void Edge::fromYaml(const YAML::Node& data, const Type edge_type)
{
  if (!data.IsSequence())
    throw std::runtime_error("Edge::from_yaml expected a sequence");
  start_idx = data[0].as<double>();
  end_idx = data[1].as<double>();
  type = edge_type;
  // load the parameters
  if (data.size() >= 2)
  {
    for (YAML::const_iterator it = data[2].begin(); it != data[2].end(); ++it)
    {
      Param p;
      p.fromYaml(it->second);
      params[it->first.as<string>()] = p;
    }
  }

  createRequiredParameters();
}

YAML::Node Edge::toYaml() const
{
  YAML::Node y;
  y.push_back(start_idx);
  y.push_back(end_idx);

  YAML::Node params_node(YAML::NodeType::Map);
  for (const auto& param : params)
    params_node[param.first] = param.second.toYaml();
  y.push_back(params_node);
  y.SetStyle(YAML::EmitterStyle::Flow);
  return y;
}

nlohmann::json Edge::toJson() const
{
  using Json = nlohmann::json;

  Json json_edge, json_geometry, json_properties;

  json_edge["type"] = "Feature";

  json_geometry["type"] = "LineString";

  json_properties["id"] = start_idx;
  json_properties["startid"] = start_idx;
  json_properties["endidx"] = end_idx;
  json_properties["frame"] = "map";
  json_properties["cost"] = 10;
  json_properties["overridable"] = false;

  json_edge["geometry"] = json_geometry;
  json_edge["properties"] = json_properties;

  return json_edge;
}

bool Edge::isBidirectional() const
{
  auto it = params.find("bidirectional");
  if (it == params.end() || it->second.type != Param::BOOL)
    return false;
  return it->second.value_bool;
}

void Edge::setParam(const std::string& name, const std::string& value)
{
  auto it = params.find(name);
  if (it == params.end())
  {
    printf("tried to set unknown parameter [%s]\n", name.c_str());
    return;  // unknown parameter
  }
  it->second.set(value);
}

template<typename T>
void Edge::createParamIfNeeded(
  const std::string& name,
  const Param::Type& param_type,
  const T& param_value)
{
  auto it = params.find(name);
  if (it == params.end() || it->second.type != param_type)
    params[name] = param_value;
}

void Edge::createRequiredParameters()
{
  // create required parameters if they don't exist yet on this edge
  if (type == MEAS)
  {
    auto it = params.find("distance");
    if (it == params.end() || it->second.type != Param::DOUBLE)
      params["distance"] = Param(1.0);
  }
  else if (type == WALL)
  {
    createParamIfNeeded("texture_name", Param::STRING,
      std::string("default"));
    createParamIfNeeded("alpha", Param::DOUBLE, 1.0);
    createParamIfNeeded("texture_height", Param::DOUBLE, 2.5);
    createParamIfNeeded("texture_width", Param::DOUBLE, 1.0);
    createParamIfNeeded("texture_scale", Param::DOUBLE, 1.0);
  }
  else if (type == LANE)
  {
    createParamIfNeeded("bidirectional", Param::BOOL, true);
    createParamIfNeeded("orientation", Param::STRING, std::string());
    createParamIfNeeded("graph_idx", Param::INT, 0);
    createParamIfNeeded("demo_mock_floor_name", Param::STRING,
      std::string());
    createParamIfNeeded("demo_mock_lift_name", Param::STRING, std::string());
    createParamIfNeeded("speed_limit", Param::DOUBLE, 0.0);
    createParamIfNeeded("mutex", Param::STRING, std::string());
  }
  else if (type == DOOR)
  {
    createParamIfNeeded("name", Param::STRING, std::string("null")); // default door name
    createParamIfNeeded("type", Param::STRING, std::string("hinged"));
    createParamIfNeeded("motion_axis", Param::STRING, std::string("start"));
    createParamIfNeeded("motion_direction", Param::INT, 1);
    createParamIfNeeded("motion_degrees", Param::DOUBLE, 90.0);  // hinged
    createParamIfNeeded("right_left_ratio", Param::DOUBLE, 1.0); // doubles
    createParamIfNeeded("plugin", Param::STRING, std::string("normal"));
  }
  else if (type == HUMAN_LANE)
  {
    createParamIfNeeded("width", Param::DOUBLE, 1.0);
    createParamIfNeeded("bidirectional", Param::BOOL, true);
    createParamIfNeeded("orientation", Param::STRING, std::string());
    createParamIfNeeded("graph_idx", Param::INT, 9);
    createParamIfNeeded("demo_mock_floor_name", Param::STRING,
      std::string());
    createParamIfNeeded("demo_mock_lift_name", Param::STRING, std::string());
  }
}

std::string Edge::typeToString() const
{
  if (type == LANE)
    return string("lane");
  else if (type == WALL)
    return string("wall");
  else if (type == MEAS)
    return string("measurement");
  else if (type == DOOR)
    return string("door");
  else if (type == HUMAN_LANE)
    return string("human_lane");
  return string("undefined");
}

QString Edge::typeToQstring() const
{
  return QString::fromStdString(typeToString());
}

void Edge::setGraphIdx(const int idx)
{
  if (type != LANE && type != HUMAN_LANE)
    return;// for now at least, only lanes have graph indices
  params["graph_idx"] = Param(idx);
}

int Edge::getGraphIdx() const
{
  if (type != LANE && type != HUMAN_LANE)
    return 0;// for now, only lanes have indices defined
  auto it = params.find("graph_idx");
  if (it == params.end() || it->second.type != Param::INT)
    return 0;// shouldn't get here
  return it->second.value_int;
}

double Edge::getWidth() const
{
  if (type != HUMAN_LANE)
    return -1.0;
  auto it = params.find("width");
  if (it == params.end() || it->second.type != Param::DOUBLE)
    return -1.0;// shouldn't get here
  return it->second.value_double;
}

bool Edge::contains(const int idx) const
{
  return start_idx == idx || end_idx == idx;
}

double Edge::displayWidth(double fallback) const
{
  const auto it = params.find("editor_width");
  if (it == params.end() || it->second.type != Param::DOUBLE)
    return fallback;
  const double width = it->second.value_double;
  return std::isfinite(width) && width > 0 ? width : fallback;
}

QColor Edge::displayColor(const QColor& fallback) const
{
  const auto it = params.find("editor_color");
  if (it == params.end() || it->second.type != Param::STRING)
    return fallback;
  const QColor color(QString::fromStdString(it->second.value_string));
  return color.isValid() ? color : fallback;
}
