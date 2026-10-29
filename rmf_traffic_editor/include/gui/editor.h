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

#ifndef EDITOR_H
#define EDITOR_H

#include <map>
#include <string>
#include <vector>

#include <QGraphicsItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsScene>
#include <QMainWindow>
#include <QSettings>
#include <QUndoStack>

#include "actions/add_edge.h"
#include "actions/move_feature.h"
#include "actions/move_fiducial.h"
#include "actions/move_model.h"
#include "actions/move_lift.hpp"
#include "actions/move_vertex.h"
#include "actions/rotate_model.h"
#include "building.h"
#include "editor_model.h"
#include "rendering_options.h"

#include "crowd_sim/crowd_sim_editor_table.h"

class BuildingTable;
class LayerTable;
class LevelTable;
class MapView;
class Level;
class LiftTable;
class TrafficTable;
class CrowdSimTable;

QT_BEGIN_NAMESPACE
class QAction;
class QButtonGroup;
class QComboBox;
class QGraphicsView;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QMenu;
class QMouseEvent;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;
class QTabWidget;
class QTimer;
class QToolButton;
QT_END_NAMESPACE


class Editor : public QMainWindow
{
  Q_OBJECT

private:
  enum ToolId
  {
    TOOL_SELECT = 1,
    TOOL_ADD_VERTEX,
    TOOL_MOVE,
    TOOL_ADD_LANE,
    TOOL_ADD_WALL,
    TOOL_ADD_MEAS,
    TOOL_ADD_DOOR,
    TOOL_ADD_MODEL,
    TOOL_ROTATE,
    TOOL_ADD_FLOOR,
    TOOL_EDIT_POLYGON,
    TOOL_ADD_ZONE,
    TOOL_ADD_FEATURE,
    TOOL_ADD_CONSTRAINT,
    TOOL_ADD_FIDUCIAL,
    TOOL_ADD_ROI,
    TOOL_ADD_HOLE,
    TOOL_ADD_HUMAN_LANE,
  };

  // mouse handlers for various tools
  enum MouseType
  {
    MOUSE_UNDEFINED = 0,
    MOUSE_PRESS = 1,
    MOUSE_RELEASE = 2,
    MOUSE_MOVE = 3
  };

public:
  Editor();

  ~Editor();

  static Editor* getInstance();

  /// Load a building, replacing the current building being edited
  bool loadBuilding(const QString& filename);

  /// Attempt to load the most recently saved project, just for convenience
  /// when starting the application since often we want to 'resume' editing.
  bool loadPreviousBuilding();

  /// Attempt to restore the previous viewport scale and center point
  void restorePreviousViewport();

  /// Event filter to catch mouse events on the map view and handle them in the editor
  bool eventFilter(QObject* obj, QEvent* event) override;

  void simTick();  // called by SimThread

protected:
  void mousePressEvent(QMouseEvent* e);

  void mouseReleaseEvent(QMouseEvent* e);

  void mouseMoveEvent(QMouseEvent* e);

  void keyPressEvent(QKeyEvent* event);

  void closeEvent(QCloseEvent* event);

  void showEvent(QShowEvent* event) override;

private:
  /////////////////
  // MENU ACTIONS
  void buildingNew();

  void buildingOpen();

  bool buildingSave();

  bool buildingExportFeatures();

  bool maybeSave();

  void editUndo();

  void editRedo();

  void editPreferences();

  void editBuildingProperties();

  void editProjectProperties();

  void editRotateAllModels();

  void editOptimizeLayerTransforms();

  void editAlignColinear();

  void levelAdd();

  void levelEdit();

  void levelTableUpdateSlot();

  void layerTableUpdate(const int row_idx);

  void layerTableUpdateSlot();

  void zoomReset();

  void viewModels();

  void viewTiles();

  void helpAbout();

  bool isMouseEventInMap(QMouseEvent* e, QPointF& p_scene);

  QToolButton* createToolButton(
    const ToolId id,
    const QString& icon_filename,
    const QString& tooltip);

