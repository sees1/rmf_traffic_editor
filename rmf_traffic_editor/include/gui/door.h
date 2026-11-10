#ifndef TRAFFIC_EDITOR_DOOR_H
#define TRAFFIC_EDITOR_DOOR_H

#include "edge.h"
#include <nlohmann/json.hpp>
#include <Eigen/Core>
#include <optional>

// Door edges retain the building-map [start, end, params] representation.
// Approach indices refer to vertices on the same level, not door endpoints.
class Door : public Edge
{
public:
  Door();
  Door(const int start_idx, const int end_idx)
  : Edge(start_idx, end_idx, Edge::Type::DOOR)
  {
    if (start_idx < 0 || end_idx < 0)
      throw std::runtime_error("Door::Door: can't create door with start_idx|end_idx < 0!");
  }
  explicit Door(const Edge& edge);

  void fromYaml(const YAML::Node& data);
  YAML::Node toYaml() const;
  
  // Feature with front -> back LineString; null geometry without coordinates.
  nlohmann::json toJson() const;
  
  int frontVertex() const;
  int backVertex() const;
  void setApproachVertices(int front, int back);

  // Snapshot of Level::vertices coordinates in level scene units.
  std::optional<Eigen::Vector2d> frontPosition() const;
  std::optional<Eigen::Vector2d> backPosition() const;

  void setApproachPositions(const Eigen::Vector2d& front, const Eigen::Vector2d& back);

public:
  int getLevel() const { return level_idx; }

  // temporal
  int getStartIdx() const { return start_idx; };
  int getEndIdx() const { return end_idx; };

public:
  std::optional<std::vector<Eigen::Vector2d>> getNavVertices(const Level& level)
  {
    double meters_per_pixel = level.drawing_meters_per_pixel;

    if (!std::isfinite(meters_per_pixel) || meters_per_pixel <= 0)
      return std::nullopt;

    auto&& start_v = level.vertices[start_idx];
    auto&& end_v = level.vertices[end_idx];

    Eigen::Vector2d direction(end_v.x - start_v.x, end_v.y - start_v.y);
    if (direction.norm() < 1e-9)
      return std::nullopt;

    Eigen::Vector2d center((start.x + end.x) / 2, (start.y + end.y) / 2);
    Eigen::Vector2d normal(-direction.y() / direction.norm(), direction.x() / direction.norm());

    door_key = "door/" + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();

    params["editor_door_id"] = Param(id)

    Eigen::Vector2d front_position = center + 0.5 / meters_per_pixel * normal;
    Vertex front_vertex(front_position.x(), front_position.y(), name + "_front");
    front_vertex.params["editor_generated_vertex"] = Param(door_key + "/front");
    front_vertex.params["door_name"] = Param(params.at("name").value_string);
    front_vertex.params["door_side"] = Param(std::string("front"));

    const Eigen::Vector2d back_position = center - 0.5 / meters_per_pixel * normal;
    Vertex back_vertex(back_position.x(), back_position.y(), name + "_back");
    back_vertex.params["editor_generated_vertex"] = Param(door_key + "/back");
    back_vertex.params["door_name"] = Param(params.at("name").value_string);
    back_vertex.params["door_side"] = Param(std::string("back"));

    front_nav_idx = static_cast<int>(level.vertices.size()) - 1;
    back_nav_idx = static_cast<int>(level.vertices.size()) - 1;

    params.erase("front_vertex_x");
    params.erase("front_vertex_y");
    params.erase("back_vertex_x");
    params.erase("back_vertex_y");
    params.erase("front_vertex");
    params.erase("back_vertex");

    if (front >= 0 && back >= 0)
    {
      params["front_vertex"] = Param(front);
      params["back_vertex"] = Param(back);
    }

    params["front_vertex_x"] = Param(front_vertex.x());
    params["front_vertex_y"] = Param(front_vertex.y());
    params["back_vertex_x"] = Param(back_vertex.x());
    params["back_vertex_y"] = Param(back_vertex.y());

    return {front_vertex, back_vertex};
  }

  std::shared_ptr<Edge> getNavEdge()
  {
    if (!front_nav_idx.has_value() || !back_nav_idx.has_value() || door_key.has_value())
      return nullptr;

    std::shared_ptr<Edge> lane = make_shared<Edge>(*front_nav_idx, *back_nav_idx, Edge::LANE);
    lane.params["editor_generated_edge"] = Param(*door_key);
    lane.params["door_name"] = Param(params.at("name").value_string);
    
    return lane;
  }

private:
  std::optional<int> front_nav_idx;
  std::optional<int> back_nav_idx;

  std::optional<std::string> door_key;
};

#endif
