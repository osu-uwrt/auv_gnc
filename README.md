AUV GNC: Guidance, Navigation, and Control for AUVs
===============================================================
CAUTION: This library is still in development. Use at your own risk.

## Overview
Guidance, Navigation, and Control (GNC) are three of the biggest challenges that all robots face, let alone autonomous robots. From unmanned aerial vehicles (UAVs) to self-driving cars to autonomous underwater vehicles (AUVs), every one of these types of robots must be capable of localizing, navigating, and guiding itself in an unknown environment, and also be capable of controlling its own movements. This library is specifically targeted at AUVs.

Finding an open-source "GNC-like" library for AUVs used to be a struggle, until now. AUVs are becomming increasingly popular, yet there never seemed to exist a solid GNC platform from which anyone can build off of. The purpose of this library is to do just that. AUV GNC is a C++ library containing (almost) all of the needed GNC-related code needed to start working with an AUV.

**Author: [Ted Sender](https://github.com/tsender) (tsender@umich.edu)**

**Former Affiliation: [The Ohio State University's Underwater Robotics Team](https://uwrt.engineering.osu.edu/), [AUVSI Robonation's RoboSub competition](https://www.robonation.org/competition/robosub)**

## Building/Installation
This library has been tested on Ubuntu 16.04 with ROS Kinetic Kame.

### Dependencies
AUV GNC is currently targeted towards the Robot Operating System ([ROS](https://www.ros.org/)), which must be installed to build the library as is. The system dependencies are listed below. It is recommended that you install these via the install script provided (see Building).
* [Eigen](https://eigen.tuxfamily.org/dox/GettingStarted.html)  (C++ linear algebra library).
    * Minimum version 3.3, but repo has only been tested with Eigen 3.3.7
* [Ceres Solver](http://ceres-solver.org/) (C++ non-linear solver developed by Google)
    * Ceres is installed with its own version of Eigen, so it must be installed first
* [CppAD](https://coin-or.github.io/CppAD/doc/cppad.htm) (C++ automatic differentiation library)

### Included Libraries
AUV GNC has two other dependencies listed below, but these libraries are included in the repo for your convenience. AUV GNC only relies on the Control Toolbox for optimal control solvers, however, because the Control Toolbox depends on Kindr, both libraries are included.
* [Control Toolbox](https://github.com/ethz-adrl/control-toolbox) (C++ optimized control library developed by researchers at ETH Zurich's Agile & Dexterous Robotics Lab)
* [Kindr](https://github.com/ANYbotics/kindr) (C++ robotics library, only included because the Control Toolbox depends on it)

### Building
To get started, clone this library into a catkin workspace.

    cd ~/catkin_ws/src
    git clone --recursive https://github.com/tsender/auv_gnc.git

For FIRST TIME users, please run the following to install the required dependencies:

    cd ~/catkin_ws/src/auv_gnc/installation/
    ./install_deps.sh
   
The suggested way to build/install this library is with [Catkin tools](https://catkin-tools.readthedocs.io/en/latest/installing.html). It is preferred that catkin be configured with the install space as follows

    cd ~/catkin_ws/
    catkin init
    catkin config --install

After initializing the install space, you should see a line indicating that the install space is merged. FInally, build the `auv_gnc` package (this will automatically build all of its dependent catkin/cmake packages).

    catkin build auv_gnc

### Installing to /opt/ros/kinetic
If you wish to install AUV GNC to the same location as where ROS Kinetic is installed to minimize the number of setup.bash files that need to be sourced, then you can run these commands. Before doing so, you must have your catkin workspace configured to use the install space as detailed above. Note, it is prefferred to build the repo normally and simply install it in root.

    cd ~/catkin_ws/
    catkin build auv_gnc
    sudo su
    source /opt/ros/kinetic/setup.bash
    catkin build auv_gnc --cmake-args -DCMAKE_INSTALL_PREFIX=/opt/ros/kinetic
    exit

### Notes
While you may also build the entire repo with the command `catkin build`, it is unnecessary to build all of the Control Toolbox packages since `auv_gnc` is only dependent on `ct_core` and `ct_optcon`. 

Building the entire AUV GNC repo can also take a while due to some of the additional overhead in the Control Toolbox. Further, the `ct_doc` package from the Control Toolbox does not build for some reason. If you build the entire repo, you will see an error message saying that there is "no target to make ct_doc" - it's just a doc package, so don't worry about it.
