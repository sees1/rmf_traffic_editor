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

#ifndef MOVE_LIFT_H
#define MOVE_LIFT_H

#include <QUndoCommand>

#include "building.h"
#include "lift.h"

class MoveLiftCommand final : public QUndoCommand
{
public:
  MoveLiftCommand(Building* building, std::string name);

  void setFinalDestination(double x, double y);

  void undo() override;

  void redo() override;

public:
  bool has_moved_ = false;

private:
  Building* building_;
  Lift* lift_;
  int reference_lvl_idx_;
  Eigen::Vector2d prev_pos_;
  Eigen::Vector2d final_pos_;

  struct CabinVertex
  {
    std::size_t level;
    QUuid id;
    Eigen::Vector2d position;
    Eigen::Matrix2d from_reference;
  };
  std::vector<CabinVertex> cabin_proj_;
  void apply(const Eigen::Vector2d& position);
};

#endif