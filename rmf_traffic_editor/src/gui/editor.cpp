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
#include "utils/geometry.hpp"
#include <cmath>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include <QtWidgets>

#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QToolBar>

#include <yaml-cpp/yaml.h>

#include "ament_index_cpp/get_package_share_directory.hpp"
#include "ament_index_cpp/get_package_prefix.hpp"
#include "ament_index_cpp/get_resource.hpp"

#include "actions/add_constraint.hpp"
#include "actions/add_feature.h"
#include "actions/edge_style.h"
#include "actions/add_fiducial.h"
#include "actions/add_model.h"
#include "actions/add_property.h"
#include "actions/add_polygon.h"
#include "actions/add_vertex.h"
#include "actions/delete.h"
#include "actions/polygon_add_vertex.h"
#include "actions/polygon_remove_vertices.h"

#include "add_param_dialog.h"
#include "building_dialog.h"
#include "delete_dialog.h"
#include "editor.h"
#include "door.h"
#include "layer_dialog.h"
#include "layer_table.h"
#include "level_dialog.h"
#include "level_table.h"
#include "lift_table.h"
#include "map_view.h"
#include "model_dialog.h"
#include "preferences_dialog.h"
#include "preferences_keys.h"
#include "traffic_table.h"
#include "ui_new_building_dialog.h"
#include "ui_transform_dialog.h"


using std::string;
using std::isnan;


Editor* Editor::instance = nullptr;

Editor::Editor()
: QMainWindow()
{
  instance = this;

  setWindowIcon(QIcon(":icons/traffic_editor.svg"));
  setWindowTitle("Traffic Editor[*]");

  QSettings settings;
  qDebug("settings filename: [%s]", qUtf8Printable(settings.fileName()));

  scene = new QGraphicsScene(this);

  map_view = new MapView(this, building);
  map_view->setScene(scene);
  map_view->setFrameShape(QFrame::NoFrame);


  layer_table = new LayerTable;
  connect(
    layer_table, &QTableWidget::cellClicked,
    [&](int row, int /*col*/)
    {
      if (row - 1 < static_cast<int>(building.levels[level_idx].layers.size()))
      {
        layer_idx = row;
        layer_table->update(building, level_idx, layer_idx);
        if (layer_idx > 0)
          populatePropertyEditor(building.levels[level_idx].layers[layer_idx-1]);
      }
    });

  connect(
    layer_table,
    &LayerTable::redrawScene,
    this,
    &Editor::layerTableUpdateSlot);

  connect(
    layer_table,
    &LayerTable::addButtonClicked,
    this,
    &Editor::layerAddButtonClicked);

  connect(
    layer_table,
    &LayerTable::editButtonClicked,
    this,
    &Editor::layerEditButtonClicked);

  level_table = new LevelTable;
  connect(
    level_table, &QTableWidget::cellClicked,
    [&](int row, int /*col*/)
    {
      if (row < static_cast<int>(building.levels.size()))
      {
        if (latest_move_lift)
          clearCurrentToolBuffer();
        // save the center point of the current level's image coordinates
        const QPoint p_center_window(
          map_view->viewport()->width() / 2,
          map_view->viewport()->height() / 2);
        QPointF p_center_scene = map_view->mapToScene(p_center_window);

        QPointF p_transformed;
        building.transformBetweenLevels(
          level_idx,
          p_center_scene,
          row,
          p_transformed);

        // maintain the view scale
        double scale = map_view->transform().m11() *
        building.levels[row].drawing_meters_per_pixel /
        building.levels[level_idx].drawing_meters_per_pixel;

        if (isnan(scale))
        {
          scale = 1.0;
          p_transformed = QPointF(0.0, 0.0);
        }

        level_idx = row;
        createScene();

        QTransform t;
        double y_flip = building.coordinate_system.isYFlipped() ? 1 : -1;
        t.scale(scale, y_flip * scale);
        map_view->setTransform(t);
        map_view->centerOn(p_transformed);
      }
    });

  connect(
    level_table,
    &LevelTable::redrawScene,
    this,
    &Editor::levelTableUpdateSlot);

  connect(
    level_table,
    &LevelTable::levelChanged,
    this,
    [this](int row)
    {
      if (latest_move_lift)
        clearCurrentToolBuffer();
      level_idx = row;
      layer_idx = 0;
      updateTables();
      level_table->setCurrentCell(row, 0);
      createScene();
      zoomReset();
      setWindowModified(true);
    });

  lift_table = new LiftTable;
  connect(
    lift_table,
    &TableList::redraw,
    [this]() { this->createScene(); });

  traffic_table = new TrafficTable;
  connect(
    traffic_table,
    &TableList::redraw,
    [this]() { this->createScene(); });

  connect(
    traffic_table,
    &QTableWidget::cellClicked,
    [&](int row, int /*col*/)
    {
      rendering_options.active_traffic_map_idx = row;
      traffic_table->update(rendering_options);
    });

  crowd_sim_table = new CrowdSimEditorTable(building);
  connect(
    crowd_sim_table,
    &QTableWidget::cellClicked,
    [&]()
    {
      crowd_sim_table->update();
      createScene();
    }
  );

  right_tab_widget = new QTabWidget;
  right_tab_widget->setDocumentMode(true);
  right_tab_widget->setUsesScrollButtons(true);
  right_tab_widget->addTab(level_table, "Levels");
  right_tab_widget->addTab(layer_table, "Layers");
  right_tab_widget->addTab(lift_table, "Lifts");
  right_tab_widget->addTab(traffic_table, "Graphs");
  right_tab_widget->addTab(crowd_sim_table, "Crowds");

  property_editor = new QTableWidget;
  property_editor->viewport()->installEventFilter(this);
  property_editor->setAlternatingRowColors(true);
  property_editor->setShowGrid(false);
  property_editor->setMinimumSize(320, 180);
  property_editor->setSizePolicy(
    QSizePolicy::Expanding,
    QSizePolicy::MinimumExpanding);
  property_editor->setColumnCount(2);
  property_editor->horizontalHeader()->setVisible(false);
  property_editor->verticalHeader()->setVisible(false);
  property_editor->horizontalHeader()->setSectionResizeMode(
    0,
    QHeaderView::ResizeToContents);
  property_editor->horizontalHeader()->setSectionResizeMode(
    1,
    QHeaderView::Stretch);
  property_editor->verticalHeader()->setSectionResizeMode(
    QHeaderView::ResizeToContents);
  property_editor->setAutoFillBackground(true);
  property_editor->setSelectionBehavior(QAbstractItemView::SelectRows);
  connect(
    property_editor, &QTableWidget::cellChanged,
    this, &Editor::propertyEditorCellChanged);
  connect(
    property_editor, &QTableWidget::itemSelectionChanged,
    [this]()
    {
      const int row = property_editor->currentRow();
      if (row < 0)
      {
        delete_param_button->setEnabled(false);
        return;
      }
      QTableWidgetItem* item = property_editor->item(row, 0);
      delete_param_button->setEnabled(
        item && item->data(Qt::UserRole).toBool());
    });

  QHBoxLayout* param_button_layout = new QHBoxLayout;

  add_param_button = new QPushButton("Add property...");
  add_param_button->setEnabled(false);
  connect(
    add_param_button, &QAbstractButton::clicked,
    this, &Editor::addParamButtonClicked);

  delete_param_button = new QPushButton("Delete property");
  delete_param_button->setEnabled(false);
  connect(
    delete_param_button, &QAbstractButton::clicked,
    this, &Editor::deleteParamButtonClicked);

  param_button_layout->addWidget(add_param_button);
  param_button_layout->addWidget(delete_param_button);

  QVBoxLayout* right_column_layout = new QVBoxLayout;
  right_column_layout->addWidget(right_tab_widget);

  QLabel* properties_label = new QLabel("Properties");
  properties_label->setObjectName("sectionHeading");
  right_column_layout->addWidget(properties_label);
  line_style_button = new QPushButton("Line style...");
  line_style_button->setObjectName("lineStyleButton");
  line_style_button->setEnabled(false);
  connect(line_style_button, &QPushButton::clicked,
    this, &Editor::editLineStyle);
  right_column_layout->addWidget(line_style_button);
  right_column_layout->addWidget(property_editor);
  right_column_layout->addLayout(param_button_layout);

  QWidget* inspector = new QWidget;
  inspector->setObjectName("inspector");
  right_column_layout->setContentsMargins(12, 12, 12, 12);
  right_column_layout->setSpacing(10);
  inspector->setLayout(right_column_layout);
  inspector->setMinimumWidth(360);

  QSplitter* workspace = new QSplitter(Qt::Horizontal, this);
  // MapView ignores tool mouse events so they reach the editor. Every
  // intermediate widget must track hover motion for live tool previews.
  workspace->setMouseTracking(true);
  workspace->addWidget(map_view);
  workspace->addWidget(inspector);
  workspace->setChildrenCollapsible(false);
  workspace->setStretchFactor(0, 1);
  workspace->setStretchFactor(1, 0);
  workspace->setSizes({1000, 420});
  setMouseTracking(true);
  setCentralWidget(workspace);

  // BUILDING MENU
  QMenu* building_menu = menuBar()->addMenu("&Building");

  building_menu->addAction(
    "&New...",
    this,
    &Editor::buildingNew,
    QKeySequence(Qt::CTRL + Qt::Key_N));

  building_menu->addAction(
    "&Open...",
    this,
    &Editor::buildingOpen,
    QKeySequence(Qt::CTRL + Qt::Key_O));

  building_menu->addAction(
    "&Save YAML",
    this,
    &Editor::buildingSave,
    QKeySequence(Qt::CTRL + Qt::Key_S));

  building_menu->addAction("Export navigation JSON...", this, [this]()
    {
      QFileDialog dialog(this, "Export navigation map");
      dialog.setNameFilter("Navigation JSON (*.json)");
      dialog.setDefaultSuffix("json");
      dialog.setAcceptMode(QFileDialog::AcceptSave);
      dialog.selectFile(QFileInfo(QString::fromStdString(
        building.getFilename())).completeBaseName() + ".json");
      if (dialog.exec() != QDialog::Accepted)
        return;
      const QString path = dialog.selectedFiles().first();
      if (!building.exportNavigation(path.toStdString()))
      {
        QMessageBox::critical(this, "Export failed",
          "Unable to export navigation JSON. Check the map and output path.");
        return;
      }
      statusBar()->showMessage("Exported " + path, 5000);
    });

  building_menu->addAction("Door vertex offset...", this, [this]()
    {
      double current = 0.5;
      const auto it = building.params.find("door_vertex_offset");
      if (it != building.params.end() && it->second.type == Param::DOUBLE)
        current = it->second.value_double;
      bool accepted = false;
      const double value = QInputDialog::getDouble(this, "Door vertices",
        "Distance from doorway (meters):", current, 0.01, 100.0, 2, &accepted);
      if (!accepted)
        return;
      building.params["door_vertex_offset"] = Param(value);
      createScene();
      setWindowModified(true);
    });

  building_menu->addAction(
    "&Export layer alignment points for level",
    this,
    &Editor::buildingExportFeatures,
    QKeySequence(Qt::CTRL + Qt::Key_E));

  building_menu->addSeparator();

  building_menu->addAction(
    "E&xit",
    this,
    &QWidget::close,
    QKeySequence(Qt::CTRL + Qt::Key_Q));

  // EDIT MENU
  QMenu* edit_menu = menuBar()->addMenu("&Edit");
  edit_menu->addAction(
    "&Undo",
    this,
    &Editor::editUndo,
    QKeySequence::Undo);
  edit_menu->addAction(
    "&Redo",
    this,
    &Editor::editRedo,
    QKeySequence::Redo);
  edit_menu->addSeparator();

  edit_menu->addAction(
    "&Building properties...",
    this,
    &Editor::editBuildingProperties);
  edit_menu->addSeparator();

  edit_menu->addAction(
    "Rotate all models...",
    this,
    &Editor::editRotateAllModels);
  edit_menu->addSeparator();

  edit_menu->addAction(
    "Optimize layer transforms",
    this,
    &Editor::editOptimizeLayerTransforms,
    QKeySequence(Qt::CTRL + Qt::Key_T));
  edit_menu->addSeparator();

  edit_menu->addAction(
    "Align &colinear",
    this,
    &Editor::editAlignColinear,
    QKeySequence(Qt::Key_Slash));
  edit_menu->addSeparator();

  edit_menu->addAction("&Preferences...", this, &Editor::editPreferences);

  // VIEW MENU
  QMenu* view_menu = menuBar()->addMenu("&View");
  view_models_action =
    view_menu->addAction("&Models", this, &Editor::viewModels);
  view_models_action->setCheckable(true);
  view_models_action->setChecked(true);

  view_tiles_action =
    view_menu->addAction("Map &Tiles", this, &Editor::viewTiles);
  view_tiles_action->setCheckable(true);
  view_tiles_action->setChecked(true);

  view_menu->addSeparator();

  view_menu->addAction("&Reset zoom level", this, &Editor::zoomReset);

  // HELP MENU
  QMenu* help_menu = menuBar()->addMenu("&Help");

  help_menu->addAction("&About", this, &Editor::helpAbout);
  help_menu->addAction("About &Qt", &QApplication::aboutQt);

  // TOOLBAR
  toolbar = new QToolBar("Map tools", this);
  toolbar->setMovable(false);
  toolbar->setIconSize(QSize(24, 24));

  tool_button_group = new QButtonGroup(this);
  tool_button_group->setExclusive(true);

  createToolButton(TOOL_SELECT, ":icons/select.svg", "Select (Esc)");
  createToolButton(TOOL_MOVE, ":icons/move.svg", "Move (M)");
  createToolButton(TOOL_ROTATE, ":icons/rotate.svg", "Rotate (R)");
  toolbar->addSeparator();
  createToolButton(TOOL_ADD_VERTEX, ":icons/vertex.svg", "Add Vertex (V)");
  createToolButton(TOOL_ADD_FEATURE, ":icons/feature.svg", "Add Feature");
  createToolButton(TOOL_ADD_CONSTRAINT,
    ":icons/constraint.svg",
    "Add Constraint");
  createToolButton(
    TOOL_ADD_FIDUCIAL,
    ":icons/fiducial.svg",
    "Add Level Alignment Fiducial");
  createToolButton(TOOL_ADD_LANE, ":icons/lane.svg", "Add Lane (L)");
  createToolButton(TOOL_ADD_WALL, ":icons/wall.svg", "Add Wall (W)");
  createToolButton(
    TOOL_ADD_MEAS,
    ":icons/measurement.svg",
    "Add Measurement (T)");
  createToolButton(TOOL_ADD_DOOR, ":icons/door.svg", "Add Door (D)");
  createToolButton(TOOL_ADD_MODEL, ":icons/model.svg", "Add Model (O)");
  toolbar->addSeparator();
  createToolButton(TOOL_ADD_FLOOR, ":icons/floor.svg",
    "Add floor polygon (F)");
  createToolButton(TOOL_ADD_HOLE, ":icons/hole.svg", "Add hole polygon");
  createToolButton(TOOL_ADD_ROI, ":icons/roi.svg", "Add region of interest");
  createToolButton(TOOL_EDIT_POLYGON, ":icons/polygon.svg", "Edit Polygon (E)");
  createToolButton(TOOL_ADD_HUMAN_LANE, ":icons/human_lane.svg", "Add Human Lane with width");

  connect(
    tool_button_group,
    QOverload<int, bool>::of(&QButtonGroup::buttonToggled),
    this, &Editor::toolToggled);

  toolbar->addSeparator();

  addToolBar(Qt::TopToolBarArea, toolbar);

  // STATUS BAR
  cache_size_label = new QLabel("cache size");
  statusBar()->addPermanentWidget(cache_size_label);
  map_view->updateCacheSizeLabel(cache_size_label);

  ///////////////////////////////////////////////////////////
  // SET SIZE
  const int width =
    settings.contains(preferences_keys::window_width) ?
    settings.value(preferences_keys::window_width).toInt() :
    QGuiApplication::primaryScreen()->availableSize().width();

  const int height =
    settings.contains(preferences_keys::window_height) ?
    settings.value(preferences_keys::window_height).toInt() :
    QGuiApplication::primaryScreen()->availableSize().height();

  const int left =
    settings.contains(preferences_keys::window_left) ?
    settings.value(preferences_keys::window_left).toInt() :
    0;

  const int top =
    settings.contains(preferences_keys::window_top) ?
    settings.value(preferences_keys::window_top).toInt() :
    0;

  setGeometry(left, top, width, height);
  map_view->adjustSize();

  // default tool is the "select" tool
  tool_button_group->button(TOOL_SELECT)->click();

  loadModelNames();
  level_table->setCurrentCell(level_idx, 0);

  cache_size_update_timer = new QTimer;
  connect(
    cache_size_update_timer,
    &QTimer::timeout,
    this,
    &Editor::cacheSizeUpdateTimerTimeout);
  cache_size_update_timer->start(10 * 1000);
}

