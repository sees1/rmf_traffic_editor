#include <QtWidgets>
#include <QTest>

#include "editor.h"
#include "door.h"
#include "level_table.h"
#include "map_view.h"
#include "actions/edge_style.h"
#include "actions/move_lift.hpp"
#include "actions/delete.h"
#include "actions/add_edge.h"

class TestGui : public QObject
{
  Q_OBJECT

private slots:
  void initTestCase()
  {
    printf("initTestCase()\n");
    QApplication::setStyle("Fusion");
    QFile theme(":/styles/light.qss");
    QVERIFY(theme.open(QIODevice::ReadOnly));
    qApp->setStyleSheet(QString::fromUtf8(theme.readAll()));
    editor = new Editor();
  }

  void importAndReplacePng()
  {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString original_directory = QDir::currentPath();
    QFile building_file(directory.filePath("test.building.yaml"));
    QVERIFY(building_file.open(QIODevice::WriteOnly));
    building_file.write("name: test\ncoordinate_system: reference_image\n"
      "levels: {}\nlifts: {}\n");
    building_file.close();
    QVERIFY(editor->loadBuilding(building_file.fileName()));
    // Loading a building changes cwd; don't leave it in a temporary directory.
    QVERIFY(QDir::setCurrent(original_directory));
    const QString first = directory.filePath("first.png");
    const QString second = directory.filePath("second.png");
    QImage image(2400, 1600, QImage::Format_RGB32);
    image.fill(Qt::black);
    QVERIFY(image.save(first));
    image = QImage(800, 1200, QImage::Format_RGB32);
    image.fill(Qt::gray);
    QVERIFY(image.save(second));

    editor->resize(1200, 800);
    editor->show();
    QApplication::processEvents();
    auto* table = editor->findChild<LevelTable*>();
    auto* view = editor->findChild<MapView*>();
    QVERIFY(table);
    QVERIFY(view);

    const auto fill_dialog = [](const QString& filename,
      const QString& name)
      {
        QTimer::singleShot(0, [filename, name]()
          {
            auto* dialog = QApplication::activeModalWidget();
            QVERIFY(dialog);
            auto* name_edit = dialog->findChild<QLineEdit*>("levelName");
            auto* drawing = dialog->findChild<QLineEdit*>("drawingFilename");
            QVERIFY(name_edit);
            QVERIFY(drawing);
            name_edit->setText(name);
            drawing->setText(filename);
            for (auto* button : dialog->findChildren<QPushButton*>())
            {
              if (button->text() == "OK")
              {
                button->click();
                return;
              }
            }
            QFAIL("Missing OK button");
          });
      };
    const auto verify_image = [view](const QSize& size)
      {
        QApplication::processEvents();
        QGraphicsPixmapItem* floorplan = nullptr;
        for (auto* item : view->scene()->items())
        {
          if (auto* pixmap = qgraphicsitem_cast<QGraphicsPixmapItem*>(item))
            floorplan = pixmap;
        }
        QVERIFY(floorplan);
        QCOMPARE(floorplan->pixmap().size(), size);
        const QRect visible = view->mapFromScene(
          floorplan->sceneBoundingRect()).boundingRect();
        QVERIFY(view->viewport()->rect().contains(visible));
        const QImage rendered = view->viewport()->grab().toImage();
        QCOMPARE(rendered.pixelColor(rendered.width()/2, rendered.height()/2),
          floorplan->pixmap().toImage().pixelColor(size.width()/2,
          size.height()/2));
      };

    fill_dialog(first, "L1");
    auto* add = qobject_cast<QPushButton*>(table->cellWidget(0, 5));
    QVERIFY(add);
    add->click();
    verify_image(QSize(2400, 1600));

    // Replacing the PNG must redraw without a manual level switch.
    fill_dialog(second, "L1");
    auto* edit = qobject_cast<QPushButton*>(table->cellWidget(0, 5));
    QVERIFY(edit);
    edit->click();
    verify_image(QSize(800, 1200));

    // Some drawing tools export SVG content with a .png filename.
    const QString disguised = directory.filePath("vector.png");
    QFile svg(disguised);
    QVERIFY(svg.open(QIODevice::WriteOnly));
    svg.write("<svg xmlns=\"http://www.w3.org/2000/svg\" "
      "width=\"600\" height=\"400\"><rect width=\"600\" "
      "height=\"400\" fill=\"black\"/></svg>");
    svg.close();
    fill_dialog(disguised, "L1");
    edit = qobject_cast<QPushButton*>(table->cellWidget(0, 5));
    QVERIFY(edit);
    edit->click();
    verify_image(QSize(600, 400));

    // Adding another level must select it even after panning off-map.
    view->centerOn(-2000, -2000);
    fill_dialog(first, "L2");
    add = qobject_cast<QPushButton*>(table->cellWidget(1, 5));
    QVERIFY(add);
    add->click();
    QCOMPARE(table->currentRow(), 1);
    verify_image(QSize(2400, 1600));
    // Hover motion must reach the editor through the splitter, without
    // holding a mouse button, so the next polyline segment follows the cursor.
    for (const QString& tooltip : {QString("Add Lane (L)"),
      QString("Add Wall (W)")})
    {
      QToolButton* tool = nullptr;
      for (auto* button : editor->findChildren<QToolButton*>())
      {
        if (button->toolTip() == tooltip)
          tool = button;
      }
      QVERIFY(tool);
      tool->click();
      QWidget* viewport = view->viewport();
      const QPoint start(100, 100);
      QTest::mouseClick(viewport, Qt::LeftButton, Qt::NoModifier, start);
      QGraphicsLineItem* preview = nullptr;
      for (const QPoint& end : {QPoint(200, 150), QPoint(250, 200)})
      {
        QMouseEvent motion(QEvent::MouseMove, end,
          viewport->mapToGlobal(end), Qt::NoButton, Qt::NoButton,
          Qt::NoModifier);
        QApplication::sendEvent(viewport, &motion);
        preview = nullptr;
        for (auto* item : view->scene()->items())
        {
          if (auto* line = qgraphicsitem_cast<QGraphicsLineItem*>(item))
          {
            if (QLineF(line->line().p2(), view->mapToScene(end)).length()
              < 0.01)
              preview = line;
          }
        }
        QVERIFY2(preview, "Polyline preview must follow hover motion");
      }
      QTest::mouseClick(viewport, Qt::RightButton, Qt::NoModifier,
        QPoint(250, 200));
      QVERIFY(!view->scene()->items().contains(preview));
    }
    editor->setWindowModified(false);
  }

