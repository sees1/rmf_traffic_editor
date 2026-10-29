
namespace utils {
namespace geometry {
  bool hasObjWithinThreshold(const Building& building, Eigen::Vector2d point, double distance, const std::vector<Lift>& lifts)
  {

  }
  bool hasObjWithinThreshold(Eigen::Vector2d point, double distance, const Lift& lift)
  {

  }
  bool hasObjWithinThreshold(const Building& building, Eigen::Vector2d point, double distance, const std::vector<Level>& levels)
  {
    
  }
  bool hasObjWithinThreshold(Eigen::Vector2d point, double distance, const Level& level)
  {

  }

  template <typename ObjType>
  Eigen::Isometry2d scenePose(const ObjType& obj, Eigen::Vector2d translation, double rotation)
  {
    Eigen::Isometry2d result = Eigen::Isometry2d::Identity();

    const double yaw = std::atan2(pose.linear()(1, 0), pose.linear()(0, 0));
    
    result.linear() = Eigen::Rotation2Dd(-yaw - rotation).toRotationMatrix();
    result.translation() = scale * (Eigen::Rotation2Dd(-rotation) * pose.translation()) + Eigen::Vector2d(translate_x, translate_y);
    return result;
  }

} // namespace utils
} // namespace geometry