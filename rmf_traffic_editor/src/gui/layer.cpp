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

#include <cmath>

#include <QImageReader>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QTableWidget>
#include "layer.h"
using std::string;
using std::vector;

Layer::Layer()
{
}

Layer::~Layer()
{
}

bool Layer::fromYaml(
  const std::string& _name,
  const YAML::Node& y,
  const CoordinateSystem& coordinate_system)
{
  if (!y.IsMap())
    throw std::runtime_error("Layer::from_yaml() expected a map");
  
  name = _name;
  filename = y["filename"].as<string>();

  if (y["meters_per_pixel"])
  {
    // legacy transform format, need to re-compute transform
    // because the origin has changed from the lower-left to the
    // upper-left of the layer image, to be consistent with the
    // handling of the base floorplan image
    transform.setScale(y["meters_per_pixel"].as<double>());

    double image_height = 0;
    QImageReader image_reader(QString::fromStdString(filename));
    image_reader.setAutoTransform(true);

    // we'll load this image here just for the purpose of getting its height
    QImage img = image_reader.read();
    if (!img.isNull())
      image_height = img.size().height() * transform.scale();

    QPointF offset;
    if (y["rotation"]) // legacy key
    {
      const double yaw = y["rotation"].as<double>();
      transform.setYaw(yaw);
      offset.setX(-image_height * sin(yaw));
      offset.setY(-image_height * cos(yaw));
    }

    if (y["translation_x"] && y["translation_y"]) // legacy keys
      transform.setTranslation(
        QPointF(
          offset.x() - y["translation_x"].as<double>(),
          offset.y() + y["translation_y"].as<double>()));
  }

  if (y["visible"])
    visible = y["visible"].as<bool>();

  if (y["transform"] && y["transform"].IsMap())
    transform.fromYaml(y["transform"], coordinate_system);

  if (y["color"] && y["color"].IsSequence() && y["color"].size() == 4)
    color = QColor::fromRgbF(
      y["color"][0].as<double>(),
      y["color"][1].as<double>(),
      y["color"][2].as<double>(),
      y["color"][3].as<double>());
  else
    color = QColor::fromRgbF(0, 0, 1, 0.5);

  if (y["features"] && y["features"].IsSequence())
  {
    const YAML::Node& fy = y["features"];
    for (YAML::const_iterator it = fy.begin(); it != fy.end(); ++it)
    {
      Feature f;
      f.fromYaml(*it);
      features.push_back(f);
    }
  }

  return loadImage();
}

bool Layer::loadImage()
{
  QImageReader image_reader(QString::fromStdString(filename));
  image_reader.setAutoTransform(true);
  image = image_reader.read();
  if (image.isNull())
  {
    qWarning("unable to read %s: %s",
      qUtf8Printable(QString::fromStdString(filename)),
      qUtf8Printable(image_reader.errorString()));
    return false;
  }
  image = image.convertToFormat(QImage::Format_Grayscale8);
  colorizeImage();
  printf("successfully opened %s\n", filename.c_str());

  return true;
}

YAML::Node Layer::toYaml(const CoordinateSystem& coordinate_system) const
{
  YAML::Node y;
  y["filename"] = filename;
  y["filename"].SetTag("!");
  y["visible"] = visible;

  y["color"].push_back(std::round(color.redF() * 1000.0) / 1000.0);
  y["color"].SetStyle(YAML::EmitterStyle::Flow);
  y["color"].push_back(std::round(color.greenF() * 1000.0) / 1000.0);
  y["color"].push_back(std::round(color.blueF() * 1000.0) / 1000.0);
  y["color"].push_back(std::round(color.alphaF() * 1000.0) / 1000.0);

  for (const auto& feature : features)
    y["features"].push_back(feature.toYaml());

  y["transform"] = transform.toYaml(coordinate_system);

  return y;
}

void Layer::draw(
  QGraphicsScene* scene,
  const double level_meters_per_pixel,
  const CoordinateSystem& coordinate_system)
{
  if (!visible)
    return;

  QGraphicsPixmapItem* item = scene->addPixmap(pixmap);

  // Store for later use in getting coordinates back out
  scene_item = item;

  item->setPos(
    transform.translation().x() / level_meters_per_pixel,
    transform.translation().y() / level_meters_per_pixel);

  item->setScale(transform.scale() / level_meters_per_pixel);

  item->setRotation(-1.0 * transform.yaw() * 180.0 / M_PI);

  if (!coordinate_system.isYFlipped())
    item->setTransform(item->transform().scale(1, -1));

  double origin_radius = 0.5 / level_meters_per_pixel;
  QPen origin_pen(color, origin_radius / 4.0, Qt::SolidLine, Qt::RoundCap);
  //origin_pen.setWidthF(origin_radius / 4);

  // for purposes of the origin mark, let's say the origin is the center
  // of the first pixel of the image
  const double y_flip = coordinate_system.isYFlipped() ? -1 : 1;
  const QPointF origin(
    transform.translation().x() / level_meters_per_pixel
    + 0.5 * transform.scale() / level_meters_per_pixel *
    cos(transform.yaw() - M_PI / 4),
    transform.translation().y() / level_meters_per_pixel
    + y_flip * 0.5 * transform.scale() / level_meters_per_pixel *
    sin(transform.yaw() - M_PI / 4));

  scene->addEllipse(
    origin.x() - origin_radius,
    origin.y() - origin_radius,
    2 * origin_radius,
    2 * origin_radius,
    origin_pen);

  QPointF x_arrow(
    origin.x() + 2.0 * origin_radius * cos(transform.yaw()),
    origin.y() + y_flip * 2.0 * origin_radius * sin(transform.yaw()));
  scene->addLine(QLineF(origin, x_arrow), origin_pen);

  for (Feature& feature : features)
    feature.draw(scene, color, transform, level_meters_per_pixel);
}