  void lineStyleDialog()
  {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QFile file(directory.filePath("style.building.yaml"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("name: style\ncoordinate_system: reference_image\n"
      "levels:\n  L1:\n    x_meters: 20\n    y_meters: 20\n"
      "    vertices: [[40, 100, 0, ''], [160, 100, 0, ''], "
      "[280, 100, 0, '']]\n    walls: [[0, 1, {}], [1, 2, {}]]\n"
      "lifts: {}\n");
    file.close();
    const QString cwd = QDir::currentPath();
    QVERIFY(editor->loadBuilding(file.fileName()));
    QVERIFY(QDir::setCurrent(cwd));
    QTest::keyClick(editor, Qt::Key_Escape);
    editor->restorePreviousViewport();
    QApplication::processEvents();
    auto* view = editor->findChild<MapView*>();
    QVERIFY(view);
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
      view->mapFromScene(QPointF(100, 100)));
    auto* style = editor->findChild<QPushButton*>("lineStyleButton");
    QVERIFY(style);
    QVERIFY(style->isEnabled());
    QTimer::singleShot(0, []()
      {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        auto* width = dialog->findChild<QDoubleSpinBox*>("lineWidth");
        auto* connected = dialog->findChild<QCheckBox*>("connectedSegments");
        auto* color = dialog->findChild<QPushButton*>("lineColor");
        QVERIFY(width);
        QVERIFY(connected);
        QVERIFY(color);
        width->setValue(0.6);
        connected->setChecked(true);
        QTimer::singleShot(0, []()
          {
            auto* picker = qobject_cast<QColorDialog*>(
              QApplication::activeModalWidget());
            QVERIFY(picker);
            picker->setCurrentColor(QColor("#e07020"));
            picker->accept();
          });
        color->click();
        dialog->accept();
      });
    style->click();
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
      view->mapFromScene(QPointF(300, 300)));
    int lines = 0;
    for (auto* item : view->scene()->items())
    {
      if (auto* line = qgraphicsitem_cast<QGraphicsLineItem*>(item))
      {
        QCOMPARE(line->pen().color(), QColor("#e07020"));
        QVERIFY(qAbs(line->pen().widthF() - 12.0) < 0.0001);
        ++lines;
      }
    }
    QCOMPARE(lines, 2);
    editor->setWindowModified(false);
  }

  void lineStylePersistenceAndUndo()
  {
    Building building;
    building.levels.emplace_back();
    auto& level = building.levels.front();
    level.vertices.emplace_back(0, 0);
    level.vertices.emplace_back(100, 0);
    level.edges.emplace_back(0, 1, Edge::WALL);
    level.edges.emplace_back(0, 1, Edge::HUMAN_LANE);
    level.edges.back().params["width"] = Param(1.5);
    QUndoStack stack;
    stack.push(new EdgeStyleCommand(building, 0, {0, 1},
      QColor("#123456"), 0.35));
    QCOMPARE(level.edges[1].getWidth(), 1.5);
    for (auto& edge : level.edges)
    {
      Edge restored;
      restored.fromYaml(YAML::Load(YAML::Dump(edge.toYaml())), edge.type);
      QCOMPARE(restored.displayColor(Qt::black), QColor("#123456"));
      QCOMPARE(restored.displayWidth(1.0), 0.35);
    }
    QGraphicsScene scene;
    std::vector<EditorModel> models;
    level.draw(&scene, models, RenderingOptions(), {},
      building.coordinate_system);
    int styled_lines = 0;
    for (auto* item : scene.items())
    {
      if (auto* line = qgraphicsitem_cast<QGraphicsLineItem*>(item))
      {
        QCOMPARE(line->pen().color(), QColor("#123456"));
        QVERIFY(qAbs(line->pen().widthF() - 7.0) < 0.0001);
        ++styled_lines;
      }
    }
    QCOMPARE(styled_lines, 2);
    stack.undo();
    QCOMPARE(level.edges[0].displayWidth(0.2), 0.2);
    QCOMPARE(level.edges[0].params.count("editor_color"), std::size_t(0));
    QCOMPARE(level.edges[1].getWidth(), 1.5);
    stack.redo();
    QCOMPARE(level.edges[0].displayColor(Qt::black), QColor("#123456"));
    level.edges[0].params["editor_width"] = Param(-1.0);
    level.edges[0].params["editor_color"] = Param(std::string("invalid"));
    QCOMPARE(level.edges[0].displayWidth(0.2), 0.2);
    QCOMPARE(level.edges[0].displayColor(Qt::blue), QColor(Qt::blue));
  }

