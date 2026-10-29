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

#ifndef MAP_VIEW_H
#define MAP_VIEW_H

#include <vector>

#include <QGraphicsPixmapItem>
#include <QGraphicsView>
#include <QWheelEvent>

#include "building.h"
#include "map_tile_cache.h"

class QNetworkAccessManager;
class QNetworkReply;
class QLabel;

class MapView : public QGraphicsView
{
  Q_OBJECT

private:
  struct MapTilePixmapItem
  {
  public:
    enum State
    {
      QUEUED,
      REQUESTED,
      COMPLETED
    };

  public:
    int zoom = 0;
    int x = 0;
    int y = 0;
    QGraphicsPixmapItem* item = nullptr;
    State state;
  };

  struct MapTileRequest
  {
  public:
    int zoom = 0;
    int x = 0;
    int y = 0;

    // todo: time it was requested?
  };

public:
  MapView(QWidget* parent, const Building& building_);

  void zoomFit(int level_index);

  void setShowTiles(const bool show_tiles_)
  {
    show_tiles = show_tiles_;
  }

  void drawTiles();

  void clear();

  void updateCacheSizeLabel(QLabel* label);

  QPointF getCenter() { return last_center; }

protected:
  void wheelEvent(QWheelEvent* event);

  void mouseMoveEvent(QMouseEvent* e);

  void mousePressEvent(QMouseEvent* e);

  void mouseReleaseEvent(QMouseEvent* e);

  void resizeEvent(QResizeEvent* e);

private:
  void requestTile(const int zoom, const int x, const int y);

  void renderTile(
    const int zoom,
    const int x,
    const int y,
    const QPixmap& pixmap,
    const MapTilePixmapItem::State state);

  void requestFinished(QNetworkReply* reply);

  void processRequestQueue();

private:
  bool is_panning = false;
  int pan_start_x = 0;
  int pan_start_y = 0;
  const Building& building;
  bool show_tiles = false;  // ignore first few resize events during startup
  MapTileCache tile_cache;
  std::vector<MapTilePixmapItem> tile_pixmap_items;
  std::vector<MapTileRequest> tile_requests;
  QNetworkAccessManager* network = nullptr;
  QPointF last_center;
};

#endif
