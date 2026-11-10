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
#include "lift_table.h"
#include <QtWidgets>

LiftTable::LiftTable()
: TableList(3)
{
  const QStringList labels = { "Name", "", ""};
  setHorizontalHeaderLabels(labels);
}

LiftTable::~LiftTable()
{
}

void LiftTable::update(Building& building)
{
  blockSignals(true);

  std::vector<std::shared_ptr<Lift>> dup_lifts;
  std::for_each(building.levels.begin(), building.levels.end(),
  [&dup_lifts](auto&& level)
  {
    auto level_lifts = level.getLifts();
    dup_lifts.insert(dup_lifts.end(), level_lifts.begin(), level_lifts.end());
  });

  std::sort(dup_lifts.begin(), dup_lifts.end());
  auto border_it = std::unique(dup_lifts.begin(), dup_lifts.end());
  dup_lifts.erase(border_it, dup_lifts.end());

  setRowCount(1 + dup_lifts.size());

  for (std::size_t i = 0; i < dup_lifts.size(); i++)
  {
    setItem(i, 0, new QTableWidgetItem(QString::fromStdString(dup_lifts[i].name)));

    QPushButton* edit_button = new QPushButton("Edit...", this);
    setCellWidget(i, 1, edit_button); 

    QPushButton* delete_button = new QPushButton("Delete", this);
    setCellWidget(i, 2, delete_button);

    connect(edit_button, &QAbstractButton::clicked,
    [this, &dup_lifts, i]()
    {
      LiftDialog* dialog = new LiftDialog(dup_lifts[i], building);
      dialog->show();
      dialog->raise();
      dialog->activateWindow();

      connect(dialog, &LiftDialog::redraw,
      [this]()
      {
        emit redraw();
      });
    });

    connect(delete_button, &QAbstractButton::clicked,
    [this, &dup_lifts, i]()
    {
      for (size_t j = 0; j < dup_lifts.size(); j++)
      {
        if (j == i)
        {
          auto lift_name = dup_lifts[j].name;

          for (auto&& level : levels)
          {
            auto remove_it = std::remove_if(level.lifts.begin(), level.lifts.end(), [lift_name](auto&& lift) { return lift.name == lift_name});

            std::set<int> remove_vertices;
            std::for_each(remove_it, lifts.end(),
            [&remove_vertices](auto&& lift)
            {
              auto lift_nav_vertices = lift->getNavVerticesIdx();
              remove_vertices.insert(remove_vertices.end(), lift_nav_vertices->begin(), lift_nav_vertices.end());
            });

            level.lifts.erase(remove_it, lift.end());

            building.updateGraphOnLevel(level, remove_vertices);
          }

          break;
        }
      }
      update(building);
      emit redraw();
    });
  }

  // we'll use the last row for the "Add" button
  const int last_row_idx = static_cast<int>(building.lifts.size());
  setCellWidget(last_row_idx, 0, nullptr);
  setCellWidget(last_row_idx, 1, nullptr);
  setItem(last_row_idx, 0, new QTableWidgetItem(QString::fromStdString("")));

  QPushButton* add_button = new QPushButton("Add...", this);
  
  setCellWidget(last_row_idx, 2, add_button);
  
  connect(add_button, &QAbstractButton::clicked,
  [this, &building]()
  {
    Lift lift;
    LiftDialog lift_dialog(lift, building);
    
    if (lift_dialog.exec() == QDialog::Accepted)
    {
      building.lifts.push_back(lift);
      update(building);
      emit redraw();
    }
  });

  blockSignals(false);
}
