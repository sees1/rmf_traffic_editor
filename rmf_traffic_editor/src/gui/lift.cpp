/*
 * Copyright (C) 2019-2021 Open Source Robotics Foundation
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

#include <algorithm>
#include "utils/geometry.hpp"
#include <cmath>

#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>

#include "lift.h"
#include "building.h"
#include "yaml_utils.h"
#include <QJsonDocument>
#include <QJsonObject>
using std::string;


Lift::Lift()
{
}

void Lift::fromYaml(
  const std::string& _name,
  const YAML::Node& data,
  const std::vector<Level>& levels)
{
  if (!data.IsMap())
    throw std::runtime_error("Lift::from_yaml() expected a map");
  navigation_vertices.clear();
  doors.clear();
  level_doors.clear();
  if (const auto vertices = data["navigation_vertices"])
  {
    for (const auto& entry : vertices)
    {
      auto& refs = navigation_vertices[entry.first.as<std::string>()];
      refs.cabin = entry.second["cabin"].as<int>();
      if (const auto p = entry.second["cabin_position"])
        refs.cabin_position = Eigen::Vector2d(p[0].as<double>(), p[1].as<double>());
      if (const auto positions = entry.second["door_positions"])
        for (const auto& position : positions)
          refs.door_positions[position.first.as<std::string>()] =
            Eigen::Vector2d(position.second[0].as<double>(), position.second[1].as<double>());
      for (const auto& door : entry.second["doors"])
        refs.doors[door.first.as<std::string>()] = door.second.as<int>();
    }
  }
  pose = Eigen::Isometry2d::Identity();
  pose.translation() = Eigen::Vector2d(
    data["x"].as<double>(), data["y"].as<double>());
  pose.linear() = Eigen::Rotation2Dd(data["yaw"].as<double>()).toRotationMatrix();
  name = _name;
  reference_floor_name = data["reference_floor_name"].as<string>();
  if (data["initial_floor_name"])
    initial_floor_name = data["initial_floor_name"].as<string>();
  else
    initial_floor_name = reference_floor_name;
  width = data["width"].as<double>();
  depth = data["depth"].as<double>();
  if (data["plugins"])
    plugins = data["plugins"].as<bool>();
  else
    plugins = true;

  if (data["doors"] && data["doors"].IsMap())
  {
    const YAML::Node& yd = data["doors"];
    for (YAML::const_iterator it = yd.begin(); it != yd.end(); ++it)
    {
      LiftDoor door;
      door.fromYaml(it->first.as<string>(), it->second);
      doors.push_back(door);
    }
  }

  if (data["highest_floor"])
    highest_floor = data["highest_floor"].as<string>();
  if (data["lowest_floor"])
    lowest_floor = data["lowest_floor"].as<string>();
  for (const auto& level : levels)
  {
    if (level.name == highest_floor)
      highest_elevation = level.elevation;
    if (level.name == lowest_floor)
      lowest_elevation = level.elevation;
  }

  // for every level, load if every door can open
  if (data["level_doors"] && data["level_doors"].IsMap())
  {
    const YAML::Node ym = data["level_doors"];
    for (YAML::const_iterator it = ym.begin(); it != ym.end(); ++it)
    {
      const std::string level_name = it->first.as<string>();
      const YAML::Node& ds = it->second;  // doors sequence node
      if (ds.IsSequence())
      {
        for (std::size_t idx = 0; idx < ds.size(); ++idx)
        {
          const std::string door_name = ds[idx].as<string>();
          level_doors[level_name].push_back(door_name);
        }
      }
    }
  }
}

YAML::Node Lift::toYaml() const
{
  // This is in image space. I think it's safe to say nobody is clicking
  // with more than 1/1000 precision inside a single pixel.

  const double x = pose.translation().x();
  const double y = pose.translation().y();
  const double yaw = std::atan2(pose.linear()(1, 0), pose.linear()(0, 0));
  YAML::Node n;
  n["x"] = std::round(x * 1000.0) / 1000.0;
  n["y"] = std::round(y * 1000.0) / 1000.0;
  // let's give yaw another decimal place because, I don't know, reasons (?)
  n["yaw"] = std::round(yaw * 10000.0) / 10000.0;
  n["reference_floor_name"] = reference_floor_name;
  n["reference_floor_name"].SetTag("!");
  n["highest_floor"] = highest_floor;
  n["highest_floor"].SetTag("!");
  n["lowest_floor"] = lowest_floor;
  n["lowest_floor"].SetTag("!");
  n["initial_floor_name"] = initial_floor_name;
  n["initial_floor_name"].SetTag("!");
  n["width"] = std::round(width * 1000.0) / 1000.0;
  n["depth"] = std::round(depth * 1000.0) / 1000.0;
  n["plugins"] = plugins;

  n["doors"] = YAML::Node(YAML::NodeType::Map);
  for (const auto& door : doors)
    n["doors"][door.name] = door.toYaml();

  n["level_doors"] = YAML::Node(YAML::NodeType::Map);
  for (LevelDoorMap::const_iterator level_it = level_doors.begin();
    level_it != level_doors.end();
    ++level_it)
  {
    const DoorNameList& dlist = level_it->second;
    for (DoorNameList::const_iterator door_it = dlist.begin();
      door_it != dlist.end();
      ++door_it)
    {
      YAML::Node door_name(*door_it);
      door_name.SetTag("!");
      n["level_doors"][level_it->first].push_back(door_name);
      n["level_doors"][level_it->first].SetStyle(YAML::EmitterStyle::Flow);
    }
  }
  if (!navigation_vertices.empty())
  {
    for (const auto& entry : navigation_vertices)
    {
      auto refs = n["navigation_vertices"][entry.first];
      refs["cabin"] = entry.second.cabin;
      if (entry.second.cabin_position)
      {
        refs["cabin_position"].push_back(entry.second.cabin_position->x());
        refs["cabin_position"].push_back(entry.second.cabin_position->y());
      }
      refs["door_positions"] = YAML::Node(YAML::NodeType::Map);
      for (const auto& position : entry.second.door_positions)
      {
        refs["door_positions"][position.first].push_back(position.second.x());
        refs["door_positions"][position.first].push_back(position.second.y());
      }
      refs["doors"] = YAML::Node(YAML::NodeType::Map);
      for (const auto& door : entry.second.doors)
        refs["doors"][door.first] = door.second;
    }
  }
  return n;
}

std::vector<nlohmann::json> Lift::toJson(const std::vector<Level>& levels) const
{
  using Json = nlohmann::json;
  std::vector<Json> features;
  const auto elevationOf = [&levels](const std::string& name)
  {
    for (const auto& level : levels)
      if (level.name == name)
        return level.elevation;
    throw std::runtime_error("Lift::toJson: unknown level " + name);
  };

  for (auto&& level : navigation_vertices)
  {
    const auto& lift = level.second;
    if (lift.cabin < 0 || !lift.cabin_position || !lift.cabin_position->allFinite())
      continue;
    const auto& cabin = *lift.cabin_position;

    Json json_lift, json_geometry, json_properties;
    json_lift["type"] = "Feature";

    json_geometry["type"] = "Point";
    json_geometry["elevation"] = elevationOf(level.first);
    json_geometry["coordinates"] = {cabin.x(), cabin.y()};

    json_properties["id"] = lift.cabin;
    json_properties["frame"] = "map";
    json_properties["metadata"] = {};

    Json switch_floor_op;
    switch_floor_op["type"] = "switch_floor";
    switch_floor_op["trigger"] = "NODE";
    switch_floor_op["metadata"] = {};
    
    Json open_door_op;
    open_door_op["type"] = "open_door";
    open_door_op["trigger"] = "NODE";
    open_door_op["metadata"] = {};

    Json close_door_op;
    close_door_op["type"] = "close_door";
    close_door_op["trigger"] = "NODE";
    close_door_op["metadata"] = {};

    json_properties["operations"].push_back(switch_floor_op);
    json_properties["operations"].push_back(open_door_op);
    json_properties["operations"].push_back(close_door_op);

    json_lift["geometry"] = json_geometry;
    json_lift["properties"] = json_properties;

    features.push_back(json_lift);
  }

  for (auto&& level : navigation_vertices)
  {
    const auto& lift = level.second;

    if (lift.cabin < 0 || !lift.cabin_position || !lift.cabin_position->allFinite())
      continue;

    for (const auto& door : lift.doors)
    {
      auto door_pos_it = lift.door_positions.find(door.first);
      
      if (door.second < 0 || door_pos_it == lift.door_positions.end() || !door_pos_it->second.allFinite())
        continue;

      const auto& door_pos = door_pos_it->second;
      const auto& cabin_pos = *lift.cabin_position;

      Json json_door, json_geometry, json_properties;

      json_door["type"] = "Feature";

      json_geometry["type"] = "LineString";
      json_geometry["elevation"] = elevationOf(level.first);
      json_geometry["coordinates"] = {{door_pos.x(), door_pos.y()}, {cabin_pos.x(), cabin_pos.y()}};

      json_properties["id"] = lift.cabin;
      json_properties["startid"] = door.second;
      json_properties["endid"] = lift.cabin;
      json_properties["frame"] = "map";
      json_properties["cost"] = 10;
      json_properties["overridable"] = false;
      json_properties["metadata"] = {};

      Json call_lift_op;
      call_lift_op["type"] = "call_lift";
      call_lift_op["trigger"] = "ON_ENTER";
      call_lift_op["metadata"] = {};

      Json switch_floor_to_lift_controller_op;
      switch_floor_to_lift_controller_op["type"] = "switch_floor_to_lift_controller";
      switch_floor_to_lift_controller_op["trigger"] = "ON_ENTER";
      switch_floor_to_lift_controller_op["metadata"] = {};

      Json switch_lift_to_floor_controller_op;
      switch_lift_to_floor_controller_op["type"] = "switch_lift_to_floor_controller";
      switch_lift_to_floor_controller_op["trigger"] = "ON_EXIT";
      switch_lift_to_floor_controller_op["metadata"] = {};

      json_properties["operations"].push_back(call_lift_op);
      json_properties["operations"].push_back(switch_floor_to_lift_controller_op);
      json_properties["operations"].push_back(switch_lift_to_floor_controller_op);

      json_door["geometry"] = json_geometry;
      json_door["properties"] = json_properties;

      features.push_back(json_door);
    }
  }

  return features;
}

/// The level_name parameter is required in order to know how to draw the
/// doors, since many lifts have more than one set of doors, which open on
/// some but not all floors. It's not being used (yet).
void Lift::draw(
  QGraphicsScene* scene,
  const double resolution,
  const int level_idx,
  const double elevation,
  const bool apply_transformation,
  Eigen::Affine2d transform) const
{
  if (elevation > highest_elevation || elevation < lowest_elevation)
    return;
  
  QPen cabin_pen(Qt::black);
  cabin_pen.setWidth(0.05 / resolution);

  const double cabin_w = width / resolution;
  const double cabin_d = depth / resolution;

  QGraphicsRectItem* cabin_rect = new QGraphicsRectItem(-cabin_w / 2.0, -cabin_d / 2.0, cabin_w, cabin_d);
  cabin_rect->setPen(cabin_pen);
  
  if (std::any_of(doors.begin(), doors.end(), [](auto&& door) { return door.hasOnLevel(level_idx); }))
    cabin_rect->setBrush(QBrush(QColor::fromRgbF(0.5, 1.0, 0.5, 0.5)));
  else
    cabin_rect->setBrush(QBrush(QColor::fromRgbF(1.0, 0.3, 0.3, 0.3)));
  
  scene->addItem(cabin_rect);

  QList<QGraphicsItem*> items;
  items.append(cabin_rect);

  if (!name.empty())
  {
    QFont font("Helvetica");
    font.setPointSize(0.2 / meters_per_pixel);
    QGraphicsSimpleTextItem* text_item = scene->addSimpleText(QString::fromStdString(name), font);
    text_item->setBrush(QColor(255, 0, 0, 255));
    text_item->setPos(-cabin_w / 3.0, 0.0);

    // todo: set font size to something reasonable
    // todo: center-align text?
    items.append(text_item);
  }

  for(auto&& door : doors)
  {
    if (door.hasOnLevel(level_idx))
    {
      const double door_x = door.pose.x() / meters_per_pixel;
      const double door_y = -door.pose.y() / meters_per_pixel;
    
      const double door_w = door.width / meters_per_pixel;

      const double door_thickness = 0.2 / meters_per_pixel;

      QGraphicsRectItem* door_item = new QGraphicsRectItem(-door_w / 2.0, -door_thickness / 2.0, door_w, door_thickness);
      door_item->setRotation(-180.0 / 3.1415926 * door.motion_axis_orientation);
      door_item->setPos(door_x, door_y);

      QPen door_pen(Qt::red);
      door_pen.setWidth(0.05 / meters_per_pixel);
      door_item->setPen(door_pen);
      door_item->setBrush(QBrush(QColor::fromRgbF(1.0, 0.0, 0.0, 0.5)));

      items.append(door_item);
    }
  }

  QGraphicsItemGroup* group = scene->createItemGroup(items);

  if (apply_transformation)
  {
    const auto displayed_pose = utils::geometry::scenePose(*this, scale, translate_x, translate_y, rotation);
    group->setRotation(180.0 / std::acos(-1.0) * std::atan2(displayed_pose.linear()(1, 0), displayed_pose.linear()(0, 0)));
    group->setPos(displayed_pose.translation().x(), displayed_pose.translation().y());
  }
}

bool Lift::levelDoorOpens(
  const std::string& level_name,
  const std::string& door_name,
  const std::vector<Level>& levels) const
{
  LevelDoorMap::const_iterator level_it = level_doors.find(level_name);
  if (level_it == level_doors.end())
    return false;
  for (const auto& level : levels)
  {
    if (level.name == level_name)
    {
      if (level.elevation < lowest_elevation ||
        level.elevation > highest_elevation)
        return false;
      break;
    }
  }
  const DoorNameList& names = level_it->second;
  if (std::find(names.begin(), names.end(), door_name) == names.end())
    return false;
  return true;
}

std::vector<Vertex> Lift::createNavVertices(int id, double resolution)
{
  cabin_id = id;

  std::vector<Vertex> result;
  std::for_each(doors.begin(), doors.end(),
  [&result, &id](auto&& door)
  {
    Vertex vert = door.createNavVertex(++id, pose, resolution);

    Eigen::Vector2d vert_pos(vert.x, vert.y);
    Eigen::Isometry2d lift_door_pose = lift->pose * vert_pos;

    vert->x = lift_door_pose.x();
    vert->y = lift_door_pose.y();
    vert->name += "_";
    vert->name += name;
  });

  return result;
}

std::vector<Edge> createNavEdges()
{
  std::vector<Edge> result;
  std::for_each(doors.begin(), doors.end(),
  [&result](auto&& door)
  {
    result.emplace_back(cabin_id, door.door_id, Edge::LANE);
  });

  return result;
}