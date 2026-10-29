/*
 * Copyright (C) 2020-2021 Open Source Robotics Foundation
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

#ifndef LIFT_DIALOG_H
#define LIFT_DIALOG_H

#include <vector>

#include <QDialog>
#include <QObject>
#include <QRadioButton>

#include "lift.h"
#include "building.h"

class QLineEdit;
class QLabel;
class QTableWidget;
class QComboBox;

class LiftDialog : public QDialog
{
  Q_OBJECT

public:
  LiftDialog(Lift& lift, Building& building);

  ~LiftDialog();

signals:
  void redraw();

private:
  void updateDoorTable();

  void setDoorCell(const int row, const int col, const QString& text);

  void doorTableCellChanged(int row, int col);

  void updateLevelTable();

  void updateLiftView();

  void updateLiftWps();

private slots:
  void okButtonClicked();

private:
  Lift& lift_;
  Building& building_;

  std::vector<QString> level_names_;

  QLineEdit* name_line_edit_;

  QComboBox* reference_floor_combo_box_;
  QComboBox* highest_floor_combo_box_;
  QComboBox* lowest_floor_combo_box_;
  QComboBox* initial_floor_combo_box_;

  QLineEdit* x_line_edit_;
  QLineEdit* y_line_edit_;
  QLineEdit* yaw_line_edit_;
  QLineEdit* width_line_edit_;
  QLineEdit* depth_line_edit_;

  QRadioButton* plugin_yes_radio_button_;
  QRadioButton* plugin_no_radio_button_;

  QTableWidget* door_table_;
  QTableWidget* level_table_;

  QGraphicsView* lift_view_;

  QGraphicsScene* lift_scene_;

  QPushButton*  ok_button_;
  QPushButton * cancel_button_;
  QPushButton*  add_wp_button_;
};

#endif
