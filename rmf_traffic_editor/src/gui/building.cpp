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

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <yaml-cpp/yaml.h>

#include <QFileInfo>
#include <QDateTime>
#include <QLocale>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QThread>
#include <QtConcurrent/QtConcurrent>
#include <QElapsedTimer>

#include <rmf_utils/math.hpp>

#include "building.h"
#include "door.h"
#include <cmath>
#include "yaml_utils.h"

using std::string;
using std::vector;
using std::make_pair;
using std::unique_ptr;
using std::shared_ptr;


Building::Building()
: name("building"),
  coordinate_system(CoordinateSystem::Type::ReferenceImage)
{ }

Building::~Building() { }

/// Load a YAML file description of a building map
///
/// This function replaces the contents of this object with what is
/// in the YAML file.
bool Building::load(const string& _filename)
{
  printf("Building::load(%s)\n", _filename.c_str());
  const QString suffix = QFileInfo(QString::fromStdString(_filename)).suffix().toLower();
  if (suffix != "yaml" && suffix != "yml")
    return false;
  filename = _filename;

  if (filename.find(".project.yaml") != string::npos)
  {
    printf(
      "\nIt looks like this is a previous traffic-editor project file. "
      "This file is no longer used. Please load the .building.yaml "
      "file instead.\n\n");
    return false;
  }

  YAML::Node y;
  try {
    y = YAML::LoadFile(filename.c_str());
  }
  catch (const std::exception& e) {
    printf("couldn't parse %s: %s\n", filename.c_str(), e.what());
    return false;
  }

  // change directory to the path of the file, so that we can correctly open
  // relative paths recorded in the file
  QString dir(QFileInfo(QString::fromStdString(filename)).absolutePath());
  qDebug("changing directory to [%s]", qUtf8Printable(dir));
  
  if (!QDir::setCurrent(dir))
  {
    printf("couldn't change directory\n");
    return false;
  }

  if (y["name"])
    name = y["name"].as<string>();

  if (y["coordinate_system"])
    coordinate_system = CoordinateSystem(CoordinateSystem::typeFromString(y["coordinate_system"].as<string>()));

  if (y["reference_level_name"])
    reference_level_name = y["reference_level_name"].as<string>();
  else
    reference_level_name = "";

  // crowd_sim_impl is initialized when creating crowd_sim_table in editor.cpp
  // just in case the pointer is not initialized
  if (crowd_sim_impl == nullptr)
    crowd_sim_impl = std::make_shared<crowd_sim::CrowdSimImplementation>();
  if (y["crowd_sim"] && y["crowd_sim"].IsMap())
  {
    if (!crowd_sim_impl->fromYaml(y["crowd_sim"]))
    {
      printf(
        "Error in loading crowd_sim configuration from yaml, re-initialize crowd_sim");
      crowd_sim_impl->clear();
      crowd_sim_impl->initDefaultConfigure();
    }
  }

  if (!y["levels"] || !y["levels"].IsMap())
  {
    printf("expected top-level dictionary named 'levels'");
    return false;
  }

  levels.clear();
  const YAML::Node yl = y["levels"];
  for (YAML::const_iterator it = yl.begin(); it != yl.end(); ++it)
  {
    Level level;
    level.fromYaml(it->first.as<string>(), it->second, coordinate_system);
    levels.push_back(level);
  }

  QtConcurrent::blockingMap(levels, [&](auto& level) { level.loadDrawing(); });

  // now that all images are loaded, we can calculate scale for annotated
  // measurement lanes
  for (auto& level : levels)
    level.calculateScale(coordinate_system);

  lifts.clear();
  if (y["lifts"] && y["lifts"].IsMap())
  {
    const YAML::Node& y_lifts = y["lifts"];
    for (YAML::const_iterator it = y_lifts.begin(); it != y_lifts.end(); ++it)
    {
      Lift lift;
      lift.fromYaml(it->first.as<string>(), it->second, levels);
      lifts.push_back(lift);
    }
  }

  if (y["graphs"] && y["graphs"].IsMap())
  {
    const YAML::Node& g_map = y["graphs"];
    for (YAML::const_iterator it = g_map.begin(); it != g_map.end(); ++it)
    {
      Graph graph;
      graph.fromYaml(it->first.as<int>(), it->second);
      graphs.push_back(graph);
    }
  }

  if (y["parameters"] && y["parameters"].IsMap())
  {
    const YAML::Node& gp = y["parameters"];
    for (YAML::const_iterator it = gp.begin(); it != gp.end(); ++it)
    {
      Param p;
      p.fromYaml(it->second);
      params[it->first.as<string>()] = p;
    }
  }

  calculateAllTransforms();
  return true;
}

