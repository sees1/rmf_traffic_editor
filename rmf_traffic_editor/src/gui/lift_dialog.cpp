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

#include "lift_dialog.h"
#include <cfloat>
#include <QtWidgets>
using std::vector;


LiftDialog::LiftDialog(Lift& lift, Building& building)
: QDialog(),
  lift_(lift),
  building_(building)
{
  setWindowTitle("Lift Properties");

  for (const auto& level : building.levels)
    level_names_.push_back(QString::fromStdString(level.name));

  QHBoxLayout* bottom_buttons_hbox = new QHBoxLayout;
  ok_button_ = new QPushButton("OK", this);  // first button = [enter] button
  cancel_button_ = new QPushButton("Cancel", this);

  connect(ok_button_, &QAbstractButton::clicked, this, &LiftDialog::okButtonClicked);
  connect(cancel_button_, &QAbstractButton::clicked, this, &QDialog::reject);
  
  bottom_buttons_hbox->addWidget(ok_button_);
  bottom_buttons_hbox->addWidget(cancel_button_);

  QHBoxLayout* name_hbox = new QHBoxLayout;
  name_line_edit_ = new QLineEdit(QString::fromStdString(lift_.name), this);
  
  connect(name_line_edit_, &QLineEdit::textEdited,
  [this](const QString& text)
  {
    lift_.name = text.toStdString();
    updateLiftView();
    emit redraw();
  });
  
  name_hbox->addWidget(new QLabel("Name:"));
  name_hbox->addWidget(name_line_edit_);

  QHBoxLayout* ref_name_hbox = new QHBoxLayout;
  reference_floor_combo_box_ = new QComboBox;
  for (const QString& level_name : level_names_)
    reference_floor_combo_box_->addItem(level_name);
  reference_floor_combo_box_->setCurrentText(QString::fromStdString(lift_.reference_floor_name));

  connect(reference_floor_combo_box_, &QComboBox::currentTextChanged,
  [this](const QString& text)
  {
    lift_.reference_floor_name = text.toStdString();
    emit redraw();
  });

  ref_name_hbox->addWidget(new QLabel("Reference floor:"));
  ref_name_hbox->addWidget(reference_floor_combo_box_);

  QHBoxLayout* init_floor_hbox = new QHBoxLayout;
  initial_floor_combo_box_ = new QComboBox;

  for (const QString& level_name : level_names_)
    initial_floor_combo_box_->addItem(level_name);
  initial_floor_combo_box_->setCurrentText(QString::fromStdString(lift_.initial_floor_name));

  connect(initial_floor_combo_box_, &QComboBox::currentTextChanged,
  [this](const QString& text)
  {
    lift_.initial_floor_name = text.toStdString();
    emit redraw();
  });
    
  init_floor_hbox->addWidget(new QLabel("Initial floor:"));
  init_floor_hbox->addWidget(initial_floor_combo_box_);

  QHBoxLayout* highest_name_hbox = new QHBoxLayout;
  highest_floor_combo_box_ = new QComboBox;  
  
  for (const QString& level_name : level_names_)
    highest_floor_combo_box_->addItem(level_name);
  
  highest_floor_combo_box_->addItem("");  // empty string for not specifying
  highest_floor_combo_box_->setCurrentText(QString::fromStdString(lift_.highest_floor));
  
  connect(highest_floor_combo_box_, &QComboBox::currentTextChanged,
  [this](const QString& text)
  {
    lift_.highest_floor = text.toStdString();

    if (lift_.highest_floor.empty())
      lift_.highest_elevation = DBL_MAX;
    else
    {
      for (const auto& level : building_.levels)
      {
        if (level.name == lift_.highest_floor)
        {
          lift_.highest_elevation = level.elevation;
          break;
        }
      }
    }

    updateLevelTable();
    emit redraw();
  });

  highest_name_hbox->addWidget(new QLabel("Highest floor:"));
  highest_name_hbox->addWidget(highest_floor_combo_box_);

  QHBoxLayout* lowest_name_hbox = new QHBoxLayout;
  lowest_floor_combo_box_ = new QComboBox;

  for (const QString& level_name : level_names_)
    lowest_floor_combo_box_->addItem(level_name);

  lowest_floor_combo_box_->addItem("");
  lowest_floor_combo_box_->setCurrentText(QString::fromStdString(lift_.lowest_floor));
  connect(lowest_floor_combo_box_, &QComboBox::currentTextChanged,
  [this](const QString& text)
  {
    lift_.lowest_floor = text.toStdString();
    if (lift_.lowest_floor.empty())
      lift_.lowest_elevation = -DBL_MAX;
    else
    {
      for (const auto& level : building_.levels)
      {
        if (level.name == lift_.lowest_floor)
        {
          lift_.lowest_elevation = level.elevation;
          break;
        }
      }
    }
    updateLevelTable();
    emit redraw();
  });

  lowest_name_hbox->addWidget(new QLabel("Lowest floor:"));
  lowest_name_hbox->addWidget(lowest_floor_combo_box_);

  QHBoxLayout* x_hbox = new QHBoxLayout;
  x_line_edit_ = new QLineEdit(QString::number(lift_.pose.translation().x()), this);
  
  connect(x_line_edit_, &QLineEdit::textEdited,
  [this](const QString& text)
  {
    lift_.pose.translation().x() = text.toDouble();
    updateLiftWps();
  });

  lowest_name_hbox->addWidget(new QLabel("Lowest floor:"));
  x_hbox->addWidget(x_line_edit_);

  QHBoxLayout* y_hbox = new QHBoxLayout;
  y_line_edit_ = new QLineEdit(QString::number(lift_.pose.translation().y()), this);

  connect(y_line_edit_, &QLineEdit::textEdited,
  [this](const QString& text)
  {
    lift_.pose.translation().y() = text.toDouble();
    updateLiftWps();
  });
    
  y_hbox->addWidget(new QLabel("Y:"));
  y_hbox->addWidget(y_line_edit_);

  QHBoxLayout* yaw_hbox = new QHBoxLayout;
  yaw_line_edit_ = new QLineEdit(QString::number(std::atan2(lift_.pose.linear()(1, 0), lift_.pose.linear()(0, 0))), this);
  connect(yaw_line_edit_, &QLineEdit::textEdited,
  [this](const QString& text)
  {
    lift_.pose.linear() = Eigen::Rotation2Dd(text.toDouble()).toRotationMatrix();
    updateLiftView();
    emit redraw();
  });

  yaw_hbox->addWidget(new QLabel("Yaw:"));
  yaw_hbox->addWidget(yaw_line_edit_);

  QHBoxLayout* width_hbox = new QHBoxLayout;
  width_line_edit_ = new QLineEdit(QString::number(lift_.width), this);
  
  connect(width_line_edit_, &QLineEdit::textEdited,
  [this](const QString& text)
  {
    lift_.width = text.toDouble();
    updateLiftView();
    emit redraw();
  });

  width_hbox->addWidget(new QLabel("Cabin width:"));
  width_hbox->addWidget(width_line_edit_);

  QHBoxLayout* depth_hbox = new QHBoxLayout;
  depth_line_edit_ = new QLineEdit(QString::number(lift_.depth), this);
  
  connect(depth_line_edit_, &QLineEdit::textEdited,
  [this](const QString& text)
  {
    lift_.depth = text.toDouble();
    updateLiftView();
    emit redraw();
  });
  depth_hbox->addWidget(new QLabel("Cabin depth:"));
  depth_hbox->addWidget(depth_line_edit_);

  QHBoxLayout* plugin_hbox = new QHBoxLayout;
  plugin_yes_radio_button_ = new QRadioButton("Yes");
  plugin_no_radio_button_ = new QRadioButton("No");

  if (lift_.plugins)
    plugin_yes_radio_button_->setChecked(true);
  else
    plugin_no_radio_button_->setChecked(true);

  plugin_hbox->addWidget(new QLabel("Plugin:"));
  plugin_hbox->addWidget(plugin_yes_radio_button_);
  plugin_hbox->addWidget(plugin_no_radio_button_);

  QHBoxLayout* add_wp_hbox = new QHBoxLayout;
  add_wp_button_ = new QPushButton("Add lift waypoints", this);
  add_wp_hbox->addWidget(add_wp_button_);
  connect(add_wp_button_, &QAbstractButton::clicked, this, &LiftDialog::updateLiftWps);

  level_table_ = new QTableWidget;
  level_table_->setMinimumSize(200, 200);
  level_table_->verticalHeader()->setVisible(false);
  level_table_->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

  level_table_->setHorizontalHeaderItem(0, new QTableWidgetItem("Level name"));

  door_table_ = new QTableWidget;
  door_table_->setMinimumSize(400, 200);
  door_table_->verticalHeader()->setVisible(false);
  door_table_->setColumnCount(6);
  door_table_->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

  door_table_->setHorizontalHeaderItem(0, new QTableWidgetItem("Door name"));
  door_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

  door_table_->setHorizontalHeaderItem(1, new QTableWidgetItem("Door type"));
  door_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

  door_table_->setHorizontalHeaderItem(2, new QTableWidgetItem("X"));
  door_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

  door_table_->setHorizontalHeaderItem(3, new QTableWidgetItem("Y"));
  door_table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

  door_table_->setHorizontalHeaderItem(4, new QTableWidgetItem("Orientation"));
  door_table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

  door_table_->setHorizontalHeaderItem(5, new QTableWidgetItem("Width"));
  door_table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);

  door_table_->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

  QVBoxLayout* left_vbox = new QVBoxLayout;
  left_vbox->addLayout(name_hbox);
  left_vbox->addLayout(ref_name_hbox);
  left_vbox->addLayout(highest_name_hbox);
  left_vbox->addLayout(lowest_name_hbox);
  left_vbox->addLayout(init_floor_hbox);
  left_vbox->addLayout(x_hbox);
  left_vbox->addLayout(y_hbox);
  left_vbox->addLayout(yaw_hbox);
  left_vbox->addLayout(width_hbox);
  left_vbox->addLayout(depth_hbox);
  left_vbox->addLayout(plugin_hbox);
  left_vbox->addLayout(add_wp_hbox);
  left_vbox->addWidget(level_table_);

  QVBoxLayout* right_vbox = new QVBoxLayout;

  lift_scene_ = new QGraphicsScene;

  lift_view_ = new QGraphicsView;
  lift_view_->setScene(lift_scene_);
  lift_view_->setMinimumSize(400, 400);
  right_vbox->addWidget(lift_view_, 1);

  right_vbox->addWidget(door_table_);

  QHBoxLayout* top_hbox = new QHBoxLayout;
  top_hbox->addLayout(left_vbox);
  top_hbox->addLayout(right_vbox, 1);

  QVBoxLayout* top_vbox = new QVBoxLayout;
  top_vbox->addLayout(top_hbox);
  // todo: some sort of separator (?)
  top_vbox->addLayout(bottom_buttons_hbox);

  setLayout(top_vbox);

  name_line_edit_->setFocus(Qt::OtherFocusReason);

  updateDoorTable();
  updateLevelTable();

  connect(door_table_, &QTableWidget::cellChanged, this, &LiftDialog::doorTableCellChanged);

  updateLiftView();
  adjustSize();
}

