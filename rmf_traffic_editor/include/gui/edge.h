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

#ifndef EDGE_H
#define EDGE_H

#include <string>
#include <map>

#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>

#include "param.h"
#include <QString>
#include <QColor>


class Edge
{
public:
  enum Type
  {
    UNDEFINED = 0,
    LANE,
    WALL,
    MEAS,
    DOOR,
    HUMAN_LANE,
  };

public:
  Edge();
  Edge(const int start_id, const int end_id, const Type t);
  Edge(const Edge& other);
  Edge(Edge&& other);
  Edge& operator=(const Edge& other);
  Edge& operator=(Edge&& other);
  ~Edge();

  void fromYaml(const YAML::Node& data, const Type edge_type);

  YAML::Node toYaml() const;

  nlohmann::json toJson() const;

  void setParam(const std::string& name, const std::string& value);

  bool isBidirectional() const;

  void createRequiredParameters();

  template<typename T>
  void createParamIfNeeded(
    const std::string& name,
    const Param::Type& param_type,
    const T& param_value);

  std::string typeToString() const;

  QString typeToQstring() const;

  void setGraphIdx(const int idx);

  int getGraphIdx() const;

  double getWidth() const;

  double displayWidth(double fallback) const;

  QColor displayColor(const QColor& fallback) const;

  bool contains(const int idx) const;

  bool valid(size_t graph_size) const
  {
    return !(start_idx < 0) || !(end_idx < 0) || (start_idx < graph_size) || (end_idx < graph_size);
  }

protected:
  int start_idx, end_idx;
  Type type;
  bool selected;  // only for visualization, not saved to YAML
  std::map<std::string, Param> params;
};

#endif