bool Building::save()
{
  if (filename.empty())
    return false;
  YAML::Node y;
  y["name"] = name;
  y["name"].SetTag("!");

  y["coordinate_system"] = static_cast<std::string>(coordinate_system);

  if (!reference_level_name.empty())
  {
    y["reference_level_name"] = reference_level_name;
    y["reference_level_name"].SetTag("!");
  }

  y["levels"] = YAML::Node(YAML::NodeType::Map);
  for (const auto& level : levels)
    y["levels"][level.name] = level.toYaml(coordinate_system);

  y["lifts"] = YAML::Node(YAML::NodeType::Map);
  for (const auto& lift : lifts)
    y["lifts"][lift.name] = lift.toYaml();
  if (lifts.empty())
    y["lifts"].SetStyle(YAML::EmitterStyle::Flow);

  if (crowd_sim_impl)
    y["crowd_sim"] = crowd_sim_impl->toYaml();

  y["graphs"] = YAML::Node(YAML::NodeType::Map);
  for (const auto& graph : graphs)
    y["graphs"][graph.idx] = graph.toYaml();

  if (!params.empty())
  {
    YAML::Node params_node(YAML::NodeType::Map);
    for (const auto& param : params)
      params_node[param.first] = param.second.toYaml();
    y["parameters"] = params_node;
  }

  YAML::Emitter emitter;
  yaml_utils::writeNode(y, emitter);
  const QByteArray data(emitter.c_str());
  QSaveFile output(QString::fromStdString(filename));
  return output.open(QIODevice::WriteOnly) &&
    output.write(data) == data.size() && output.commit();
}

bool Building::exportNavigation(const std::string& path) const
{
  const QString output_path = QString::fromStdString(path);
  try
  {
    using Json = nlohmann::json;
    
    Json json = {
      {"type", "FeatureCollection"},
      {"name", "graph"},
      {"crs", {
        {"type", "name"},
        {"properties", {{"name", "urn:ogc:def:crs:EPSG::3857"}}}
      }},
      {"date_generated", QLocale::c().toString(
        QDateTime::currentDateTime(), "ddd MMM dd hh:mm:ss AP t yyyy").toStdString()},
      {"features", Json::array()}
    };
    
    for (const auto& level : levels)
      for (const auto& feature : level.toJson(coordinate_system))
        json["features"].push_back(feature);

    for (const auto& lift : lifts)
      for (const auto& feature : lift.toJson(levels))
        json["features"].push_back(feature);
    
    const QByteArray data = QByteArray::fromStdString(json.dump(2));
    QSaveFile output(output_path);
    if (!output.open(QIODevice::WriteOnly) || output.write(data) != data.size() || !output.commit())
    {
      qWarning("Unable to save %s: %s", qUtf8Printable(output_path), qUtf8Printable(output.errorString()));
      return false;
    }

    return true;
  }
  catch (const std::exception& error)
  {
    qWarning("Unable to serialize JSON: %s", error.what());
    return false;
  }
}

bool Building::exportFeatures(
  int level_index,
  const std::string& dest_filename) const
{
  return levels[level_index].exportFeatures(dest_filename);
}

void Building::addVertex(int level_index, double x, double y)
{
  if (level_index >= static_cast<int>(levels.size()))
    return;
  levels[level_index].addVertex(x, y);
}

QUuid Building::addFiducial(int level_index, double x, double y)
{
  if (level_index >= static_cast<int>(levels.size()))
    return NULL;
  levels[level_index].fiducials.push_back(Fiducial(x, y));
  return levels[level_index].fiducials.rbegin()->uuid;
}

QUuid Building::addFeature(
  int level_idx,
  int layer_idx,
  double x,
  double y)
{
  if (level_idx >= static_cast<int>(levels.size()))
    return NULL;
  return levels[level_idx].addFeature(layer_idx, x, y);
}

void Building::removeFeature(
  const int level_idx,
  const int layer_idx,
  QUuid feature_uuid)
{
  if (level_idx >= static_cast<int>(levels.size()))
    return;

  levels[level_idx].removeFeature(layer_idx, feature_uuid);
}

int Building::findNearestVertexIndex(
  int level_index,
  double x,
  double y,
  double& distance)
{
  double min_dist = 1e100;
  int min_index = -1;
  for (std::size_t i = 0; i < levels[level_index].vertices.size(); i++)
  {
    const Vertex& v = levels[level_index].vertices[i];
    const double dx = x - v.x;
    const double dy = y - v.y;
    const double dist2 = dx*dx + dy*dy;  // no need for sqrt each time
    if (dist2 < min_dist)
    {
      min_dist = dist2;
      min_index = i;
    }
  }
  distance = sqrt(min_dist);
  return min_index;  // will be -1 if vertices vector is empty
}

void Building::addEdge(
  const int level_index,
  const int start_vertex_index,
  const int end_vertex_index,
  const Edge::Type edge_type)
{
  if (level_index >= static_cast<int>(levels.size()))
    return;

  if (edge_type == Edge::DOOR)
  {
    addDoor(level_index, start_vertex_index, end_vertex_index);
    return;
  }

  printf("Building::add_edge(%d, %d, %d, %d)\n",
    level_index,
    start_vertex_index,
    end_vertex_index,
    static_cast<int>(edge_type));

  levels[level_index].edges.push_back(
    Edge(start_vertex_index, end_vertex_index, edge_type));
}

