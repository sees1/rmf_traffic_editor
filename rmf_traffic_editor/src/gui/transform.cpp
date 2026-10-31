/*
 * Copyright (C) 2021 Open Source Robotics Foundation
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

#include "transform.hpp"

using std::string;


Transform::Transform()
{
}

bool Transform::fromYaml(
  const YAML::Node& data,
  const CoordinateSystem& coordinate_system)
{
  if (!coordinate_system.isGlobal())
  {
    if (data["translation_x"])
      _translation.setX(data["translation_x"].as<double>());
    if (data["translation_y"])
      _translation.setY(data["translation_y"].as<double>());
  }
  else if (data["translation_lat"] && data["translation_lon"])
  {
    Eigen::Vector2d wgs84_point(data["translation_lon"].as<double>(), data["translation_lat"].as<double>());
    Eigen::Vector2d epsg_3857_point = CoordinateSystem(CoordinateSystem::Type::WebMercator).convert(wgs84_point, CoordinateSystem::Type::WGS84);
    _translation.setX(epsg_3857_point.x());
    _translation.setY(epsg_3857_point.y());
  }

  if (data["yaw"])
    _yaw = data["yaw"].as<double>();

  if (data["scale"])
    _scale = data["scale"].as<double>();

  return true;
}

YAML::Node Transform::toYaml(const CoordinateSystem& coordinate_system) const
{
  YAML::Node y;
  y["yaw"] = _yaw;
  y["scale"] = _scale;

  if (!coordinate_system.isGlobal())
  {
    y["translation_x"] = _translation.x();
    y["translation_y"] = _translation.y();
  }
  else
  {
    Eigen::Vector2d epsg_3857_point(_translation.x(), _translation.y());
    Eigen::Vector2d wgs_84_point = coordinate_system.convert(epsg_3857_point, CoordinateSystem::Type::WebMercator);
    y["translation_lat"] = wgs_84_point.y();
    y["translation_lon"] = wgs_84_point.x();
  }
  return y;
}

QPointF Transform::forwards(const QPointF& p) const
{
  const double qx =
    ( cos(_yaw) * p.x() + sin(_yaw) * p.y()) * _scale + _translation.x();

  const double qy =
    (-sin(_yaw) * p.x() + cos(_yaw) * p.y()) * _scale + _translation.y();

  return QPointF(qx, qy);
}

QPointF Transform::backwards(const QPointF& p) const
{
  // translate back and scale
  const double tsx = (p.x() - _translation.x()) / _scale;
  const double tsy = (p.y() - _translation.y()) / _scale;

  // rotate back
  const double rx = cos(-_yaw) * tsx + sin(-_yaw) * tsy;
  const double ry = -sin(-_yaw) * tsx + cos(-_yaw) * tsy;
  return QPointF(rx, ry);
}

Transform Transform::inverse() const
{
  Transform inv;
  inv.setYaw(-_yaw);
  inv.setScale(1.0 / _scale);
  inv.setTranslation(
    1.0 / _scale *
    QPointF(
      cos(-_yaw) * translation().x() - sin(-_yaw) * translation().y(),
      sin(-_yaw) * translation().x() + cos(-_yaw) * translation().y()));
  return inv;
}

string Transform::toString() const
{
  // make a string the old fashioned way...
  char buf[1024] = {0};
  snprintf(
    buf,
    sizeof(buf),
    "rotation: %.5f\n"
    "scale: %.5f\n"
    "trans X: %.5f\n"
    "trans Y: %.5f",
    _yaw,
    _scale,
    _translation.x(),
    _translation.y());
  return string(buf);
}
