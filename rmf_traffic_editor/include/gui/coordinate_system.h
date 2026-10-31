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

#ifndef COORDINATE_SYSTEM_H
#define COORDINATE_SYSTEM_H

#include <string>
#include <proj.h>

#include <Eigen/Dense>

class CoordinateSystem final
{
public:
  enum Type {Undefined = 0, ReferenceImage, WebMercator, CartesianMeters, WGS84};

public:
  CoordinateSystem();
  CoordinateSystem(const CoordinateSystem::Type value);
  CoordinateSystem(const CoordinateSystem& other);
  CoordinateSystem(CoordinateSystem&& other);
  CoordinateSystem& operator=(const CoordinateSystem& other);
  CoordinateSystem& operator=(CoordinateSystem&& other);

  ~CoordinateSystem();

  void initProj();

  bool isYFlipped() const;
  double defaultScale() const;
  bool hasTiles() const;
  bool isGlobal() const;

  // Convert from point_system to this system; WGS84 uses (longitude, latitude).
  Eigen::Vector2d convert(const Eigen::Vector2d& point, CoordinateSystem::Type point_system) const;

public:
  // implicit-cast operator's
  operator std::string() const;

public:
  static CoordinateSystem::Type typeFromString(const std::string& s);

  // equatorial radius of the earth in WGS84 (meters)
  static constexpr double wgs84_a = 6378137.0;

private:
  void reset();

private:
  Type type;

  PJ_CONTEXT* proj_context;
  PJ* epsg_3857_to_wgs84;

  /*
  // even for maps defined in WGS84, we often want to generate a local
  // cartesian plane for simulation and navigation. This field is
  // intended to hold the CRS name, such as "EPSG:XXXXX" that will be
  // eventually used by downstream tools to generate cartesian
  // approximations of this world.
  std::string generate_crs;
  */
};
#endif