void Building::addDoor(const int level_idx, const int start_idx, const int end_idx)
{
  if (level_idx < 0 || level_idx >= static_cast<int>(levels.size()))
    return;

  auto& level = levels[level_idx];

  if (start_idx < 0 || end_idx < 0 ||
      start_idx >= static_cast<int>(level.vertices.size()) ||
      end_idx >= static_cast<int>(level.vertices.size()))
    return;

  const double meters_per_pixel = level.drawing_meters_per_pixel;

  if (!std::isfinite(meters_per_pixel) || meters_per_pixel <= 0)
    return;

  const auto& start = level.vertices[start_idx];
  const auto& end = level.vertices[end_idx];
  const Eigen::Vector2d direction(end.x - start.x, end.y - start.y);
  if (direction.norm() < 1e-9)
    return;

  const Eigen::Vector2d center((start.x + end.x) / 2, (start.y + end.y) / 2);
  const Eigen::Vector2d normal(-direction.y() / direction.norm(), direction.x() / direction.norm());

  const std::string id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
  const std::string key = "door/" + id;

  Door door(Edge(start_idx, end_idx, Edge::DOOR));
  door.params["editor_door_id"] = Param(id);

  const std::string name = door.params.at("name").value_string;

  const Eigen::Vector2d front_position = center + 0.5 / meters_per_pixel * normal;
  Vertex front_vertex(front_position.x(), front_position.y(), name + "_front");
  
  front_vertex.params["editor_generated_vertex"] = Param(key + "/front");
  front_vertex.params["door_name"] = Param(name);
  front_vertex.params["door_side"] = Param(std::string("front"));
  
  level.addVertex(front_vertex);
  
  const int front = static_cast<int>(level.vertices.size()) - 1;

  const Eigen::Vector2d back_position = center - 0.5 / meters_per_pixel * normal;
  
  Vertex back_vertex(back_position.x(), back_position.y(), name + "_back");
  
  back_vertex.params["editor_generated_vertex"] = Param(key + "/back");
  back_vertex.params["door_name"] = Param(name);
  back_vertex.params["door_side"] = Param(std::string("back"));
  
  level.addVertex(back_vertex);
  
  const int back = static_cast<int>(level.vertices.size()) - 1;

  door.setApproachVertices(front, back);
  door.setApproachPositions(front_position, back_position);
  level.edges.push_back(door);

  Edge lane(front, back, Edge::LANE);
  lane.params["editor_generated_edge"] = Param(key);
  lane.params["door_name"] = Param(name);
  level.edges.push_back(lane);
}

void Building::moveVertex(const int level_idx, const int vertex_idx, double x, double y)
{
  if (level_idx < 0 || level_idx >= static_cast<int>(levels.size()))
    return;
  auto& level = levels[level_idx];
  if (vertex_idx < 0 || vertex_idx >= static_cast<int>(level.vertices.size()))
    return;

  level.vertices[vertex_idx].x = x;
  level.vertices[vertex_idx].y = y;

  for (auto& edge : level.edges)
  {
    if (edge.type != Edge::DOOR)
      continue;
    Door door(edge);
    const int front = door.frontVertex();
    const int back = door.backVertex();
    if (!edge.contains(vertex_idx) && front != vertex_idx && back != vertex_idx)
      continue;
    if (front < 0 || back < 0 || front >= static_cast<int>(level.vertices.size()) ||
      back >= static_cast<int>(level.vertices.size()))
      continue;

    if (edge.contains(vertex_idx))
    {
      if (edge.start_idx < 0 || edge.end_idx < 0 ||
        edge.start_idx >= static_cast<int>(level.vertices.size()) ||
        edge.end_idx >= static_cast<int>(level.vertices.size()))
        continue;
      const double meters_per_pixel = level.drawing_meters_per_pixel;
      if (!std::isfinite(meters_per_pixel) || meters_per_pixel <= 0)
        continue;
      const auto& start = level.vertices[edge.start_idx];
      const auto& end = level.vertices[edge.end_idx];
      const Eigen::Vector2d direction(end.x - start.x, end.y - start.y);
      if (direction.norm() < 1e-9)
        continue;
      const Eigen::Vector2d center((start.x + end.x) / 2, (start.y + end.y) / 2);
      const Eigen::Vector2d normal(-direction.y() / direction.norm(), direction.x() / direction.norm());

      const Eigen::Vector2d front_position = center + 0.5 / meters_per_pixel * normal;
      level.vertices[front].x = front_position.x();
      level.vertices[front].y = front_position.y();

      const Eigen::Vector2d back_position = center - 0.5 / meters_per_pixel * normal;
      level.vertices[back].x = back_position.x();
      level.vertices[back].y = back_position.y();
    }

    const auto& front_vertex = level.vertices[front];
    const auto& back_vertex = level.vertices[back];
    door.setApproachPositions(Eigen::Vector2d(front_vertex.x, front_vertex.y),
      Eigen::Vector2d(back_vertex.x, back_vertex.y));
    edge = door;
  }
}

void Building::addLane(
  const int level_index,
  const int start_vertex_index,
  const int end_vertex_index,
  const int graph_idx)
{
  if (level_index >= static_cast<int>(levels.size()))
    return;

  printf("Building::add_lane(%d, %d, %d, graph=%d)\n",
    level_index,
    start_vertex_index,
    end_vertex_index,
    graph_idx);
  Edge e(start_vertex_index, end_vertex_index, Edge::LANE);
  e.setGraphIdx(graph_idx);
  levels[level_index].edges.push_back(e);
}