Editor::~Editor()
{
  delete latest_move_lift;
}

void Editor::loadModelNames()
{
  QSettings settings;
  QString thumbnail_path(
    settings.value(preferences_keys::thumbnail_path).toString());
  if (thumbnail_path.isEmpty())
  {
    std::string assets_dir;
    std::string share_dir;

    try
    {
      share_dir =
        ament_index_cpp::get_package_share_directory(
        "rmf_traffic_editor_assets");

      ament_index_cpp::get_resource("rmf_traffic_editor_assets",
        "assets",
        assets_dir);
    }
    catch (const ament_index_cpp::PackageNotFoundError& e)
    {
      qWarning("Could not load default thumbnail directory! "
        "traffic_editor_assets package not found in workspace!");
      return;
    }

    // Strip newlines from assets_dir
    assets_dir.erase(std::remove(assets_dir.begin(), assets_dir.end(), '\n'),
      assets_dir.end());

    // Obtain thumbnail path from traffic_editor_assets ament package
    thumbnail_path =
      QDir::cleanPath(
      QDir(QApplication::applicationDirPath()).filePath(
        (share_dir + "/" + assets_dir + "/thumbnails").c_str()
      )
      );
    settings.setValue(preferences_keys::thumbnail_path, thumbnail_path);
  }

  QString model_list_path = QDir(thumbnail_path).filePath("model_list.yaml");

  YAML::Node y;
  std::string filename(model_list_path.toStdString());
  try
  {
    y = YAML::LoadFile(filename);
  }
  catch (const std::exception& e)
  {
    qWarning("couldn't parse %s: %s", filename.c_str(), e.what());
    return;
  }
  qInfo("parsed %s successfully", filename.c_str());

  const double model_meters_per_pixel = y["meters_per_pixel"].as<double>();
  const YAML::Node ym = y["models"];
  editor_models.reserve(y["models"].size());
  for (YAML::const_iterator it = ym.begin(); it != ym.end(); ++it)
    editor_models.emplace_back(
      it->as<std::string>(),
      model_meters_per_pixel);
}

QToolButton* Editor::createToolButton(
  const ToolId id,
  const QString& icon_filename,
  const QString& tooltip)
{
  QToolButton* b = new QToolButton(toolbar);
  b->setCheckable(true);
  b->setIconSize(QSize(24, 24));
  b->setAccessibleName(tooltip);
  b->setStatusTip(tooltip);

  if (!icon_filename.isEmpty())
  {
    QIcon icon(icon_filename);
    b->setIcon(icon);
    b->setToolTip(tooltip);
  }
  else
  {
    b->setText(toolIdToString(id));
    b->setToolTip(tooltip);
    //b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
  }
  tools[id] = toolbar->addWidget(b);
  tool_button_group->addButton(b, id);
  return b;
}

Editor* Editor::getInstance()
{
  return instance;
}

bool Editor::loadBuilding(const QString& filename)
{
  if (latest_move_lift)
    clearCurrentToolBuffer();
  const QString absolute_path = QFileInfo(filename).absoluteFilePath();
  if (!building.load(absolute_path.toStdString()))
    return false;

  level_idx = 0;

  map_view->setShowTiles(false);

  if (building.coordinate_system.isGlobal())
  {
    // use the EPSG 3857 extents: "projected-meters"
    scene->setSceneRect(QRectF(
        -M_PI * CoordinateSystem::wgs84_a,
        M_PI * CoordinateSystem::wgs84_a,
        2. * M_PI * CoordinateSystem::wgs84_a,
        -2. * M_PI * CoordinateSystem::wgs84_a));

    const double y_flip = building.coordinate_system.isYFlipped() ? 1 : -1;

    QTransform t;
    t.scale(1, y_flip);
    map_view->setTransform(t);

    map_view->setShowTiles(true);
    map_view->drawTiles();
  }
  else if (!building.levels.empty())
  {
    const Level& level = building.levels[level_idx];
    scene->setSceneRect(
      QRectF(0, 0, level.drawing_width, level.drawing_height));
    previous_mouse_point = QPointF(level.drawing_width, level.drawing_height);
  }

  createScene();

  updateTables();

  setWindowModified(false);

  return true;
}

void Editor::restorePreviousViewport()
{
  QSettings settings;

  if (settings.contains(preferences_keys::level_name))
  {
    const std::string level_name =
      settings.value(preferences_keys::level_name).toString().toStdString();
    for (std::size_t i = 0; i < building.levels.size(); i++)
    {
      if (building.levels[i].name == level_name)
      {
        level_idx = i;
        createScene();
        level_table->setCurrentCell(i, 0);
        break;
      }
    }
  }

  double viewport_center_x =
    settings.contains(preferences_keys::viewport_center_x) ?
    settings.value(preferences_keys::viewport_center_x).toDouble() :
    0.0;

  double viewport_center_y =
    settings.contains(preferences_keys::viewport_center_y) ?
    settings.value(preferences_keys::viewport_center_y).toDouble() :
    0.0;

  double viewport_scale =
    settings.contains(preferences_keys::viewport_scale) ?
    settings.value(preferences_keys::viewport_scale).toDouble() :
    1.0;

  // sanity-check the viewport center and scale, since they can be garbage
  // if the last loaded level doesn't have its scale set up.
  if (isnan(viewport_center_x))
    viewport_center_x = 0.0;
  if (isnan(viewport_center_y))
    viewport_center_y = 0.0;
  if (isnan(viewport_scale))
    viewport_scale = 1.0;

  // See if the previous building name matches the current building name
  // if they mismatch, don't attempt to use the previous viewport!
  // This is a really big deal if we're mixing coordinate systems :boom:
  const std::string previous_filename =
    settings.value(preferences_keys::previous_building_path)
    .toString().toStdString();

  printf("previous filename: %s\n", previous_filename.c_str());
  printf("current building filename: %s\n", building.getFilename().c_str());

  if (previous_filename == building.getFilename())
  {
    printf("restoring viewport: (%.1f, %.1f, %3f)\n",
      viewport_center_x,
      viewport_center_y,
      viewport_scale);

    const double y_flip = building.coordinate_system.isYFlipped() ? 1 : -1;
    QTransform t;
    t.scale(viewport_scale, y_flip * viewport_scale);
    map_view->setTransform(t);

    map_view->centerOn(QPointF(viewport_center_x, viewport_center_y));
    map_view->drawTiles();
  }
  else
  {
    printf("resetting view...\n");
    zoomReset();
  }
}

bool Editor::eventFilter(QObject* obj, QEvent* event)
{
  if (obj == property_editor->viewport() &&
    event->type() == QEvent::MouseButtonPress)
  {
    QMouseEvent* me = static_cast<QMouseEvent*>(event);
    QModelIndex idx = property_editor->indexAt(me->pos());
    if (!idx.isValid())
    {
      // clicking the empty space would clear the selection
      property_editor->clearSelection();
      property_editor->setCurrentIndex(QModelIndex());
      delete_param_button->setEnabled(false);
    }
  }
  return QMainWindow::eventFilter(obj, event);
}

bool Editor::loadPreviousBuilding()
{
  QSettings settings;
  const QString filename(
    settings.value(preferences_keys::previous_building_path).toString());
  if (!filename.isEmpty())
    return loadBuilding(filename);
  return true;
}

