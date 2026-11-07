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

#include "polygon.h"
using std::string;
using std::vector;


Polygon::Polygon()
: selected(false),
  type(Type::UNDEFINED)
{
  createRequiredParameters();
}

Polygon::Polygon(const std::vector<int>& vert, const Type type)
: selected(false),
  type(type),
  vertices(vert)
{ }

Polygon::Polygon(const YAML::Node& data, const Type type)
: selected(false)
{
  if (!data.IsMap())
    throw std::runtime_error("Polygon::from_yaml() expected a map");
  
  type = polygon_type;

  for (YAML::const_iterator it = data["vertices"].begin(); it != data["vertices"].end(); ++it)
    vertices.push_back(it->as<int>());

  // load the parameters
  if (data["parameters"])
  {
    for (YAML::const_iterator it = data["parameters"].begin(); it != data["parameters"].end(); ++it)
    {
      Param p;
      p.fromYaml(it->second);
      params[it->first.as<string>()] = p;
    }
  }

  createRequiredParameters();
}

Polygon::Polygon(const Polygon& other)
: selected(other.selected),
  type(other.type),
  vertices(other.vertices),
  params(other.params)
{ }

Polygon::Polygon(Polygon&& other)
: selected(other.selected),
  type(other.type),
  vertices(std::move(other.vertices)),
  params(std::move(other.params))
{ }

Polygon& Polygon::operator=(const Polygon& other)
{
  if (this == &other)
    return *this;
  
  selected = other.selected;
  type = other.type;
  vertices = other.vertices;
  params = other.params;
}

Polygon& Polygon::operator=(Polygon&& other)
{
  if (this == &other)
    return *this;

  selected = other.selected;
  type = other.type;
  vertices = std::move(other.vertices);
  params = std::move(other.params);
}

void Polygon::setParam(const std::string& name, const std::string& value)
{
  auto it = params.find(name);
  if (it == params.end())
  {
    printf("tried to set unknown parameter [%s]\n", name.c_str());
    return;  // unknown parameter
  }
  it->second.set(value);
}

void Polygon::createRequiredParameters()
{
  // create required parameters if they don't exist yet on this edge
  if (type == FLOOR)
  {
    createParamIfNeeded("texture_name", Param::STRING, std::string("blue_linoleum"));
    createParamIfNeeded("texture_scale", Param::DOUBLE, 1.0);
    createParamIfNeeded("texture_rotation", Param::DOUBLE, 0.0);
    createParamIfNeeded("indoor", Param::INT, 0);
    createParamIfNeeded("ceiling_texture", Param::STRING, std::string("blue_linoleum"));
    createParamIfNeeded("ceiling_scale", Param::DOUBLE, 1.0);
  }
}

template<typename T>
void Polygon::createParamIfNeeded(const std::string& name, const Param::Type& param_type, const T& param_value)
{
  auto it = params.find(name);
  if (it == params.end() || it->second.type != param_type)
    params[name] = param_value;
}

std::string Polygon::typeToStr() const
{
  if (type == FLOOR)
    return string("floor");
  else if (type == ZONE)
    return string("zone");
  else if (type == ROI)
    return string("roi");
  else if (type == HOLE)
    return string("hole");
  return string("undefined");
}

bool Polygon::hasVertex(const int vertex_idx) const
{
  return std::find(vertices.begin(), vertices.end(), vertex_idx) != vertices.end();
}

Polygon::operator YAML::Node() const;
{
  YAML::Node y;

  for (const auto& vertex_idx : vertices)
    y["vertices"].push_back(vertex_idx);

  y["vertices"].SetStyle(YAML::EmitterStyle::Flow);
  y["parameters"] = YAML::Node(YAML::NodeType::Map);

  for (const auto& param : params)
    y["parameters"][param.first] = param.second.toYaml();
  
  y["parameters"].SetStyle(YAML::EmitterStyle::Flow);

  return y;
}

Polygon::iterator Polygon::insert(const_iterator pos, const int& value)
{
  return vertices.insert(pos, value);
}

Polygon::iterator Polygon::insert(const_iterator pos, int&& value)
{
  return vertices.insert(pos, std::move(value));
}

Polygon::iterator Polygon::insert(const_iterator pos, size_type count, const int& value)
{
  return vertices.insert(pos, count, value);
}

Polygon::iterator Polygon::insert(const_iterator pos, std::initializer_list<int> ilist)
{
  return vertices.insert(pos, ilist);
}

Polygon::iterator Polygon::erase(iterator pos)
{
  return vertices.erase(pos);
}

Polygon::iterator Polygon::erase(const_iterator pos)
{
  return vertices.erase(pos);
}

Polygon::iterator Polygon::erase(iterator first, iterator last)
{
  return vertices.erase(first, last);
}

Polygon::iterator Polygon::erase(const_iterator first, const_iterator last)
{
  return vertices.erase(first, last);
}