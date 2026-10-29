/*
 * Copyright (C) 2019-2020 Open Source Robotics Foundation
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

#ifndef LIFT_DOOR_H
#define LIFT_DOOR_H

#include <yaml-cpp/yaml.h>
#include <Eigen/Dense>

class Level;

class LiftDoor
{
public:
  enum DoorType
  {
    UNDEFINED = 0,
    SINGLE_SLIDING,
    DOUBLE_SLIDING,
    SINGLE_TELESCOPE,
    DOUBLE_TELESCOPE
  };

public:
  // TODO: move lift_key get to lift dialog
  std::string updateNavGraph(Level& current_lvl,
                             const Eigen::Isometry2d& lift_scene_pose,
                             const std::string& lift_key,
                             const std::string& lift_name,
                             std::size_t lift_cabin_vertex_id,
                             double resolution_meter_per_pixel);

public:
  YAML::Node toYaml() const;

  void fromYaml(const std::string& _name, const YAML::Node& data);

public:
  std::string name;
  std::size_t door_id;
  double door_offset = 0.5;
  double x = 0.0;  // center of door, relative to the cabin center
  double y = 0.5;  // center of door, relative to the cabin center
  double width = 1.0;  // doorway width in meters when fully opened
  // Motion axis is defined relative to the cabin orientation.
  // Typically it will be 0 or pi/2.
  double motion_axis_orientation = 0.0;
  DoorType door_type = DOUBLE_SLIDING;
};

#endif