void Editor::buildingNew()
{
  QFileDialog file_dialog(this, "New Building");
  file_dialog.setNameFilter("Building YAML (*.yaml *.yml)");
  file_dialog.setDefaultSuffix("building.yaml");
  file_dialog.setAcceptMode(QFileDialog::AcceptMode::AcceptSave);
  file_dialog.setConfirmOverwrite(true);

  if (file_dialog.exec() != QDialog::Accepted)
    return;

  QFileInfo file_info(file_dialog.selectedFiles().first());
  std::string fn = file_info.fileName().toStdString();

  // Guess the building name as the "filename before the dot".
  // User can override this guess in the dialog that opens afterwards.
  std::string guessed_building_name(fn.begin(), fn.begin() + fn.find('.'));

  QDialog new_building_dialog;
  Ui::NewBuildingDialog new_building_dialog_ui;
  new_building_dialog_ui.setupUi(&new_building_dialog);
  new_building_dialog_ui.name_line_edit->setText(
    QString::fromStdString(guessed_building_name));

  if (new_building_dialog.exec() != QDialog::Accepted)
    return;

  building.clear();
  if (new_building_dialog_ui.geolocated_radio->isChecked())
    building.coordinate_system.value = CoordinateSystem::Value::WGS84;
  else
    building.coordinate_system.value = CoordinateSystem::Value::ReferenceImage;

  if (!building.setFilename(file_info.absoluteFilePath().toStdString()))
    return;
  QString dir_path = file_info.dir().path();
  QDir::setCurrent(dir_path);

  createScene();
  buildingSave();
  zoomReset();
  updateTables();

  QSettings settings;
  settings.setValue(
    preferences_keys::previous_building_path,
    QString::fromStdString(building.getFilename()));
}

void Editor::buildingOpen()
{
  QFileDialog file_dialog(this, "Open Building");
  file_dialog.setFileMode(QFileDialog::ExistingFile);
  file_dialog.setNameFilter(
    "Building YAML (*.yaml *.yml)");

  if (file_dialog.exec() != QDialog::Accepted)
    return;

  QFileInfo file_info(file_dialog.selectedFiles().first());
  if (!file_info.exists())
  {
    QMessageBox::critical(
      this,
      "File does not exist",
      "File does not exist. Cannot open file.");
    return;
  }
  try
  {
    if (loadBuilding(file_info.filePath()))
      return;
  }
  catch (const std::exception& error)
  {
    qWarning("Unable to load building: %s", error.what());
  }
  QMessageBox::critical(this, "Unable to open building",
    "Select a valid building YAML map containing levels. Navigation JSON cannot be opened as a building.");
}

bool Editor::buildingSave()
{
  if (!building.save())
  {
    QMessageBox::critical(
      this,
      "Unable to save",
      "Save failed! Maybe a bad path?");
    return false;
  }
  QSettings settings;
  settings.setValue(preferences_keys::previous_building_path,
    QString::fromStdString(building.getFilename()));
  statusBar()->showMessage("Saved " +
    QString::fromStdString(building.getFilename()), 5000);
  setWindowModified(false);
  return true;
}

bool Editor::buildingExportFeatures()
{
  QFileDialog dialog(this, "Export layer alignment points for level");
  dialog.setNameFilter("*.yaml");
  dialog.setDefaultSuffix(".yaml");
  dialog.setAcceptMode(QFileDialog::AcceptMode::AcceptSave);
  dialog.setConfirmOverwrite(true);

  if (dialog.exec() != QDialog::Accepted)
    return true;

  QFileInfo file_info(dialog.selectedFiles().first());
  auto result = building.exportFeatures(
    level_idx,
    file_info.absoluteFilePath().toStdString());
  setWindowModified(false);
  return result;
}

void Editor::helpAbout()
{
  QMessageBox::about(this, "About", "Welcome to the Traffic Editor");
}

void Editor::editUndo()
{
  if (latest_move_lift)
  {
    clearCurrentToolBuffer();
    return;
  }
  undo_stack.undo();
  if (
    tool_id == TOOL_ADD_LANE
    || tool_id == TOOL_ADD_WALL)
  {
    clicked_idx = -1;
    prev_clicked_idx = -1;
  }
  createScene();
  updatePropertyEditor();
  setWindowModified(true);
}

void Editor::editRedo()
{
  if (latest_move_lift)
    clearCurrentToolBuffer();
  undo_stack.redo();
  createScene();
  setWindowModified(true);
}

void Editor::editPreferences()
{
  PreferencesDialog preferences_dialog(this);

  if (preferences_dialog.exec() == QDialog::Accepted)
    loadModelNames();
}

void Editor::editBuildingProperties()
{
  BuildingDialog building_dialog(building);
  if (building_dialog.exec() == QDialog::Accepted)
    setWindowModified(true);
}

void Editor::editRotateAllModels()
{
  QDialog dialog;
  Ui::TransformDialog dialog_ui;
  dialog_ui.setupUi(&dialog);
  if (dialog.exec() != QDialog::Accepted)
    return;

  const double rotation =
    dialog_ui.rotate_all_models_line_edit->text().toDouble();
  building.rotateAllModels(rotation);
  createScene();
  setWindowModified(true);
}

void Editor::editOptimizeLayerTransforms()
{
  printf("Editor::edit_optimize_layer_transforms()\n");
  if (level_idx < static_cast<int>(building.levels.size()))
    building.levels[level_idx].optimizeLayerTransforms();
  createScene();
}

void Editor::editAlignColinear()
{
  printf("Editor::edit_align_colinear()\n");
  Level* level = activeLevel();
  if (!level)
    return;
  level->alignColinear();
  createScene();
}

void Editor::viewModels()
{
  rendering_options.show_models = view_models_action->isChecked();
  createScene();
}

void Editor::viewTiles()
{
  map_view->setShowTiles(view_tiles_action->isChecked());
  createScene();
}

void Editor::zoomReset()
{
  map_view->zoomFit(level_idx);
}

void Editor::mouseEvent(const MouseType t, QMouseEvent* e)
{
  QPointF p;
  if (!isMouseEventInMap(e, p))
  {
    if (latest_move_lift)
    {
      const QPoint local = map_view->mapFromGlobal(mapToGlobal(e->pos()));
      mouseMove(t, e, map_view->mapToScene(local));
      return;
    }
    e->ignore();
    return;
  }
  if (level_idx >= static_cast<int>(building.levels.size()))
  {
    if (t == MOUSE_RELEASE)
    {
      if (building.getFilename().empty())
      {
        QMessageBox::critical(
          this,
          "No building",
          "Please try File->New Building... or File->Open Building...");
      }
      else if (building.levels.empty())
      {
        QMessageBox::critical(
          this,
          "No levels defined",
          "No levels defined. Use Level->Add...");
      }
    }
    return;
  }
  // dispatch to individual mouse handler functions to save indenting...
  switch (tool_id)
  {
    case TOOL_SELECT:       mouseSelect(t, e, p); break;
    case TOOL_ADD_VERTEX:   mouseAddVertex(t, e, p); break;
    case TOOL_MOVE:         mouseMove(t, e, p); break;
    case TOOL_ADD_LANE:     mouseAddLane(t, e, p); break;
    case TOOL_ADD_WALL:     mouseAddWall(t, e, p); break;
    case TOOL_ADD_MEAS:     mouseAddMeas(t, e, p); break;
    case TOOL_ADD_DOOR:     mouseAddDoor(t, e, p); break;
    case TOOL_ADD_MODEL:    mouseAddModel(t, e, p); break;
    case TOOL_ROTATE:       mouseRotate(t, e, p); break;
    case TOOL_ADD_FLOOR:    mouseAddFloor(t, e, p); break;
    case TOOL_ADD_HOLE:     mouseAddHole(t, e, p); break;
    case TOOL_EDIT_POLYGON: mouseEditPolygon(t, e, p); break;
    case TOOL_ADD_FEATURE:  mouseAddFeature(t, e, p); break;
    case TOOL_ADD_CONSTRAINT: mouseAddConstraint(t, e, p); break;
    case TOOL_ADD_FIDUCIAL: mouseAddFiducial(t, e, p); break;
    case TOOL_ADD_ROI:      mouseAddRoi(t, e, p); break;
    case TOOL_ADD_HUMAN_LANE: mouseAddHumanLane(t, e, p); break;

    default: break;
  }
  previous_mouse_point = p;
}

void Editor::mousePressEvent(QMouseEvent* e)
{
  mouseEvent(MOUSE_PRESS, e);
}

void Editor::mouseReleaseEvent(QMouseEvent* e)
{
  mouseEvent(MOUSE_RELEASE, e);
}

void Editor::mouseMoveEvent(QMouseEvent* e)
{
  mouseEvent(MOUSE_MOVE, e);
}

bool Editor::isMouseEventInMap(QMouseEvent* e, QPointF& p_scene)
{
  const QPoint p_global = mapToGlobal(e->pos());
  const QPoint p_map = map_view->mapFromGlobal(p_global);
  if (p_map.x() < 0 || p_map.y() < 0 ||
    p_map.x() >= map_view->width() || p_map.y() >= map_view->height())
    return false;
  // This event point is valid. Now we can set p_scene.
  p_scene = map_view->mapToScene(p_map);
  return true;
}

void Editor::keyPressEvent(QKeyEvent* e)
{
  switch (e->key())
  {
    case Qt::Key_Delete:
      if (building.canDeleteCurrentSelection(level_idx))
      {
        undo_stack.push(new DeleteCommand(&building, level_idx));
        createScene();
      }
      else
      {
        // Get the selected vertex index
        int selected_vertex_idx = -1;
        for (int i = 0;
          i < static_cast<int>(building.levels[level_idx].vertices.size());
          i++)
        {
          if (building.levels[level_idx].vertices[i].selected)
          {
            selected_vertex_idx = i;
            break;
          }
        }

        if (selected_vertex_idx != -1)
        {
          DeleteDialog dialog(this, building, level_idx, selected_vertex_idx);
          if (dialog.exec() == QDialog::Accepted)
          {
            undo_stack.push(new DeleteCommand(&building, level_idx, true));
            clearCurrentToolBuffer();
            updatePropertyEditor();
            createScene();
          }
          else
          {
            building.clearSelection(level_idx);
            clearCurrentToolBuffer();
            updatePropertyEditor();
            createScene();
          }
        }
      }
      break;
    case Qt::Key_S:
    case Qt::Key_Escape:
      building.clearSelection(level_idx);
      clearCurrentToolBuffer();
      tool_button_group->button(TOOL_SELECT)->click();
      updatePropertyEditor();
      createScene();
      break;
    case Qt::Key_V:
      clearCurrentToolBuffer();
      tool_button_group->button(TOOL_ADD_VERTEX)->click();
      break;
    case Qt::Key_M:
      clearCurrentToolBuffer();
      tool_button_group->button(TOOL_MOVE)->click();
      break;
    case Qt::Key_L:
      clearCurrentToolBuffer();
      tool_button_group->button(TOOL_ADD_LANE)->click();
      break;
    case Qt::Key_W:
      tool_button_group->button(TOOL_ADD_WALL)->click();
      break;
    case Qt::Key_T:
      tool_button_group->button(TOOL_ADD_MEAS)->click();
      break;
    case Qt::Key_O:
      tool_button_group->button(TOOL_ADD_MODEL)->click();
      break;
    case Qt::Key_R:
      tool_button_group->button(TOOL_ROTATE)->click();
      break;
    case Qt::Key_F:
      tool_button_group->button(TOOL_ADD_FLOOR)->click();
      break;
    case Qt::Key_E:
      tool_button_group->button(TOOL_EDIT_POLYGON)->click();
      break;
    case Qt::Key_D:
      tool_button_group->button(TOOL_ADD_DOOR)->click();
      break;
    case Qt::Key_B:
      for (auto& edge : building.levels[level_idx].edges)
      {
        if (edge.type == Edge::LANE && edge.selected)
        {
          // toggle bidirectional flag
          edge.setParam("bidirectional",
            edge.isBidirectional() ? "false" : "true");
          createScene();
        }
      }
      break;
    case Qt::Key_0: numberKeyPressed(0); break;
    case Qt::Key_1: numberKeyPressed(1); break;
    case Qt::Key_2: numberKeyPressed(2); break;
    case Qt::Key_3: numberKeyPressed(3); break;
    case Qt::Key_4: numberKeyPressed(4); break;
    case Qt::Key_5: numberKeyPressed(5); break;
    case Qt::Key_6: numberKeyPressed(6); break;
    case Qt::Key_7: numberKeyPressed(7); break;
    case Qt::Key_8: numberKeyPressed(8); break;
    case Qt::Key_9: numberKeyPressed(9); break;
    default:
      break;
  }
}