  void toolToggled(int id, bool checked);

  Level* activeLevel();

  Layer* activeLayer();

  const QString toolIdToString(const int id);

  QTableWidget* createTabbedTable();

  void updateTables();

  void populateLayersTable();

  void layersTableSetRow(
    const int row_idx,
    const QString& label,
    const bool checked);

  void layerEditButtonClicked(const int row_idx);

  void layerAddButtonClicked();

  void updatePropertyEditor();

  void clearPropertyEditor();

  void populatePropertyEditor(const Edge& edge);

  void populatePropertyEditor(const Model& model);

  void populatePropertyEditor(const Vertex& vertex, const int index);

  void populatePropertyEditor(const Feature& feature);

  void populatePropertyEditor(const Fiducial& fiducial);

  void populatePropertyEditor(const Polygon& polygon);

  void populatePropertyEditor(const Layer& layer);

  QTableWidgetItem* createTableItem(const QString& str,
    bool editable = false);

  void propertyEditorCellChanged(int row, int column);

  void propertyEditorSetRow(
    const int row_idx,
    const QString& label,
    const QString& value,
    const bool editable = false);

  void propertyEditorSetRow(
    const int row_idx,
    const QString& label,
    const int& value,
    const bool editable = false);

  void propertyEditorSetRow(
    const int row_idx,
    const QString& label,
    const double& value,
    const int max_decimal_places = 3,
    const bool editable = false);

  void editLineStyle();

  void addParamButtonClicked();

  void deleteParamButtonClicked();

  void clearCurrentToolBuffer(); // Necessary for tools like edge drawing that store temporary states

#ifdef HAS_IGNITION_PLUGIN
  void sim_reset();
  void sim_play_pause();
  void scene_update_timer_timeout();
#endif

#if defined(HAS_IGNITION_PLUGIN) && defined(HAS_OPENCV)
  void record_start_stop();
  void record_frame_to_video();
#endif

  void loadModelNames();

  bool createScene();

  void drawMouseMotionLineItem(const double mouse_x, const double mouse_y);

  void removeMouseMotionItem();

  void numberKeyPressed(const int n);

  void mouseEvent(const MouseType t, QMouseEvent* e);

  // helper function to avoid repeating lots of "add edge" code
  void mouseAddEdge(
    const MouseType t,
    QMouseEvent* e,
    const QPointF& p,
    const Edge::Type& edge_type);

  // helper function to avoid repeating lots of "add polygon" code
  void mouseAddPolygon(
    const MouseType t,
    QMouseEvent* e,
    const QPointF& p,
    const Polygon::Type& polygon_type);

  QGraphicsPixmapItem* getClosestPixmapItem(const QPointF& p);

  double discretizeAngle(const double& angle);

  void alignPoint(const QPointF& start, QPointF& end);

  void mouseSelect(const MouseType t, QMouseEvent* e, const QPointF& p);

  void mouseMove(const MouseType t, QMouseEvent* e, const QPointF& p);

  void mouseRotate(const MouseType t, QMouseEvent* e, const QPointF& p);

  void mouseAddVertex(const MouseType t, QMouseEvent* e, const QPointF& p);

  void mouseAddFeature(const MouseType t, QMouseEvent* e, const QPointF& p);

  void mouseAddFiducial(const MouseType t, QMouseEvent* e, const QPointF& p);

  void mouseAddLane(const MouseType t, QMouseEvent* e, const QPointF& p);

  void mouseAddWall(const MouseType t, QMouseEvent* e, const QPointF& p);

  void mouseAddMeas(const MouseType t, QMouseEvent* e, const QPointF& p);

  void mouseAddDoor(const MouseType t, QMouseEvent* e, const QPointF& p);

  void mouseAddModel(const MouseType t, QMouseEvent* e, const QPointF& p);

  void mouseAddFloor(const MouseType t, QMouseEvent* e, const QPointF& p);

  void mouseAddHole(const MouseType t, QMouseEvent* e, const QPointF& p);

