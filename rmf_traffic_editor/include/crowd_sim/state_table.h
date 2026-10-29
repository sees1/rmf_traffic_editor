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

#ifndef CROWD_SIM_STATE_TABLE__H
#define CROWD_SIM_STATE_TABLE__H

#include <crowd_sim/crowd_sim_impl.h>
#include "crowd_sim_table_base.h"

using namespace crowd_sim;

class StatesTab : public CrowdSimTableBase
{
public:
  static std::shared_ptr<StatesTab> initAndMake(
    CrowdSimImplPtr crowd_sim_impl);

  StatesTab(CrowdSimImplPtr crowd_sim_impl, const QStringList& labels)
  : CrowdSimTableBase(crowd_sim_impl, labels)
  {
    _cache = getImpl()->getStates();
  }

  ~StatesTab() {}

  int getCacheSize() const override
  {
    return static_cast<int>(_cache.size());
  }

  void listItemInCache() override;

  void save() override;

  void saveToImpl() override;

  void addButtonClick() override;

  void deleteButtonClick(size_t row_number) override;

private:
  void listGoalSetsInCombo(QComboBox* comboBox,
    size_t current_goal_set_id);

  void listFinalStatesInCombo(QComboBox* comboBox, bool current_state);

  void listNavmeshFileInCombo(QComboBox* comboBox,
    std::string navmesh_filename);

private:
  std::vector<crowd_sim::State> _cache;
};

#endif