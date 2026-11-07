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
#include <QCoreApplication>
#include <QGraphicsColorizeEffect>
#include <QLabel>
#include <QScrollBar>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "map_view.h"

using std::string;

MapView::MapView(QWidget* parent, const Building& b)
: QGraphicsView(parent),
  building(b),
  is_panning(false),
  pan_start_x(0),
  pan_start_y(0),
  show_tiles(false)
{
  network = new QNetworkAccessManager(this);
  connect(network, &QNetworkAccessManager::finished, this, &MapView::requestFinished);

  setMouseTracking(true);
  viewport()->setMouseTracking(true);
  setTransformationAnchor(QGraphicsView::NoAnchor);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
}

void MapView::wheelEvent(QWheelEvent* e)
{
  e->accept();

  // calculate the map position before we scale things
  const QPointF p_start = mapToScene(e->pos());

  // sanity check: clamp the scale if it has become super tiny
  const double scale_factor = transform().m11();

  if (!building.coordinate_system.isGlobal())
  {
    if (e->delta() > 0)
    {
      scale(1.25, 1.25);
    }
    else
    {
      if (scale_factor > 0.01)
        scale(0.75, 0.75);
    }
    // calculate the mouse map position now that we've scaled
    const QPointF p_end = mapToScene(e->pos());

    // translate the map back so hopefully the mouse stays in the same spot
    const QPointF diff = p_end - p_start;
    translate(diff.x(), diff.y());
  }
  else
  {
    // in the global frame when we're rendering map tiles,
    // we want to keep to an even scaling of the tile
    // so it doesn't have too many aliasing effects
    if (e->delta() > 0)
    {
      //double max_scale_factor = 100;
      // go to the nearest power of 2 above where we are
      // const double next_scale = pow(2, round(log(scale_factor*2)/log(2)));
      scale(2.0, 2.0);
    }
    else
    {
      // go to the nearest power of 2 below where we are
      // const double next_scale = pow(2, round(log(scale_factor/2)/log(2)));

      // limit to 2^-17 = 0.0000076294

      if (scale_factor > 0.0000076294)
        scale(0.5, 0.5);
    }

    // calculate the mouse map position now that we've scaled
    const QPointF p_end = mapToScene(e->pos());

    // translate the map back so hopefully the mouse stays in the same spot
    const QPointF diff = p_end - p_start;
    translate(diff.x(), diff.y());

    const int viewport_w = viewport()->width();
    const int viewport_h = viewport()->height();
    QPointF ul = mapToScene(QPoint(0, 0));
    QPointF lr = mapToScene(QPoint(viewport_w, viewport_h));
    const double scene_w = lr.x() - ul.x();
    const double scene_h = ul.y() - lr.y();

    if (scene_w < 0.1 * M_PI * CoordinateSystem::wgs84_a)
    {
      const int n = 10;
      setSceneRect(QRectF(p_start.x() - n * scene_w, p_start.y() + n * scene_h, 2 * n * scene_w, -2 * n * scene_h));
    }
    else
      setSceneRect(QRectF()); // set a null rect, in order to the original scene rect from the scene
    
    drawTiles();
  }
}

void MapView::mousePressEvent(QMouseEvent* e)
{
  if (e->button() == Qt::MiddleButton)
  {
    is_panning = true;
    pan_start_x = e->x();
    pan_start_y = e->y();
    // setCursor(Qt::ClosedHandCursor);
    e->accept();
    return;
  }
  e->ignore();
}

void MapView::mouseReleaseEvent(QMouseEvent* e)
{
  if (e->button() == Qt::MiddleButton)
  {
    is_panning = false;
    e->accept();
    return;
  }
  e->ignore();
}

void MapView::mouseMoveEvent(QMouseEvent* e)
{
  if (is_panning)
  {
    const int dx = e->x() - pan_start_x;
    const int dy = e->y() - pan_start_y;
    const double s = transform().m11();
    const double y_flip = building.coordinate_system.isYFlipped() ? 1 : -1;
    translate(dx / s, y_flip * dy / s);
    pan_start_x = e->x();
    pan_start_y = e->y();
    e->accept();
    drawTiles();
    return;
  }
  e->ignore();
}

void MapView::resizeEvent(QResizeEvent*)
{
  drawTiles();
}

