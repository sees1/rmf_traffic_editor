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

#ifndef CROWD_SIM_CROWDSIM_TABLE_BASE__H
#define CROWD_SIM_CROWDSIM_TABLE_BASE__H

#include <memory>

#include <crowd_sim/crowd_sim_impl.h>

#include "table_list.h"

using namespace crowd_sim;

class CrowdSimTableBase : public TableList
{
public:
  CrowdSimTableBase(CrowdSimImplPtr impl, const QStringList& labels);

  virtual ~CrowdSimTableBase() {}

  CrowdSimImplPtr getImpl() const { return _crowd_sim_impl; }

  size_t getLabelSize() const { return _label_size; }

  void setLabelSize(size_t label_size) { _label_size = label_size; }

  virtual int getCacheSize() const = 0;

  virtual void listItemInCache() = 0;

  virtual void save() = 0;

  virtual void saveToImpl() = 0;

  virtual void addButtonClick() = 0;

  virtual void deleteButtonClick(size_t row_num) = 0;

  virtual void update();

private:
  CrowdSimImplPtr _crowd_sim_impl;
  size_t _label_size;
};

using CrowdSimTablePtr = std::shared_ptr<CrowdSimTableBase>;

#endif