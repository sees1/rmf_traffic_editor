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

#include <cmath>

#include "lift_door.h"
#include "level.h"

Vertex createNavVertex(int id, const Eigen::Isometry2d& lift_pose, double resolution)
{
  door_id = id;

  Eigen::Vector2d normal(std::sin(motion_axis_orientation), std::cos(motion_axis_orientation));
  
  if (normal.dot(local_center) < 0)
    normal = -normal;

  Eigen::Vector2d position = lift_pose * ((pose + door_offset * normal) / resolution);

  return Vertex(position.x(), position.y(), name);
}

Edge createNavEdge()

std::string LiftDoor::updateNavVertex(Level& level,
  const Eigen::Isometry2d& lift_scene_pose,
  const std::string& lift_key,
  const std::string& lift_name,
  std::size_t lift_cabin_vertex_id,
  double resolution_meter_per_pixel)
{
  const std::string door_key = lift_key + "/door/" + name;
  const Eigen::Vector2d local_center(x, -y);
  Eigen::Vector2d normal(std::sin(motion_axis_orientation), std::cos(motion_axis_orientation));
  if (normal.dot(local_center) < 0)
    normal = -normal;
  const Eigen::Vector2d position = lift_scene_pose *
    ((local_center + door_offset * normal) / resolution_meter_per_pixel);

  const auto existing = level.getEditorGeneratedVertexByKey(door_key);
  if (existing)
    door_id = *existing;
  else
  {
    Vertex vertex(position.x(), position.y(), lift_name + "_" + name);
    vertex.params["editor_generated_vertex"] = Param(door_key);
    level.addVertex(vertex);
    door_id = level.vertices.size() - 1;
  }
  auto& vertex = level.vertices[door_id];
  vertex.x = position.x();
  vertex.y = position.y();
  vertex.params["lift_name"] = Param(lift_name);
  vertex.params["lift_door_name"] = Param(name);

  const auto existing_edge = level.getEditorGeneratedEdgeByKey(door_key);
  Edge* edge;
  if (existing_edge)
    edge = *existing_edge;
  else
  {
    level.edges.emplace_back(door_id, lift_cabin_vertex_id, Edge::LANE);
    edge = &level.edges.back();
    edge->params["editor_generated_edge"] = Param(door_key);
  }
  edge->start_idx = door_id;
  edge->end_idx = lift_cabin_vertex_id;
  return door_key;
}

YAML::Node LiftDoor::toYaml() const
{
  // This is in image space. I think it's safe to say nobody is clicking
  // with more than 1/1000 precision inside a single pixel.

  YAML::Node n;
  n["x"] = std::round(x * 1000.0) / 1000.0;
  n["y"] = std::round(y * 1000.0) / 1000.0;
  n["width"] = std::round(width * 1000.0) / 1000.0;
  n["door_type"] = static_cast<int>(door_type);
  // let's give yaw another decimal place because, I don't know, reasons (?)
  n["motion_axis_orientation"] =
    std::round(motion_axis_orientation * 10000.0) / 10000.0;
  return n;
}

void LiftDoor::fromYaml(const std::string& _name, const YAML::Node& data)
{
  if (!data.IsMap())
    throw std::runtime_error("LiftDoor::from_yaml() expected a map");
  x = data["x"].as<double>();
  y = data["y"].as<double>();
  width = data["width"].as<double>();
  door_type = static_cast<DoorType>(data["door_type"].as<int>());
  motion_axis_orientation = data["motion_axis_orientation"].as<double>();
  name = _name;
}