void MapView::zoomFit(int level_idx)
{
  const Level* level = nullptr;

  if (level_idx >= 0 && level_idx < static_cast<int>(building.levels.size()))
    level = &building.levels[level_idx];
  else if (building.levels.size() > 0)
    level = &building.levels[0];

  if (level != nullptr)
  {
    const double viewport_scale = 1.0;
    const double y_flip = building.coordinate_system.isYFlipped() ? 1 : -1;

    QTransform t;
    t.scale(viewport_scale, y_flip * viewport_scale);
    setTransform(t);

    // A newly imported floorplan may have no vertices yet. Fit the image
    // itself, not the padded scene rectangle or a previous level's viewport.
    if (!building.coordinate_system.isGlobal() && level->drawing_width > 0 && level->drawing_height > 0)
    {
      fitInView(QRectF(0, 0, level->drawing_width, level->drawing_height), Qt::KeepAspectRatio);
      return;
    }

    // compute center of all vertices on the active level
    const size_t n_vertex = level->vertices.size();
    if (n_vertex > 0)
    {
      auto [x_sum, y_sum] = std::accumulate(level->vertices.begin(), level->vertices.end(), std::pair<double, double>{0.0, 0.0}, 
      [](std::pair<double, double> res, auto&& vertex) -> std::pair<double, double>
      {
        return {res.first + vertex.x, res.second + vertex.y};
      });
      
      const double xc = x_sum / n_vertex;
      const double yc = y_sum / n_vertex;
      
      printf("center: (%.3f, %.3f)\n", xc, yc);
      centerOn(QPointF(xc, yc));
    }
  }

  if (building.coordinate_system.isGlobal())
    drawTiles();
}

void MapView::drawTiles()
{
  if (!show_tiles || !building.coordinate_system.hasTiles())
    return;

  const int viewport_w = viewport()->width();
  const int viewport_h = viewport()->height();

  QPointF ul = mapToScene(QPoint(0, 0));
  QPointF lr = mapToScene(QPoint(viewport_w, viewport_h));
  
  last_center = mapToScene(QPoint(viewport_w/2, viewport_h/2));

  const double x_extent = lr.x() - ul.x();
  const double tiles_visible_x = viewport_w / 256.0;
  const double x_meters_per_tile = x_extent / tiles_visible_x;
  const double zoom_exact = log(M_PI * CoordinateSystem::wgs84_a / x_meters_per_tile) / log(2);

  const int MAX_ZOOM = 20;
  int zoom = static_cast<int>(std::clamp(ceil(zoom_exact), 0.0, static_cast<double>(MAX_ZOOM)));

  const double eps = 0.0001;
  const double MAX_X = M_PI * CoordinateSystem::wgs84_a;
  const double ulx_clamped = std::clamp(ul.x(), -MAX_X, MAX_X - eps);
  const double lrx_clamped = std::clamp(lr.x(), -MAX_X, MAX_X - eps);
  const double uly_clamped = std::clamp(ul.y(), -MAX_X, MAX_X - eps);
  const double lry_clamped = std::clamp(lr.y(), -MAX_X, MAX_X - eps);

  const int x_min_tile = floor((ulx_clamped + MAX_X) / (2. * MAX_X) * (1 << zoom));
  const int x_max_tile = floor((lrx_clamped + MAX_X) / (2. * MAX_X) * (1 << zoom));
  const int y_min_tile = (1 << zoom) - 1 - floor((uly_clamped + MAX_X) / (2. * MAX_X) * (1 << zoom));
  const int y_max_tile = (1 << zoom) - 1 - floor((lry_clamped + MAX_X) / (2. * MAX_X) * (1 << zoom));

  auto erase_it = std::remove_if(tile_pixmap_items.begin(), tile_pixmap_items.end(),
  [zoom, x_min_tile, x_max_tile, y_min_tile, y_max_tile](auto&& pixmap_tile)
  {
    return pixmap_tile.zoom != zoom ||
           pixmap_tile.x < x_min_tile ||
           pixmap_tile.x > x_max_tile ||
           pixmap_tile.y < y_min_tile ||
           pixmap_tile.y > y_max_tile;
  });

  std::for_each(erase_it, tile_pixmap_items.end(),
  [this](auto&& tile)
  {
    scene()->removeItem(tile.item);
    delete tile.item;
  });

  tile_pixmap_items.erase(erase_it, tile_pixmap_items.end());

  for (int y = y_min_tile; y <= y_max_tile; ++y)
  {
    for (int x = x_min_tile; x <= x_max_tile; ++x)
    {
      auto found_it = std::find_if(tile_pixmap_items.begin(), tile_pixmap_items.end(),
      [x, y](auto&& tile)
      {
        return tile.x == x && tile.y == y;
      });

      if (found_it == tile_pixmap_items.end())
        continue;

      std::optional<const QByteArray> p = tile_cache.get(zoom, x, y);
      
      if (p.has_value())
      {
        QImage image;
        bool parse_ok = image.loadFromData(p.value());
        if (parse_ok)
        {
          QPixmap pixmap(QPixmap::fromImage(image.convertToFormat(QImage::Format_Grayscale8)));
          renderTile(zoom, x, y, pixmap, MapTilePixmapItem::State::COMPLETED);
        }
        else
          printf("oh no! couldn't parse image file in cache\n");
      }
      else
      {
        // create a dummy image while waiting for the server
        QImage image(256, 256, QImage::Format_RGB888);
        image.fill(qRgb(255, 255, 0));

        QPainter painter;
        painter.begin(&image);
        painter.setPen(QPen(Qt::red));
        QString label;
        label.sprintf("%d (%d,%d)", zoom, x, y);
        painter.drawText(10, 10, 245, 100, Qt::AlignLeft, label);
        painter.end();
        QPixmap pixmap(QPixmap::fromImage(image));

        renderTile(zoom, x, y, pixmap, MapTilePixmapItem::State::QUEUED);
      }
    }
  }

  processRequestQueue();
}