const QString Editor::toolIdToString(const int id)
{
  switch (id)
  {
    case TOOL_SELECT: return "&select";
    case TOOL_MOVE: return "&move";
    case TOOL_ROTATE: return "&rotate";
    case TOOL_ADD_VERTEX: return "add &vertex";
    case TOOL_ADD_LANE: return "add &lane";
    case TOOL_ADD_WALL: return "add &wall";
    case TOOL_ADD_MEAS: return "add measuremen&t";
    case TOOL_ADD_DOOR: return "add door";
    case TOOL_ADD_MODEL: return "add m&odel";
    case TOOL_ADD_FLOOR: return "add &floor";
    case TOOL_ADD_HOLE: return "add hole";
    case TOOL_EDIT_POLYGON: return "&edit polygon";
    case TOOL_ADD_HUMAN_LANE: return "add human lane";
    case TOOL_ADD_FEATURE: return "add &feature";
    default: return "unknown tool ID";
  }
}

void Editor::toolToggled(int id, bool checked)
{
  if (!checked)
    return;
  if (latest_move_lift)
    clearCurrentToolBuffer();

  clicked_idx = -1;
  removeMouseMotionItem();

  tool_id = static_cast<ToolId>(id);

#if 0
  // TODO: need to improve logic to set back to "normal" cursor...
  // set the cursor
  Qt::CursorShape cursor = Qt::ArrowCursor;
  if (tool_id == TOOL_ADD_VERTEX)
    cursor = Qt::CrossCursor;
  map_view->setCursor(cursor);
#endif

  // set the status bar
  switch (tool_id)
  {
    case TOOL_SELECT:
      statusBar()->showMessage("Click an item to select it.");
      break;
    case TOOL_ADD_LANE:
    case TOOL_ADD_WALL:
    case TOOL_ADD_MEAS:
      statusBar()->showMessage(
        "Click one vertex and then another vertex to add an edge.");
      break;
    case TOOL_ADD_FLOOR:
      statusBar()->showMessage(
        "Left-click to add polygon vertices. "
        "Right-click to close polygon.");
      break;
    case TOOL_EDIT_POLYGON:
      statusBar()->showMessage(
        "Left-click drag an edge to introduce a new vertex. "
        "Right-click a polygon vertex to remove it from the polygon.");
      break;
    default:
      statusBar()->clearMessage();
      break;
  }

  // execute dialogs as needed
  if (tool_id == TOOL_ADD_MODEL)
  {
    Model model;
    ModelDialog dialog(this, model, editor_models);
    if (dialog.exec() == QDialog::Accepted)
    {
      // find the EditorModel with the requested name
      for (auto& em : editor_models)
      {
        if (em.name == model.model_name)
        {
          mouse_motion_editor_model = &em;
          const QPixmap pixmap(mouse_motion_editor_model->getPixmap());
          mouse_motion_model = scene->addPixmap(pixmap);
          mouse_motion_model->setOffset(-pixmap.width()/2, -pixmap.height()/2);
          mouse_motion_model->setScale(
            mouse_motion_editor_model->meters_per_pixel /
            building.levels[level_idx].drawing_meters_per_pixel);
          mouse_motion_model->setPos(
            previous_mouse_point.x(),
            previous_mouse_point.y());
          statusBar()->showMessage("Left-click to instantiate this model.");
          break;
        }
      }
    }
    else
      tool_button_group->button(TOOL_SELECT)->click();// back to select mode
  }
}

void Editor::updatePropertyEditor()
{
  line_style_button->setEnabled(false);
  add_param_button->setEnabled(false);
  delete_param_button->setEnabled(false);

  if (building.levels.empty())
    return;

  for (const auto& p : building.levels[level_idx].polygons)
  {
    if (p.selected)
    {
      populatePropertyEditor(p);
      return;
    }
  }

  for (const auto& e : building.levels[level_idx].edges)
  {
    if (e.selected)
    {
      populatePropertyEditor(e);
      return;  // stop after finding the first one
    }
  }

  for (const auto& m : building.levels[level_idx].models)
  {
    if (m.selected)
    {
      populatePropertyEditor(m);
      return;  // stop after finding the first one
    }
  }

  for (size_t i = 0; i < building.levels[level_idx].vertices.size(); i++)
  {
    const Vertex& v = building.levels[level_idx].vertices[i];
    if (v.selected)
    {
      populatePropertyEditor(v, i);
      return;  // stop after finding the first one
    }
  }

  for (const auto& feature : building.levels[level_idx].floorplan_features)
  {
    if (feature.selected())
    {
      populatePropertyEditor(feature);
      return;
    }
  }

  for (const auto& layer : building.levels[level_idx].layers)
  {
    for (const auto& feature : layer.features)
    {
      if (feature.selected())
      {
        populatePropertyEditor(feature);
        return;
      }
    }
  }

  for (const auto& f : building.levels[level_idx].fiducials)
  {
    if (f.selected)
    {
      populatePropertyEditor(f);
      return;  // stop after finding the first one
    }
  }

  // if we get here, we never found anything :(
  clearPropertyEditor();
}

QTableWidgetItem* Editor::createTableItem(
  const QString& str,
  bool editable)
{
  QTableWidgetItem* item = new QTableWidgetItem(str);
  if (!editable)
    item->setFlags(Qt::NoItemFlags);
  else
    item->setBackground(QBrush(Qt::white));
  return item;
}

void Editor::propertyEditorSetRow(
  const int row_idx,
  const QString& label,
  const QString& value,
  const bool editable)
{
  QTableWidgetItem* label_item = new QTableWidgetItem(label);
  label_item->setFlags(Qt::NoItemFlags);

  QTableWidgetItem* value_item = new QTableWidgetItem(value);
  if (!editable)
    value_item->setFlags(Qt::NoItemFlags);
  else
    value_item->setBackground(QBrush(Qt::white));

  property_editor->setItem(row_idx, 0, label_item);
  property_editor->setItem(row_idx, 1, value_item);
}

void Editor::propertyEditorSetRow(
  const int row_idx,
  const QString& label,
  const int& value,
  const bool editable)
{
  propertyEditorSetRow(row_idx, label, QString::number(value), editable);
}

void Editor::propertyEditorSetRow(
  const int row_idx,
  const QString& label,
  const double& value,
  const int num_decimal_places,
  const bool editable)
{
  propertyEditorSetRow(
    row_idx,
    label,
    QString::number(value, 'f', num_decimal_places + 1),
    editable);
}

void Editor::addParamButtonClicked()
{
  const string object_type =
    add_param_button->property("object_type").toString().toStdString();
  printf("add param object type: %s\n", object_type.c_str());

  if (object_type == "vertex")
  {
    AddParamDialog dialog(this, Vertex::allowed_params);
    if (dialog.exec() != QDialog::Accepted)
      return;

    AddPropertyCommand* cmd = new AddPropertyCommand(
      &building,
      dialog.getParamName(),
      Param(dialog.getParamType()),
      level_idx
    );

    undo_stack.push(cmd);
    auto updated_id = cmd->getVertexUpdated();
    populatePropertyEditor(
      building.levels[level_idx].vertices[updated_id],
      updated_id);
    setWindowModified(true);
  }
}

void Editor::deleteParamButtonClicked()
{
  const int row = property_editor->currentRow();
  if (row < 0)
    return;

  QTableWidgetItem* name_item = property_editor->item(row, 0);
  if (!name_item)
    return;

  const std::string param_name = name_item->text().toStdString();

  for (size_t i = 0; i < building.levels[level_idx].vertices.size(); i++)
  {
    Vertex& v = building.levels[level_idx].vertices[i];
    if (!v.selected)
      continue;

    auto it = v.params.find(param_name);
    if (it != v.params.end())
    {
      v.params.erase(it);
      populatePropertyEditor(v, i);
      property_editor->clearSelection(); // prevents itemSelectionChanged re-enabling it
      delete_param_button->setEnabled(false);
      setWindowModified(true);
      createScene();
      return;
    }
  }
}

void Editor::layerEditButtonClicked(const int row_idx)
{
  printf("layer row clicked: [%d]\n", row_idx);
  if (level_idx >= static_cast<int>(building.levels.size()))
    return;

  Level& level = building.levels[level_idx];

  // make sure the requested layer exists
  if (row_idx <= 0)
    return;

  if (row_idx - 1 >= static_cast<int>(level.layers.size()))
    return;

  Layer& layer = level.layers[row_idx - 1];
  LayerDialog* dialog = new LayerDialog(this, layer, true);
  dialog->show();
  dialog->raise();
  dialog->activateWindow();
  connect(
    dialog,
    &LayerDialog::redraw,
    [=]()
    {
      layer_table->update(building, level_idx, layer_idx);
      createScene();
    }
  );
  connect(
    dialog,
    &LayerDialog::centerLayer,
    [=]()
    {
      dialog->setCenter(
        map_view->getCenter().x(),
        map_view->getCenter().y());
    }
  );
}

void Editor::sanityCheck()
{
  // do some checks on the building and pop up errors if we find them
  for (std::size_t i = 0; i < building.levels.size(); i++)
  {
    if (!building.levels[i].areLayerNamesUnique())
    {
      QMessageBox::critical(
        this,
        "Duplicate layer name",
        "Layers must have unique names within a level. "
        "There is a duplicate layer name. Please correct this "
        "to avoid data loss after save/load.");
    }
  }
}

void Editor::layerAddButtonClicked()
{
  if (level_idx >= static_cast<int>(building.levels.size()))
    return;
  Level& level = building.levels[level_idx];
  Layer layer;
  LayerDialog layer_dialog(this, layer);
  if (layer_dialog.exec() != QDialog::Accepted)
    return;
  printf("added a layer: [%s]\n", layer.name.c_str());
  layer.color = Layer::defaultColor(level.layers.size());
  layer.loadImage();
  level.layers.push_back(layer);
  layer_table->update(building, level_idx, layer_idx);
  createScene();
  sanityCheck();
  setWindowModified(true);
}

void Editor::editLineStyle()
{
  if (!activeLevel())
    return;
  const auto& edges = activeLevel()->edges;
  std::vector<int> indices;
  for (std::size_t i = 0; i < edges.size(); ++i)
  {
    if (edges[i].selected && (edges[i].type == Edge::WALL ||
      edges[i].type == Edge::LANE || edges[i].type == Edge::HUMAN_LANE))
      indices.push_back(static_cast<int>(i));
  }
  if (indices.empty())
    return;

  const auto& first = edges[indices.front()];
  QColor default_color("#000080");
  if (first.type != Edge::WALL)
  {
    switch (first.getGraphIdx())
    {
      case 0: default_color = QColor("#008000"); break;
      case 1: default_color = QColor("#000080"); break;
      case 2: default_color = QColor("#008080"); break;
      case 3: default_color = QColor("#808000"); break;
      case 4: default_color = QColor("#800080"); break;
      case 5: default_color = QColor("#cc0000"); break;
      case 9: default_color = QColor("#4d4d4d"); break;
      default: default_color = QColor(Qt::black); break;
    }
  }
  QColor color = first.displayColor(default_color);
  double default_width = first.type == Edge::WALL ? 0.2 : 1.0;
  if (first.type != Edge::WALL)
  {
    for (const auto& graph : building.graphs)
    {
      if (graph.idx == first.getGraphIdx() && graph.default_lane_width > 0)
        default_width = graph.default_lane_width;
    }
    if (first.getWidth() > 0)
      default_width = first.getWidth();
  }

  QDialog dialog(this);
  dialog.setWindowTitle("Line style");
  QFormLayout layout(&dialog);
  QPushButton color_button(color.name());
  color_button.setObjectName("lineColor");
  const auto update_swatch = [&]()
    {
      QPixmap swatch(24, 24);
      swatch.fill(color);
      color_button.setIcon(QIcon(swatch));
      color_button.setText(color.name());
    };
  update_swatch();
  connect(&color_button, &QPushButton::clicked, &dialog, [&]()
    {
      const QColor chosen = QColorDialog::getColor(color, &dialog,
        "Line color");
      if (chosen.isValid())
      {
        color = chosen;
        update_swatch();
      }
    });
  QDoubleSpinBox width;
  width.setObjectName("lineWidth");
  width.setRange(0.001, 1000.0);
  width.setDecimals(3);
  width.setSingleStep(0.05);
  width.setSuffix(" m");
  width.setValue(first.displayWidth(default_width));
  QCheckBox connected("Apply to connected segments of the same type");
  connected.setObjectName("connectedSegments");
  QLabel note("Display thickness only. Route width is unchanged.\n"
    "Selected lines keep their red selection highlight.");
  note.setWordWrap(true);
  QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout.addRow("Color", &color_button);
  layout.addRow("Thickness", &width);
  layout.addRow(&connected);
  layout.addRow(&note);
  layout.addRow(&buttons);
  if (dialog.exec() != QDialog::Accepted)
    return;

  if (connected.isChecked())
  {
    for (std::size_t n = 0; n < indices.size(); ++n)
    {
      const auto& edge = edges[indices[n]];
      for (std::size_t i = 0; i < edges.size(); ++i)
      {
        if (edges[i].type == edge.type &&
          (edges[i].contains(edge.start_idx) ||
          edges[i].contains(edge.end_idx)) &&
          std::find(indices.begin(), indices.end(), static_cast<int>(i)) ==
          indices.end())
          indices.push_back(static_cast<int>(i));
      }
    }
  }
  undo_stack.push(new EdgeStyleCommand(building, level_idx,
    indices, color, width.value()));
  createScene();
  updatePropertyEditor();
  setWindowModified(true);
}