  void jsonMapRoundTrip()
  {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath("map.building.json");
    Building original;
    QVERIFY(original.setFilename(path.toStdString()));
    original.name = "001";
    original.reference_level_name = "001";
    original.levels.emplace_back();
    auto& level = original.levels.front();
    level.name = "001";
    level.vertices.emplace_back(10.5, 20.25, "false");
    level.vertices.emplace_back(30, 40, "node\"\nname");
    level.vertices[0].params["is_transition_zone"] = Param(true);
    level.vertices[0].params["dock_name"] = Param(std::string("001"));
    level.edges.emplace_back(0, 1, Edge::WALL);
    level.edges[0].params["editor_color"] = Param(std::string("#123456"));
    level.edges[0].params["editor_width"] = Param(0.35);
    const auto level_json = level.toJson(original.coordinate_system);
    QCOMPARE(level_json["vertices"][0][0].get<double>(), 10.5);
    QCOMPARE(level_json["vertices"][0][3].get<std::string>(), std::string("false"));
    QCOMPARE(level_json["walls"][0][0].get<int>(), 0);
    QCOMPARE(level_json["walls"][0][1].get<int>(), 1);
    QVERIFY(original.save());
    QFile json_file(path);
    QVERIFY(json_file.open(QIODevice::ReadOnly));
    const QByteArray data = json_file.readAll();
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(data, &error);
    QCOMPARE(error.error, QJsonParseError::NoError);
    QVERIFY(document.isObject());
    QCOMPARE(document.object()["name"].toString(), QString("001"));
    const auto vertex = document.object()["levels"].toObject()["001"]
      .toObject()["vertices"].toArray()[0].toArray();
    QCOMPARE(vertex[0].toDouble(), 10.5);
    QCOMPARE(vertex[3].toString(), QString("false"));
    const auto params = vertex[4].toObject();
    QVERIFY(params["is_transition_zone"].toArray()[1].isBool());
    QVERIFY(params["is_transition_zone"].toArray()[1].toBool());
    QCOMPARE(params["dock_name"].toArray()[1].toString(), QString("001"));
    Building restored;
    const QString cwd = QDir::currentPath();
    QVERIFY(restored.load(path.toStdString()));
    QVERIFY(QDir::setCurrent(cwd));
    QCOMPARE(restored.levels[0].vertices[1].name,
      level.vertices[1].name);
    QVERIFY(restored.levels[0].vertices[0].params["is_transition_zone"]
      .value_bool);
    QCOMPARE(restored.levels[0].edges[0].displayWidth(1), 0.35);

    // Legacy YAML opens normally and saves to a sibling JSON file.
    QFile legacy(directory.filePath("legacy.building.yaml"));
    QVERIFY(legacy.open(QIODevice::WriteOnly));
    const QByteArray yaml("name: legacy\ncoordinate_system: reference_image\n"
      "levels: {}\nlifts: {}\n");
    legacy.write(yaml);
    legacy.close();
    QVERIFY(restored.load(legacy.fileName().toStdString()));
    QVERIFY(QDir::setCurrent(cwd));
    QVERIFY(restored.save());
    QCOMPARE(QString::fromStdString(restored.getFilename()),
      directory.filePath("legacy.building.json"));
    QVERIFY(legacy.open(QIODevice::ReadOnly));
    QCOMPARE(legacy.readAll(), yaml);
  }

