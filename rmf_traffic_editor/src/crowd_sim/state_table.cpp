/*
 * Copyright (C) 2020 Open Source Robotics Foundation
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

#include <QWidget>
#include <QComboBox>

#include "state_table.h"

using namespace crowd_sim;

//========================================
std::shared_ptr<StatesTab> StatesTab::initAndMake(
  CrowdSimImplPtr crowd_sim_impl)
{
  const QStringList labels =
  { "Name", "Is Final", "Navmesh File Name", "Goal Set Id", ""};

  auto state_tab_ptr = std::make_shared<StatesTab>(crowd_sim_impl, labels);
  if (!state_tab_ptr)
  {
    printf("Failed to create state table! Exiting");
    return nullptr;
  }
  return state_tab_ptr;
}

//========================================
void StatesTab::listItemInCache()
{
  auto cache_count = _cache.size();

  //row 0 for external_state
  auto external_state = _cache[0];
  setItem(0, 0,
    new QTableWidgetItem(QString::fromStdString(external_state.getName() )));
  setItem(0, 1,
    new QTableWidgetItem(QString::number(
      external_state.getFinalState() ? 1 : 0)));

  for (size_t i = 1; i < cache_count; i++)
  {
    auto& current_state = _cache.at(i);
    setItem(i, 0,
      new QTableWidgetItem(QString::fromStdString(current_state.getName()) ) );

    QComboBox* final_state_combo = new QComboBox;
    listFinalStatesInCombo(final_state_combo,
      current_state.getFinalState());
    setCellWidget(i, 1, final_state_combo);

    QComboBox* navmesh_list_combo = new QComboBox;
    listNavmeshFileInCombo(navmesh_list_combo,
      current_state.getNavmeshFileName() );
    setCellWidget(i, 2, navmesh_list_combo);

    QComboBox* goal_set_combo = new QComboBox;
    listGoalSetsInCombo(goal_set_combo, current_state.getGoalSetId());
    setCellWidget(i, 3, goal_set_combo);
  }
}

//========================================
void StatesTab::addButtonClick()
{
  _cache.emplace_back("new_state");
}

//========================================
void StatesTab::deleteButtonClick(size_t row_number)
{
  if (row_number == 0)
  {
    std::cout <<
      "Default state for external agent is not allowed to be deleted." <<
      std::endl;
    return;
  }
  if (row_number > _cache.size())
    return;
  _cache.erase(_cache.begin() + row_number);
}

//========================================
void StatesTab::save()
{
  auto rows_count = rowCount();
  std::vector<crowd_sim::State> tmp_cache;
  //row 0 is not allowed to be changed
  tmp_cache.emplace_back("external_static");

  //check duplicate names
  std::set<std::string> saved_names;

  for (auto i = 1; i < rows_count - 1; i++)
  {
    auto name_item = item(i, 0)->text().toStdString();

    auto final_item =
      static_cast<QComboBox*>(cellWidget(i, 1))->currentText().toStdString();

    auto navmesh_file_name =
      static_cast<QComboBox*>(cellWidget(i, 2))->currentText().toStdString();

    bool OK_status;
    auto goal_set_id =
      static_cast<QComboBox*>(cellWidget(i,
      3))->currentText().toInt(&OK_status);

    if (saved_names.find(name_item) != saved_names.end())
    {
      std::cout << "Defined duplicate state name for [" <<
        name_item << "]. Skip saving this state." <<
        std::endl;
      continue;
    }

    tmp_cache.emplace_back(name_item);
    auto& cur_it = tmp_cache.back();
    cur_it.setFinalState(final_item == "1");
    cur_it.setNavmeshFileName(navmesh_file_name);
    cur_it.setGoalSetId(static_cast<size_t>(goal_set_id));
    saved_names.insert(name_item);
  }

  _cache = tmp_cache;
}

//========================================
void StatesTab::saveToImpl()
{
  save();
  getImpl()->saveStates(_cache);
}

//========================================
void StatesTab::listGoalSetsInCombo(
  QComboBox* comboBox,
  size_t current_goal_set_id)
{
  for (auto goal_set : getImpl()->getGoalSets())
  {
    comboBox->addItem(QString::number(
        static_cast<int>(goal_set.getGoalSetId())));
  }
  auto index =
    comboBox->findText(QString::number(static_cast<int>(current_goal_set_id) ));
  if (index >= 0)
  {
    comboBox->setCurrentIndex(index);
  }
}

//========================================
void StatesTab::listFinalStatesInCombo(
  QComboBox* comboBox,
  bool current_state)
{
  comboBox->addItem("1");
  comboBox->addItem("0");
  if (!current_state)
  {
    comboBox->setCurrentIndex(1);
  }
  else
  {
    comboBox->setCurrentIndex(0);
  }
}

//========================================
void StatesTab::listNavmeshFileInCombo(
  QComboBox* comboBox,
  std::string navmesh_filename)
{
  auto navmesh_list = getImpl()->getNavmeshFileName();
  for (size_t i = 0; i < navmesh_list.size(); i++)
  {
    comboBox->addItem(QString::fromStdString(navmesh_list[i]));
    if (navmesh_list[i] == navmesh_filename)
    {
      comboBox->setCurrentIndex(i);
    }
  }
}