void Editor::populatePropertyEditor(const Edge& edge)
{
  line_style_button->setEnabled(edge.type == Edge::WALL ||
    edge.type == Edge::LANE || edge.type == Edge::HUMAN_LANE);
  const Level& level = building.levels[level_idx];
  const double scale = level.drawing_meters_per_pixel;
  const Vertex& sv = level.vertices[edge.start_idx];
  const Vertex& ev = level.vertices[edge.end_idx];

  const double sx = sv.x * scale;
  const double sy = sv.y * scale;
  const double ex = ev.x * scale;
  const double ey = ev.y * scale;

  const double dx = ex - sx;
  const double dy = ey - sy;
  const double len = sqrt(dx*dx + dy*dy);

  property_editor->blockSignals(true);  // otherwise we get tons of callbacks
  property_editor->setRowCount(8 + edge.params.size());

  propertyEditorSetRow(0, "edge_type", edge.typeToQstring());
  propertyEditorSetRow(1, "start_idx", edge.start_idx);
  propertyEditorSetRow(2, "end_idx", edge.end_idx);
  propertyEditorSetRow(3, "start x (m)", sx);
  propertyEditorSetRow(4, "start y (m)", sy);
  propertyEditorSetRow(5, "end x (m)", ex);
  propertyEditorSetRow(6, "end y (m)", ey);
  propertyEditorSetRow(7, "length (m)", len);

  int row = 8;
  for (const auto& param : edge.params)
  {
    propertyEditorSetRow(
      row,
      QString::fromStdString(param.first),
      param.second.toQstring(),
      true);
    row++;
  }

  property_editor->blockSignals(false);  // re-enable callbacks
}

void Editor::populatePropertyEditor(const Vertex& vertex, const int index)
{
  property_editor->blockSignals(true);  // otherwise we get tons of callbacks
  property_editor->setRowCount(6 + vertex.params.size());

  propertyEditorSetRow(0, "index", index);
  if (!building.coordinate_system.isGlobal())
  {
    propertyEditorSetRow(1, "x (pixels)", vertex.x, 3, true);
    propertyEditorSetRow(2, "y (pixels)", vertex.y, 3, true);

    const size_t ref_level_idx =
      static_cast<size_t>(building.getReferenceLevelIdx());

    if (ref_level_idx < building.levels.size())
    {
      QPointF p_ref_level;
      building.transformBetweenLevels(
        level_idx,
        QPoint(vertex.x, vertex.y),
        ref_level_idx,
        p_ref_level);

      const double y_flip = building.coordinate_system.isYFlipped() ? -1 : 1;
      const double scale =
        building.levels[ref_level_idx].drawing_meters_per_pixel;
      const QPointF p_meters(
        p_ref_level.x() * scale,
        p_ref_level.y() * scale * y_flip);
      propertyEditorSetRow(3, "x (m)", p_meters.x());
      propertyEditorSetRow(4, "y (m)", p_meters.y());
    }
    else
    {
      propertyEditorSetRow(3, "x (m)", "");
      propertyEditorSetRow(4, "y (m)", "");
    }
  }
  else
  {
    propertyEditorSetRow(1, "x (m)", vertex.x);
    propertyEditorSetRow(2, "y (m)", vertex.y);

    CoordinateSystem::WGS84Point wgs84_point =
      building.coordinate_system.toWgs84({vertex.x, vertex.y});

    propertyEditorSetRow(3, "lon (deg)", wgs84_point.lon, 6, true);
    propertyEditorSetRow(4, "lat (deg)", wgs84_point.lat, 6, true);
  }
  propertyEditorSetRow(
    5,
    "name",
    QString::fromStdString(vertex.name),
    true);

  int row = 6;
  for (const auto& param : vertex.params)
  {
    propertyEditorSetRow(
      row,
      QString::fromStdString(param.first),
      param.second.toQstring(),
      true);
    // mark this row as a deletable param so itemSelectionChanged can detect it
    property_editor->item(row, 0)->setData(Qt::UserRole, true);
    row++;
  }

  add_param_button->setEnabled(true);
  add_param_button->setProperty("object_type", QVariant("vertex"));
  delete_param_button->setProperty("object_type", QVariant("vertex"));
  // delete_param_button enable state is managed by itemSelectionChanged

  property_editor->blockSignals(false);  // re-enable callbacks
}

void Editor::populatePropertyEditor(const Feature& feature)
{
  property_editor->blockSignals(true);

  property_editor->setRowCount(1);
  propertyEditorSetRow(
    0,
    "name",
    QString::fromStdString(feature.name()),
    true);

  property_editor->blockSignals(false);
}

void Editor::populatePropertyEditor(const Fiducial& fiducial)
{
  property_editor->blockSignals(true);

  property_editor->setRowCount(1);
  propertyEditorSetRow(
    0,
    "name",
    QString::fromStdString(fiducial.name),
    true);  // true means that this cell value is editable

  property_editor->blockSignals(false);
}

void Editor::populatePropertyEditor(const Model& model)
{
  property_editor->blockSignals(true);  // otherwise we get tons of callbacks

  property_editor->setRowCount(5);

  propertyEditorSetRow(
    0,
    "name",
    QString::fromStdString(model.instance_name),
    true);

  propertyEditorSetRow(
    1,
    "model_name",
    QString::fromStdString(model.model_name));

  propertyEditorSetRow(
    2,
    "elevation",
    model.state.z,
    3,
    true);

  propertyEditorSetRow(
    3,
    "static",
    model.is_static ? QString("true") : QString("false"),
    true);

  propertyEditorSetRow(
    4,
    "dispensable",
    model.is_dispensable ? QString("true") : QString("false"),
    true);

  property_editor->blockSignals(false);  // re-enable callbacks
}

void Editor::populatePropertyEditor(const Polygon& polygon)
{
  printf("populate_property_editor(polygon)\n");
  property_editor->blockSignals(true);  // otherwise we get tons of callbacks

  property_editor->setRowCount(polygon.params.size());

  int row = 0;
  for (const auto& param : polygon.params)
  {
    propertyEditorSetRow(
      row,
      QString::fromStdString(param.first),
      param.second.toQstring(),
      true);
    row++;
  }

  property_editor->blockSignals(false);  // re-enable callbacks
}

void Editor::populatePropertyEditor(const Layer& layer)
{
  Level* level = activeLevel();
  if (level == nullptr)
    return;
  level->computeLayerTransforms();
  layer.populatePropertyEditor(property_editor);
}

void Editor::clearPropertyEditor()
{
  line_style_button->setEnabled(false);
  property_editor->setRowCount(0);
  add_param_button->setEnabled(false);
  delete_param_button->setEnabled(false);
}

void Editor::propertyEditorCellChanged(int row, int column)
{
  std::string name = property_editor->item(row, 0)->text().toStdString();
  std::string value = property_editor->item(row, 1)->text().toStdString();
  printf("property_editor_cell_changed(%d, %d) = param %s\n",
    row, column, name.c_str());

  for (auto& v : building.levels[level_idx].vertices)
  {
    if (!v.selected)
      continue;
    if (name == "name")
      v.name = value;
    else if (name == "x (pixels)")
      building.moveVertex(level_idx, &v - building.levels[level_idx].vertices.data(), stof(value), v.y);
    else if (name == "y (pixels)")
      building.moveVertex(level_idx, &v - building.levels[level_idx].vertices.data(), v.x, stof(value));
    else
      v.setParam(name, value);
    createScene();
    setWindowModified(true);
    return;  // stop after finding the first one
  }

  for (auto& e : building.levels[level_idx].edges)
  {
    if (!e.selected)
      continue;
    e.setParam(name, value);
    if (e.type == Edge::DOOR && name == "name")
    {
      auto& level = building.levels[level_idx];
      const Door door(e);
      const int front = door.frontVertex();
      const int back = door.backVertex();
      if (front >= 0 && front < static_cast<int>(level.vertices.size()))
      {
        level.vertices[front].name = value + "_front";
        level.vertices[front].params["door_name"] = Param(value);
      }
      if (back >= 0 && back < static_cast<int>(level.vertices.size()))
      {
        level.vertices[back].name = value + "_back";
        level.vertices[back].params["door_name"] = Param(value);
      }
      const auto id = Vertex::getStringParam(e.params, "editor_door_id");
      if (id)
      {
        const auto lane = level.getEditorGeneratedEdgeByKey("door/" + *id);
        if (lane)
          (*lane)->params["door_name"] = Param(value);
      }
    }
    createScene();
    setWindowModified(true);
    return;  // stop after finding the first one
  }

  for (auto& f : building.levels[level_idx].fiducials)
  {
    if (!f.selected)
      continue;
    if (name == "name")
      f.name = value;
    createScene();
    setWindowModified(true);
    return;  // stop after finding the first one
  }

  for (auto& p : building.levels[level_idx].polygons)
  {
    if (!p.selected)
      continue;
    p.setParam(name, value);
    setWindowModified(true);
    return;  // stop after finding the first one
  }

  for (auto& m : building.levels[level_idx].models)
  {
    if (!m.selected)
      continue;
    m.setParam(name, value);
    setWindowModified(true);
    return; // stop after finding the first one
  }
}

bool Editor::createScene()
{
  scene->clear();  // destroys the mouse_motion_* items if they are there
  map_view->clear();  // reset the list of currently rendered tiles
  building.clearScene();  // forget all pointers to the graphics items
  mouse_motion_line = nullptr;
  mouse_motion_model = nullptr;
  mouse_motion_ellipse = nullptr;
  mouse_motion_polygon = nullptr;

  // first draw the background map tiles (if requested)
  map_view->drawTiles();

  building.draw(scene, level_idx, editor_models, rendering_options);

  return true;
}

void Editor::drawMouseMotionLineItem(
  const double mouse_x,
  const double mouse_y)
{
  double pen_width = 1;
  QColor color;
  switch (tool_id)
  {
    case TOOL_ADD_LANE:
      pen_width = 1.0 / building.coordinate_system.defaultScale();
      color = QColor::fromRgbF(0, 0, 1, 0.5);
      break;
    case TOOL_ADD_WALL:
      pen_width = 0.25 / building.coordinate_system.defaultScale();
      color = QColor::fromRgbF(0, 0, 1, 0.5);
      break;
    case TOOL_ADD_MEAS:
      pen_width = 0.25 / building.coordinate_system.defaultScale();
      color = QColor::fromRgbF(1, 0, 1, 0.5);
      break;
    default:
      break;
  }

  QPen pen(QBrush(color), pen_width, Qt::SolidLine, Qt::RoundCap);
  const auto& start =
    building.levels[level_idx].vertices[clicked_idx];
  if (!mouse_motion_line)
    mouse_motion_line = scene->addLine(start.x, start.y, mouse_x, mouse_y, pen);
  else
    mouse_motion_line->setLine(start.x, start.y, mouse_x, mouse_y);
}

