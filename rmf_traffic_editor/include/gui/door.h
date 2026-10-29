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
  void setApproachPositions(const Eigen::Vector2d& front,
    const Eigen::Vector2d& back);
};

#endif
