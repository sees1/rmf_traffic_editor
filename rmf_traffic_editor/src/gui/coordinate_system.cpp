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

#include "coordinate_system.h"

CoordinateSystem::CoordinateSystem()
: CoordinateSystem(CoordinateSystem::Type::Undefined)
{ }

CoordinateSystem::CoordinateSystem(const CoordinateSystem::Type t)
: type(t),
  proj_context(nullptr),
  epsg_3857_to_wgs84(nullptr)
{
  printf("CoordinateSystem::CoordinateSystem(Type)\n");
  initProj();
}

CoordinateSystem::CoordinateSystem(const CoordinateSystem& other)
: CoordinateSystem(other.type)
{ }

CoordinateSystem::CoordinateSystem(CoordinateSystem&& other)
: CoordinateSystem(other)
{ }

CoordinateSystem& CoordinateSystem::operator=(const CoordinateSystem& other)
{
  if (&other == this)
    return *this;

  type = other.type;
  reset();
  initProj();

  return *this;
}

CoordinateSystem& CoordinateSystem::operator=(CoordinateSystem&& other)
{
  return CoordinateSystem::operator=(other);
}

CoordinateSystem::~CoordinateSystem()
{
  printf("CoordinateSystem::~CoordinateSystem()\n");

  reset();
}

void CoordinateSystem::initProj()
{
  printf("CoordinateSystem::init_proj()\n");

  if (proj_context)
  {
    printf("CoordinateSystem::init_proj() called twice!\n");
    return;
  }

  proj_context = proj_context_create();
  epsg_3857_to_wgs84 = proj_create_crs_to_crs(proj_context, "EPSG:3857", "EPSG:4326", NULL);
}

CoordinateSystem::operator std::string() const
{
  switch(type)
  {
    case ReferenceImage: return "reference_image";
    case WebMercator: return "web_mercator";
    case CartesianMeters: return "cartesian_meters";
    case WGS84: return "wgs84";
    case Undefined: return "undefined";
    default: return "undefined";
  }
}

bool CoordinateSystem::isYFlipped() const
{
  return type == CoordinateSystem::Type::ReferenceImage;
}

bool CoordinateSystem::isGlobal() const
{
  return type == CoordinateSystem::Type::WGS84;
}

double CoordinateSystem::defaultScale() const
{
  // Image-based maps are often somewhere around 5cm
  // pixel cell size, since that's the common discretization
  // of SLAM-based robot maps. Let's use that for the
  // default scale (unless computed otherwise; hopefully
  // the map provides a few proper measurements). However,
  // for other coordinate systems, we should assume a
  // scale of 1.0 so that the coordinates stay as specified
  // and the UI tools work as expected in, for example,
  // Cartesian-meters coordinate systems.

  if (type == CoordinateSystem::Type::ReferenceImage)
    return 0.05;
    
  return 1.0;
}

bool CoordinateSystem::hasTiles() const
{
  return type == CoordinateSystem::Type::WGS84;
}

CoordinateSystem::Type CoordinateSystem::typeFromString(const std::string& s)
{
  if (s == "reference_image")
    return ReferenceImage;
  else if (s == "web_mercator")
    return WebMercator;
  else if (s == "cartesian_meters")
    return CartesianMeters;
  else if (s == "wgs84")
    return WGS84;
  else
    return Undefined;
}

Eigen::Vector2d CoordinateSystem::convert(const Eigen::Vector2d& point, CoordinateSystem::Type point_system) const
{
  if (type == point_system)
    return point;
  else if (type == CoordinateSystem::Type::WGS84 && point_system == CoordinateSystem::Type::WebMercator)
  {
    const PJ_COORD c = proj_coord(point.x(), point.y(), 0, 0);
    const PJ_COORD wgs84_coord = proj_trans(epsg_3857_to_wgs84, PJ_FWD, c);
    return {wgs84_coord.v[1], wgs84_coord.v[0]};
  }
  else if (type == CoordinateSystem::Type::WebMercator && point_system == CoordinateSystem::Type::WGS84)
  {
    const PJ_COORD p_in = proj_coord(point.y(), point.x(), 0, 0);
    const PJ_COORD p_out = proj_trans(epsg_3857_to_wgs84, PJ_INV, p_in);
    return {p_out.v[0], p_out.v[1]};
  }
  else
    return {0.0, 0.0};
}

void CoordinateSystem::reset()
{
  if (epsg_3857_to_wgs84)
    proj_destroy(epsg_3857_to_wgs84);

  if (proj_context)
    proj_context_destroy(proj_context);

  epsg_3857_to_wgs84 = nullptr;
  proj_context = nullptr;
}