void Editor::removeMouseMotionItem()
{
  if (mouse_motion_line)
  {
    scene->removeItem(mouse_motion_line);
    delete mouse_motion_line;
    mouse_motion_line = nullptr;
  }
  if (mouse_motion_model)
  {
    scene->removeItem(mouse_motion_model);
    delete mouse_motion_model;
    mouse_motion_model = nullptr;
  }
  if (mouse_motion_ellipse)
  {
    scene->removeItem(mouse_motion_ellipse);
    delete mouse_motion_ellipse;
    mouse_motion_ellipse = nullptr;
  }
  if (mouse_motion_polygon)
  {
    scene->removeItem(mouse_motion_polygon);
    delete mouse_motion_polygon;
    mouse_motion_polygon = nullptr;
  }
  mouse_motion_editor_model = nullptr;

  mouse_vertex_idx = -1;
  mouse_fiducial_idx = -1;
  mouse_feature_idx = -1;
  mouse_feature_layer_idx = -1;
}

///////////////////////////////////////////////////////////////////////
// MOUSE HANDLERS
///////////////////////////////////////////////////////////////////////

void Editor::mouseSelect(
  const MouseType type,
  QMouseEvent* e,
  const QPointF& p)
{
  if (type != MOUSE_PRESS)
    return;
  const QPoint p_global = mapToGlobal(e->pos());
  const QPoint p_map = map_view->mapFromGlobal(p_global);
  QGraphicsItem* item = map_view->itemAt(p_map);

  building.levels[level_idx].mouseSelectPress(
    p.x(),
    p.y(),
    item,
    rendering_options,
    e->modifiers());

  // todo: figure out something smarter than this abomination
  selected_polygon = building.getSelectedPolygon(level_idx);

  // todo: be smarter and go find the actual GraphicsItem to avoid
  // a full repaint here?
  createScene();
  updatePropertyEditor();
}

void Editor::mouseAddVertex(
  const MouseType t,
  QMouseEvent*,
  const QPointF& p)
{
  if (t == MOUSE_PRESS)
  {
    undo_stack.push(
      new AddVertexCommand(
        &building,
        level_idx,
        p.x(),
        p.y()));
    setWindowModified(true);
    createScene();
  }
}

void Editor::mouseAddFeature(
  const MouseType t,
  QMouseEvent*,
  const QPointF& p)
{
  if (t == MOUSE_PRESS)
  {
    undo_stack.push(
      new AddFeatureCommand(
        &building,
        level_idx,
        layer_idx,
        p.x(),
        p.y()));
    setWindowModified(true);
    createScene();
  }
}

void Editor::mouseAddFiducial(
  const MouseType t,
  QMouseEvent*,
  const QPointF& p)
{
  if (t == MOUSE_PRESS)
  {
    AddFiducialCommand* command = new AddFiducialCommand(
      &building,
      level_idx,
      p.x(),
      p.y());
    undo_stack.push(command);
    setWindowModified(true);
    createScene();
  }
}

void Editor::mouseMove(
  const MouseType t,
  QMouseEvent* e,
  const QPointF& p)
{
  if (latest_move_lift)
  {
    if ((t == MOUSE_MOVE    && (e->buttons() & Qt::LeftButton)) ||
        (t == MOUSE_RELEASE &&  e->button() == Qt::LeftButton))
    {
      const Eigen::Vector2d destination = lift_scene_to_reference * Eigen::Vector2d(p.x(), p.y()) + lift_drag_offset;
      
      latest_move_lift->setFinalDestination(destination.x(), destination.y());
      latest_move_lift->redo(); // preview; the original snapshot stays unchanged
      
      if (t == MOUSE_RELEASE)
      {
        if (latest_move_lift->has_moved_)
        {
          undo_stack.push(latest_move_lift);
          setWindowModified(true);
        }
        else
        {
          latest_move_lift->undo();
          delete latest_move_lift;
        }
        latest_move_lift = nullptr;
        updatePropertyEditor();
      }
      createScene();
    }
    return;
  }
  
  if (t == MOUSE_PRESS && e->button() != Qt::LeftButton)
    return;

  if (t == MOUSE_PRESS)
  {
    Level::NearestItem ni = building.levels[level_idx].nearestItems(p.x(), p.y());

    printf(
      "mouse press (%.3f, %.3f) feature_dist = %.3f, feature_idx = %d, feature_layer_idx = %d\n",
      p.x(),
      p.y(),
      ni.feature_dist,
      ni.feature_idx,
      ni.feature_layer_idx);

    // todo: use QGraphics stuff to see if we clicked a model pixmap...
    const double model_dist_thresh = 0.5 /
      building.levels[level_idx].drawing_meters_per_pixel;

    if (ni.model_idx >= 0 && ni.model_dist < model_dist_thresh)
    {
      // Now we need to find the pixmap item for this model.
      const Model& model =
        building.levels[level_idx].models[ni.model_idx];
      mouse_motion_model = getClosestPixmapItem(
        QPointF(model.state.x, model.state.y));
      mouse_model_idx = ni.model_idx;
      latest_move_model = new MoveModelCommand(
        &building,
        level_idx,
        mouse_model_idx);
    }
    else if (ni.vertex_idx >= 0 && ni.vertex_dist < 10.0)
    {
      mouse_vertex_idx = ni.vertex_idx;

      latest_move_vertex = new MoveVertexCommand(
        &building,
        level_idx,
        mouse_vertex_idx);
    }
    else if (ni.feature_idx >= 0 &&
      ni.feature_layer_idx >= 0 &&
      ni.feature_dist < 10.0)
    {
      mouse_feature_idx = ni.feature_idx;
      mouse_feature_layer_idx = ni.feature_layer_idx;

      latest_move_feature = new MoveFeatureCommand(
        &building,
        level_idx,
        ni.feature_layer_idx,
        ni.feature_idx);
    }
    else if (ni.fiducial_idx >= 0 && ni.fiducial_dist < 10.0)
    {
      mouse_fiducial_idx = ni.fiducial_idx;

      latest_move_fiducial = new MoveFiducialCommand(
        &building,
        level_idx,
        mouse_fiducial_idx);
    }
    else
    {
      const auto& level = building.levels[level_idx];

      if (level.drawing_meters_per_pixel <= 0.0)
        return;
      
      for (const auto& lift : building.lifts)
      {
        if (level.elevation < lift.lowest_elevation ||
            level.elevation > lift.highest_elevation)
          continue;
        
        Building::Transform t;
        
        for (std::size_t i = 0; i < building.levels.size(); ++i)
        {
          if (building.levels[i].name == lift.reference_floor_name)
            {
              t = building.getTransform(i, level_idx);
              break;
            }
        }

        if (t.scale <= 0.0)
          continue;
        
        const Eigen::Vector2d point(p.x(), p.y());
        
        if (!lift.nearestItemIndexIfWithinDistance(
            point, 0.1 / level.drawing_meters_per_pixel,
            level.drawing_meters_per_pixel,
            utils::geometry::scenePose(lift, t.scale, t.dx, t.dy, t.rotation)))
          continue;

        Eigen::Affine2d reference_to_scene = Eigen::Affine2d::Identity();
        reference_to_scene.translate(Eigen::Vector2d(t.dx, t.dy));
        reference_to_scene.rotate(Eigen::Rotation2Dd(-t.rotation));
        reference_to_scene.scale(t.scale);
        lift_scene_to_reference = reference_to_scene.inverse();
        lift_drag_offset = lift.pose.translation() - lift_scene_to_reference * point;
         
        latest_move_lift = new MoveLiftCommand(&building, lift.name);
        
        break;
      }
    }
  }
  else if (t == MOUSE_RELEASE)
  {
    if (mouse_vertex_idx >= 0) //Add mouse move vertex.
    {
      if (latest_move_vertex->has_moved)
      {
        undo_stack.push(latest_move_vertex);
      }
      else
      {
        delete latest_move_vertex;
        latest_move_vertex = NULL;
      }
    }

    if (mouse_model_idx >= 0) //Add mouse move model
    {
      if (latest_move_model->has_moved)
      {
        undo_stack.push(latest_move_model);
      }
      else
      {
        delete latest_move_model;
        latest_move_model = NULL;
      }
    }

    if (mouse_feature_idx >= 0) //Add mouse move feature
    {
      if (latest_move_feature->has_moved)
      {
        undo_stack.push(latest_move_feature);
      }
      else
      {
        delete latest_move_feature;
        latest_move_feature = NULL;
      }
    }

    if (mouse_fiducial_idx >= 0) //Add mouse move fiducial
    {
      if (latest_move_fiducial->has_moved)
      {
        undo_stack.push(latest_move_fiducial);
      }
      else
      {
        delete latest_move_fiducial;
        latest_move_fiducial = NULL;
      }
    }
    mouse_vertex_idx = -1;
    mouse_feature_idx = -1;
    mouse_feature_layer_idx = -1;
    mouse_fiducial_idx = -1;
    createScene();  // this will free mouse_motion_model
    setWindowModified(true);
  }
  else if (t == MOUSE_MOVE)
  {
    if (!(e->buttons() & Qt::LeftButton))
      return;// we only care about mouse-dragging, not just motion
    /*
    printf(
      "mouse move, vertex_idx = %d, "
      "feature_idx = %d, feature_layer_idx = %d, "
      "fiducial_idx = %d\n",
      mouse_vertex_idx,
      mouse_feature_idx,
      mouse_feature_layer_idx,
      mouse_fiducial_idx);
    */
    if (mouse_motion_model != nullptr)
    {
      // we're dragging a model
      // update both the nav_model data and the pixmap in the scene
      Model& model =
        building.levels[level_idx].models[mouse_model_idx];
      model.state.x = p.x();
      model.state.y = p.y();
      mouse_motion_model->setPos(p);
      latest_move_model->setFinalDestination(p.x(), p.y());
    }
    else if (mouse_vertex_idx >= 0)
    {
      if (!building.levels[level_idx].vertices[mouse_vertex_idx].liftCabin().empty())
      {
        auto lift_name = building.levels[level_idx].vertices[mouse_vertex_idx].liftCabin();
        auto lift = building.getLift(lift_name);
        auto lift_ref_level = lift.reference_floor_name;
        if (lift_ref_level != building.levels[level_idx].name)
        {
          QMessageBox::critical(
            this,
            "Could not move vertex",
            "Vertex is not on the reference level.");
          return;
        }
      }

      // we're dragging a vertex
      Vertex& pt = building.levels[level_idx].vertices[mouse_vertex_idx];
      building.moveVertex(level_idx, mouse_vertex_idx, p.x(), p.y());

      // update the other levels if a lift cabin vertex is moved
      if (!building.levels[level_idx].vertices[mouse_vertex_idx].liftCabin().empty())
      {
        for (size_t i = 0; i < building.levels.size(); i++)
        {
          if (i == static_cast<size_t>(level_idx))
            continue;

          auto& level = building.levels[i];
          auto& vertices = level.vertices;
          auto t = building.getTransform(level_idx, i);

          for (auto& vertex : level.vertices)
          {
            if (vertex.liftCabin() == pt.liftCabin())
            {
              auto rotated_x = std::cos(-t.rotation) * p.x() - std::sin(-t.rotation) * p.y();
              auto rotated_y = std::sin(-t.rotation) * p.x() + std::cos(-t.rotation) * p.y();
              vertex.x = rotated_x * t.scale + t.dx;
              vertex.y = rotated_y * t.scale + t.dy;
            }
          }
        }
      }

      latest_move_vertex->setFinalDestination(p.x(), p.y());
      createScene();
    }
    else if (mouse_feature_idx >= 0 && mouse_feature_layer_idx >= 0)
    {
      Level& level = building.levels[level_idx];
      Feature* feature = nullptr;

      QPointF q(p);  // transform if necessary to layer coordinates
      if (mouse_feature_layer_idx == 0)
        feature = &level.floorplan_features[mouse_feature_idx];
      else
      {
        Layer& layer = level.layers[mouse_feature_layer_idx - 1];
        const double mpp = level.drawing_meters_per_pixel;
        const QPointF p_meters(p.x() * mpp, p.y() * mpp);
        q = layer.transform.backwards(p_meters);
        feature = &layer.features[mouse_feature_idx];
      }

      feature->setX(q.x());
      feature->setY(q.y());
      latest_move_feature->setFinalDestination(q.x(), q.y());

      printf("moved feature %d on layer %d to (%.1f, %.1f)\n",
        mouse_feature_idx,
        mouse_feature_layer_idx,
        feature->x(),
        feature->y());
      createScene();
    }
    else if (mouse_fiducial_idx >= 0)
    {
      Fiducial& f =
        building.levels[level_idx].fiducials[mouse_fiducial_idx];
      f.x = p.x();
      f.y = p.y();
      latest_move_fiducial->setFinalDestination(p.x(), p.y());
      printf("moved fiducial %d to (%.1f, %.1f)\n",
        mouse_fiducial_idx,
        f.x,
        f.y);
      createScene();
    }
  }
}

