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

#include "condition_dialog.h"

using namespace crowd_sim;

//=========================================================
ConditionDialog::ConditionDialog(
  CrowdSimImplPtr crowd_sim_impl,
  const std::string& dialog_title,
  Transition& transition)
: CrowdSimDialog(crowd_sim_impl, dialog_title),
  _current_transition(transition)
{
  std::string title = "from_state:" + transition.getFromState();
  setWindowTitle(QString::fromStdString(title) );

  _root_value = new QLineEdit(QString::number(_root_value_d));
  _root_condition_value_container = new QWidget;
  _condition1_container = new QWidget;
  _condition2_container = new QWidget;
  _condition1_value = new QLineEdit(QString::number(_condition1_value_d));
  _condition2_value = new QLineEdit(QString::number(_condition2_value_d));

  QHBoxLayout* root_condition_type = new QHBoxLayout;
  _root_type = new QComboBox;
  constructRootConditionType(
    _root_type,
    _current_transition.getCondition()->getType() );
  root_condition_type->addWidget(new QLabel("Condition type:"));
  root_condition_type->addWidget(_root_type);

  QHBoxLayout* root_condition_value = new QHBoxLayout(
    _root_condition_value_container);
  root_condition_value->addWidget(new QLabel(
      "Value (duration(s) / goal distance(m))"));
  root_condition_value->addWidget(_root_value);
  connect(
    _root_value,
    &QLineEdit::editingFinished,
    [&]()
    {
      bool OK_status;
      double temp = _root_value->text().toDouble(&OK_status);
      if (!OK_status)
      {
        std::cout << "Invalid condition value input!" << std::endl;
        return;
      }
      auto root_condition = _current_transition.getCondition();
      if (crowd_sim::Condition::GOAL == root_condition->getType())
      {
        auto goal_condition =
        std::dynamic_pointer_cast<crowd_sim::ConditionGOAL>(root_condition);
        goal_condition->setValue(temp);
      }
      if (crowd_sim::Condition::TIMER == root_condition->getType())
      {
        auto timer_condition =
        std::dynamic_pointer_cast<crowd_sim::ConditionTIMER>(root_condition);
        timer_condition->setValue(temp);
      }
    }
  );

  _condition1_type = new QComboBox;
  _condition2_type = new QComboBox;
  constructLeafConditionWidget(_condition1_container, _condition1_type,
    _condition1_value, 1);
  constructLeafConditionWidget(_condition2_container, _condition2_type,
    _condition2_value, 2);

  top_vbox->addLayout(root_condition_type);
  top_vbox->addWidget(_root_condition_value_container);
  top_vbox->addWidget(_condition1_container);
  top_vbox->addWidget(_condition2_container);
  top_vbox->addLayout(bottom_buttons_hbox);

  update();
}

//=========================================================
void ConditionDialog::save()
{
  if (_root_condition_value_container->isEnabled())
  {
    _root_value->editingFinished();
  }
  if (_condition1_container->isEnabled())
  {
    _condition1_value->editingFinished();
  }
  if (_condition2_container->isEnabled())
  {
    _condition2_value->editingFinished();
  }
}

//========================================================
void ConditionDialog::update()
{
  auto root_condition = _current_transition.getCondition();
  _root_type->setCurrentIndex(root_condition->getType());

  if (Condition::BASE == root_condition->getType())
    return;

  if (Condition::GOAL == root_condition->getType() ||
    Condition::TIMER == root_condition->getType() )
  {
    auto root_condition_tmp =
      std::dynamic_pointer_cast<crowd_sim::LeafCondition>(root_condition);
    _root_value_d = root_condition_tmp->getValue();
    _root_value->setText(QString::number(_root_value_d));
    return;
  }

  auto root_condition_tmp = std::dynamic_pointer_cast<crowd_sim::BoolCondition>(
    root_condition);
  auto sub_condition_1 = root_condition_tmp->getCondition(1);
  _condition1_type->setCurrentIndex(
    sub_condition_1->getType() - crowd_sim::Condition::GOAL);
  _condition1_value_d = std::dynamic_pointer_cast<crowd_sim::LeafCondition>(
    sub_condition_1)->getValue();
  _condition1_value->setText(QString::number(_condition1_value_d));

  if (Condition::NOT == root_condition->getType())
    return;
  auto sub_condition_2 = root_condition_tmp->getCondition(2);
  _condition2_type->setCurrentIndex(
    sub_condition_2->getType()- crowd_sim::Condition::GOAL);
  _condition2_value_d = std::dynamic_pointer_cast<crowd_sim::LeafCondition>(
    sub_condition_2)->getValue();
  _condition2_value->setText(QString::number(_condition2_value_d));
}