QColor Layer::defaultColor(const int layer_idx)
{
  switch (layer_idx)
  {
    case 0:
    default: return QColor::fromRgbF(1, 0, 0, 1.0);
    case 1:  return QColor::fromRgbF(0, 1, 0, 1.0);
    case 2:  return QColor::fromRgbF(0, 0, 1, 1.0);
    case 3:  return QColor::fromRgbF(1, 1, 0, 1.0);
    case 4:  return QColor::fromRgbF(0, 1, 1, 1.0);
    case 5:  return QColor::fromRgbF(1, 0, 1, 1.0);
  }
}

void Layer::removeFeature(QUuid feature_id)
{
  int index_to_remove = -1;

  for (std::size_t i = 0; i < features.size(); i++)
  {
    if (feature_id == features[i].id())
      index_to_remove = i;
  }

  if (index_to_remove < 0)
    return;

  features.erase(features.begin() + index_to_remove);
}

QUuid Layer::addFeature(
  const double x,
  const double y,
  const double level_meters_per_pixel)
{
  printf("Layer::add_feature(%s, %.3f, %.3f, %.3f)\n",
    name.c_str(),
    x,
    y,
    level_meters_per_pixel);

  // convert clicks in the working area to meters
  const double mpp = level_meters_per_pixel;
  QPointF layer_pixel = transform.backwards(QPointF(x * mpp, y * mpp));

  printf("  transformed: (%.3f, %.3f)\n", layer_pixel.x(), layer_pixel.y());
  features.push_back(Feature(layer_pixel));

  return features.rbegin()->id();
}

const Feature* Layer::findFeature(
  const double x,
  const double y,
  const double level_meters_per_pixel) const
{
  const double mpp = level_meters_per_pixel;
  QPointF layer_pixel = transform.backwards(QPointF(x * mpp, y * mpp));

  double min_dist = 1e9;
  const Feature* min_feature = nullptr;

  for (std::size_t i = 0; i < features.size(); i++)
  {
    const Feature& f = features[i];
    const double dx = layer_pixel.x() - f.x();
    const double dy = layer_pixel.y() - f.y();
    const double dist = sqrt(dx * dx + dy * dy);
    if (dist < min_dist)
    {
      min_dist = dist;
      min_feature = &features[i];
    }
  }

  printf("min_dist = %.3f   layer scale = %.3f\n", min_dist, transform.scale());

  if (min_dist * transform.scale() < Feature::radius_meters)
    return min_feature;

  return nullptr;
}

QPointF Layer::transformGlobalToLayer(const QPointF& global_point)
{
  return transform.backwards(global_point);
}

QPointF Layer::transformLayerToGlobal(const QPointF& layer_point)
{
  return transform.forwards(layer_point);
}

void Layer::clearSelection()
{
  for (auto& feature : features)
    feature.setSelected(false);
}

void Layer::colorizeImage()
{
  color.setAlphaF(0.5);
  colorized_image = QImage(image.size(), QImage::Format_ARGB32);
  for (int row_idx = 0; row_idx < image.height(); row_idx++)
  {
    const uint8_t* const in_row = (uint8_t*)image.scanLine(row_idx);
    QRgb* out_row = (QRgb*)colorized_image.scanLine(row_idx);

    for (int col_idx = 0; col_idx < image.width(); col_idx++)
    {
      const uint8_t in = in_row[col_idx];
      if (in < 100 || row_idx == 0 || row_idx == image.height() - 1)
        out_row[col_idx] = color.rgba();
      else if (in > 200)
        out_row[col_idx] = qRgba(0, 0, 0, 0);
      else
        out_row[col_idx] = qRgba(in, in, in, 50);
    }

    // draw bold first/last columns the requested color on the image,
    // so it's easier to see what's going on with its transform
    out_row[0] = color.rgba();
    out_row[image.width()-1] = color.rgba();
  }

  pixmap = QPixmap::fromImage(colorized_image);
}

void Layer::populatePropertyEditor(QTableWidget* property_editor) const
{
  property_editor->blockSignals(true);
  property_editor->setRowCount(transform_strings.size());
  for (std::size_t i = 0; i < transform_strings.size(); i++)
  {
    QTableWidgetItem* label_item = new QTableWidgetItem(
      QString::fromStdString(transform_strings[i].first));
    label_item->setFlags(Qt::NoItemFlags);

    QTableWidgetItem* value_item = new QTableWidgetItem(
      QString::fromStdString(transform_strings[i].second));
    value_item->setFlags(Qt::NoItemFlags);

    property_editor->setItem(i, 0, label_item);
    property_editor->setItem(i, 1, value_item);
  }
  property_editor->blockSignals(false);
}