  void liftPoseAndHitTest()
  {
    Lift lift;
    lift.pose.translation() = Eigen::Vector2d(100, 200);
    lift.pose.linear() = Eigen::Rotation2Dd(0.4).toRotationMatrix();
    lift.width = 2;
    lift.depth = 1;
    const auto displayed = lift.scenePose(2, 30, 40, 0.7);
    const Eigen::Vector2d expected =
      2 * (Eigen::Rotation2Dd(-0.7) * Eigen::Vector2d(100, 200))
      + Eigen::Vector2d(30, 40);
    QVERIFY((displayed.translation() - expected).norm() < 1e-9);
    QVERIFY(std::abs(std::atan2(displayed.linear()(1, 0),
        displayed.linear()(0, 0)) + 1.1) < 1e-9);
    // At 0.05 m/pixel the cabin's half-width is 20 scene units.
    QVERIFY(lift.nearestItemIndexIfWithinDistance(
        displayed * Eigen::Vector2d(21, 0), 2, 0.05, displayed));
    QVERIFY(!lift.nearestItemIndexIfWithinDistance(
        displayed * Eigen::Vector2d(23, 0), 2, 0.05, displayed));
    QVERIFY(!lift.nearestItemIndexIfWithinDistance(
        displayed.translation(), 2, 0.05, displayed));
    Lift restored;
    restored.fromYaml("lift", lift.toYaml(), {});
    QVERIFY((restored.pose.matrix() - lift.pose.matrix()).norm() < 1e-9);

    QGraphicsScene scene;
    lift.draw(&scene, 0.05, "L1", 0, true, 2, 30, 40, 0.7);
    QGraphicsItemGroup* group = nullptr;
    for (auto* item : scene.items())
      if (auto* candidate = dynamic_cast<QGraphicsItemGroup*>(item))
        group = candidate;
    QVERIFY(group);
    QVERIFY(std::abs(group->pos().x() - expected.x()) < 1e-9);
    QVERIFY(std::abs(group->rotation() + 1.1 * 180 / std::acos(-1.0)) < 1e-9);
  }

  void liftMoveUndoRedo()
  {
    Building building;
    building.levels.resize(2);
    building.levels[0].name = "L1";
    building.levels[1].name = "L2";
    Lift lift;
    lift.name = "lift";
    lift.reference_floor_name = "L1";
    lift.pose.translation() = Eigen::Vector2d(10, 20);
    lift.pose.linear() = Eigen::Rotation2Dd(0.4).toRotationMatrix();
    building.lifts.push_back(lift);
    Building::Transform transform;
    transform.scale = 2;
    transform.rotation = std::acos(-1.0) / 2;
    building.transforms[{0, 0}] = Building::Transform();
    building.transforms[{0, 1}] = transform;
    for (auto& level : building.levels)
    {
      Vertex v(50, 60);
      v.params["lift_cabin"] = Param(std::string("lift"));
      level.vertices.push_back(v);
    }
    auto* command = new MoveLiftCommand(&building, "lift");
    command->setFinalDestination(13, 24);
    command->redo(); // live preview before QUndoStack::push()
    command->setFinalDestination(16, 28);
    command->redo();
    QUndoStack stack;
    stack.push(command);
    QCOMPARE(building.lifts[0].pose.translation().x(), 16.0);
    QCOMPARE(building.levels[0].vertices[0].x, 56.0);
    QVERIFY(std::abs(building.levels[1].vertices[0].x - 66) < 1e-9);
    QVERIFY(std::abs(building.levels[1].vertices[0].y - 48) < 1e-9);
    stack.undo();
    QCOMPARE(building.lifts[0].pose.translation().x(), 10.0);
    QCOMPARE(building.levels[1].vertices[0].x, 50.0);
    QCOMPARE(building.levels[1].vertices[0].y, 60.0);
    stack.redo();
    QCOMPARE(building.lifts[0].pose.translation().x(), 16.0);
    QVERIFY(std::abs(building.levels[1].vertices[0].y - 48) < 1e-9);
    QVERIFY((building.lifts[0].pose.linear() - lift.pose.linear()).norm() < 1e-9);
  }

