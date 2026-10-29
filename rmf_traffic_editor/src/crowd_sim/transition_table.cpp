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
#include <QPushButton>

#include "transition_table.h"
#include "to_state_dialog.h"
#include "condition_dialog.h"

//==================================================
std::shared_ptr<TransitionTab> TransitionTab::initAndMake(
  CrowdSimImplPtr crowd_sim_impl)
{
  const QStringList labels =
  { "From State", "To State", "To-State edit", "Condition", "Condition edit",
    ""};

  auto transition_tab_ptr = std::make_shared<TransitionTab>(crowd_sim_impl,
      labels);
  if (!transition_tab_ptr)
  {
    printf("Failed to create goal set table! Exiting");
    return nullptr;
  }
  return transition_tab_ptr;
}

//==================================================
void TransitionTab::listItemInCache()
{
  auto cache_count = _cache.size();
  for (size_t i = 0; i < cache_count; i++)
  {
    auto& transition = _cache.at(i);

    QComboBox* from_state_comboBox = new QComboBox;
    listFromStatesInCombo(from_state_comboBox, transition);
    setCellWidget(i, 0, from_state_comboBox);

    auto to_state = transition.getToState();
    std::string to_state_name = "";
    for (auto state : to_state)
    {
      to_state_name += state.first + ";";
    }
    setItem(i, 1, new QTableWidgetItem(QString::fromStdString(to_state_name)));

    QPushButton* to_state_edit = new QPushButton("Edit", this);
    setCellWidget(i, 2, to_state_edit);
    connect(
      to_state_edit,
      &QAbstractButton::clicked,
      [this, &transition]()
      {
        ToStateDialog to_state_dialog(getImpl(), "To_State", transition);
        to_state_dialog.exec();
        update();
      }
    );

    auto condition_name = transition.getCondition()->getConditionName();
    setItem(i, 3, new QTableWidgetItem(QString::fromStdString(condition_name)));

    QPushButton* condition_edit = new QPushButton("Edit", this);
    setCellWidget(i, 4, condition_edit);
    connect(
      condition_edit,
      &QAbstractButton::clicked,
      [this, &transition]()
      {
        ConditionDialog condition_dialog(getImpl(), "Condition", transition);
        condition_dialog.exec();
        update();
      }
    );
  }
}

//==================================================
void TransitionTab::listFromStatesInCombo(
  QComboBox* comboBox,
  crowd_sim::Transition& transition)
{
  for (auto state : getImpl()->getStates())
  {
    if (state.getFinalState())
    {
      continue;
    }
    comboBox->addItem(QString::fromStdString(state.getName() ) );
  }
  auto index =
    comboBox->findText(QString::fromStdString(transition.getFromState()) );
  comboBox->setCurrentIndex(index >= 0 ? index : 0);
  connect(
    comboBox,
    &QComboBox::currentTextChanged,
    [this](const QString& /*text*/)
    {
      save();
    }
  );
}

//==================================================
void TransitionTab::save()
{
  auto tmp_cache = _cache;
  auto row_count = rowCount();
  for (auto i = 0; i < row_count-1; i++)
  {
    auto& current_transition = tmp_cache.at(i);

    auto pItem_from_state = static_cast<QComboBox*>(cellWidget(i, 0));
    current_transition.setFromState(
      pItem_from_state->currentText().toStdString());
  }
  _cache = tmp_cache;
  update();
}

//==================================================
void TransitionTab::saveToImpl()
{
  save();
  std::vector<size_t> invalid_trasition;
  for (size_t i = 0; i < _cache.size(); i++)
  {
    if (!_cache[i].isValid())
    {
      std::cout << _cache[i].getFromState() <<
        _cache[i].getToState().size() <<
        _cache[i].getCondition()->getConditionName() << std::endl;
      invalid_trasition.push_back(i);
    }
  }
  while (!invalid_trasition.empty())
  {
    size_t index = invalid_trasition.back();
    _cache.erase(_cache.begin() + index);
    invalid_trasition.pop_back();
  }
  getImpl()->saveTransitions(_cache);
}

//==================================================
void TransitionTab::addButtonClick()
{
  _cache.emplace_back("");
}

//==================================================
void TransitionTab::deleteButtonClick(size_t row_number)
{
  if (row_number > _cache.size())
    return;
  _cache.erase(_cache.begin() + row_number);
}
