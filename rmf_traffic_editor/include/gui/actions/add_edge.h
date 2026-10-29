/*
 * Copyright (C) 2019-2021 Open Source Robotics Foundation
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

#ifndef _ADD_EDGE_H_
#define _ADD_EDGE_H_

#include <QUndoCommand>
#include "building.h"
#include "rendering_options.h"

class AddEdgeCommand : public QUndoCommand
{
public:
  AddEdgeCommand(
    Building* building,
    int level_idx,
    const RenderingOptions& rendering_options
  );

  virtual ~AddEdgeCommand();

  void undo() override;

  void redo() override;

  int setFirstPoint(double x, double y);

  int setSecondPoint(double x, double y);

  void setEdgeType(Edge::Type type);

private:
  Building* building_;
  RenderingOptions rendering_options_;
  double first_x_, first_y_;
  double second_x_, second_y_;
  bool first_point_not_exist_, first_point_drawn_;
  bool second_point_not_exist_, second_point_drawn_;
  int level_idx_;
  std::vector<Edge> edge_snapshot_;
  std::vector<Vertex> vert_snapshot_, final_snapshot_;
  int vert_id_first_, vert_id_second_;
  Edge::Type type_;
  const double vertex_radius_meters_ = 0.1;
};


#endif
