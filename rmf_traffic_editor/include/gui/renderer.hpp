#ifndef LEVEL_H
#define LEVEL_H

#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>
#include <string>

#include "constraint.hpp"
#include "coordinate_system.h"
#include "edge.h"
#include "editor_model.h"
#include "feature.hpp"
#include "fiducial.h"
#include "graph.h"
#include "layer.h"
#include "model.h"
#include "polygon.h"
#include "rendering_options.h"
#include "vertex.h"

#include <QPixmap>
#include <QPainterPath>
class QGraphicsScene;

class Renderer final
{
public:
  Renderer(double r, double g, double b, double f)
  : brush_(QColor::fromRgbF(r, g, b, f));
  { }
  Renderer(const QBrush& brush)
  : brush_(brush)
  { }

  template <typename ObjectType>
  bool draw(QGraphicsScene* scene, const ObjectType object, const std::vector<Vertices>& vertices) const { }

private:
  QBrush brush_;
};

template<>
bool Renderer::draw<Polygon>(QGraphicsScene* scene, const Polygon object, const std::vector<Vertices>& vertices) const
{
  QVector<QPointF> polygon_vertices;
  for (const auto& vertex_idx: polygon.vertices)
  {
    const Vertex& v = vertices[vertex_idx];
    polygon_vertices.append(QPointF(v.x, v.y));
  }

  QPen pen(Qt::black);
  pen.setWidthF(0.05 / drawing_meters_per_pixel);

  scene->addPolygon(
    QPolygonF(polygon_vertices),
    pen,
    polygon.selected ? selected_brush : brush);
}

#endif