void MapView::requestTile(const int zoom, const int x, const int y)
{
  static int s_num_requests = 0;

  s_num_requests++;

  if (s_num_requests <= 2000)
  {
    printf("requesting tile %d: zoom=%d, x=%d, y=%d\n", s_num_requests, zoom, x, y);

    QString request_url;
    request_url.sprintf("https://tiles.sandbox.open-rmf.org/tile/%d/%d/%d.png", zoom, x, y);
    
    QNetworkRequest request;
    request.setUrl(QUrl(request_url));
    request.setRawHeader("User-Agent", "TrafficEditor/1.4 (http://open-rmf.org)");
    network->get(request);
  }
  else
    printf("past max number of requests this run (%d)... in case this is a wild bug, I'm stopping now!\n", s_num_requests);
}

void MapView::renderTile(const int zoom,
                         const int tile_x,
                         const int tile_y,
                         const QPixmap& pixmap,
                         const MapTilePixmapItem::State state)
{
  const double MAX_X = M_PI * CoordinateSystem::wgs84_a;

  // printf("  adding tile: zoom=%d, (%d, %d)\n", zoom, x, y);
  QGraphicsPixmapItem* pixmap_item = scene()->addPixmap(pixmap);

  pixmap_item->setScale(2. * MAX_X / 256.0 / (1 << zoom));
  pixmap_item->setZValue(-10.0);
  //pixmap_item->setScale(360. / 256.0 / (1 << zoom));

  // magic math from the OpenStreetMap "Slippy map tilenames" wiki page
  // const double lon_deg = x / static_cast<double>(1 << zoom) * 360. - 180.;
  // const double n = M_PI - 2.0 * M_PI * y / static_cast<double>(1 << zoom);
  // const double lat = atan(0.5 * (exp(n) - exp(-n)));
  const double x = (tile_x / static_cast<double>(1 << zoom) * 2. * MAX_X - MAX_X);
  const double y = (-tile_y / static_cast<double>(1 << zoom) * 2. * MAX_X + MAX_X);

  // project the latitude to "web mercator" latitude
  //const double lat_deg_mercator = 180. / M_PI * log(tan(lat) + 1. / cos(lat));

  // printf("  %.5f -> %.5f\n", 180. / M_PI * lat, lat_deg_mercator);
  //pixmap_item->setPos(lon_deg, lat_deg_mercator);
  pixmap_item->setPos(x, y);

  // flip Y because we want +Y = up
  pixmap_item->setTransform(pixmap_item->transform().scale(1., -1.));

  // todo: pixmap_item->setOffset() to deal with the upside-down rendering?
  // or setTransformOriginPoint?

  MapTilePixmapItem item;
  item.zoom = zoom;
  item.x = tile_x;
  item.y = tile_y;
  item.item = pixmap_item;
  item.state = state;
  tile_pixmap_items.push_back(item);
}

