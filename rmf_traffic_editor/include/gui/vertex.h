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

#ifndef VERTEX_H
#define VERTEX_H

#include <QUuid>
#include <map>
#include <string>
#include <vector>
#include <optional>
#include <yaml-cpp/yaml.h>

#include <QColor>

#include "coordinate_system.h"
#include "param.h"

class QGraphicsScene;


class Vertex
{
public:
  Vertex();

  Vertex(double _x, double _y, const std::string& _name = std::string());

  void fromYaml(
    const YAML::Node& data,
    const CoordinateSystem& coordinate_system);

  YAML::Node toYaml(const CoordinateSystem& coordinate_system) const;

  void setParam(const std::string& name, const std::string& value);

  void draw(
    QGraphicsScene* scene,
    const double radius,
    const double drawing_meters_per_pixel,
    const QFont& font,
    const CoordinateSystem& coordinate_system) const;

  bool isParkingPoint() const;

  bool isHoldingPoint() const;

  bool isCleaningZone() const;

  bool isCharger() const;

  std::optional<double> mergeRadius() const;

  std::string dropoffIngestor() const;

  std::string pickupDispenser() const;

  std::string liftCabin() const;

public:
  template <typename ParamSetType>
  static std::optional<std::string> getStringParam(const ParamSetType& params, const std::string& param_name)
  {
    const auto it = params.find(param_name);
    if (it != params.end() && it->second.type == Param::STRING)
      return it->second.value_string;
    return std::nullopt;
  };

public:
  double x;
  double y;
  std::string name;
  bool selected;
  QUuid uuid;
  std::map<std::string, Param> params;
  ////////////////////////////////////////////////////////////
  static const std::vector<std::pair<std::string, Param::Type>> allowed_params;
};

#endif