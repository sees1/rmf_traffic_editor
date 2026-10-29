#ifndef TRAFFIC_EDITOR__UTILS__GEOMETRY_HPP
#define TRAFFIC_EDITOR__UTILS__GEOMETRY_HPP

#include <Eigen/Geometry>

namespace utils {
namespace geometry {

template <typename ObjType>
Eigen::Isometry2d scenePose(
  const ObjType& obj, double scale, double translate_x,
  double translate_y, double rotation)
{
  Eigen::Isometry2d local_pose = Eigen::Isometry2d::Identity();
  local_pose.linear() = obj.pose.linear().transpose();
  local_pose.translation() = scale * obj.pose.translation();
  return Eigen::Translation2d(translate_x, translate_y)
    * Eigen::Rotation2Dd(-rotation) * local_pose;
}

} // namespace geometry
} // namespace utils

#endif