void MapView::requestFinished(QNetworkReply* reply)
{
  /*
  printf("mapview::request_finished()\n");
  printf("  request url: %s\n",
    reply->request().url().path().toStdString().c_str());
  */
  const string url(reply->request().url().path().toStdString());
  if (url.size() < 10)
    return;
  const size_t zoom_start = 6;
  const size_t zoom_end = url.find('/', zoom_start);
  if (zoom_end == string::npos)
    return;
  const string zoom_str = url.substr(zoom_start, zoom_end - zoom_start);

  const size_t x_start = zoom_end + 1;
  if (x_start >= url.size())
    return;
  const size_t x_end = url.find('/', x_start);
  const string x_str = url.substr(x_start, x_end - x_start);

  const size_t y_start = x_end + 1;
  if (y_start >= url.size())
    return;
  const size_t y_end = url.find('.', y_start);
  const string y_str = url.substr(y_start, y_end - y_start);

  const int zoom = std::stoi(zoom_str);
  const int x = std::stoi(x_str);
  const int y = std::stoi(y_str);

  QByteArray bytes = reply->readAll();
  printf("received %d-byte tile: zoom=%d x=%d y=%d\n",
    bytes.length(),
    zoom,
    x,
    y);

  QImage image;
  bool parse_ok = image.loadFromData(bytes);
  if (parse_ok)
  {
    QPixmap pixmap(QPixmap::fromImage(image.convertToFormat(
        QImage::Format_Grayscale8)));
    tile_cache.set(zoom, x, y, bytes);
    // find the placeholder pixmapitem and update its pixmap
    for (auto& item : tile_pixmap_items)
    {
      if (item.x == x && item.y == y && item.zoom == zoom)
      {
        item.item->setPixmap(pixmap);
        item.item->setGraphicsEffect(nullptr);
        item.state = MapTilePixmapItem::State::COMPLETED;
        break;
      }
    }
  }
  else
  {
    printf("  unable to parse\n");
  }

  // Now that this request is completed, we can issue the next request
  // in the queue.
  processRequestQueue();

  // schedule this reply object for deletion (eventually)
  reply->deleteLater();
}

void MapView::clear()
{
  tile_pixmap_items.clear();
}

void MapView::updateCacheSizeLabel(QLabel* label)
{
  if (!label)
    return;

  MapTileCache::CacheSize size = tile_cache.getSize();
  label->setText(
    QString("Tile cache: %1 files, %2 MB")
    .arg(size.files)
    .arg(size.bytes / 1.0e6, 0, 'g', 3));
}

void MapView::processRequestQueue()
{
  int n_requested = 0;

  for (const auto& tile : tile_pixmap_items)
  {
    if (tile.state == MapTilePixmapItem::State::REQUESTED)
      n_requested++;
  }
  // printf("  %d tile requests currently in flight\n", n_requested);

  for (auto& tile : tile_pixmap_items)
  {
    // todo: tune this, or allow it to be a user-configurable parameter.
    // It seems to behave fairly nicely with a cap at just one
    // request in flight, but that may vary depending on server load
    // and latency.
    const int MAX_REQUESTS = 1;
    if (n_requested >= MAX_REQUESTS)
      break;

    // since we have less than N requests outstanding, let's request
    // some more tiles
    if (tile.state == MapTilePixmapItem::State::QUEUED)
    {
      requestTile(tile.zoom, tile.x, tile.y);
      tile.state = MapTilePixmapItem::State::REQUESTED;

      QGraphicsColorizeEffect* colorize = new QGraphicsColorizeEffect;
      colorize->setColor(QColor::fromRgbF(1.0, 0.0, 0.0, 1.0));
      colorize->setStrength(1.0);
      tile.item->setGraphicsEffect(colorize);

      n_requested++;
    }
  }
}