void Editor::mouseAddEdge(
  const MouseType t,
  QMouseEvent* e,
  const QPointF& p,
  const Edge::Type& edge_type)
{
  QPointF p_aligned(p);

  if (clicked_idx >= 0 && e->modifiers() & Qt::ShiftModifier)
  {
    const auto& start = building.levels[level_idx].vertices[clicked_idx];
    alignPoint(QPointF(start.x, start.y), p_aligned);
  }

  if (t == MOUSE_PRESS)
  {
    if (e->buttons() & Qt::RightButton)
    {
      // right button means "exit edge drawing mode please"
      clicked_idx = -1;
      prev_clicked_idx = -1;
      if (latest_add_edge != NULL)
      {
        //Need to check if new vertex was added.
        delete latest_add_edge;
        latest_add_edge = NULL;
      }
      removeMouseMotionItem();
      return;
    }

    if (prev_clicked_idx < 0)
    {
      latest_add_edge = new AddEdgeCommand(&building, level_idx, rendering_options);

      clicked_idx = latest_add_edge->setFirstPoint(p_aligned.x(), p_aligned.y());
      
      latest_add_edge->setEdgeType(edge_type);
      
      prev_clicked_idx = clicked_idx;
      createScene();
      setWindowModified(true);
      return; // no previous vertex click happened; nothing else to do
    }

    clicked_idx =
      latest_add_edge->setSecondPoint(p_aligned.x(), p_aligned.y());

    if (clicked_idx == prev_clicked_idx)  // don't create self edge loops
    {
      removeMouseMotionItem();
      return;
    }
    undo_stack.push(latest_add_edge);

    if (edge_type == Edge::DOOR || edge_type == Edge::MEAS)
    {
      clicked_idx = -1;  // doors and measurements don't usually chain
      latest_add_edge = NULL;
      removeMouseMotionItem();
    }
    else
    {
      latest_add_edge = new AddEdgeCommand(
        &building,
        level_idx,
        rendering_options);
      latest_add_edge->setFirstPoint(p_aligned.x(), p_aligned.y());
      latest_add_edge->setEdgeType(edge_type);
    }
    prev_clicked_idx = clicked_idx;
    createScene();
    setWindowModified(true);
  }
  else if (t == MOUSE_MOVE)
  {
    if (clicked_idx < 0)
      return;

    drawMouseMotionLineItem(p_aligned.x(), p_aligned.y());
  }
}

void Editor::mouseAddConstraint(
  const MouseType t,
  QMouseEvent* /*e*/,
  const QPointF& p)
{
  const Level* level = activeLevel();
  if (!level)
    return;

  if (t == MOUSE_PRESS)
  {
    // look up the feature nearest this click
    const Feature* f = level->findFeature(p.x(), p.y());
    if (!f)
    {
      printf("no feature near (%.3f, %.3f)\n", p.x(), p.y());
      clicked_feature_id = QUuid();
      removeMouseMotionItem();
      return;
    }

    printf("found feature %s\n", f->id().toString().toStdString().c_str());

    if (!clicked_feature_id.isNull())
    {
      // create an edge between this feature and the previously clicked one
      printf("creating constraint between %s and %s\n",
        clicked_feature_id.toString().toStdString().c_str(),
        f->id().toString().toStdString().c_str());
      AddConstraintCommand* command = new AddConstraintCommand(
        &building,
        level_idx,
        clicked_feature_id,
        f->id());
      undo_stack.push(command);

      clicked_feature_id = QUuid();
      setWindowModified(true);
      createScene();
    }
    else
    {
      clicked_feature_id = f->id();
    }
  }
  else if (t == MOUSE_MOVE)
  {
    if (clicked_feature_id.isNull())
      return;

    QPointF feature_point;
    level->getFeaturePoint(clicked_feature_id, feature_point);

    QPen pen(
      QBrush(QColor::fromRgbF(0.8, 0.8, 0.4, 0.8)),
      0.1 / level->drawing_meters_per_pixel,
      Qt::SolidLine,
      Qt::RoundCap);

    if (!mouse_motion_line)
    {
      mouse_motion_line = scene->addLine(
        QLineF(feature_point, p),
        pen);
      mouse_motion_line->setZValue(300);
    }
    else
      mouse_motion_line->setLine(QLineF(feature_point, p));
  }
}


void Editor::mouseAddLane(
  const MouseType t, QMouseEvent* e, const QPointF& p)
{
  mouseAddEdge(t, e, p, Edge::LANE);
}

void Editor::mouseAddWall(
  const MouseType t, QMouseEvent* e, const QPointF& p)
{
  mouseAddEdge(t, e, p, Edge::WALL);
}

void Editor::mouseAddMeas(
  const MouseType t, QMouseEvent* e, const QPointF& p)
{
  mouseAddEdge(t, e, p, Edge::MEAS);
}

void Editor::mouseAddDoor(
  const MouseType t, QMouseEvent* e, const QPointF& p)
{
  mouseAddEdge(t, e, p, Edge::DOOR);
}

void Editor::mouseAddHumanLane(
  const MouseType t, QMouseEvent* e, const QPointF& p)
{
  mouseAddEdge(t, e, p, Edge::HUMAN_LANE);
}

void Editor::mouseAddModel(
  const MouseType t, QMouseEvent*, const QPointF& p)
{
  if (t == MOUSE_PRESS)
  {
    if (mouse_motion_editor_model == nullptr)
      return;

    AddModelCommand* cmd = new AddModelCommand(
      &building,
      level_idx,
      p.x(),
      p.y(),
      mouse_motion_editor_model->name
    );
    undo_stack.push(cmd);
    setWindowModified(true);
    createScene();
  }
  else if (t == MOUSE_MOVE)
  {
    if (mouse_motion_editor_model == nullptr)
      return;// nothing currently selected. nothing to do.
    if (mouse_motion_model == nullptr)
    {
      const QPixmap pixmap(mouse_motion_editor_model->getPixmap());
      mouse_motion_model = scene->addPixmap(pixmap);
      mouse_motion_model->setOffset(-pixmap.width()/2, -pixmap.height()/2);
      mouse_motion_model->setScale(
        mouse_motion_editor_model->meters_per_pixel /
        building.levels[level_idx].drawing_meters_per_pixel);
    }
    mouse_motion_model->setPos(p.x(), p.y());
  }
}

double Editor::discretizeAngle(const double& angle)
{
  const double discretization = 45.0 * M_PI / 180.0;
  return discretization * round(angle / discretization);
}

void Editor::alignPoint(const QPointF& start, QPointF& end)
{
  if (qAbs(start.x() - end.x()) < qAbs(start.y() - end.y()))
    end.setX(start.x());
  else
    end.setY(start.y());
}

void Editor::mouseRotate(
  const MouseType t, QMouseEvent* mouse_event, const QPointF& p)
{
  if (t == MOUSE_PRESS)
  {
    clicked_idx = building.nearestItemIndexIfWithinDistance(
      level_idx,
      p.x(),
      p.y(),
      50.0,
      Level::MODEL);
    if (clicked_idx < 0)
      return;// nothing to do. click wasn't on a model.

    latest_rotate_model = new RotateModelCommand(
      &building,
      level_idx,
      clicked_idx);
    const Model& model = building.levels[level_idx].models[clicked_idx];
    mouse_motion_model = getClosestPixmapItem(
      QPointF(model.state.x, model.state.y));
    QPen pen(Qt::red);
    pen.setWidth(4);
    const double r = static_cast<double>(rotation_indicator_radius);
    mouse_motion_ellipse = scene->addEllipse(
      model.state.x - r,  // ellipse upper-left column
      model.state.y - r,  // ellipse upper-left row
      2 * r,  // ellipse width
      2 * r,  // ellipse height
      pen);
    mouse_motion_line = scene->addLine(
      model.state.x,
      model.state.y,
      model.state.x + r * cos(model.state.yaw),
      model.state.y - r * sin(model.state.yaw),
      pen);
  }
  else if (t == MOUSE_RELEASE)
  {
    //remove_mouse_motion_item();
    if (clicked_idx < 0)
      return;
    const Model& model = building.levels[level_idx].models[clicked_idx];
    const double dx = p.x() - model.state.x;
    const double dy = -(p.y() - model.state.y);  // vertical axis is flipped
    double mouse_yaw = atan2(dy, dx);
    if (mouse_event->modifiers() & Qt::ShiftModifier)
      mouse_yaw = discretizeAngle(mouse_yaw);
    latest_rotate_model->setFinalDestination(mouse_yaw);
    undo_stack.push(latest_rotate_model);
    clicked_idx = -1;  // we're done rotating it now
    setWindowModified(true);
    // now re-render the whole scene (could optimize in the future...)
    createScene();
  }
  else if (t == MOUSE_MOVE)
  {
    if (clicked_idx < 0)
      return;// nothing currently selected. nothing to do.

    // re-orient the mouse_motion_model item and heading indicator as needed
    const Model& model = building.levels[level_idx].models[clicked_idx];
    const double dx = p.x() - model.state.x;
    const double dy = -(p.y() - model.state.y);  // vertical axis is flipped
    double mouse_yaw = atan2(dy, dx);
    if (mouse_event->modifiers() & Qt::ShiftModifier)
      mouse_yaw = discretizeAngle(mouse_yaw);
    const double r = static_cast<double>(rotation_indicator_radius);
    mouse_motion_line->setLine(
      model.state.x,
      model.state.y,
      model.state.x + r * cos(mouse_yaw),
      model.state.y - r * sin(mouse_yaw));

    if (mouse_motion_model)
      mouse_motion_model->setRotation(
        (-mouse_yaw + M_PI / 2.0) * 180.0 / M_PI);
  }
}

QGraphicsPixmapItem* Editor::getClosestPixmapItem(const QPointF& p)
{
  // todo: use fancier calls if the scene graph gets so big that a linear
  // search becomes intolerably slow
  const QList<QGraphicsItem*> items = scene->items();
  QGraphicsPixmapItem* pixmap_item = nullptr;
  double min_dist = 1.0e9;
  for (const auto item : items)
  {
    if (item->type() != QGraphicsPixmapItem::Type)
      continue;// ignore anything other than the pixmaps (models)
    const double model_click_distance = QLineF(p, item->pos()).length();
    const double width = item->boundingRect().width();
    const double height = item->boundingRect().height();
    printf("model_click_distance = %.2f bounds = (%.1f, %.1f)\n",
      model_click_distance, width, height);
    if (model_click_distance < min_dist)
    {
      min_dist = model_click_distance;
      pixmap_item = qgraphicsitem_cast<QGraphicsPixmapItem*>(item);
    }
  }
  return pixmap_item;
}