  void interactiveLiftDrag()
  {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QFile file(directory.filePath("lift.building.yaml"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("name: test\ncoordinate_system: reference_image\n"
      "levels:\n  L1:\n    elevation: 0\n    x_meters: 20\n    y_meters: 20\n"
      "lifts:\n  lift:\n    x: 100\n    y: 100\n    yaw: 0\n"
      "    width: 2\n    depth: 2\n    reference_floor_name: L1\n");
    file.close();
    const auto cwd = QDir::currentPath();
    QVERIFY(editor->loadBuilding(file.fileName()));
    QVERIFY(QDir::setCurrent(cwd));
    editor->resize(1200, 800);
    editor->show();
    QApplication::processEvents();
    auto* view = editor->findChild<MapView*>();
    QVERIFY(view);
    QToolButton* move = nullptr;
    for (auto* button : editor->findChildren<QToolButton*>())
      if (button->accessibleName().startsWith("Move"))
        move = button;
    QVERIFY(move);
    move->click();
    QAction* save = nullptr;
    for (auto* action : editor->findChildren<QAction*>())
      if (action->text() == "&Save")
        save = action;
    QVERIFY(save);
    const auto send = [&](QEvent::Type type, const QPointF& scene_point)
      {
        const QPoint local = view->mapFromScene(scene_point);
        QMouseEvent event(type, local,
          type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
          type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,
          Qt::NoModifier);
        QApplication::sendEvent(view->viewport(), &event);
      };
    const auto saved_x = [&]()
      {
        save->trigger();
        return YAML::LoadFile(directory.filePath("lift.building.json")
          .toStdString())["lifts"]["lift"]["x"].as<double>();
      };
    send(QEvent::MouseButtonPress, QPointF(120, 100));
    send(QEvent::MouseMove, QPointF(150, 120));
    // Verify the preview before releasing the mouse.
    const double preview = saved_x();
    QVERIFY(std::abs(preview - 130) < 2);
    send(QEvent::MouseButtonRelease, QPointF(150, 120));
    QVERIFY(std::abs(saved_x() - preview) < 0.001);
    for (auto* action : editor->findChildren<QAction*>())
      if (action->text().contains("Undo"))
        action->trigger();
    QVERIFY(std::abs(saved_x() - 100) < 0.001);
    for (auto* action : editor->findChildren<QAction*>())
      if (action->text().contains("Redo"))
        action->trigger();
    QVERIFY(std::abs(saved_x() - preview) < 0.001);

    // Escape rolls back an unfinished drag.
    send(QEvent::MouseButtonPress, QPointF(150, 120));
    send(QEvent::MouseMove, QPointF(170, 140));
    QTest::keyClick(editor, Qt::Key_Escape);
    QVERIFY(std::abs(saved_x() - preview) < 0.001);
  }

  void doorApproachVertices()
  {
    Building building;
    building.levels.emplace_back();
    auto& level = building.levels[0];
    level.name = "L1";
    level.drawing_meters_per_pixel = 0.05;
    level.vertices.emplace_back(0, 0);
    level.vertices.emplace_back(40, 0);
    level.edges.emplace_back(0, 1, Edge::DOOR);
    building.syncDoorVertices();
    QCOMPARE(level.vertices.size(), std::size_t(4));
    QCOMPARE(level.edges.size(), std::size_t(2));
    QCOMPARE(level.vertices[2].x, 20.0);
    QCOMPARE(level.vertices[2].y, 10.0);
    QCOMPARE(level.vertices[3].x, 20.0);
    QCOMPARE(level.vertices[3].y, -10.0);
    QCOMPARE(level.edges[1].start_idx, 2);
    QCOMPARE(level.edges[1].end_idx, 3);
    const auto front_id = level.vertices[2].uuid;
    building.syncDoorVertices();
    QCOMPARE(level.vertices.size(), std::size_t(4));
    QCOMPARE(level.vertices[2].uuid, front_id);
    // Rotate the source door and change offset, retaining the graph IDs.
    level.vertices[1].x = 0;
    level.vertices[1].y = 40;
    building.params["door_vertex_offset"] = Param(1.0);
    building.syncDoorVertices();
    QCOMPARE(level.vertices[2].x, -20.0);
    QCOMPARE(level.vertices[2].y, 20.0);
    QCOMPARE(level.vertices[2].uuid, front_id);

    // Manual lanes attached to generated points survive regeneration.
    level.vertices.emplace_back(100, 100);
    level.edges.emplace_back(2, 4, Edge::LANE);
    building.syncDoorVertices();
    QCOMPARE(level.edges.back().start_idx, 2);
    QCOMPARE(level.edges.back().end_idx, 4);
    // Source deletion removes its derived graph; undo restores all indices.
    level.edges[0].selected = true;
    QUndoStack history;
    history.push(new DeleteCommand(&building, 0));
    QCOMPARE(level.vertices.size(), std::size_t(3));
    QCOMPARE(level.edges.size(), std::size_t(0));
    QCOMPARE(level.vertices[2].x, 100.0);
    history.undo();
    building.syncDoorVertices();
    QCOMPARE(level.vertices.size(), std::size_t(5));
    QCOMPARE(level.edges.size(), std::size_t(3));
    QCOMPARE(level.edges.back().end_idx, 4);
    QCOMPARE(level.vertices[2].uuid, front_id);
    history.redo();
    QCOMPARE(level.vertices.size(), std::size_t(3));
  }

  void doorGeoJsonLineString()
  {
    Door door;
    door.start_idx = 8;
    door.end_idx = 9;
    door.params["name"] = Param(std::string("D1"));
    door.setApproachVertices(2, 3);
    door.setApproachPositions(Eigen::Vector2d(20, 10), Eigen::Vector2d(20, -10));
    const auto json = door.toJson();
    QVERIFY(json.is_object());
    QCOMPARE(json.at("type").get<std::string>(), std::string("Feature"));
    QCOMPARE(json.at("geometry").at("type").get<std::string>(), std::string("LineString"));
    QVERIFY(json.at("geometry").at("coordinates") == nlohmann::json({{20.0, 10.0}, {20.0, -10.0}}));
    QCOMPARE(json.at("properties").at("startid").get<int>(), 2);
    QCOMPARE(json.at("properties").at("endid").get<int>(), 3);
    QCOMPARE(json.at("properties").at("metadata").at("name").get<std::string>(), std::string("D1"));
    // Do not invent positions for legacy doors that have no snapshot.
    door.setApproachVertices(-1, -1);
    QVERIFY(door.toJson().at("geometry").is_null());
  }

  void liftGeoJsonLineStrings()
  {
    Lift lift;
    lift.name = "lift";
    auto& l1 = lift.navigation_vertices["L1"];
    l1.cabin = 5;
    l1.cabin_position = Eigen::Vector2d(100, 200);
    l1.doors = {{"entry", 4}, {"exit", 6}};
    l1.door_positions = {{"entry", Eigen::Vector2d(100, 170)},
      {"exit", Eigen::Vector2d(100, 230)}};
    auto& l2 = lift.navigation_vertices["L2"];
    l2.cabin = 1;
    l2.cabin_position = Eigen::Vector2d(300, 400);
    l2.doors = {{"entry", 0}};
    l2.door_positions = {{"entry", Eigen::Vector2d(300, 370)}};
    const auto json = lift.toJson();
    QVERIFY(json.contains("features"));
    QCOMPARE(json.at("type").get<std::string>(), std::string("FeatureCollection"));
    QCOMPARE(json.at("features").size(), std::size_t(3));
    for (const auto& feature : json.at("features"))
    {
      QCOMPARE(feature.at("type").get<std::string>(), std::string("Feature"));
      const auto& props = feature.at("properties");
      const auto& meta = props.at("metadata");
      QCOMPARE(meta.at("lift_name").get<std::string>(), lift.name);
      const auto& refs = lift.navigation_vertices.at(meta.at("level_name").get<std::string>());
      const auto door_name = meta.at("door_name").get<std::string>();
      QCOMPARE(props.at("startid").get<int>(), refs.doors.at(door_name));
      QCOMPARE(props.at("endid").get<int>(), refs.cabin);
      QCOMPARE(feature.at("geometry").at("type").get<std::string>(), std::string("LineString"));
      const auto& p = refs.door_positions.at(door_name);
      QVERIFY(feature.at("geometry").at("coordinates") ==
        nlohmann::json({{p.x(), p.y()}, {refs.cabin_position->x(), refs.cabin_position->y()}}));
    }
    l1.cabin_position.reset();
    l2.door_positions.clear();
    QVERIFY(lift.toJson().at("features").empty());
    QVERIFY(Lift().toJson().at("features").is_array());
  }

  void navigationVertexCoordinates()
  {
    Building building;
    building.levels.emplace_back();
    auto& level = building.levels.front();
    level.name = "L1";
    level.drawing_meters_per_pixel = 0.05;
    level.vertices.emplace_back(0, 0);
    level.vertices.emplace_back(40, 0);
    level.edges.emplace_back(0, 1, Edge::DOOR);
    Lift lift;
    lift.name = "lift";
    lift.reference_floor_name = "L1";
    lift.pose.translation() = Eigen::Vector2d(100, 200);
    LiftDoor opening;
    opening.name = "entry";
    opening.y = 1;
    lift.doors.push_back(opening);
    lift.level_doors["L1"] = {"entry"};
    building.lifts.push_back(lift);
    building.syncDoorVertices();
    const auto json = Door(level.edges.front()).toJson();
    QCOMPARE(json.at("geometry").at("coordinates")[0][0].get<double>(), 20.0);
    QCOMPARE(json.at("geometry").at("coordinates")[0][1].get<double>(), 10.0);
    const auto lift_json = building.lifts.front().toJson();
    QCOMPARE(lift_json.at("features").size(), std::size_t(1));
    QCOMPARE(lift_json.at("features")[0].at("geometry").at("coordinates")[1][0].get<double>(), 100.0);
    QCOMPARE(lift_json.at("features")[0].at("geometry").at("coordinates")[0][1].get<double>(), 170.0);
  }

  void navigationVertexSerialization()
  {
    Building building;
    building.levels.emplace_back();
    auto& level = building.levels.front();
    level.name = "L1";
    level.vertices.emplace_back(0, 0);
    level.vertices.emplace_back(40, 0);
    level.edges.emplace_back(0, 1, Edge::DOOR);
    Lift lift;
    lift.name = "lift";
    lift.reference_floor_name = "L1";
    LiftDoor opening;
    opening.name = "entry";
    opening.y = 1;
    lift.doors.push_back(opening);
    lift.level_doors["L1"] = {"entry"};
    building.lifts.push_back(lift);
    building.syncDoorVertices();
    Door door(level.edges.front());
    QCOMPARE(door.frontVertex(), 2);
    QCOMPARE(door.backVertex(), 3);
    Door restored;
    restored.fromYaml(YAML::Load(YAML::Dump(door.toYaml())));
    QCOMPARE(restored.frontVertex(), door.frontVertex());
    QCOMPARE(restored.backVertex(), door.backVertex());
    QVERIFY(restored.frontPosition().has_value());
    QCOMPARE(restored.frontPosition()->x(), door.frontPosition()->x());
    QCOMPARE(restored.backPosition()->y(), door.backPosition()->y());
    restored.fromYaml(door.toYaml());
    QCOMPARE(restored.frontVertex(), 2);
    QCOMPARE(restored.frontPosition()->y(), door.frontPosition()->y());
    restored.setApproachVertices(-1, -1);
    QVERIFY(!restored.frontPosition());
    QVERIFY(!restored.backPosition());
    Door legacy;
    legacy.setApproachVertices(2, 3);
    QVERIFY(!legacy.frontPosition());
    QVERIFY(!legacy.backPosition());
    Lift loaded;
    loaded.fromYaml("lift", YAML::Load(building.lifts[0].toJson().at("properties").dump()), building.levels);
    QCOMPARE(loaded.navigation_vertices.at("L1").cabin, 5);
    QCOMPARE(loaded.navigation_vertices.at("L1").doors.at("entry"), 4);
    QVERIFY(loaded.navigation_vertices.at("L1").cabin_position);
    QCOMPARE(loaded.navigation_vertices.at("L1").door_positions.at("entry").y(),
      building.lifts[0].navigation_vertices.at("L1").door_positions.at("entry").y());
    loaded.fromYaml("lift", building.lifts[0].toYaml(), building.levels);
    QCOMPARE(loaded.doors.size(), std::size_t(1));
    QCOMPARE(loaded.navigation_vertices.at("L1").cabin, 5);
    QCOMPARE(loaded.navigation_vertices.at("L1").cabin_position->x(),
      building.lifts[0].navigation_vertices.at("L1").cabin_position->x());
    auto legacy_lift = building.lifts[0].toYaml();
    legacy_lift["navigation_vertices"]["L1"].remove("cabin_position");
    legacy_lift["navigation_vertices"]["L1"].remove("door_positions");
    loaded.fromYaml("lift", legacy_lift, building.levels);
    QVERIFY(!loaded.navigation_vertices.at("L1").cabin_position);
    QVERIFY(loaded.navigation_vertices.at("L1").door_positions.empty());
    level.vertices[0].x += 20;
    level.vertices[1].x += 20;
    building.syncDoorVertices();
    QCOMPARE(Door(level.edges.front()).frontPosition()->x(), 40.0);
    // Removing a door shifts lift indices; published references must follow.
    level.edges.erase(level.edges.begin());
    building.syncDoorVertices();
    QCOMPARE(building.lifts[0].navigation_vertices.at("L1").cabin, 3);
    QCOMPARE(building.lifts[0].navigation_vertices.at("L1").doors.at("entry"), 2);
    building.lifts[0].level_doors.clear();
    building.syncDoorVertices();
    QVERIFY(building.lifts[0].navigation_vertices.empty());
  }

  void doorApproachSaveAndAddUndo()
  {
    Building building;
    building.levels.emplace_back();
    building.levels[0].name = "L1";
    QUndoStack history;
    auto* command = new AddEdgeCommand(&building, 0, RenderingOptions());
    command->setFirstPoint(0, 0);
    command->setSecondPoint(40, 0);
    command->setEdgeType(Edge::DOOR);
    history.push(command);
    building.syncDoorVertices();
    QCOMPARE(building.levels[0].vertices.size(), std::size_t(4));
    history.undo();
    building.syncDoorVertices();
    QVERIFY(building.levels[0].vertices.empty());
    QVERIFY(building.levels[0].edges.empty());
    history.redo();
    building.syncDoorVertices();
    QCOMPARE(building.levels[0].vertices.size(), std::size_t(4));
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    building.setFilename(directory.filePath("doors.building.json").toStdString());
    QVERIFY(building.save());
    const auto cwd = QDir::currentPath();
    Building loaded;
    QVERIFY(loaded.load(building.getFilename()));
    QVERIFY(QDir::setCurrent(cwd));
    loaded.syncDoorVertices();
    QCOMPARE(loaded.levels[0].vertices.size(), std::size_t(4));
    QCOMPARE(loaded.levels[0].edges.size(), std::size_t(2));
    QCOMPARE(std::count_if(loaded.levels[0].edges.begin(), loaded.levels[0].edges.end(),
        [](const Edge& edge) { return edge.type == Edge::LANE; }), 1);
  }

  void liftApproachVerticesOnFloors()
  {
    Building building;
    building.levels.resize(3);
    for (std::size_t i = 0; i < 3; ++i)
    {
      building.levels[i].name = "L" + std::to_string(i);
      building.levels[i].drawing_meters_per_pixel = i == 1 ? 0.1 : 0.05;
      building.transforms[{0, static_cast<int>(i)}] = Building::Transform();
    }
    Building::Transform transform;
    transform.scale = 0.5;
    transform.dx = 300;
    transform.dy = 200;
    transform.rotation = std::acos(-1.0) / 2;
    building.transforms[{0, 1}] = transform;
    Lift lift;
    lift.name = "lift";
    lift.reference_floor_name = "L0";
    lift.pose.translation() = Eigen::Vector2d(100, 200);
    lift.width = 2;
    lift.depth = 2;
    LiftDoor door;
    door.name = "front";
    door.y = 1;
    lift.doors.push_back(door);
    door.name = "back";
    door.y = -1;
    lift.doors.push_back(door);
    lift.level_doors["L0"] = {"front"};
    lift.level_doors["L1"] = {"front", "back"};
    building.lifts.push_back(lift);
    building.syncDoorVertices();
    QCOMPARE(building.levels[0].vertices.size(), std::size_t(2));
    QCOMPARE(building.levels[1].vertices.size(), std::size_t(3));
    QVERIFY(building.levels[2].vertices.empty());
    QCOMPARE(building.levels[0].vertices[0].x, 100.0);
    QCOMPARE(building.levels[0].vertices[0].y, 170.0);
    QVERIFY(std::abs(building.levels[1].vertices[0].x - 385) < 1e-9);
    QVERIFY(std::abs(building.levels[1].vertices[0].y - 150) < 1e-9);
    QCOMPARE(building.levels[1].edges.size(), std::size_t(2));
    const auto check_positions = [&]()
    {
      for (const auto& entry : building.lifts[0].navigation_vertices)
      {
        const auto& level = building.levels[entry.first == "L0" ? 0 : 1];
        const auto& refs = entry.second;
        QVERIFY(refs.cabin_position);
        QCOMPARE(refs.cabin_position->x(), level.vertices[refs.cabin].x);
        QCOMPARE(refs.cabin_position->y(), level.vertices[refs.cabin].y);
        for (const auto& door : refs.doors)
        {
          QCOMPARE(refs.door_positions.at(door.first).x(), level.vertices[door.second].x);
          QCOMPARE(refs.door_positions.at(door.first).y(), level.vertices[door.second].y);
        }
      }
    };
    check_positions();
    const auto id = building.levels[0].vertices[0].uuid;
    MoveLiftCommand move(&building, "lift");
    move.setFinalDestination(110, 220);
    move.redo();
    building.syncDoorVertices();
    QCOMPARE(building.levels[0].vertices[0].x, 110.0);
    QCOMPARE(building.levels[0].vertices[0].y, 190.0);
    check_positions();
    QCOMPARE(building.levels[0].vertices[0].uuid, id);
    move.undo();
    building.syncDoorVertices();
    QCOMPARE(building.levels[0].vertices[0].x, 100.0);
    check_positions();
    building.syncDoorVertices();
    QCOMPARE(building.levels[1].vertices.size(), std::size_t(3));
    building.lifts[0].level_doors["L1"].clear();
    building.syncDoorVertices();
    QVERIFY(building.levels[1].vertices.empty());
    QVERIFY(building.levels[1].edges.empty());
    building.lifts.clear();
    building.syncDoorVertices();
    QVERIFY(building.levels[0].vertices.empty());
  }

  void interactiveDoorApproachVertices()
  {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QFile file(directory.filePath("door.building.yaml"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("name: test\ncoordinate_system: reference_image\n"
      "levels:\n  L1:\n    elevation: 0\n    x_meters: 20\n    y_meters: 20\n"
      "lifts: {}\n");
    file.close();
    const auto cwd = QDir::currentPath();
    QVERIFY(editor->loadBuilding(file.fileName()));
    QVERIFY(QDir::setCurrent(cwd));
    auto* view = editor->findChild<MapView*>();
    QVERIFY(view);
    QToolButton* door = nullptr;
    for (auto* button : editor->findChildren<QToolButton*>())
      if (button->accessibleName() == "Add Door (D)")
        door = button;
    QVERIFY(door);
    door->click();
    const auto click = [&](const QPointF& point)
      {
        QTest::mouseClick(view->viewport(), Qt::LeftButton,
          Qt::NoModifier, view->mapFromScene(point));
      };
    click(QPointF(50, 50));
    click(QPointF(90, 50));
    QAction* save = nullptr;
    QAction* undo = nullptr;
    QAction* redo = nullptr;
    for (auto* action : editor->findChildren<QAction*>())
    {
      if (action->text() == "&Save") save = action;
      if (action->text().contains("Undo")) undo = action;
      if (action->text().contains("Redo")) redo = action;
    }
    QVERIFY(save && undo && redo);
    const auto count_vertices = [&]()
      {
        save->trigger();
        return YAML::LoadFile(directory.filePath("door.building.json")
          .toStdString())["levels"]["L1"]["vertices"].size();
      };
    QCOMPARE(count_vertices(), std::size_t(4));
    const auto saved = YAML::LoadFile(directory.filePath("door.building.json").toStdString());
    QCOMPARE(saved["levels"]["L1"]["doors"].size(), std::size_t(1));
    QCOMPARE(saved["levels"]["L1"]["lanes"].size(), std::size_t(1));
    undo->trigger();
    QCOMPARE(count_vertices(), std::size_t(0));
    redo->trigger();
    QCOMPARE(count_vertices(), std::size_t(4));
  }

  void cleanupTestCase()
  {
    printf("cleanupTestCase()\n");
    delete editor;
  }

private:
  Editor* editor = nullptr;
};

QTEST_MAIN(TestGui)
#include "test_gui.moc"