//=========================================================
void ConditionDialog::constructRootConditionType(
  QComboBox* root_type,
  Condition::TYPE current_type)
{
  rootIsInvalid();
  if (Condition::GOAL == current_type ||
    Condition::TIMER == current_type)
    rootIsLeafCondition();
  if (Condition::AND == current_type ||
    Condition::OR == current_type)
    rootIsBool2Condition();
  if (Condition::NOT == current_type)
    rootIsNotCondition();

  root_type->addItem("base_condition(invalid)");
  root_type->addItem("goal_reached");
  root_type->addItem("timer");
  root_type->addItem("and");
  root_type->addItem("or");
  root_type->addItem("not");

  connect(
    root_type,
    QOverload<int>::of(&QComboBox::activated),
    [this](int index)
    {
      switch (index)
      {
        case 1:
          rootIsLeafCondition();
          if (_current_transition.getCondition()->getType() !=
          crowd_sim::Condition::GOAL)
            _current_transition.setCondition(
              std::make_shared<crowd_sim::ConditionGOAL>() );
          break;
        case 2:
          rootIsLeafCondition();
          if (_current_transition.getCondition()->getType() !=
          crowd_sim::Condition::TIMER)
            _current_transition.setCondition(
              std::make_shared<crowd_sim::ConditionTIMER>() );
          break;
        case 3:
          rootIsBool2Condition();
          if (_current_transition.getCondition()->getType() !=
          crowd_sim::Condition::AND)
            _current_transition.setCondition(
              std::make_shared<crowd_sim::ConditionAND>() );
          break;
        case 4:
          rootIsBool2Condition();
          if (_current_transition.getCondition()->getType() !=
          crowd_sim::Condition::OR)
            _current_transition.setCondition(
              std::make_shared<crowd_sim::ConditionOR>() );
          break;
        case 5:
          rootIsNotCondition();
          if (_current_transition.getCondition()->getType() !=
          crowd_sim::Condition::NOT)
            _current_transition.setCondition(
              std::make_shared<crowd_sim::ConditionNOT>() );
          break;
        default:
          rootIsInvalid();
      }
    }
  );
}

//=======================================
void ConditionDialog::rootIsInvalid()
{
  _root_condition_value_container->setEnabled(false);
  _condition1_container->setEnabled(false);
  _condition2_container->setEnabled(false);
}
//=======================================
void ConditionDialog::rootIsLeafCondition()
{
  _root_condition_value_container->setEnabled(true);
  _condition1_container->setEnabled(false);
  _condition2_container->setEnabled(false);
}

//=======================================
void ConditionDialog::rootIsBool2Condition()
{
  _root_condition_value_container->setEnabled(false);
  _condition1_container->setEnabled(true);
  _condition2_container->setEnabled(true);
}

//=======================================
void ConditionDialog::rootIsNotCondition()
{
  _root_condition_value_container->setEnabled(false);
  _condition1_container->setEnabled(true);
  _condition2_container->setEnabled(false);
}

//=========================================
void ConditionDialog::constructLeafConditionWidget(
  QWidget* condition_container,
  QComboBox* condition_type,
  QLineEdit* condition_value,
  int condition_index)
{
  QHBoxLayout* condition_hbox = new QHBoxLayout(condition_container);
  condition_type->addItem("goal_reached");
  condition_type->addItem("timer");

  if (condition_index == 1)
    condition_value->setText(QString::number(_condition1_value_d));
  else
    condition_value->setText(QString::number(_condition2_value_d));
  std::string condition_label = "condition" + std::to_string(condition_index) +
    ":";
  condition_hbox->addWidget(new QLabel(QString::fromStdString(
      condition_label) ));
  condition_hbox->addWidget(condition_type);
  condition_hbox->addWidget(condition_value);
  connect(
    condition_value,
    &QLineEdit::editingFinished,
    [this, condition_type, condition_value, condition_index]()
    {
      bool OK_status;
      double temp_value = condition_value->text().toDouble(&OK_status);
      if (!OK_status)
      {
        std::cout << "Invalid condition value input!" << std::endl;
        return;
      }
      if (condition_type->currentIndex() == 0)
      {
        setSubConditionInRootCondition(
          crowd_sim::Condition::GOAL,
          temp_value,
          condition_index);
      }
      if (condition_type->currentIndex() == 1)
      {
        setSubConditionInRootCondition(
          crowd_sim::Condition::TIMER,
          temp_value,
          condition_index);
      }
    }
  );
}

//======================================================
void ConditionDialog::setSubConditionInRootCondition(
  crowd_sim::Condition::TYPE type,
  double value,
  int condition_index)
{
  auto root_condition = _current_transition.getCondition();
  if (root_condition->getType() == Condition::GOAL ||
    root_condition->getType() == Condition::TIMER)
    return;
  if (root_condition->getType() == Condition::NOT)
    condition_index = 1;
  auto bool_root_condition =
    std::dynamic_pointer_cast<crowd_sim::BoolCondition>(root_condition);

  if (type == Condition::GOAL)
  {
    auto sub_condition = std::make_shared<crowd_sim::ConditionGOAL>();
    sub_condition->setValue(value);
    bool_root_condition->setCondition(sub_condition, condition_index);
  }
  if (type == crowd_sim::Condition::TIMER)
  {
    auto sub_condition = std::make_shared<crowd_sim::ConditionTIMER>();
    sub_condition->setValue(value);
    bool_root_condition->setCondition(sub_condition, condition_index);
  }
}

//===================================================
void ConditionDialog::okButtonClick()
{
  save();
  accept();
}