void Editor::mouseAddPolygon(
  const MouseType t,
  QMouseEvent* e,
  const QPointF& p,
  const Polygon::Type& polygon_type)
{
  if (t == MOUSE_PRESS)
  {
    if (e->buttons() & Qt::LeftButton)
    {
      Level* level = activeLevel();
      if (level == nullptr)
        return;

      const Level::NearestItem ni = level->nearestItems(p.x(), p.y());
      clicked_idx = ni.vertex_dist < 10.0 ? ni.vertex_idx : -1;
      if (clicked_idx < 0)
        return;// nothing to do. click wasn't on a vertex.

      Vertex* v = &building.levels[level_idx].vertices[clicked_idx];
      v->selected = true;  // todo: colorize it?

      if (mouse_motion_polygon == nullptr)
      {
        QPen pen(Qt::black);
        pen.setWidthF(0.05 / level->drawing_meters_per_pixel);
        QVector<QPointF> polygon_vertices;
        polygon_vertices.append(QPointF(v->x, v->y));
        QPolygonF polygon(polygon_vertices);
        mouse_motion_polygon = scene->addPolygon(
          polygon,
          pen,
          QBrush(QColor::fromRgbF(1.0, 0.0, 0.0, 0.5)));
        mouse_motion_polygon_vertices.clear();
      }

      // only add vertex_idx is NOT already in the vertex list
      if (std::find(
          mouse_motion_polygon_vertices.begin(),
          mouse_motion_polygon_vertices.end(),
          clicked_idx) == mouse_motion_polygon_vertices.end())
        mouse_motion_polygon_vertices.push_back(clicked_idx);
    }
    else if (e->buttons() & Qt::RightButton)
    {
      if (mouse_motion_polygon == nullptr)
        return;
      if (mouse_motion_polygon_vertices.size() >= 3)
      {
        Polygon polygon;
        polygon.type = polygon_type;
        polygon.createRequiredParameters();
        for (const auto& i : mouse_motion_polygon_vertices)
        {
          polygon.vertices.push_back(i);
        }

        AddPolygonCommand* command = new AddPolygonCommand(
          &building,
          polygon,
          level_idx);

        undo_stack.push(command);
      }
      scene->removeItem(mouse_motion_polygon);
      delete mouse_motion_polygon;
      mouse_motion_polygon = nullptr;

      setWindowModified(true);
      building.clearSelection(level_idx);
      createScene();
    }
  }
  else if (t == MOUSE_MOVE)
  {
    if (mouse_motion_polygon == nullptr)
      return;

    // first, remove the previous polygon
    scene->removeItem(mouse_motion_polygon);
    delete mouse_motion_polygon;

    // now, make the updated polygon
    QVector<QPointF> polygon_vertices;
    for (const auto& vertex_idx: mouse_motion_polygon_vertices)
    {
      const Vertex* v = &building.levels[level_idx].vertices[vertex_idx];
      polygon_vertices.append(QPointF(v->x, v->y));
    }
    polygon_vertices.append(QPointF(p.x(), p.y()));

    QPen pen(Qt::black);
    pen.setWidthF(0.05 / activeLevel()->drawing_meters_per_pixel);

    // insert the updated polygon into the scene
    QPolygonF polygon(polygon_vertices);
    mouse_motion_polygon = scene->addPolygon(
      polygon,
      pen,
      QBrush(QColor::fromRgbF(1.0, 0.0, 0.0, 0.5)));
  }
}

void Editor::mouseAddFloor(
  const MouseType t, QMouseEvent* e, const QPointF& p)
{
  mouseAddPolygon(t, e, p, Polygon::FLOOR);
}

void Editor::mouseAddHole(
  const MouseType t, QMouseEvent* e, const QPointF& p)
{
  mouseAddPolygon(t, e, p, Polygon::HOLE);
}

void Editor::mouseAddRoi(
  const MouseType t, QMouseEvent* e, const QPointF& p)
{
  mouseAddPolygon(t, e, p, Polygon::ROI);
}

void Editor::mouseEditPolygon(
  const MouseType t, QMouseEvent* e, const QPointF& p)
{
  if (selected_polygon == nullptr)
    return;// no polygon is selected, nothing to do

  if (t == MOUSE_PRESS)
  {
    if (e->buttons() & Qt::RightButton)
    {
      const Level::NearestItem ni = building.levels[level_idx].nearestItems(
        p.x(),
        p.y());
      if (ni.vertex_dist > 10.0)
      {
        printf("right-click wasn't near a vertex: %.1f\n", ni.vertex_dist);
        return;  // click wasn't near a vertex
      }
      else
      {
        printf("removing vertex %d\n", ni.vertex_idx);
      }
      PolygonRemoveVertCommand* command = new PolygonRemoveVertCommand(
        selected_polygon, ni.vertex_idx);
      undo_stack.push(command);
      setWindowModified(true);
      createScene();
    }
    else if (e->buttons() & Qt::LeftButton)
    {
      mouse_edge_drag_polygon = building.polygonEdgeDragPress(
        level_idx,
        selected_polygon,
        p.x(),
        p.y());
      if (mouse_edge_drag_polygon.movable_vertex < 0)
        return;

      if (mouse_motion_polygon != nullptr)
      {
        qWarning("edit_polygon_release() without null mouse_motion_polygon!");
        return;
      }

      mouse_motion_polygon = scene->addPolygon(
        mouse_edge_drag_polygon.polygon,
        QPen(Qt::black),
        QBrush(QColor::fromRgbF(1.0, 1.0, 0.5, 0.5)));
    }
  }
  else if (t == MOUSE_RELEASE)
  {
    // todo by drag mode (left/right button?)
    if (mouse_motion_polygon == nullptr)
    {
      qInfo("woah! edit_polygon_release() with null mouse_motion_polygon!");
      return;
    }
    printf("replacing vertices of polygon...\n");
    scene->removeItem(mouse_motion_polygon);
    delete mouse_motion_polygon;
    mouse_motion_polygon = nullptr;

    const Level::NearestItem ni = building.levels[level_idx].nearestItems(
      p.x(),
      p.y());

    if (ni.vertex_dist > 10.0)
      return;// nothing to do; didn't release near a vertex

    const int release_vertex_idx = ni.vertex_idx;

    if (std::find(
        selected_polygon->vertices.begin(),
        selected_polygon->vertices.end(),
        release_vertex_idx) != selected_polygon->vertices.end())
      return;// Release vertex is already in the polygon. Don't do anything.

    PolygonAddVertCommand* command = new PolygonAddVertCommand(
      selected_polygon,
      mouse_edge_drag_polygon.movable_vertex,
      release_vertex_idx);

    undo_stack.push(command);

    setWindowModified(true);
    createScene();
  }
  else if (t == MOUSE_MOVE)
  {
    if (e->buttons() & Qt::LeftButton)
    {
      if (mouse_motion_polygon == nullptr)
      {
        qInfo("woah! edit_polygon_release() with null mouse_motion_polygon!");
        return;
      }
      QPolygonF polygon = mouse_motion_polygon->polygon();
      polygon[mouse_edge_drag_polygon.movable_vertex] = QPointF(p.x(), p.y());
      mouse_motion_polygon->setPolygon(polygon);
    }
  }
}

void Editor::numberKeyPressed(const int n)
{
  bool found_edge = false;
  for (auto& edge : building.levels[level_idx].edges)
  {
    if (edge.selected && edge.type == Edge::LANE)
    {
      edge.setGraphIdx(n);
      found_edge = true;
    }
  }
  if (found_edge)
  {
    createScene();
    updatePropertyEditor();
  }

  rendering_options.active_traffic_map_idx = n;
  traffic_table->update(rendering_options);
}

bool Editor::maybeSave()
{
  if (!isWindowModified())
    return true;// no need to ask to save the document
  const QMessageBox::StandardButton button_clicked =
    QMessageBox::warning(
    this,
    "Building not saved!",
    "Do you want to save your changes?",
    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
  switch (button_clicked)
  {
    case QMessageBox::Save:
      return buildingSave();
    case QMessageBox::Cancel:
      return false;
    default:
      break;
  }
  return true;
}

void Editor::showEvent(QShowEvent* event)
{
  QMainWindow::showEvent(event);
}

void Editor::closeEvent(QCloseEvent* event)
{
  // save window geometry
  QSettings settings;
  settings.setValue(preferences_keys::window_left, geometry().x());
  settings.setValue(preferences_keys::window_top, geometry().y());
  settings.setValue(preferences_keys::window_width, geometry().width());
  settings.setValue(preferences_keys::window_height, geometry().height());

  // save viewport center and scale
  const QPoint p_center_window(
    map_view->viewport()->width() / 2,
    map_view->viewport()->height() / 2);
  const QPointF p_center_scene = map_view->mapToScene(p_center_window);

  printf("closeEvent:  (%d, %d) -> (%.1f, %.1f)\n",
    p_center_window.x(),
    p_center_window.y(),
    p_center_scene.x(),
    p_center_scene.y());

  const double scale = map_view->transform().m11();
  settings.setValue(preferences_keys::viewport_center_x, p_center_scene.x());
  settings.setValue(preferences_keys::viewport_center_y, p_center_scene.y());
  settings.setValue(preferences_keys::viewport_scale, scale);

  settings.setValue(
    preferences_keys::previous_building_path,
    QString::fromStdString(building.getFilename()));

  if (!building.levels.empty())
    settings.setValue(
      preferences_keys::level_name,
      QString::fromStdString(building.levels[level_idx].name));

  if (maybeSave())
    event->accept();
  else
    event->ignore();
}

#if 0
void Editor::set_tool_visibility(const ToolId id, const bool visible)
{
  QAction* a = tools[id];
  if (a)
    a->setVisible(visible);
  else
    printf("unable to find tool action %d\n", static_cast<int>(id));
}

void Editor::set_mode(const EditorModeId _mode, const QString& mode_string)
{
  if (mode_combo_box->currentText() != mode_string)
  {
    mode_combo_box->blockSignals(true);
    mode_combo_box->setCurrentText(mode_string);
    mode_combo_box->blockSignals(false);
  }

  mode = _mode;

  // building tools
  set_tool_visibility(TOOL_ADD_WALL, mode == MODE_BUILDING);
  set_tool_visibility(TOOL_ADD_MEAS, mode == MODE_BUILDING);
  set_tool_visibility(TOOL_ADD_DOOR, mode == MODE_BUILDING);
  set_tool_visibility(TOOL_ADD_MODEL, mode == MODE_BUILDING);
  set_tool_visibility(TOOL_ADD_FLOOR, mode == MODE_BUILDING);
  set_tool_visibility(TOOL_ADD_HOLE, mode == MODE_BUILDING);
  set_tool_visibility(TOOL_ADD_CORRESPONDENCE_POINT, mode == MODE_BUILDING);
  set_tool_visibility(TOOL_ADD_FIDUCIAL, mode == MODE_BUILDING);

  // traffic tools
  set_tool_visibility(TOOL_ADD_LANE, mode == MODE_TRAFFIC);

  // scenario tools
  set_tool_visibility(TOOL_ADD_ROI, mode == MODE_SCENARIO);

  // crowd_sim tools
  set_tool_visibility(TOOL_ADD_HUMAN_LANE, mode == MODE_CROWD_SIM);

  // "multi-purpose" tools
  set_tool_visibility(TOOL_EDIT_POLYGON,
    mode != MODE_TRAFFIC && mode != MODE_CROWD_SIM);
}
#endif

void Editor::updateTables()
{
  level_table->update(building);
  lift_table->update(building);
  traffic_table->update(rendering_options);
  crowd_sim_table->update();
  layer_table->update(building, level_idx, layer_idx);
}

void Editor::clearCurrentToolBuffer()
{
  if (latest_move_lift)
  {
    latest_move_lift->undo();
    delete latest_move_lift;
    latest_move_lift = nullptr;
    createScene();
  }
  if (
    tool_id == TOOL_ADD_WALL
    || tool_id == TOOL_ADD_LANE
    || tool_id == TOOL_ADD_MEAS
    || tool_id == TOOL_ADD_HUMAN_LANE
    || tool_id == TOOL_ADD_DOOR)
  {
    prev_clicked_idx = -1;
    clicked_idx = -1;
    if (latest_add_edge)
    {
      delete latest_add_edge;
      latest_add_edge = NULL;
    }
  }

  if (!clicked_feature_id.isNull())
    clicked_feature_id = QUuid();
}

void Editor::levelTableUpdateSlot()
{
  updateTables();
  createScene();
}

void Editor::layerTableUpdateSlot()
{
  layer_table->update(building, level_idx, layer_idx);
  createScene();
}

Level* Editor::activeLevel()
{
  if (level_idx < static_cast<int>(building.levels.size()))
    return &building.levels[level_idx];

  return nullptr;
}

Layer* Editor::activeLayer()
{
  if (layer_idx <= 0)
    return nullptr;

  Level* const level = activeLevel();
  if (!level)
    return nullptr;

  if (layer_idx - 1 < static_cast<int>(level->layers.size()))
    return &level->layers[layer_idx - 1];

  return nullptr;
}

void Editor::cacheSizeUpdateTimerTimeout()
{
  printf("cache_size_update_timer_timeout()\n");
  map_view->updateCacheSizeLabel(cache_size_label);
}
