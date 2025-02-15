#ifndef TRAJECTORY_LINE
#define TRAJECTORY_LINE

#include "auv_guidance/trajectory_generator_abstract.hpp"
#include "auv_guidance/segment_planner.hpp"
#include "eigen3/Eigen/Dense"
#include "math.h"

using namespace Eigen;

namespace AUV_GNC
{
namespace Trajectory
{
// Creates a line segment in space using the SegmentPlanner for position/speed along the segment.
// Assumes the speed is either zero or equal to the speed entered in the constructor
class Line : public TrajectoryGenerator
{
private:
  SegmentPlanner *segPlanner_;
  float cruiseSpeed_, acceleration_;
  int accelSeq_;
  Vector3f initialPos_, finalPos_, insertionMap_;

public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  static const float DEFAULT_SPEED = 0.5; // [m/s]
  static const float DEFAULT_ACCEL = 0.2; // [m/s^2]

  Line(const Ref<const Vector3f> initialPos, const Ref<const Vector3f> finalPos,
       float nominalSpeed = Line::DEFAULT_SPEED, float acceleration = 0.0, int seq = SegmentPlanner::SEQ_NONE);
  float getTravelTime();
  Vector12f computeState(float time);
};
} // namespace Trajectory
} // namespace AUV_GNC

#endif