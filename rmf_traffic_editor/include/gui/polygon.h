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

#ifndef POLYGON_H
#define POLYGON_H

#include <map>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

#include <QPolygonF>

#include "param.h"

struct EdgeDragPolygon
{
  QPolygonF polygon;
  int movable_vertex = -1;
};

class Polygon final
{
public:
  using iterator = std::vector<int>::iterator;
  using const_iterator = std::vector<int>::const_iterator;

  using reference = std::vector<int>::reference;
  using const_reference = std::vector<int>::const_reference;

public:
  enum Type { UNDEFINED = 0, FLOOR, ZONE, ROI, HOLE };

public:
  Polygon();
  Polygon(const std::vector<int>& vert, const Type type);
  Polygon(const YAML::Node& data, const Type type);
  Polygon(const Polygon& other);
  Polygon(Polygon&& other);
  Polygon& operator=(const Polygon& other);
  Polygon& operator=(Polygon&& other);
  ~Polygon() = default;

  void createRequiredParameters();

  template<typename T>
  void createParamIfNeeded(const std::string& name, const Param::Type& param_type, const T& param_value);

  std::string typeToStr() const;

public:
  // getter's
  bool isSelected() const { return selected; }
  bool getType() const { return type; }
  bool hasVertex(const int vertex_idx) const;

  const std::map<std::string, Param>& getParams() const { return params; }
  
  // setter's
  void setSelected(const bool fl) { selected = fl }
  void setParam(const std::string& name, const std::string& value);

public:
  operator YAML::Node() const;

public:
  // container behaviour method's
  bool empty() const { return vertices.empty(); }
  size_t size() const { return vertices.size(); }

  reference operator[](size_t idx) { return vertices[idx]; };
  const_reference operator[](size_t idx) const { return vertices[idx]; };

  iterator insert(const_iterator pos, const int& value);
  iterator insert(const_iterator pos, int&& value);
  iterator insert(const_iterator pos, size_type count, const int& value);
  iterator insert(const_iterator pos, std::initializer_list<int> ilist);

  iterator erase(iterator pos);
  iterator erase(const_iterator pos);
  iterator erase(iterator first, iterator last);
  iterator erase(const_iterator first, const_iterator last);

public:
  // range-based for support
  iterator begin() noexcept { return vertices.begin(); }
  const_iterator begin() const noexcept { return vertices.begin(); }

  iterator end() noexcept { return vertices.end(); }
  const_iterator end() const noexcept { return vertices.end(); }

  const_iterator cbegin() const noexcept { return vertices.cbegin(); }
  const_iterator cend() const noexcept { return vertices.cend(); }

private:
  bool selected;
  
  Type type;
  
  std::vector<int> vertices;
  std::map<std::string, Param> params;
};

#endif
