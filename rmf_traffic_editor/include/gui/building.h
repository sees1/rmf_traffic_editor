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

#ifndef BUILDING_H
#define BUILDING_H

#include <optional>
#include <set>


class QGraphicsScene;

#include <memory>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

#include <QGraphicsLineItem>
#include <QPointF>

#include "coordinate_system.h"
#include "graph.h"
#include "level.h"
#include "lift.h"
#include "param.h"
#include <crowd_sim/crowd_sim_impl.h>
#include "rendering_options.h"

class Building
{
public:
  struct LevelPair
  {
  public:
    bool operator<(const LevelPair& rhs) const
    {
      return std::tie(from_idx, to_idx) < std::tie(rhs.from_idx, rhs.to_idx);
    }

  public:
    int from_idx = -1;
    int to_idx = -1;
  };

  // to apply transform: first scale, then translate
  struct Transform
  {
  public:
    double scale = 1.0;
    double dx = 0.0;
    double dy = 0.0;
    double rotation = 0.0;
  };

  typedef std::map<LevelPair, Transform> TransformMap;

public:
  Building();

  virtual ~Building();

  bool setFilename(const std::string& _filename);

  std::string getFilename() { return filename; }

  bool load(const std::string& filename);

  bool save();
  bool exportNavigation(const std::string& path) const;

  void updateLifts(Lift* lift, int reference_lvl);
  
  // Compact the shared graph and remap every object's references on this level.
  std::vector<int> updateGraphOnLevel(Level& level,
                                      const std::string& prefix,
                                      const std::set<std::string>& wanted_vertices,
                                      const std::set<std::string>& wanted_edges,
                                      const std::set<int>& removed_vertices = {});

  void clear();  // clear all internal data structures

  bool exportFeatures(
    int level_index,
    const std::string& dest_filename) const;

  void clearSelection(const int level_idx);

  bool canDeleteCurrentSelection(const int level_idx);

  void addLevel(const Level& level);

  void addVertex(int level_index, double x, double y);
  void moveVertex(int level_idx, int vertex_idx, double x, double y);

  QUuid addFiducial(int level_index, double x, double y);

  QUuid addFeature(int level, int layer, double x, double y);

  void removeFeature(const int level, const int layer, QUuid feature_uuid);

  void addConstraint(const int level_idx, const QUuid& a, const QUuid& b);

  void removeConstraint(const int level_idx, const QUuid& a, const QUuid& b);

  int nearestItemIndexIfWithinDistance(
    const double level_idx,
    const double x,
    const double y,
    const double distance_threshold,
    const Level::ItemType item_type);

  int findNearestVertexIndex(
    int level_index, double x, double y, double& distance);

  void addEdge(const int level_idx, std::shared_ptr<Edge> edge);
  void addDoor(const int level_idx, std::shared_ptr<Door> door);
  void addLane(const int level_idx, const int graph_idx, std::shared_ptr<Edge> edge);
  void addLift(std::shared_ptr<Lift> lift);

  QUuid addModel(
    const int level_idx,
    const double x,
    const double y,
    const double z,
    const double yaw,
    const std::string& model_name);

  bool deleteSelected(const int level_index);

  void setModelYaw(
    const int level_idx,
    const int model_idx,
    const double yaw);

  bool transformBetweenLevels(
    const std::string& from_level_name,
    const QPointF& from_point,
    const std::string& to_level_name,
    QPointF& to_point);

  bool transformBetweenLevels(
    const int from_level_idx,
    const QPointF& from_point,
    const int to_level_idx,
    QPointF& to_point);

  void clearTransformCache();

  Transform computeTransform(
    const int from_level_idx,
    const int to_level_idx);

  Transform getTransform(const int from_level_idx, const int to_level_idx);

  void calculateAllTransforms();

  int getReferenceLevelIdx();

  void clearScene();

  double levelMetersPerPixel(const std::string& level_name) const;

  void rotateAllModels(const double rotation);

  void getSelectedItems(const int level_idx, std::vector<Level::SelectedItem>& selected);

  void draw(
    QGraphicsScene* scene,
    const int level_idx,
    std::vector<EditorModel>& editor_models,
    const RenderingOptions& rendering_options);

  /*
  void mouse_select_press(
    const int level_idx,
    const double x,
    const double y,
    QGraphicsItem* graphics_item,
    const RenderingOptions& rendering_options);
  */
  std::optional<Polygon*> getSelectedPolygon(const int level_idx);

  EdgeDragPolygon polygonEdgeDragPress(
    const int level_idx,
    const Polygon* polygon,
    const double x,
    const double y);

  Lift getLift(const std::string& name) const;

public:
  std::string name;
  std::string reference_level_name;
  std::vector<Level> levels;
  std::vector<Lift> lifts;
  std::vector<Graph> graphs;
  std::map<std::string, Param> params;
  CoordinateSystem coordinate_system;
  mutable crowd_sim::CrowdSimImplPtr crowd_sim_impl;
  TransformMap transforms;

private:
  std::string filename;
};

#endif