bool Building::deleteSelected(const int level_index, bool delete_used_entities)
{
  if (level_index < 0 || level_index >= static_cast<int>(levels.size()))
    return false;

  auto& level = levels[level_index];
  const Level before = level;
  if (delete_used_entities)
  {
    const auto selected = std::find_if(level.vertices.begin(), level.vertices.end(),
      [](const Vertex& vertex) { return vertex.selected; });
    if (selected != level.vertices.end())
    {
      const auto cabin = Vertex::getStringParam(selected->params, "lift_cabin");
      if (cabin)
      {
        purgeLiftCabinVertices(*cabin);
        return true;
      }
      level.deleteUsedEntities(std::distance(level.vertices.begin(), selected));
    }
  }
  if (!level.deleteSelected())
  {
    level = before;
    return false;
  }

  // Level has already remapped edge endpoints and polygons. Update the
  // navigation references using vertex identity before compacting further.
  std::map<QUuid, int> vertex_indices;
  for (std::size_t i = 0; i < level.vertices.size(); ++i)
    vertex_indices.emplace(level.vertices[i].uuid, static_cast<int>(i));
  std::vector<int> remap(before.vertices.size(), -1);
  for (std::size_t i = 0; i < before.vertices.size(); ++i)
  {
    const auto it = vertex_indices.find(before.vertices[i].uuid);
    if (it != vertex_indices.end())
      remap[i] = it->second;
  }
  // Update door approach indices and coordinate snapshots.
  for (auto& edge : level.edges)
  {
    if (edge.type != Edge::DOOR)
      continue;

    Door door(edge);
    const int old_front = door.frontVertex();
    const int old_back = door.backVertex();
    const int front = old_front >= 0 && old_front < static_cast<int>(remap.size())
      ? remap[old_front] : -1;
    const int back = old_back >= 0 && old_back < static_cast<int>(remap.size())
      ? remap[old_back] : -1;

    door.setApproachVertices(front, back);
    if (front >= 0 && back >= 0)
    {
      const auto& front_vertex = level.vertices[front];
      const auto& back_vertex = level.vertices[back];
      door.setApproachPositions(
        Eigen::Vector2d(front_vertex.x, front_vertex.y),
        Eigen::Vector2d(back_vertex.x, back_vertex.y));
    }
    edge = door;
  }

  // Update lift cabin and door references on this level.
  for (auto& lift : lifts)
  {
    const auto navigation_it = lift.navigation_vertices.find(level.name);
    if (navigation_it == lift.navigation_vertices.end())
      continue;

    auto& navigation = navigation_it->second;
    const int old_cabin = navigation.cabin;
    navigation.cabin = old_cabin >= 0 && old_cabin < static_cast<int>(remap.size())
      ? remap[old_cabin] : -1;
    navigation.cabin_position.reset();
    navigation.door_positions.clear();
    if (navigation.cabin < 0)
    {
      lift.navigation_vertices.erase(navigation_it);
      continue;
    }

    const auto& cabin_vertex = level.vertices[navigation.cabin];
    navigation.cabin_position = Eigen::Vector2d(cabin_vertex.x, cabin_vertex.y);
    for (auto door_it = navigation.doors.begin(); door_it != navigation.doors.end();)
    {
      const int old_door = door_it->second;
      door_it->second = old_door >= 0 && old_door < static_cast<int>(remap.size())
        ? remap[old_door] : -1;
      if (door_it->second < 0)
        door_it = navigation.doors.erase(door_it);
      else
      {
        const auto& door_vertex = level.vertices[door_it->second];
        navigation.door_positions[door_it->first] = Eigen::Vector2d(door_vertex.x, door_vertex.y);
        ++door_it;
      }
    }
    if (navigation.doors.empty())
      lift.navigation_vertices.erase(navigation_it);
  }

  std::set<std::string> wanted_vertices;
  std::set<std::string> wanted_edges;
  for (const auto& edge : level.edges)
  {
    if (edge.type != Edge::DOOR)
      continue;
    const auto id = Vertex::getStringParam(edge.params, "editor_door_id");
    if (!id)
      continue;
    const std::string key = "door/" + *id;
    wanted_vertices.insert(key + "/front");
    wanted_vertices.insert(key + "/back");
    wanted_edges.insert(key);
  }
  updateGraphOnLevel(level, "door/", wanted_vertices, wanted_edges);
  return true;
}

QUuid Building::addModel(
  const int level_idx,
  const double x,
  const double y,
  const double z,
  const double yaw,
  const std::string& model_name)
{
  if (level_idx >= static_cast<int>(levels.size()))
    return NULL;

  printf("Building::add_model(%d, %.1f, %.1f, %.1f, %.2f, %s)\n",
    level_idx, x, y, z, yaw, model_name.c_str());
  Model m;
  m.state.x = x;
  m.state.y = y;
  m.state.z = z;
  m.state.yaw = yaw;
  m.model_name = model_name;
  m.instance_name = model_name;  // todo: add unique numeric suffix?
  m.is_static = true;
  m.is_dispensable = false;
  levels[level_idx].models.push_back(m);
  return levels[level_idx].models.rbegin()->uuid;
}

void Building::setModelYaw(
  const int level_idx,
  const int model_idx,
  const double yaw)
{
  if (level_idx >= static_cast<int>(levels.size()))
    return;

  levels[level_idx].models[model_idx].state.yaw = yaw;
}

void Building::clear()
{
  name.clear();
  filename.clear();
  reference_level_name.clear();
  levels.clear();
  lifts.clear();
  clearTransformCache();
}

void Building::addLevel(const Level& new_level)
{
  // make sure we don't have this level already
  for (const auto& level : levels)
  {
    if (level.name == new_level.name)
      return;
  }
  levels.push_back(new_level);
}

