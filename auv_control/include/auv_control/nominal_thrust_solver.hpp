#ifndef NOMINAL_THRUST_SOLVER
#define NOMINAL_THRUST_SOLVER

#include "ceres/ceres.h"
#include "eigen3/Eigen/Dense"
#include "eigen3/Eigen/Core"
#include "auv_core/math_lib.hpp"
#include "auv_core/constants.hpp"

namespace auv_control
{
typedef Eigen::Matrix<double, 6, 1> Vector6d;
typedef Eigen::Matrix<double, 6, 2> Matrix62d;
typedef Eigen::Matrix<double, 6, 8> Matrix68d;
class NominalThrustSolver
{
private:
   double mass_, Fg_, Fb_;
   Eigen::Matrix3d inertia_;    // Inertia matrix expressed in B-frame
   Eigen::Vector3d CoB_;        // Center of buoyancy relative to center of mass
   Matrix62d dragCoeffs_;       // 1st and 2nd order drag coefficients for translational and rotational motion
   Matrix68d thrustCoeffs_;     // Thrust coefficients (effective contributions of each thruster for force and moment)
   double *quaternion_;         // (Pointer) to quaternion for orientation
   double *uvw_;                // (Pointer) Inertial translational velocity expressed in B-frame (U, V, W)
   double *pqr_;                // (Pointer) Angular velocity expressed in B-frame (P, Q, R)
   double *inertialTransAccel_; // (Pointer) Inertial translational acceleration expressed in B-frame
   double *pqrDot_;             // (Pointer) Time derivative of angular velocity measured in B-frame

   static constexpr double GRAVITY = 9.80665;

public:
   EIGEN_MAKE_ALIGNED_OPERATOR_NEW
   NominalThrustSolver(double Fg, double Fb, const Eigen::Ref<const Eigen::Vector3d> &CoB, const Eigen::Ref<const Eigen::Matrix3d> &inertia,
                       const Eigen::Ref<const Matrix62d> &dragCoeffs, const Eigen::Ref<const Matrix68d> &thrustCoeffs,
                       double *quaternion, double *uvw, double *pqr, double *inertialTransAccel, double *pqrDot)
   {
      Fg_ = Fg;
      Fb_ = Fb;
      mass_ = Fg_ / auv_core::constants::GRAVITY;
      CoB_ = CoB;
      inertia_ = inertia;
      dragCoeffs_ = dragCoeffs;
      thrustCoeffs_ = thrustCoeffs;
      quaternion_ = quaternion;
      uvw_ = uvw;
      pqr_ = pqr;
      inertialTransAccel_ = inertialTransAccel;
      pqrDot_ = pqrDot;
   }

   template <typename T>
   bool operator()(const T *const nominalForces, T *residuals) const
   {
      // NOTE: MUST have extra space in nested <> to compile: "<<...> >(...)"
      //                                                             ^

      // Cast Eigen objects to Jet type
      Eigen::Matrix<T, 6, 2> dragCoeffsT = dragCoeffs_.cast<T>();
      Eigen::Matrix<T, 6, 8> thrustCoeffsT = thrustCoeffs_.cast<T>();
      Eigen::Matrix<T, 3, 3> inertiaT = inertia_.cast<T>();
      Eigen::Matrix<T, 3, 1> CoBT = CoB_.cast<T>();

      // Cast arrays to Jet type
      Eigen::Matrix<T, 3, 1> uvwT, pqrT, pqrDotT, inertialTransAccelT, inertialRotAccelT;
      for (int i = 0; i < 3; i++)
      {
         uvwT(i) = T(uvw_[i]);
         pqrT(i) = T(pqr_[i]);
         inertialTransAccelT(i) = T(inertialTransAccel_[i]);
         pqrDotT(i) = T(pqrDot_[i]);
      }

      // Map ceres parameters and residuals to Eigen object with Jet type
      typedef Eigen::Matrix<T, 8, 1> Vector8T;
      typedef Eigen::Matrix<T, 6, 1> Vector6T;
      Eigen::Map<const Vector8T> nominalForcesT(nominalForces);
      Eigen::Map<Vector6T> residualsT(residuals);

      // Compute quaternion
      // Using Eigen::Quaternion: quaternion * vector = rotates vector by the described axis-angle
      // So: B-frame vector = quaternion.conjugate() * I-frame vector
      // So: I-frame vector = quaternion * B-frame vector
      Eigen::Quaterniond quat(quaternion_[0], quaternion_[1], quaternion_[2], quaternion_[3]);
      Eigen::Quaternion<T> quatT = quat.cast<T>();

      // Translational Equations
      Eigen::Matrix<T, 3, 1> weightT, transDragT;
      weightT.setZero();
      transDragT.setZero();
      weightT(2) = T(Fg_ - Fb_);

      transDragT(0) = dragCoeffsT(0, 0) * uvwT(0) + T(auv_core::math_lib::sign(uvw_[0])) * dragCoeffsT(3, 0) * uvwT(0) * uvwT(0);
      transDragT(1) = dragCoeffsT(1, 0) * uvwT(1) + T(auv_core::math_lib::sign(uvw_[1])) * dragCoeffsT(4, 0) * uvwT(1) * uvwT(1);
      transDragT(2) = dragCoeffsT(2, 0) * uvwT(2) + T(auv_core::math_lib::sign(uvw_[2])) * dragCoeffsT(5, 0) * uvwT(2) * uvwT(2);

      // Net_F = ma OR
      // 0 = ma - Net_F
      residualsT.template head<3>() = (T(mass_) * inertialTransAccelT) - (quatT.conjugate() * weightT - transDragT +
                                                                          (thrustCoeffsT.template block<3, 8>(0, 0)) * nominalForcesT);

      // Rotational Equations
      Eigen::Matrix<T, 3, 1> forceBuoyancyT, rotDragT;
      forceBuoyancyT.setZero();
      rotDragT.setZero();
      forceBuoyancyT(2) = T(-Fb_);

      rotDragT(0) = dragCoeffsT(0, 1) * pqrT(0) + T(auv_core::math_lib::sign(pqr_[0])) * dragCoeffsT(3, 1) * pqrT(0) * pqrT(0);
      rotDragT(1) = dragCoeffsT(1, 1) * pqrT(1) + T(auv_core::math_lib::sign(pqr_[1])) * dragCoeffsT(4, 1) * pqrT(1) * pqrT(1);
      rotDragT(2) = dragCoeffsT(2, 1) * pqrT(2) + T(auv_core::math_lib::sign(pqr_[2])) * dragCoeffsT(5, 1) * pqrT(2) * pqrT(2);

      inertialRotAccelT = inertiaT * pqrDotT + pqrT.cross(inertiaT * pqrT); // I*wDot + w x I*w

      // Net_Moment = Iw + w x Iw OR
      // 0 = Iw + w x Iw - Net_Moment
      residualsT.template tail<3>() = inertialRotAccelT - (CoBT.cross(quatT.conjugate() * forceBuoyancyT) - rotDragT +
                                                           (thrustCoeffsT.template block<3, 8>(3, 0)) * nominalForcesT);
      return true;
   }
};

} // namespace auv_control

#endif