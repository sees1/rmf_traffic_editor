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

#ifndef CROWD_SIM_AGENT_GROUP__H
#define CROWD_SIM_AGENT_GROUP__H

#include <string>
#include <vector>
#include <memory>

#include <yaml-cpp/yaml.h>

namespace crowd_sim {

class AgentGroup
{
public:
  AgentGroup(size_t group_id, bool is_external_group = false)
  : _group_id(group_id),
    _is_external_group(is_external_group),
    _spawn_point_x(0.0),
    _spawn_point_y(0.0),
    _spawn_number(0),
    _external_agent_name({}),
    _agent_profile(""),
    _initial_state("")
  {}

  AgentGroup(const YAML::Node& input)
  : _group_id(65535),
    _is_external_group(false),
    _spawn_point_x(0.0),
    _spawn_point_y(0.0),
    _spawn_number(0),
    _external_agent_name({}),
    _agent_profile(""),
    _initial_state("")
  {
    fromYaml(input);
  }

  bool isValid() const
  {
    return _agent_profile.size() > 0 && _initial_state.size() > 0;
  }

  bool isExternalGroup() const { return _is_external_group; }

  size_t getGroupId() const { return _group_id; }

  std::pair<double, double> getSpawnPoint() const
  {
    return std::pair<double, double>(_spawn_point_x, _spawn_point_y);
  }

  std::vector<std::string> getExternalAgentName() const
  {
    return _external_agent_name;
  }

  int getSpawnNumber() const { return _spawn_number; }

  std::string getAgentProfile() const { return _agent_profile; }

  std::string getInitialState() const { return _initial_state; }

  YAML::Node toYaml() const;

  void fromYaml(const YAML::Node& input);

  void setSpawnPoint(double x, double y)
  {
    _spawn_point_x = x;
    _spawn_point_y = y;
  }

  void setExternalAgentName(const std::vector<std::string>& external_name)
  {
    _external_agent_name.clear();
    _external_agent_name = external_name;
    _spawn_number = _external_agent_name.size();
  }

  void setSpawnNumber(int number)
  {
    _spawn_number = static_cast<size_t>(number);
  }

  void setAgentProfile(std::string profile)
  {
    _agent_profile = profile;
  }

  void setInitialState(std::string state)
  {
    _initial_state = state;
  }

private:
  size_t _group_id;
  bool _is_external_group;
  double _spawn_point_x, _spawn_point_y;
  size_t _spawn_number;
  std::vector<std::string> _external_agent_name;
  std::string _agent_profile, _initial_state;
};

} //namespace crowd_sim

#endif