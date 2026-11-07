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

#ifndef LEVEL_H
#define LEVEL_H

#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>
#include <string>

#include "constraint.hpp"
#include "coordinate_system.h"
#include "edge.h"
#include "editor_model.h"
#include "feature.hpp"
#include "fiducial.h"
#include "graph.h"
#include "layer.h"
#include "model.h"
#include "polygon.h"
#include "rendering_options.h"
#include "vertex.h"

#include <QPixmap>
#include <QPainterPath>
class QGraphicsScene;


class Level final
{
public:
  enum ItemType {VERTEX = 1, MODEL, FIDUCIAL};

  struct NearestItem
  {
  public:
    double model_dist = 1e100;
    int model_idx = -1;
    double vertex_dist = 1e100;
    int vertex_idx = -1;
    double feature_dist = 1e100;
    int feature_layer_idx = -1;
    int feature_idx = -1;
    double fiducial_dist = 1e100;
    int fiducial_idx = -1;
  };

  struct SelectedItem
  {
  public:
    int model_idx = -1;
    int vertex_idx = -1;
    int fiducial_idx = -1;
    int edge_idx = -1;
    int polygon_idx = -1;
    int feature_idx = -1;
    int feature_layer_idx = -1;
    int constraint_idx = -1;
  };

public:
  Level() = default;

  ~Level() = default;

  bool areLayerNamesUnique();

  Polygon::EdgeDragPolygon polygonEdgeDragPress(const Polygon* polygon, const double x, const double y);

  void addVertex(const double x, const double y);
  void addVertex(const Vertex& new_vertex);
  std::size_t getVertexById(QUuid vertex_id);
  std::optional<std::size_t> getEditorGeneratedVertexByKey(const std::string& key);

  bool fromYaml(const std::string& name, const YAML::Node& data, const CoordinateSystem& coordinate_system);

  YAML::Node toYaml(const CoordinateSystem& coordinate_system) const;
  std::vector<nlohmann::json> toJson(const CoordinateSystem& coordinate_system) const;

  const Feature* findFeature(const QUuid& id) const;

  const Feature* findFeature(const double x, const double y) const;

  bool getFeaturePoint(const QUuid& id, QPointF& point) const;

  void addConstraint(const QUuid& a, const QUuid& b);

  void removeConstraint(const QUuid& a, const QUuid& b);

  NearestItem nearestItems(const double x, const double y);

  int nearestItemIndexIfWithinDistance(const double x,
                                       const double y,
                                       const double distance_threshold,
                                       const ItemType item_type);

  void mouseSelectPress(const double x,
                        const double y,
                        QGraphicsItem* graphics_item,
                        const RenderingOptions& rendering_options,
                        const Qt::KeyboardModifiers& modifiers);

  bool canDeleteCurrentSelection();

  bool deleteSelected();

  bool deleteUsedEntities(int selected_vertex_idx);

  bool deleteLiftVertex(std::string lift_name);

  std::vector<Edge> edgesWithVertex(int vertex_idx) const;

  std::optional<Edge*> getEditorGeneratedEdgeByKey(const std::string& key);

  std::vector<Polygon> polygonsWithVertex(int vertex_idx) const;

  void calculateScale(const CoordinateSystem& coordinate_system);

  void clearSelection();

  void getSelectedItems(std::vector<SelectedItem>& selected_items);

  void setSelectedLineItem(QGraphicsLineItem* line_item, const RenderingOptions& rendering_options);

  void setSelectedContainingPolygon(const double x, const double y);

  void draw(QGraphicsScene* scene,
            std::vector<EditorModel>& editor_models,
            const RenderingOptions& rendering_options,
            const std::vector<Graph>& graphs,
            const CoordinateSystem& coordinate_system);

  void clearScene();

  bool loadDrawing(QString* error_message = nullptr);

  void setDrawingVisible(bool value) { _drawing_visible = value; }

  bool getDrawingVisible() const { return _drawing_visible; }

  QUuid addFeature(const int layer, const double x, const double y);

  void removeFeature(const int layer_idx, QUuid feature_uuid);

  bool exportFeatures(const std::string& filename) const;

  void optimizeLayerTransforms();

  void computeLayerTransforms();

  void computeLayerTransform(const std::size_t layer_idx);

  void alignColinear();

private:
  double pointToLineSegmentDistance(const double x,
                                    const double y,
                                    const double x0,
                                    const double y0,
                                    const double x1,
                                    const double y1,
                                    double& x_proj,
                                    double& y_proj);

  void loadYamlEdgeSequence(const YAML::Node& data,
                            const char* sequence_name,
                            const Edge::Type type);

  void drawLane(QGraphicsScene* scene,
                const Edge& edge,
                const RenderingOptions& rendering_options,
                const std::vector<Graph>& graphs) const;

  void drawWall(QGraphicsScene* scene, const Edge& edge) const;

  void drawMeas(QGraphicsScene* scene, const Edge& edge) const;

  void drawDoor(QGraphicsScene* scene, const Edge& edge) const;

  void drawFiducials(QGraphicsScene* scene) const;

  void drawPolygons(QGraphicsScene* scene) const;

  void drawConstraint(
    QGraphicsScene* scene,
    const Constraint& constraint,
    int constraint_idx) const;

  // helper function
  void drawPolygon(
    QGraphicsScene* scene,
    const QBrush& brush,
    const Polygon& polygon) const;

  void addDoorSwingPath(
    QPainterPath& path,
    double hinge_x,
    double hinge_y,
    double door_length,
    double start_angle,
    double end_angle) const;

  void addDoorSlidePath(
    QPainterPath& path,
    double hinge_x,
    double hinge_y,
    double door_length,
    double door_angle) const;

public:
  std::string name;

  // primitives
  std::vector<Vertex> vertices;
  std::vector<Edge> edges;
  std::vector<Polygon> polygons;
  std::vector<Layer> layers;
  std::vector<Model> models;
  std::vector<Fiducial> fiducials;
  std::vector<Feature> floorplan_features;
  std::vector<Constraint> constraints;

  // temporary, just for debugging polygon edge projection...
  double polygon_edge_proj_x = 0.0;
  double polygon_edge_proj_y = 0.0;

  std::string drawing_filename;
  int drawing_width = 0;
  int drawing_height = 0;
  double drawing_meters_per_pixel = 0.05;
  double elevation = 0.0;
  double vertex_radius = 0.1;  // meters or degrees, depending on coordinates
  double x_meters = 10.0;  // manually specified if no drawing supplied
  double y_meters = 10.0;  // manually specified if no drawing supplied

  QPixmap floorplan_pixmap;

  Renderer renderer;

private:
  bool _drawing_visible = true;
};

#endif