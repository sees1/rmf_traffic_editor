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

#include "move_lift.hpp"

MoveLiftCommand::MoveLiftCommand(Building* building, std::string lift_name)
: building_(building),
  prev_pos_(Eigen::Vector2d::Zero()),
  final_pos_(Eigen::Vector2d::Zero())
{
  auto lift_it = std::find_if(building_->lifts.begin(), building_->lifts.end(),
  [&lift_name](auto&& lift)
  {
    return lift.name == lift_name;
  });

  if (lift_it == building_->lifts.end())
    throw std::runtime_error("MoveLiftCommand::ctr: no such lift with this name!");

  lift_ = &(*lift_it);

  prev_pos_ = final_pos_ = lift_it->pose.translation();
  
  auto reference_it = std::find_if(building_->levels.begin(), building_->levels.end(),
  [&lift_it](auto&& level)
  {
    return level.name == lift_it->reference_floor_name;
  });

  if (reference_it == building_->levels.end())
    reference_lvl_idx_ = -1;
  else
    reference_lvl_idx_ = std::distance(building_->levels.begin(), reference_it);
    
  for (std::size_t i = 0; i < building_->levels.size(); ++i)
  {
    auto cabin_vertex_it = std::find_if(building_->levels[i].vertices.begin(), building_->levels[i].vertices.end(),
    [&lift_name](auto&& vertex)
    {
      return vertex.liftCabin() == lift_name;
    });

    if (cabin_vertex_it != building_->levels[i].vertices.end())
    {
      Building::Transform t;

      if (reference_lvl_idx_ >= 0)
        t = building_->getTransform(reference_lvl_idx_, i);

      cabin_proj_.push_back({i, cabin_vertex_it->uuid, {cabin_vertex_it->x, cabin_vertex_it->y}, t.scale * Eigen::Rotation2Dd(-t.rotation).toRotationMatrix()});
    }
  }
}

void MoveLiftCommand::setFinalDestination(double x, double y)
{
  final_pos_ = Eigen::Vector2d(x, y);
  has_moved_ = (final_pos_ - prev_pos_).norm() > 1e-9;
}

void MoveLiftCommand::apply(const Eigen::Vector2d& position)
{
  lift_->pose.translation() = position;

  for (const auto& cabin : cabin_proj_)
  {
    auto cabin_vertex_it = std::find_if(building_->levels[cabin.level].vertices.begin(), building_->levels[cabin.level].vertices.end(),
    [cabin_uuid = cabin.id](auto&& vertex)
    {
      return vertex.uuid == cabin_uuid;
    });

    const Eigen::Vector2d p = cabin.position + cabin.from_reference * (position - prev_pos_);
      
    cabin_vertex_it->x = p.x();
    cabin_vertex_it->y = p.y();
      
  }

  if (reference_lvl_idx_ > 0)
    building_->updateLifts(lift_, reference_lvl_idx_);
}

void MoveLiftCommand::undo()
{
  apply(prev_pos_);
}

void MoveLiftCommand::redo()
{
  apply(final_pos_);
}
