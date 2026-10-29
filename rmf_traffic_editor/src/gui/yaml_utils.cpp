/*
 * Copyright (C) 2019 Open Source Robotics Foundation
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

#include <algorithm>
#include <cmath>
#include <QJsonArray>
#include <QJsonObject>

#include "yaml_utils.h"

#include <string>
using std::string;


// Recursive function to write YAML ordered maps. Credit: Dave Hershberger
// posted to this GitHub issue: https://github.com/jbeder/yaml-cpp/issues/169
void yaml_utils::writeNode(
  const YAML::Node& node,
  YAML::Emitter& emitter)
{
  switch (node.Style())
  {
    case YAML::EmitterStyle::Block:
      emitter << YAML::Block;
      break;
    case YAML::EmitterStyle::Flow:
      emitter << YAML::Flow;
      break;
    default:
      break;
  }

  switch (node.Type())
  {
    case YAML::NodeType::Sequence:
    {
      emitter << YAML::BeginSeq;
      for (std::size_t i = 0; i < node.size(); i++)
        writeNode(node[i], emitter);
      emitter << YAML::EndSeq;
      break;
    }
    case YAML::NodeType::Map:
    {
      emitter << YAML::BeginMap;
      // the keys are stored in random order, so we need to collect and sort
      std::vector<string> keys;
      keys.reserve(node.size());
      for (YAML::const_iterator it = node.begin(); it != node.end(); ++it)
        keys.push_back(it->first.as<string>());
      std::sort(keys.begin(), keys.end());
      for (std::size_t i = 0; i < keys.size(); i++)
      {
        emitter << YAML::Key << keys[i] << YAML::Value;
        writeNode(node[keys[i]], emitter);
      }
      emitter << YAML::EndMap;
      break;
    }
    default:
      emitter << node;
      break;
  }
}

QJsonValue yaml_utils::toJson(const YAML::Node& node)
{
  if (!node || node.IsNull())
    return QJsonValue(QJsonValue::Null);
  if (node.IsMap())
  {
    QJsonObject object;
    for (const auto& entry : node)
      object.insert(QString::fromStdString(entry.first.as<string>()),
        toJson(entry.second));
    return object;
  }
  if (node.IsSequence())
  {
    QJsonArray array;
    for (const auto& entry : node)
      array.append(toJson(entry));
    return array;
  }
  const string value = node.Scalar();
  if (node.Tag() == "!" || node.Tag() == "tag:yaml.org,2002:str")
    return QString::fromStdString(value);
  if (value == "true")
    return true;
  if (value == "false")
    return false;
  double number;
  if (YAML::convert<double>::decode(node, number))
  {
    if (!std::isfinite(number))
      throw std::runtime_error("JSON cannot store non-finite numbers");
    return number;
  }
  return QString::fromStdString(value);
}