LiftDialog::~LiftDialog()
{
}

void LiftDialog::okButtonClicked()
{
  if (name_line_edit_->text().isEmpty())
  {
    QMessageBox::critical(this, "Error", "Lift name is empty");
    return;
  }

  if (lift_.lowest_elevation > lift_.highest_elevation)
  {
    QMessageBox::critical(this, "Error", "Lowest floor above highest floor");
    return;
  }

  lift_.name = name_line_edit_->text().toStdString();
  lift_.reference_floor_name = reference_floor_combo_box_->currentText().toStdString();
  lift_.highest_floor = highest_floor_combo_box_->currentText().toStdString();
  lift_.lowest_floor = lowest_floor_combo_box_->currentText().toStdString();
  lift_.initial_floor_name = initial_floor_combo_box_->currentText().toStdString();

  lift_.pose.translation().x() = x_line_edit_->text().toDouble();
  lift_.pose.translation().y() = y_line_edit_->text().toDouble();
  lift_.pose.linear() = Eigen::Rotation2Dd(yaw_line_edit_->text().toDouble()).toRotationMatrix();

  lift_.width = width_line_edit_->text().toDouble();
  lift_.depth = depth_line_edit_->text().toDouble();

  if (plugin_yes_radio_button_->isChecked())
    lift_.plugins = true;
  else if (plugin_no_radio_button_->isChecked())
    lift_.plugins = false;

  // grab all the level-door checkbox matrix states and save them
  for (int level_row = 0; level_row < level_table_->rowCount(); level_row++)
  {
    const std::string level_name = level_table_->item(level_row, 0)->text().toStdString();
    
    lift_.level_doors.erase(level_name);
    
    for (int door_col = 1; door_col < level_table_->columnCount(); door_col++)
    {
      const std::string door_name = level_table_->horizontalHeaderItem(door_col)->text().toStdString();
      const QWidget* widget = level_table_->cellWidget(level_row, door_col);
      const QCheckBox* checkbox = qobject_cast<const QCheckBox*>(widget);
      
      if (checkbox)
      {
        const bool checked = checkbox->isChecked();
        printf("level %s door %s: %d\n", level_name.c_str(), door_name.c_str(), checked ? 1 : 0);
        
        if (checked)
          lift_.level_doors[level_name].push_back(door_name);
      }
      else
      {
        printf("level %s door %s: indeterminate state!\n",
          level_name.c_str(),
          door_name.c_str());
      }
    }
  }

  updateLiftView();
  emit redraw();
  accept();
}

