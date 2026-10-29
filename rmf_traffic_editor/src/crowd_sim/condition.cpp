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

#include <crowd_sim/condition.h>

#include <iostream>

using namespace crowd_sim;

//===========================================================
ConditionPtr Condition::initFromYaml(const YAML::Node& input)
{
  if (input["type"] && input["type"].as<std::string>() == "goal_reached")
  {
    return std::make_shared<ConditionGOAL>();
  }
  if (input["type"] && input["type"].as<std::string>() == "timer")
  {
    return std::make_shared<ConditionTIMER>();
  }
  if (input["type"] && input["type"].as<std::string>() == "and")
  {
    return std::make_shared<ConditionAND>();
  }
  if (input["type"] && input["type"].as<std::string>() == "or")
  {
    return std::make_shared<ConditionOR>();
  }
  if (input["type"] && input["type"].as<std::string>() == "not")
  {
    return std::make_shared<ConditionNOT>();
  }
  //default
  return std::make_shared<Condition>();
}

//===========================================================
void BoolCondition::setCondition(ConditionPtr condition, int condition_index)
{
  if (!condition)
    return;
  if (condition_index == 1)
    this->_condition1 = condition;
  if (condition_index == 2)
    this->_condition2 = condition;
}

//===========================================================
void BoolCondition::setCondition(ConditionPtr condition)
{
  setCondition(condition, 1);
}

//===========================================================
ConditionPtr BoolCondition::getCondition(int condition_index) const
{
  if (condition_index == 1)
    return this->_condition1;
  if (condition_index == 2)
  {
    if (this->getType() == Condition::TYPE::NOT)
    {
      return this->_condition1;
    }
    return this->_condition2;
  }
  return this->_condition1;
}

//===========================================================
ConditionPtr BoolCondition::getCondition() const
{
  return getCondition(1);
}

//===========================================================
bool BoolCondition::isValid() const
{
  if (this->getType() == Condition::TYPE::NOT)
  {
    if (_condition1->isValid())
      return true;
  }
  else
  {
    if (_condition1->isValid() && _condition2->isValid())
    {
      return true;
    }
  }
  std::cout << "Invalid <" << this->getConditionName() << "> condition" <<
    std::endl;
  return false;
}

//===========================================================
YAML::Node BoolCondition::toYaml() const
{
  YAML::Node bool_node = YAML::Node(YAML::NodeType::Map);
  bool_node.SetStyle(YAML::EmitterStyle::Block);
  bool_node["type"] = getConditionName();
  bool_node["condition1"] = getCondition(1)->toYaml();
  if (getCondition(2))
  {
    bool_node["condition2"] = getCondition(2)->toYaml();
  }
  return bool_node;
}

//===========================================================
void BoolCondition::fromYaml(const YAML::Node& input)
{
  if (input["condition1"])
  {
    auto condition1_ptr = initFromYaml(input["condition1"]);
    condition1_ptr->fromYaml(input["condition1"]);
    setCondition(condition1_ptr, 1);
  }

  if (input["condition2"])
  {
    auto condition2_ptr = initFromYaml(input["condition2"]);
    condition2_ptr->fromYaml(input["condition2"]);
    setCondition(condition2_ptr, 2);
  }
}

//===========================================================
YAML::Node ConditionGOAL::toYaml() const
{
  YAML::Node goal_node = YAML::Node(YAML::NodeType::Map);
  goal_node.SetStyle(YAML::EmitterStyle::Flow);
  goal_node["type"] = getConditionName();
  goal_node["distance"] = getValue();
  return goal_node;
}

//===========================================================
void ConditionGOAL::fromYaml(const YAML::Node& input)
{
  if (input["type"].as<std::string>() != "goal_reached")
  {
    throw std::runtime_error("Error in parsing goal_reached condition");
  }
  if (input["distance"] && input["distance"].as<double>() > 0)
  {
    setValue(input["distance"].as<double>() );
  }
}

//===========================================================
YAML::Node ConditionTIMER::toYaml() const
{
  YAML::Node timer_node = YAML::Node(YAML::NodeType::Map);
  timer_node.SetStyle(YAML::EmitterStyle::Flow);
  timer_node["type"] = getConditionName();
  timer_node["dist"] = _distribution; //currently only support const distribution
  timer_node["value"] = getValue();
  timer_node["per_agent"] = "true";
  return timer_node;
}

//===========================================================
void ConditionTIMER::fromYaml(const YAML::Node& input)
{
  if (input["type"].as<std::string>() != "timer")
  {
    throw std::runtime_error("Error in parsing timer condition");
  }
  if (input["value"] && input["value"].as<double>() > 0)
  {
    setValue(input["value"].as<double>() );
  }
}