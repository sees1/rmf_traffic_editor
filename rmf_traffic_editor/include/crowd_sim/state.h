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

#ifndef CROWD_SIM_STATE__H
#define CROWD_SIM_STATE__H

#include <string>
#include <memory>

#include <yaml-cpp/yaml.h>

namespace crowd_sim {
class State
{
public:
  State(std::string state_name)
  : _name(state_name),
    _navmesh_file_name(""),
    _is_final_state(true),
    _goal_set_id(-1)
  {}

  State(const YAML::Node& input)
  : _name("N.A."),
    _navmesh_file_name(""),
    _is_final_state(true),
    _goal_set_id(-1)
  {
    fromYaml(input);
  }

  void setNavmeshFileName(std::string file_name)
  {
    this->_navmesh_file_name = file_name;
  }

  void setFinalState(bool is_final) { this->_is_final_state = is_final; }

  void setGoalSetId(size_t goal_set_id)
  {
    this->_goal_set_id = static_cast<int>(goal_set_id);
  }

  void setName(std::string name) { this->_name = name; }

  bool isValid() const;

  std::string getName() const {return this->_name;}

  std::string getNavmeshFileName() const {return this->_navmesh_file_name;}

  bool getFinalState() const {return this->_is_final_state;}

  int getGoalSetId() const {return this->_goal_set_id;}

  YAML::Node toYaml() const;

  void fromYaml(const YAML::Node& input);

private:
  std::string _name;
  std::string _navmesh_file_name;
  bool _is_final_state;
  int _goal_set_id;
};

using StatePtr = std::shared_ptr<State>;

} //namespace crowd_sim

#endif