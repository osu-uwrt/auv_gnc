#include "auv_guidance/basic_trajectory.hpp"

namespace auv_guidance
{
/**
 * @param start Starting waypoint
 * @param end Ending waypoint
 * @param travelDuration Desired travel duration [s]
 */
BasicTrajectory::BasicTrajectory(Waypoint *wStart, Waypoint *wEnd, TGenLimits *tGenLimits)
{
    wStart_ = wStart;
    wEnd_ = wEnd;
    tGenLimits_ = tGenLimits;

    qStop_ = Eigen::Quaterniond::Identity();
    deltaVec_.setZero();
    unitVec_.setZero();
    maxVelocityVec_.setZero();

    maxVelocity_ = 0;
    stopDuration_ = 0;
    simultaneousDuration_ = 0;
    longDuration_ = 0;
    distance_ = 0;

    longTrajectory_ = false;
    simultaneousTrajectory_ = false;
    exceedsMaxSpeed_ = false;

    BasicTrajectory::setStopTrajectory();
    BasicTrajectory::setPrimaryTrajectory();
}

void BasicTrajectory::setStopTrajectory()
{
    // Get stop position
    double transVel = wStart_->velI().squaredNorm();
    double transAccel = wStart_->accelI().squaredNorm();
    double accel = 0;
    if (fabs(wStart_->velI()(0)) > fabs(wStart_->velI()(1))) // Xvel > Yvel
    {
        accel = tGenLimits_->maxXAccel();
        if (fabs(wStart_->velI()(2)) > fabs(wStart_->velI()(0))) // Zvel > Xvel
            accel = tGenLimits_->maxZAccel();
    }
    else
    {
        accel = tGenLimits_->maxYAccel();
        if (fabs(wStart_->velI()(2)) > fabs(wStart_->velI()(1))) // Zvel > Yvel
            accel = tGenLimits_->maxZAccel();
    }
    double distance = (2.0 / 3.0) * (transVel * transVel) / accel;
    Eigen::Vector3d restDeltaVec = wStart_->velI().normalized() * distance;
    Eigen::Vector3d stopPos = wStart_->posI() + restDeltaVec;

    // Get stop quaternion
    Eigen::Vector4d angleAxis = Eigen::Vector4d::Zero();
    angleAxis.tail<3>() = wStart_->angVelB().normalized();
    //std::cout << "BT set stop trajectory: ang vel start " << std::endl << wStart_->angVelB().normalized() << std::endl;
    //std::cout << "BT set stop trajectory: angle axis " << std::endl << angleAxis << std::endl;

    double angVel = wStart_->angVelB().squaredNorm(); // Magnitude
    double angularDistance = (2.0 / 3.0) * (angVel * angVel) / tGenLimits_->maxRotAccel();
    angleAxis(0) = angularDistance;

    Eigen::Quaterniond qRotate = auv_core::math_lib::angleAxis2Quaternion(angleAxis); // Relative to B-frame
    qStop_ = wStart_->quaternion() * qRotate;                                         // Apply qRotate FROM qStart
    //std::cout << "BT set stop trajetory: start waypoint quat" << std::endl << wStart_->quaternion().w() << std::endl << wStart_->quaternion().vec() << std::endl;
    //std::cout << "BT set stop trajetory: rotate quat" << std::endl << qRotate.w() << std::endl << qRotate.vec() << std::endl;
    //std::cout << "BT set stop trajetory:  qStop" << std::endl << qStop_.w() << std::endl << qStop_.vec() << std::endl;

    Eigen::Vector3d zero3d = Eigen::Vector3d::Zero();
    wStop_ = new Waypoint(stopPos, zero3d, zero3d, qStop_, zero3d);

    // Find travel time for translation and rotation, take the longer one
    Eigen::Vector4d transStart = Eigen::Vector4d::Zero();
    Eigen::Vector4d transEnd = Eigen::Vector4d::Zero();
    Eigen::Vector4d rotStart = Eigen::Vector4d::Zero();
    Eigen::Vector4d rotEnd = Eigen::Vector4d::Zero();

    transStart << 0, transVel, transAccel, tGenLimits_->xyzJerk(distance);
    transEnd << distance, 0, 0, tGenLimits_->xyzJerk(distance);
    rotStart << 0, angVel, 0, tGenLimits_->rotJerk(angularDistance);
    rotEnd << angularDistance, 0, 0, tGenLimits_->rotJerk(angularDistance);

    double timeTrans = 0, timeRot = 0;
    MinJerkTimeSolver *mjts;
    mjts = new MinJerkTimeSolver(transStart, transEnd);
    timeTrans = mjts->getTime();
    mjts = new MinJerkTimeSolver(rotStart, rotEnd);
    timeRot = mjts->getTime();
    stopDuration_ = std::max(timeTrans, timeRot); // Take longer duration

    stStop_ = new SimultaneousTrajectory(wStart_, wStop_, stopDuration_);
    totalDuration_ = stopDuration_;
}

void BasicTrajectory::computeMaxVelocity()
{
    deltaVec_ = wEnd_->posI() - wStop_->posI();
    unitVec_ = deltaVec_.normalized();

    double distanceXY = deltaVec_.head<2>().squaredNorm();
    double distanceZ = fabs(deltaVec_(2));
    distance_ = deltaVec_.squaredNorm();

    if (distanceXY > tGenLimits_->maxXYDistance())
    {
        longTrajectory_ = true;
        std::cout << "BT: dixtance XY too large: " << distanceXY << " > " << tGenLimits_->maxXYDistance() << std::endl; // Debug
    }
    if (distanceZ > tGenLimits_->maxZDistance())
    {
        longTrajectory_ = true;
        std::cout << "BT: dixtance Z too large: " << distanceZ << " > " << tGenLimits_->maxZDistance() << std::endl; // Debug
    }

    BasicTrajectory::computeSimultaneousTime();

    // Find max translational velocity
    Eigen::Vector3d transStart = Eigen::Vector3d::Zero();
    Eigen::Vector3d transEnd = Eigen::Vector3d::Zero();
    transEnd(0) = distance_;
    mjtHelper_ = new MinJerkTrajectory(transStart, transEnd, simultaneousDuration_);

    initialMaxVelocity_ = fabs(mjtHelper_->getMiddleVelocity());
    maxVelocity_ = initialMaxVelocity_;
    maxVelocityVec_ = unitVec_ * maxVelocity_;
}

void BasicTrajectory::computeSimultaneousTime()
{
    // Translation
    Eigen::Vector4d transStart = Eigen::Vector4d::Zero();
    Eigen::Vector4d transEnd = Eigen::Vector4d::Zero();
    transStart << 0, 0, 0, tGenLimits_->xyzJerk(distance_);
    transEnd << distance_, 0, 0, tGenLimits_->xyzJerk(distance_);

    // Rotation
    qEnd_ = wEnd_->quaternion();
    Eigen::Quaterniond qDiff = qStop_.conjugate() * qEnd_; // Error quaternion wrt B-frame (q2 * q1.conjugate is wrt I-frame)
    double angularDistance = auv_core::math_lib::quaternion2AngleAxis(qDiff)(0);

    Eigen::Vector4d rotStart = Eigen::Vector4d::Zero();
    Eigen::Vector4d rotEnd = Eigen::Vector4d::Zero();
    rotStart << 0, 0, 0, tGenLimits_->rotJerk(angularDistance);
    rotEnd << angularDistance, 0, 0, tGenLimits_->rotJerk(angularDistance);

    // Compute durations
    double timeTrans = 0, timeRot = 0;
    MinJerkTimeSolver *mjts;
    mjts = new MinJerkTimeSolver(transStart, transEnd);
    timeTrans = mjts->getTime();
    mjts = new MinJerkTimeSolver(rotStart, rotEnd);
    timeRot = mjts->getTime();

    simultaneousDuration_ = std::max(timeTrans, timeRot); // Take longer duration
    std::cout << "BT: simultaneous duration: " << simultaneousDuration_ << std::endl; // Debug
}

void BasicTrajectory::setPrimaryTrajectory()
{
    BasicTrajectory::computeMaxVelocity();

    double maxXVel = fabs(maxVelocityVec_(0));
    double maxYVel = fabs(maxVelocityVec_(1));
    double maxZVel = fabs(maxVelocityVec_(2));

    // Verify XYZ velocities are not violated
    if (maxXVel > tGenLimits_->maxXVel())
    {
        exceedsMaxSpeed_ = true;
        maxXVel = tGenLimits_->maxXVel();
        std::cout << "BT: max X velocity too large: " << maxXVel <<  " > " << tGenLimits_->maxXVel() << std::endl; // Debug
    }
    if (maxYVel > tGenLimits_->maxYVel())
    {
        exceedsMaxSpeed_ = true;
        maxYVel = tGenLimits_->maxYVel();
        std::cout << "BT: max Y velocity too large: " << maxYVel <<  " > " << tGenLimits_->maxYVel() << std::endl; // Debug
    }
    if (maxZVel > tGenLimits_->maxZVel())
    {
        exceedsMaxSpeed_ = true;
        maxZVel = tGenLimits_->maxZVel();
        std::cout << "BT: max Z velocity too large: " << maxZVel << " > " << tGenLimits_->maxZVel() << std::endl; // Debug
    }

    // Update max velocity vector
    maxVelocity_ = sqrt(maxXVel * maxXVel + maxYVel * maxYVel + maxZVel * maxZVel);
    maxVelocityVec_ = unitVec_ * maxVelocity_;

    if (longTrajectory_ || exceedsMaxSpeed_) // Execute long trajectory
    {
        longTrajectory_ = true;
        double cruiseRatio = 1.0 - maxVelocity_ / initialMaxVelocity_;
        ltPrimary_ = new LongTrajectory(wStop_, wEnd_, tGenLimits_, cruiseRatio, maxVelocity_);
        longDuration_ = ltPrimary_->getTime();
        totalDuration_ += longDuration_;
    }
    else // Execute simultaneous trajectory
    {
        simultaneousTrajectory_ = true;
        stPrimary_ = new SimultaneousTrajectory(wStop_, wEnd_, simultaneousDuration_);
        totalDuration_ = simultaneousDuration_;
    }
    std::cout << "BT long trajectory " << longTrajectory_ << std::endl; // Debug
    std::cout << "BT simultaneous trajectory " << simultaneousTrajectory_ << std::endl; // Debug
}

double BasicTrajectory::getTime()
{
    return totalDuration_;
}

/**
 * @param time Time to compute the state at
 * Computes the trajectory state at the specified time
 */
Vector13d BasicTrajectory::computeState(double time)
{
    if (time <= stopDuration_)
    {
        std::cout << "ST: stop trajectory at time " << time << std::endl;
        return stStop_->computeState(time);
    }
    else if (simultaneousTrajectory_)
    {
        std::cout << "ST: main trajectory at time " << time - stopDuration_ << std::endl;
        return stPrimary_->computeState(time - stopDuration_);
    }
    else if (longTrajectory_)
    {
        return ltPrimary_->computeState(time - stopDuration_);
    }
}

/**
 * @param time Time to compute accelerations at
 * Compute inertial translational acceleration and time-derivative of angular veocity, 
 * both expressed in B-frame, at specified time
 */
Vector6d BasicTrajectory::computeAccel(double time)
{
    if (time <= stopDuration_)
    {
        return stStop_->computeAccel(time);
    }
    else if (simultaneousTrajectory_)
    {
        return stPrimary_->computeAccel(time - stopDuration_);
    }
    else if (longTrajectory_)
    {
        return ltPrimary_->computeAccel(time - stopDuration_);
    }
}
} // namespace auv_guidance