  void mouseAddRoi(const MouseType t, QMouseEvent* e, const QPointF& p);

  void mouseEditPolygon(const MouseType t, QMouseEvent* e, const QPointF& p);

  void mouseAddHumanLane(const MouseType t, QMouseEvent* e,
    const QPointF& p);

  void mouseAddConstraint(
    const MouseType t,
    QMouseEvent* e,
    const QPointF& p);

  void sanityCheck();

  void cacheSizeUpdateTimerTimeout();

private:
  QUndoStack undo_stack;
  ToolId tool_id = TOOL_SELECT;
  std::map<ToolId, QAction*> tools;
  QToolBar* toolbar;
  QComboBox* mode_combo_box;
  /////////////////////////////
  static Editor* instance;  // there will only be one instance
  Building building;
  RenderingOptions rendering_options;
  int level_idx = 0;  // level that we are currently editing
  int layer_idx = 0;  // currently selected layer
  int clicked_idx = -1;  // point most recently clicked
  int prev_clicked_idx = -1; // Previously clicked ID.
  //int polygon_idx = -1;  // currently selected polygon
  Polygon* selected_polygon = nullptr;
  QUuid clicked_feature_id;
  QGraphicsScene* scene = nullptr;
  MapView* map_view = nullptr;
  QAction* view_models_action = nullptr;
  QAction* view_tiles_action = nullptr;
  QButtonGroup* tool_button_group = nullptr;
  QTabWidget* right_tab_widget = nullptr;
  LevelTable* level_table = nullptr;
  LayerTable* layer_table = nullptr;
  LiftTable* lift_table = nullptr;
  TrafficTable* traffic_table = nullptr;
  CrowdSimEditorTable* crowd_sim_table = nullptr;
  QTableWidget* property_editor = nullptr;
  QPushButton* add_param_button, * delete_param_button;
  QPushButton* line_style_button;
#ifdef HAS_IGNITION_PLUGIN
  QAction* sim_reset_action;
  QAction* sim_play_pause_action;
  SimThread sim_thread;
  QTimer* scene_update_timer;
#endif
#if defined(HAS_IGNITION_PLUGIN) && defined(HAS_OPENCV)
  QAction* record_start_stop_action;
  bool is_recording = false;
  cv::VideoWriter* video_writer = nullptr;
#endif
  std::vector<EditorModel> editor_models;
  EditorModel* mouse_motion_editor_model = nullptr;
  const static int rotation_indicator_radius = 50;
  QGraphicsLineItem* mouse_motion_line = nullptr;
  QGraphicsEllipseItem* mouse_motion_ellipse = nullptr;
  QGraphicsPixmapItem* mouse_motion_model = nullptr;
  QGraphicsPolygonItem* mouse_motion_polygon = nullptr;
  int mouse_model_idx = -1;
  int mouse_vertex_idx = -1;
  int mouse_feature_idx = -1;
  int mouse_feature_layer_idx = -1;
  int mouse_fiducial_idx = -1;
  std::vector<int> mouse_motion_polygon_vertices;
  //int mouse_motion_polygon_vertex_idx = -1;
  Polygon::EdgeDragPolygon mouse_edge_drag_polygon;
  QPointF previous_mouse_point;
  // For undo related support
  AddEdgeCommand* latest_add_edge = nullptr;
  MoveFeatureCommand* latest_move_feature = nullptr;
  MoveFiducialCommand* latest_move_fiducial = nullptr;
  MoveModelCommand* latest_move_model = nullptr;
  MoveLiftCommand* latest_move_lift = nullptr;
  Eigen::Affine2d lift_scene_to_reference = Eigen::Affine2d::Identity();
  Eigen::Vector2d lift_drag_offset = Eigen::Vector2d::Zero();
  MoveVertexCommand* latest_move_vertex = nullptr;
  RotateModelCommand* latest_rotate_model = nullptr;
  QLabel* cache_size_label = nullptr;
  QTimer* cache_size_update_timer = nullptr;
};

#endif
