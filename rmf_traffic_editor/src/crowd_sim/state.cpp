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
#include <crowd_sim/state.h>

using namespace crowd_sim;

//==============================================
bool State::isValid() const
{
  if (_is_final_state && _name.size() > 0)
    return true;
  if (_name.size() > 0 && _navmesh_file_name.size() > 0 && _goal_set_id >= 0)
    return true;
  return false;
}

//==============================================
YAML::Node State::toYaml() const
{
  YAML::Node state_node(YAML::NodeType::Map);
  state_node.SetStyle(YAML::EmitterStyle::Flow);
  state_node["name"] = getName();
  state_node["name"].SetTag("!");
  state_node["goal_set"] = getGoalSetId();
  state_node["navmesh_file_name"] = getNavmeshFileName();
  state_node["navmesh_file_name"].SetTag("!");
  state_node["final"] = getFinalState() ? 1 : 0;
  return state_node;
}

//==============================================
void State::fromYaml(const YAML::Node& input)
{
  setName(input["name"].as<std::string>());
  setNavmeshFileName(input["navmesh_file_name"].as<std::string>());
  setFinalState(input["final"].as<int>() == 0 ? false : true);
  setGoalSetId(input["goal_set"].as<int>());
}