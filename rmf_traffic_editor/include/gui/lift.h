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

#ifndef LIFT_H
#define LIFT_H

class Building;
class QGraphicsScene;
class QGraphicsView;

/*
 * This class represents a lift, including the shape of the lift cabin,
 * the location and names of its doors, and the floors at which it can stop.
 */

#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>
#include <Eigen/Geometry>

#include <string>
#include <vector>
#include <cfloat>
#include <optional>

#include "level.h"
#include "lift_door.h"

// Indices into each named level's vertices; maintained by Lift.
// One shared cabin per level.
struct NavigationVertices
{
  int cabin = -1;
  std::map<std::string, int> doors; // door name -> door vertice id
  // Coordinates in the named level's scene units, not cabin-local meters.
  std::optional<Eigen::Vector2d> cabin_position;
  std::map<std::string, Eigen::Vector2d> door_positions;
};

class Lift
{
public:
  typedef std::list<std::string> DoorNameList;
  typedef std::map<std::string, DoorNameList> LevelDoorMap;

public:
  ////////////////////////////////////////////////////////////////////////
  Lift();

  YAML::Node toYaml() const;
  // Cabin points and outside -> cabin LineStrings for each level.
  std::vector<nlohmann::json> toJson(const std::vector<Level>& levels) const;

  void fromYaml(const std::string& _name, const YAML::Node& data,
    const std::vector<Level>& levels);

  void draw(QGraphicsScene* scene,
            const double meters_per_pixel,
            const int level_idx,
            const double elevation,
            const bool apply_transformation = true,
            Eigen::Affine2d& transform) const;

  bool levelDoorOpens(
    const std::string& level_name,
    const std::string& door_name,
    const std::vector<Level>& levels) const;

  std::vector<Vertex> createNavVertex(int id, double resolution);
  std::vector<Edge> createNavEdges();

public:
  std::vector<int> getNavVerticesIdx()
  {
    std::vector<int> result;

    std::transform(doors.begin(), doors.end(), std::back_inserter(result)
    [](auto&& door)
    {
      return door->door_id;
    });

    result.push_back(cabin_id);
  }

public:
  std::string name;
  std::string reference_floor_name;
  std::string initial_floor_name;

  int cabin_id;

  int reference_lvl;

  // (x, y, yaw) of the cabin center, relative to reference_floor_name origin
  Eigen::Isometry2d pose = Eigen::Isometry2d::Identity();
  // for now, we will model all lift cabins as rectangles
  double width = 1.0;  // meters
  double depth = 1.0;  // meters
  std::string highest_floor;  // highest floor the lift can reach
  std::string lowest_floor;  // lowest floor the lift can reach
  double highest_elevation = DBL_MAX;
  double lowest_elevation = -DBL_MAX;

  std::vector<LiftDoor> doors;

  // When this option is false, it indicates that a lift plugin and its lift
  // door plugins should not be included when building the simulation world.
  // This will help speed up the simulation as well as represent lifts that are
  // not accessible by AGVs.
  bool plugins = true;

  std::map<std::string, NavigationVertices> navigation_vertices;
};

#endif