void Building::drawLifts(QGraphicsScene* scene, const int level_idx)
{
  const Level& level = levels[level_idx];
  for (const auto& lift : lifts)
  {
    // find the level index referenced by the lift
    int reference_floor_idx = -1;
    for (std::size_t i = 0; i < levels.size(); i++)
    {
      if (levels[i].name == lift.reference_floor_name)
      {
        reference_floor_idx = static_cast<int>(i);
        break;
      }
    }

    Transform t;
    if (reference_floor_idx >= 0)
      t = getTransform(reference_floor_idx, level_idx);

    lift.draw(
      scene,
      level.drawing_meters_per_pixel,
      level.name,
      level.elevation,
      true,
      t.scale,
      t.dx,
      t.dy,
      t.rotation);
  }
}

bool Building::transformBetweenLevels(
  const std::string& from_level_name,
  const QPointF& from_point,
  const std::string& to_level_name,
  QPointF& to_point)
{
  int from_level_idx = -1;
  int to_level_idx = -1;
  for (std::size_t i = 0; i < levels.size(); i++)
  {
    if (levels[i].name == from_level_name)
      from_level_idx = i;
    if (levels[i].name == to_level_name)
      to_level_idx = i;
  }
  if (from_level_idx < 0 || to_level_idx < 0)
  {
    to_point = from_point;
    return false;
  }
  return transformBetweenLevels(
    from_level_idx,
    from_point,
    to_level_idx,
    to_point);
}

bool Building::transformBetweenLevels(
  const int from_level_idx,
  const QPointF& from_point,
  const int to_level_idx,
  QPointF& to_point)
{

  if (from_level_idx < 0 ||
    from_level_idx >= static_cast<int>(levels.size()) ||
    to_level_idx < 0 ||
    to_level_idx >= static_cast<int>(levels.size()))
  {
    to_point = from_point;
    return false;
  }

  const Transform t = getTransform(from_level_idx, to_level_idx);

  to_point.rx() = t.scale * from_point.x() + t.dx;
  to_point.ry() = t.scale * from_point.y() + t.dy;
  return true;
}

void Building::clearTransformCache()
{
  transforms.clear();
}

Building::Transform Building::computeTransform(
  const int from_level_idx,
  const int to_level_idx)
{
  // short-circuit if it's the same level
  if (from_level_idx == to_level_idx)
  {
    Building::Transform t;
    t.scale = 1.0;
    t.dx = 0.0;
    t.dy = 0.0;
    t.rotation = 0.0;
    return t;
  }

  // this internal function assumes that bounds checking has already happened
  const Level& from_level = levels[from_level_idx];
  const Level& to_level = levels[to_level_idx];

  // assemble a vector of fudicials in common to these levels
  vector<std::pair<Fiducial, Fiducial>> fiducials;
  for (const Fiducial& f0 : from_level.fiducials)
  {
    for (const Fiducial& f1 : to_level.fiducials)
    {
      if (f0.name == f1.name)
      {
        fiducials.push_back(make_pair(f0, f1));
        break;
      }
    }
  }

  // calculate the rotation between each pair of fiducials
  vector<std::pair<double, double>> rotations;

  if (fiducials.size() < 2)
  {
    printf(
      "not enough fiducials to compute transform between levels %d and %d\n",
      from_level_idx,
      to_level_idx);
    return Building::Transform();
  }

  // we take the first fiducial as the reference point
  std::pair<Fiducial, Fiducial> ref_rotation = make_pair(fiducials[0].first,
      fiducials[0].second);
  for (std::size_t f_idx = 1; f_idx < fiducials.size(); f_idx++)
  {
    rotations.push_back(
      make_pair(
        fiducials[f_idx].first.rotation(ref_rotation.first),
        fiducials[f_idx].second.rotation(ref_rotation.second)));
  }

  // it is a crude method for now, but we will just compute the mean of the angles of all the fiducial pairs
  double relative_rotation_sum = 0;
  for (std::size_t i = 0; i < rotations.size(); i++)
  {
    // calculate shortest angle diff between two angles
    double diff = rotations[i].first - rotations[i].second;
    relative_rotation_sum += rmf_utils::wrap_to_pi(diff);
  }
  const double rotation = relative_rotation_sum / rotations.size();

  // calculate the distances between each fiducial on their levels
  vector<std::pair<double, double>> distances;
  for (std::size_t f0_idx = 0; f0_idx < fiducials.size(); f0_idx++)
  {
    for (std::size_t f1_idx = f0_idx + 1; f1_idx < fiducials.size(); f1_idx++)
    {
      distances.push_back(
        make_pair(
          fiducials[f0_idx].first.distance(fiducials[f1_idx].first),
          fiducials[f0_idx].second.distance(fiducials[f1_idx].second)));
    }
  }

  if (distances.empty())
    return Building::Transform();

  // for now, we'll just compute the mean of the relative scale estimates.
  // we can do fancier statistics later, if needed.
  double relative_scale_sum = 0;
  for (std::size_t i = 0; i < distances.size(); i++)
    relative_scale_sum += distances[i].second / distances[i].first;
  const double scale = relative_scale_sum / distances.size();

  // scale the fiducials and estimate the "optimal" translation.
  // for now, we'll just use the mean of the translation estimates.
  double trans_x_sum = 0;
  double trans_y_sum = 0;
  for (const auto& fiducial : fiducials)
  {
    trans_x_sum += fiducial.second.x -
      (std::cos(rotation) * fiducial.first.x + std::sin(rotation) *
      fiducial.first.y) * scale;
    trans_y_sum += fiducial.second.y -
      (-std::sin(rotation) * fiducial.first.x + std::cos(rotation) *
      fiducial.first.y) * scale;
  }
  const double trans_x = trans_x_sum / fiducials.size();
  const double trans_y = trans_y_sum / fiducials.size();

  Building::Transform t;
  t.scale = scale;
  t.dx = trans_x;
  t.dy = trans_y;
  t.rotation = rotation;

  printf(
    "transform %d->%d: scale = %.5f translation = (%.2f, %.2f) rotation = %.5f\n",
    from_level_idx,
    to_level_idx,
    t.scale,
    t.dx,
    t.dy,
    t.rotation);

  return t;
}

