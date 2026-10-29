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

#include "move_vertex.h"

MoveVertexCommand::MoveVertexCommand(
  Building* building,
  int level_idx,
  int mouse_vertex_idx)
{
  _building = building;
  _to_move = _building->levels[level_idx].vertices[mouse_vertex_idx];
  _level_idx = level_idx;
  has_moved = false;
}

MoveVertexCommand::~MoveVertexCommand()
{

}

void MoveVertexCommand::setFinalDestination(double x, double y)
{
  _x = x;
  _y = y;
  has_moved = true;
}

void MoveVertexCommand::undo()
{
  const auto& vertices = _building->levels[_level_idx].vertices;
  for (std::size_t i = 0; i < vertices.size(); ++i)
  {
    if (vertices[i].uuid == _to_move.uuid)
    {
      _building->moveVertex(_level_idx, i, _to_move.x, _to_move.y);
      return;
    }
  }
}

void MoveVertexCommand::redo()
{
  const auto& vertices = _building->levels[_level_idx].vertices;
  for (std::size_t i = 0; i < vertices.size(); ++i)
  {
    if (vertices[i].uuid == _to_move.uuid)
    {
      _building->moveVertex(_level_idx, i, _x, _y);
      return;
    }
  }
}
