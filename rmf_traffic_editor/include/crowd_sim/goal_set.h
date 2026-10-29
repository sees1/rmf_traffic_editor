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

#ifndef CROWD_SIM_GOAL_SET__H
#define CROWD_SIM_GOAL_SET__H

#include <string>
#include <set>
#include <memory>

#include <yaml-cpp/yaml.h>

namespace crowd_sim {

class GoalSet
{
public:
  GoalSet(size_t goal_id)
  : _id(goal_id),
    _capacity(1),
    _goal_area_contained({})
  {}

  GoalSet(const YAML::Node& input)
  : _id(65535), //initialize with invalid id
    _capacity(1),
    _goal_area_contained({})
  {
    fromYaml(input);
  }

  void addGoalArea(std::string goal_area_name);

  void setCapacity(size_t capacity) { this->_capacity = capacity; }

  std::set<std::string> getGoalAreas() const
  {
    return this->_goal_area_contained;
  }

  YAML::Node getGoalAreasToYaml() const;

  size_t getGoalSetId() const { return this->_id; }

  size_t getCapacity() const {return this->_capacity; }

  YAML::Node toYaml() const;

  void fromYaml(const YAML::Node& input);

private:
  void setGoalSetId(size_t id_) { this->_id = id_; }

private:
  size_t _id;
  size_t _capacity;
  std::set<std::string> _goal_area_contained;
};

using GoalSetPtr = std::shared_ptr<GoalSet>;
} //namespace crowd_sim

#endif