Building::Transform Building::getTransform(
  const int from_level_idx,
  const int to_level_idx)
{
  // this operation is a bit "heavy" so we'll cache the transformations
  // as they are computed
  LevelPair level_pair;
  level_pair.from_idx = from_level_idx;
  level_pair.to_idx = to_level_idx;

  TransformMap::iterator transform_it = transforms.find(level_pair);
  Transform t;

  if (transform_it == transforms.end())
  {
    // the transform wasn't in the cache, so we need to compute it
    t = computeTransform(from_level_idx, to_level_idx);
    transforms[level_pair] = t;
  }
  else
    t = transform_it->second;

  return t;
}

void Building::calculateAllTransforms()
{
  if (levels.empty())
    return;// let's not crash

  clearTransformCache();
  for (std::size_t i = 0; i < levels.size(); i++)
  {
    for (std::size_t j = 0; j < levels.size(); j++)
    {
      getTransform(i, j);
    }
  }

  // set drawing scale using this data
  const int ref_idx = getReferenceLevelIdx();
  const double ref_scale = levels[ref_idx].drawing_meters_per_pixel;
  for (int i = 0; i < static_cast<int>(levels.size()); i++)
  {
    if (i != getReferenceLevelIdx())
    {
      Transform t = getTransform(ref_idx, i);
      if (levels[i].fiducials.size() >= 2)
        levels[i].drawing_meters_per_pixel = ref_scale / t.scale;
    }
  }
}

int Building::getReferenceLevelIdx()
{
  if (reference_level_name.empty())
    return 0;
  for (std::size_t i = 0; i < levels.size(); i++)
  {
    if (levels[i].name == reference_level_name)
      return static_cast<int>(i);
  }
  return 0;
}

void Building::clearScene()
{
  for (auto& level : levels)
    level.clearScene();
}

double Building::levelMetersPerPixel(const string& level_name) const
{
  for (const auto& level : levels)
  {
    if (level.name == level_name)
      return level.drawing_meters_per_pixel;
  }
  return 0.05;  // just a somewhat sane default
}

void Building::rotateAllModels(const double rotation)
{
  for (auto& level : levels)
  {
    for (auto& model : level.models)
    {
      model.state.yaw += rotation;
    }
  }
}

bool Building::setFilename(const std::string& _fn)
{
  const QFileInfo info(QString::fromStdString(_fn));
  if ((info.suffix().compare("yaml", Qt::CaseInsensitive) != 0 &&
    info.suffix().compare("yml", Qt::CaseInsensitive) != 0) ||
    info.completeBaseName().isEmpty())
    return false;
  QString base = info.completeBaseName();
  if (base.endsWith(".building"))
    base.chop(9);
  const string stem = base.toStdString();

  filename = _fn;

  if (name.empty())
    name = stem;

  printf(
    "set building filename to [%s]\n",
    filename.c_str());
  return true;
}

void Building::clearSelection(const int level_idx)
{
  if (levels.empty())
    return;
  levels[level_idx].clearSelection();
}

bool Building::canDeleteCurrentSelection(const int level_idx)
{
  if (level_idx >= static_cast<int>(levels.size()))
    return false;
  return levels[level_idx].canDeleteCurrentSelection();
}

void Building::getSelectedItems(
  const int level_idx,
  std::vector<Level::SelectedItem>& selected)
{
  if (level_idx >= static_cast<int>(levels.size()))
    return;
  levels[level_idx].getSelectedItems(selected);
}

void Building::draw(
  QGraphicsScene* scene,
  const int level_idx,
  std::vector<EditorModel>& editor_models,
  const RenderingOptions& rendering_options)
{
  if (levels.empty())
  {
    printf("nothing to draw!\n");
    return;
  }

  levels[level_idx].draw(
    scene,
    editor_models,
    rendering_options,
    graphs,
    coordinate_system);

  drawLifts(scene, level_idx);
}

std::optional<Polygon*> Building::getSelectedPolygon(const int level_idx)
{
  auto polygon_it = std::find_if(levels[level_idx].begin(), levels[level_idx].end(),
  [](auto&& polygon)
  {
    return polygon.isSelected();
  });

  if (polygon_it != levels[level_idx].end())
    return std::addressof(*polygon_it);

  return std::nullopt;
}

Polygon::EdgeDragPolygon Building::polygonEdgeDragPress(const int level_idx,
                                                        const Polygon* polygon,
                                                        const double x,
                                                        const double y)
{
  Polygon::EdgeDragPolygon edp;

  if (level_idx < 0 || level_idx >= static_cast<int>(levels.size()))
    return edp;

  return levels[level_idx].polygonEdgeDragPress(polygon, x, y);
}