void LiftDialog::updateLiftWps()
{
  int reference_lvl = -1;
  auto reference_it = std::find_if(building_.levels.begin(), building_.levels.end(),
  [this](auto&& level)
  {
    return level.name == lift_.reference_floor_name;
  });
  if (reference_it != building_.levels.end())
    reference_lvl = std::distance(building_.levels.begin(), reference_it);

  building_.updateLiftGraph(&lift_, reference_lvl);
  emit redraw();
}

void LiftDialog::updateDoorTable()
{
  door_table_->setRowCount(1 + lift_.doors.size());
  for (std::size_t i = 0; i < lift_.doors.size(); i++)
  {
    const LiftDoor& door = lift_.doors[i];  // save some typing
    setDoorCell(i, 0, QString::fromStdString(door.name));

    // set the numeric fields
    setDoorCell(i, 2, QString::number(door.x));
    setDoorCell(i, 3, QString::number(door.y));
    setDoorCell(i, 4, QString::number(door.motion_axis_orientation));
    door_table_->setCellWidget(i, 5, nullptr);
    setDoorCell(i, 5, QString::number(door.width));

    // create a drop-down list for the door type
    QComboBox* type_box = new QComboBox;
    type_box->addItem("<undefined>", QVariant(0));
    type_box->addItem("Single sliding", QVariant(1));
    type_box->addItem("Double sliding", QVariant(2));
    type_box->addItem("Single telescoping", QVariant(3));
    type_box->addItem("Double telescoping", QVariant(4));
    type_box->setCurrentIndex(static_cast<int>(door.door_type));

    door_table_->setCellWidget(i, 1, type_box);
  }

  // we'll use the last row for the "Add" button
  const int last_row_idx = static_cast<int>(lift_.doors.size());
  door_table_->setCellWidget(last_row_idx, 0, nullptr);
  door_table_->setCellWidget(last_row_idx, 1, nullptr);
  door_table_->setCellWidget(last_row_idx, 2, nullptr);
  door_table_->setCellWidget(last_row_idx, 3, nullptr);
  door_table_->setCellWidget(last_row_idx, 4, nullptr);
  QPushButton* add_button = new QPushButton("Add...", this);
  door_table_->setCellWidget(last_row_idx, 5, add_button);
  connect(
    add_button,
    &QAbstractButton::clicked,
    [this]()
    {
      LiftDoor door;
      door.name = "name";
      door.door_type = LiftDoor::DOUBLE_SLIDING;
      lift_.doors.push_back(door);
      updateDoorTable();
      updateLiftView();
    });
}

