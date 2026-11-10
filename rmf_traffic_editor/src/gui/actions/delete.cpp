/*
 * Copyright (C) 2019-2020 Open Source Robotics Foundation
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

#include "delete.h"

DeleteCommand::DeleteCommand(Building* building, int level_idx, bool delete_used_entities)
: _before(building->levels),
  delete_used_entities_(delete_used_entities),
  _building(building),
  _level_idx(level_idx)
{
  for (const auto& lift : building->lifts)
    navigation_before_.emplace(lift.name, lift.navigation_vertices);
}

DeleteCommand::~DeleteCommand() = default;

void DeleteCommand::undo()
{
  _building->levels = _before;
  for (auto& lift : _building->lifts)
  {
    const auto it = navigation_before_.find(lift.name);
    if (it != navigation_before_.end())
      lift.navigation_vertices = it->second;
  }
}

void DeleteCommand::redo()
{
  if (_applied)
  {
    _building->levels = _after;
    for (auto& lift : _building->lifts)
    {
      const auto it = navigation_after_.find(lift.name);
      if (it != navigation_after_.end())
        lift.navigation_vertices = it->second;
    }
  }
  else
  {
    _building->deleteSelected(_level_idx, delete_used_entities_);
  
    _after = _building->levels;
  
    for (const auto& lift : _building->lifts)
      navigation_after_.emplace(lift.name, lift.navigation_vertices);
    _applied = true;
  }
}
