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

#include "add_edge.h"

AddEdgeCommand::AddEdgeCommand(
  Building* building,
  int level_idx,
  const RenderingOptions& rendering_options)
{
  building_ = building;
  rendering_options_ = rendering_options;
  level_idx_ = level_idx;
  first_point_not_exist_ = false;
  first_point_drawn_ = false;
  second_point_not_exist_ = false;
  second_point_drawn_ = false;
  edge_snapshot_ = building_->levels[level_idx_].edges;
  vert_snapshot_ = building_->levels[level_idx_].vertices;
}

AddEdgeCommand::~AddEdgeCommand()
{

}

void AddEdgeCommand::redo()
{
  building_->levels[level_idx_].vertices = final_snapshot_;
  if (type_ == Edge::LANE)
  {
    building_->addLane(
      level_idx_,
      vert_id_first_,
      vert_id_second_,
      rendering_options_.active_traffic_map_idx);
  }
  else if (type_ == Edge::DOOR)
  {
    building_->addDoor(
      level_idx_,
      vert_id_first_,
      vert_id_second_);
  }
  else
  {
    building_->addEdge(
      level_idx_,
      vert_id_first_,
      vert_id_second_,
      type_);
  }
}

void AddEdgeCommand::undo()
{
  //Just use snapshots to keep things simpler
  building_->levels[level_idx_].edges = edge_snapshot_;
  building_->levels[level_idx_].vertices = vert_snapshot_;
}

int AddEdgeCommand::setFirstPoint(double x, double y)
{
  first_x_ = x;
  first_y_ = y;

  const double vertex_dist_thresh_pixels =
    vertex_radius_meters_ /
    building_->levels[level_idx_].drawing_meters_per_pixel;

  int clicked_idx = building_->nearestItemIndexIfWithinDistance(
    level_idx_,
    x,
    y,
    vertex_dist_thresh_pixels,
    Level::VERTEX);

  first_point_drawn_ = true;
  if (clicked_idx < 0)
  {
    first_point_not_exist_ = true;
    building_->addVertex(level_idx_, x, y);
    clicked_idx = building_->levels[level_idx_].vertices.size()-1;
  }
  vert_id_first_ = clicked_idx;
  final_snapshot_ = building_->levels[level_idx_].vertices;
  return clicked_idx;
}

int AddEdgeCommand::setSecondPoint(double x, double y)
{
  second_x_ = x;
  second_y_ = y;

  const double vertex_dist_thresh_pixels =
    vertex_radius_meters_ /
    building_->levels[level_idx_].drawing_meters_per_pixel;

  int clicked_idx = building_->nearestItemIndexIfWithinDistance(
    level_idx_,
    x,
    y,
    vertex_dist_thresh_pixels,
    Level::VERTEX);

  second_point_drawn_ = true;

  if (clicked_idx < 0)
  {
    second_point_not_exist_ = true;
    second_point_drawn_ = true;
    building_->addVertex(level_idx_, x, y);
    clicked_idx = building_->levels[level_idx_].vertices.size()-1;
  }
  vert_id_second_ = clicked_idx;
  final_snapshot_ = building_->levels[level_idx_].vertices;
  return clicked_idx;
}

void AddEdgeCommand::setEdgeType(Edge::Type type)
{
  type_ = type;
}
