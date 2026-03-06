#!/bin/bash

# Define paths
WORKSPACE="$HOME/Downloads/robot_ws"

# Setup ROS and workspace
source /opt/ros/jazzy/setup.bash
source "$WORKSPACE/install/setup.bash"

echo "Starting Gazebo Simulation and Robot State Publisher..."
gnome-terminal -- bash -c "cd $WORKSPACE && source /opt/ros/jazzy/setup.bash && source install/setup.bash && ros2 launch simulation_pkg gaz_sim_launch.py; exec bash"

sleep 8

echo "Starting SLAM Simulation (Live Mapping)..."
gnome-terminal -- bash -c "cd $WORKSPACE && source /opt/ros/jazzy/setup.bash && source install/setup.bash && ros2 launch slam_pkg slam_sim.launch.py; exec bash"

sleep 5

echo "Starting Navigation 2 Stack..."
gnome-terminal -- bash -c "cd $WORKSPACE && source /opt/ros/jazzy/setup.bash && source install/setup.bash && ros2 launch nav_pkg navigation_launch_sim.py; exec bash"

sleep 3

echo "Starting RViz2 Visualization..."
gnome-terminal -- bash -c "cd $WORKSPACE && source /opt/ros/jazzy/setup.bash && source install/setup.bash && rviz2 -d src/simulation_pkg/config/sim_view.rviz; exec bash"

sleep 2

echo "Starting Teleop Keyboard Control..."
gnome-terminal -- bash -c "cd $WORKSPACE && source /opt/ros/jazzy/setup.bash && source install/setup.bash && ros2 run teleop_twist_keyboard teleop_twist_keyboard --ros-args -r cmd_vel:=/cmd_vel_nav; exec bash"

echo "All services have been launched in separate terminals."