void LiftDialog::setDoorCell(
  const int row,
  const int col,
  const QString& text)
{
  door_table_->setItem(row, col, new QTableWidgetItem(text));
}

void LiftDialog::updateLevelTable()
{
  level_table_->setColumnCount(1 + static_cast<int>(lift_.doors.size()));
  level_table_->setHorizontalHeaderItem(0, new QTableWidgetItem("Level"));
  for (std::size_t door_idx = 0; door_idx < lift_.doors.size(); door_idx++)
  {
    level_table_->setHorizontalHeaderItem(
      door_idx + 1,
      new QTableWidgetItem(
        QString::fromStdString(lift_.doors[door_idx].name)));
  }
  //blockSignals(true);
  level_table_->setRowCount(level_names_.size());
  for (std::size_t level_idx = 0; level_idx < level_names_.size(); level_idx++)
  {
    const QString& level_name = level_names_[level_idx];
    QTableWidgetItem* name_item = new QTableWidgetItem(level_name);
    name_item->setFlags(name_item->flags() & ~Qt::ItemIsEditable);
    level_table_->setItem(level_idx, 0, name_item);

    for (std::size_t door_idx = 0; door_idx < lift_.doors.size(); door_idx++)
    {
      QCheckBox* checkbox = new QCheckBox;
      checkbox->setStyleSheet("margin-left: 50%; margin-right: 50%");
      if (lift_.levelDoorOpens(
          level_name.toStdString(),
          lift_.doors[door_idx].name,
          building_.levels))
        checkbox->setChecked(true);
      level_table_->setCellWidget(level_idx, door_idx + 1, checkbox);
    }
  }
  //blockSignals(false);
}

