#!/bin/bash

# Define paths
WORKSPACE="/home/argo/Downloads/robot_ws"
MAP_PATH="$WORKSPACE/office_map.yaml"

# Setup ROS and workspace
source /opt/ros/jazzy/setup.bash
source "$WORKSPACE/install/setup.bash"

echo "Starting Gazebo Simulation..."
gnome-terminal -- bash -c "cd $WORKSPACE && source /opt/ros/jazzy/setup.bash && source install/setup.bash && ros2 launch simulation_pkg gaz_sim_launch.py; exec bash"

sleep 8

echo "Starting Localization (AMCL) with map: $MAP_PATH"
gnome-terminal -- bash -c "cd $WORKSPACE && source /opt/ros/jazzy/setup.bash && source install/setup.bash && ros2 launch nav_pkg localization_launch_sim.py map:=$MAP_PATH; exec bash"

sleep 5

echo "Starting Navigation 2 Stack..."
gnome-terminal -- bash -c "cd $WORKSPACE && source /opt/ros/jazzy/setup.bash && source install/setup.bash && ros2 launch nav_pkg navigation_launch_sim.py; exec bash"

sleep 3

echo "Starting RViz2 Visualization..."
gnome-terminal -- bash -c "cd $WORKSPACE && source /opt/ros/jazzy/setup.bash && source install/setup.bash && rviz2 -d src/simulation_pkg/config/sim_view.rviz; exec bash"

sleep 2

echo "Starting Teleop Keyboard Control..."
gnome-terminal -- bash -c "cd $WORKSPACE && source /opt/ros/jazzy/setup.bash && source install/setup.bash && ros2 run teleop_twist_keyboard teleop_twist_keyboard --ros-args -r cmd_vel:=/cmd_vel_nav; exec bash"

echo "Navigation stack with static map has been launched in separate terminals."