Lift Building::getLift(const std::string& name) const
{
  for (const auto& lift : lifts)
  {
    if (lift.name == name)
      return lift;
  }
  return Lift();
}

void Building::purgeLiftCabinVertices(std::string lift_name)
{
  const std::string prefix = "lift/" + std::to_string(lift_name.size()) + ":" + lift_name + "/";
  for (auto& level : levels)
  {
    std::set<int> removed_vertices;
    for (std::size_t i = 0; i < level.vertices.size(); ++i)
    {
      const auto& vertex = level.vertices[i];
      const auto cabin = Vertex::getStringParam(vertex.params, "lift_cabin");
      const auto owner = Vertex::getStringParam(vertex.params, "lift_name");
      if ((cabin && *cabin == lift_name) || (owner && *owner == lift_name))
        removed_vertices.insert(static_cast<int>(i));
    }
    updateGraphOnLevel(level, prefix, {}, {}, removed_vertices);
  }
}

void Building::addConstraint(
  const int level_idx,
  const QUuid& a,
  const QUuid& b)
{
  if (level_idx < 0 || level_idx >= static_cast<int>(levels.size()))
    return;
  levels[level_idx].addConstraint(a, b);
}

void Building::removeConstraint(
  const int level_idx,
  const QUuid& a,
  const QUuid& b)
{
  if (level_idx < 0 || level_idx >= static_cast<int>(levels.size()))
    return;
  levels[level_idx].removeConstraint(a, b);
}

int Building::nearestItemIndexIfWithinDistance(
  const double level_idx,
  const double x,
  const double y,
  const double distance_threshold,
  const Level::ItemType item_type)
{
  if (level_idx >= levels.size())
    return -1;

  return levels[level_idx].nearestItemIndexIfWithinDistance(
    x,
    y,
    distance_threshold,
    item_type);
}

void Building::updateLiftGraph(Lift* lift, int reference_lvl)
{
  if (!lift || reference_lvl < 0 || reference_lvl >= static_cast<int>(levels.size()))
    return;

  lift->navigation_vertices.clear();

  const std::string lift_key = "lift/" + std::to_string(lift->name.size()) + ":" + lift->name;

  for (std::size_t level_idx = 0; level_idx < levels.size(); ++level_idx)
  {
    auto& level = levels[level_idx];

    std::set<std::string> wanted_vertices;
    std::set<std::string> wanted_edges;

    auto level_doors_it = lift->level_doors.find(level.name);

    // lift doesn't have doors at current level
    if (level_doors_it == lift->level_doors.end() || level_doors_it->second.empty())
      continue;

    // lift doesn't stop at current level
    if (lift->lowest_elevation < level.elevation && lift->highest_elevation > level.elevation)
      continue;

    const Transform transform = getTransform(reference_lvl, level_idx);
    Eigen::Isometry2d local_pose = Eigen::Isometry2d::Identity();
    local_pose.linear() = lift->pose.linear().transpose();
    local_pose.translation() = transform.scale * lift->pose.translation();
    Eigen::Isometry2d scene_pose = Eigen::Translation2d(transform.dx, transform.dy) * Eigen::Rotation2Dd(-transform.rotation) * local_pose;

    const auto cabin_it = std::find_if(level.vertices.begin(), level.vertices.end(),
    [&](const Vertex& vertex)
    {
      return vertex.liftCabin() == lift->name && !Vertex::getStringParam(vertex.params, "editor_generated_vertex");
    });

    int cabin_id;
    if (cabin_it != level.vertices.end())
      cabin_id = std::distance(level.vertices.begin(), cabin_it);
    else
    {
      const std::string key = lift_key + "/cabin";
      const auto existing = level.getEditorGeneratedVertexByKey(key);

      if (existing)
        cabin_id = *existing;
      else
      {
        Vertex vertex(0, 0, lift->name + "_cabin");
        vertex.params["editor_generated_vertex"] = Param(key);
        level.addVertex(vertex);
        cabin_id = static_cast<int>(level.vertices.size()) - 1;
      }
      level.vertices[cabin_id].x = scene_pose.translation().x();
      level.vertices[cabin_id].y = scene_pose.translation().y();
      level.vertices[cabin_id].params["lift_cabin"] = Param(lift->name);
      
      wanted_vertices.insert(key);
    }
      
    for (auto& door : lift->doors)
    {
      if (std::find(level_doors_it->second.begin(), level_doors_it->second.end(), door.name) == level_doors_it->second.end())
        continue;

      const auto key = door.updateNavGraph(level, scene_pose, lift_key, lift->name, cabin_id, level.drawing_meters_per_pixel);
      
      wanted_vertices.insert(key);
      wanted_edges.insert(key);
      
      lift->navigation_vertices[level.name].cabin = cabin_id;
      lift->navigation_vertices[level.name].doors[door.name] = door.door_id;
    }

    // Also compact unserved floors: their previous cabin/approach graph is obsolete.
    const auto remap = updateGraphOnLevel(level, lift_key + "/", wanted_vertices, wanted_edges);
    // Dialogs may update a lift before it has been inserted into Building::lifts.
    if (std::none_of(lifts.begin(), lifts.end(), [&](const Lift& item) { return &item == lift; }))
    {
      const auto navigation = lift->navigation_vertices.find(level.name);
      if (navigation != lift->navigation_vertices.end())
      {
        auto& refs = navigation->second;
        refs.cabin = remap[refs.cabin];
        refs.cabin_position = Eigen::Vector2d(level.vertices[refs.cabin].x, level.vertices[refs.cabin].y);
        for (auto& entry : refs.doors)
        {
          entry.second = remap[entry.second];
          refs.door_positions[entry.first] = Eigen::Vector2d(
            level.vertices[entry.second].x, level.vertices[entry.second].y);
        }
      }
    }

  }
}

