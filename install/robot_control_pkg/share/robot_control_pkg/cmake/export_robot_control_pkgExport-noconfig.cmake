#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "robot_control_pkg::robot_control_pkg" for configuration ""
set_property(TARGET robot_control_pkg::robot_control_pkg APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(robot_control_pkg::robot_control_pkg PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/librobot_control_pkg.so"
  IMPORTED_SONAME_NOCONFIG "librobot_control_pkg.so"
  )

list(APPEND _cmake_import_check_targets robot_control_pkg::robot_control_pkg )
list(APPEND _cmake_import_check_files_for_robot_control_pkg::robot_control_pkg "${_IMPORT_PREFIX}/lib/librobot_control_pkg.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