void LiftDialog::doorTableCellChanged(int row, int col)
{
  // printf("door_table_cell_changed(%d, %d)\n", row, col);
  if (row >= static_cast<int>(lift_.doors.size()))
  {
    printf("invalid door row: %d\n", row);
    return;  // let's not crash
  }

  // If a door name was changed, we need to update the options shown in all
  // the level_table combo boxes
  if (col == 0)  // name
  {
    lift_.doors[row].name = door_table_->item(row, col)->text().toStdString();
    updateLevelTable();
  }
  else if (col == 1)
  {
    // todo: door type
  }
  else if (col == 2) // x
    lift_.doors[row].x = door_table_->item(row, col)->text().toDouble();
  else if (col == 3) // y
    lift_.doors[row].y = door_table_->item(row, col)->text().toDouble();
  else if (col == 4) // orientation
    lift_.doors[row].motion_axis_orientation =
      door_table_->item(row, col)->text().toDouble();
  else if (col == 5) // width
    lift_.doors[row].width = door_table_->item(row, col)->text().toDouble();

  updateLiftView();
  emit redraw();
}

void LiftDialog::updateLiftView()
{
  int reference_lvl = -1;
  auto reference_it = std::find_if(building_.levels.begin(), building_.levels.end(),
  [this](auto&& level)
  {
    return level.name == lift_.reference_floor_name;
  });
  if (reference_it != building_.levels.end())
    reference_lvl = std::distance(building_.levels.begin(), reference_it);

  building_.updateLiftGraph(&lift_, reference_lvl);

  lift_scene_->clear();
  lift_.draw(lift_scene_, 0.01, std::string(), lift_.lowest_elevation, false);
}