std::vector<int> Building::updateGraphOnLevel(
  Level& level,
  const std::string& prefix,
  const std::set<std::string>& wanted_vertices,
  const std::set<std::string>& wanted_edges,
  const std::set<int>& removed_vertices)
{
  std::vector<int> remap(level.vertices.size(), -1);
  std::set<std::string> seen;
  std::vector<Vertex> retained_vertices;
  retained_vertices.reserve(level.vertices.size());
  for (std::size_t i = 0; i < level.vertices.size(); ++i)
  {
    const auto& vertex = level.vertices[i];
    const auto key = Vertex::getStringParam(vertex.params, "editor_generated_vertex");
    if (removed_vertices.count(static_cast<int>(i)) ||
      (key && key->compare(0, prefix.size(), prefix) == 0 &&
      (!wanted_vertices.count(*key) || !seen.insert(*key).second)))
      continue;
    remap[i] = static_cast<int>(retained_vertices.size());
    retained_vertices.push_back(vertex);
  }

  const auto new_index = [&](int index)
  {
    return index >= 0 && index < static_cast<int>(remap.size()) ? remap[index] : -1;
  };

  level.edges.erase(std::remove_if(level.edges.begin(), level.edges.end(),
  [&](Edge& edge)
  {
    const auto key = Vertex::getStringParam(edge.params, "editor_generated_edge");
    if (key && key->compare(0, prefix.size(), prefix) == 0 && !wanted_edges.count(*key))
      return true;
    const int start = new_index(edge.start_idx);
    const int end = new_index(edge.end_idx);
    if (start < 0 || end < 0)
      return true;
    edge.start_idx = start;
    edge.end_idx = end;
    return false;
  }),
  level.edges.end());

  // A polygon using a removed vertex can no longer represent its original shape.
  level.polygons.erase(std::remove_if(level.polygons.begin(), level.polygons.end(),
  [this](auto&& polygon)
  {
    return std::any_of(polygon.begin(), polygon.end(), [&](int index) { return new_index(index) < 0; })
  }),
  level.polygons.end());

  for(auto&& polygon : level.polygons)
  {
    std::for_each(polygon.begin(), polygon.end(),
    [](auto&& vertex_idx)
    {
      vertex_idx = new_index(vertex_idx)
    });
  }

  level.vertices = std::move(retained_vertices);
  // Update door approach indices and coordinate snapshots.
  for (auto& edge : level.edges)
  {
    if (edge.type != Edge::DOOR)
      continue;

    Door door(edge);
    const int old_front = door.frontVertex();
    const int old_back = door.backVertex();
    const int front = old_front >= 0 && old_front < static_cast<int>(remap.size())
      ? remap[old_front] : -1;
    const int back = old_back >= 0 && old_back < static_cast<int>(remap.size())
      ? remap[old_back] : -1;

    door.setApproachVertices(front, back);
    if (front >= 0 && back >= 0)
    {
      const auto& front_vertex = level.vertices[front];
      const auto& back_vertex = level.vertices[back];
      door.setApproachPositions(
        Eigen::Vector2d(front_vertex.x, front_vertex.y),
        Eigen::Vector2d(back_vertex.x, back_vertex.y));
    }
    edge = door;
  }

  // Update lift cabin and door references on this level.
  for (auto& lift : lifts)
  {
    const auto navigation_it = lift.navigation_vertices.find(level.name);
    if (navigation_it == lift.navigation_vertices.end())
      continue;

    auto& navigation = navigation_it->second;
    const int old_cabin = navigation.cabin;
    navigation.cabin = old_cabin >= 0 && old_cabin < static_cast<int>(remap.size())
      ? remap[old_cabin] : -1;
    navigation.cabin_position.reset();
    navigation.door_positions.clear();
    if (navigation.cabin < 0)
    {
      lift.navigation_vertices.erase(navigation_it);
      continue;
    }

    const auto& cabin_vertex = level.vertices[navigation.cabin];
    navigation.cabin_position = Eigen::Vector2d(cabin_vertex.x, cabin_vertex.y);
    for (auto door_it = navigation.doors.begin(); door_it != navigation.doors.end();)
    {
      const int old_door = door_it->second;
      door_it->second = old_door >= 0 && old_door < static_cast<int>(remap.size())
        ? remap[old_door] : -1;
      if (door_it->second < 0)
        door_it = navigation.doors.erase(door_it);
      else
      {
        const auto& door_vertex = level.vertices[door_it->second];
        navigation.door_positions[door_it->first] = Eigen::Vector2d(door_vertex.x, door_vertex.y);
        ++door_it;
      }
    }
    if (navigation.doors.empty())
      lift.navigation_vertices.erase(navigation_it);
  }
  return remap;
}
