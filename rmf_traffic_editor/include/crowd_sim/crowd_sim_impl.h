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

#ifndef CROWD_SIM_IMPL__H
#define CROWD_SIM_IMPL__H

#include <string>
#include <set>
#include <memory>

#include <yaml-cpp/yaml.h>

#include <crowd_sim/state.h>
#include <crowd_sim/goal_set.h>
#include <crowd_sim/transition.h>
#include <crowd_sim/agent_profile.h>
#include <crowd_sim/agent_group.h>
#include <crowd_sim/model_type.h>

namespace crowd_sim {

class CrowdSimImplementation
{
public:
  CrowdSimImplementation()
  : _enable_crowd_sim(false),
    _update_time_step(0.1)
  {
    initDefaultConfigure();
  }

  ~CrowdSimImplementation() {}

  YAML::Node toYaml();

  bool fromYaml(const YAML::Node& input);

  void clear();

  void initDefaultConfigure();

  void setNavmeshFileName(std::vector<std::string> navmesh_filename)
  {
    _navmesh_filename_list = navmesh_filename;
  }

  std::vector<std::string> getNavmeshFileName() const
  {
    return _navmesh_filename_list;
  }

  void setEnableCrowdSim(bool is_enable) { _enable_crowd_sim = is_enable; }

  bool getEnableCrowdSim() const { return _enable_crowd_sim; }

  void setUpdateTimeStep(double update_time_step)
  {
    _update_time_step = update_time_step;
  }

  double getUpdateTimeStep() const { return _update_time_step; }

  void setGoalAreas(std::set<std::string> goal_areas)
  {
    _goal_areas = goal_areas;
  }

  std::vector<std::string> getGoalAreas() const
  {
    return std::vector<std::string>(_goal_areas.begin(), _goal_areas.end());
  }

  void saveGoalSets(const std::vector<GoalSet>& goal_sets);

  std::vector<GoalSet> getGoalSets() const { return _goal_sets; }

  void saveStates(const std::vector<State>& states);

  std::vector<State> getStates() const { return _states; }

  void saveTransitions(const std::vector<Transition>& transitions);

  std::vector<Transition> getTransitions() const { return _transitions; }

  void saveAgentProfiles(const std::vector<AgentProfile>& agent_profiles);

  std::vector<AgentProfile> getAgentProfiles() const
  {
    return _agent_profiles;
  }

  void saveAgentGroups(const std::vector<AgentGroup>& agent_groups);

  std::vector<AgentGroup> getAgentGroups() const { return _agent_groups; }

  void saveModelTypes(const std::vector<ModelType>& model_types);

  std::vector<ModelType> getModelTypes() const { return _model_types; }

private:
  void initializeState();

  void initializeAgentProfile();

  void initializeAgentGroup();

  void initializeModelType();

  YAML::Node outputObstacleNode() const;

private:
  // update from project.building in crowd_sim_table
  std::set<std::string> _goal_areas;
  std::vector<std::string> _navmesh_filename_list;
  // real configurations
  bool _enable_crowd_sim;
  double _update_time_step;
  std::vector<State> _states;
  std::vector<GoalSet> _goal_sets;
  std::vector<Transition> _transitions;
  std::vector<AgentProfile> _agent_profiles;
  std::vector<AgentGroup> _agent_groups;
  std::vector<ModelType> _model_types;
};

using CrowdSimImplPtr = std::shared_ptr<CrowdSimImplementation>;

} //namespace crowd_sim

#endif