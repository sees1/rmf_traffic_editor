cmake_minimum_required(VERSION 3.10.0)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

project(rmf_traffic_editor)

find_package(ament_cmake REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(Ceres REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(Qt5 COMPONENTS Widgets Concurrent Test Network REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(rmf_utils REQUIRED)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()
# set(CMAKE_VERBOSE_MAKEFILE TRUE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wshadow -Wextra")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(CMAKE_VERSION VERSION_LESS "3.7.0")
  set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

include_directories(.)
include_directories(include include/gui include/gui/actions include/crowd_sim)

set(gui_sources
  src/gui/actions/add_constraint.cpp
  src/gui/actions/add_edge.cpp
  src/gui/actions/add_feature.cpp
  src/gui/actions/add_fiducial.cpp
  src/gui/actions/add_model.cpp
  src/gui/actions/add_polygon.cpp
  src/gui/actions/add_property.cpp
  src/gui/actions/add_vertex.cpp
  src/gui/actions/delete.cpp
  src/gui/actions/move_feature.cpp
  src/gui/actions/move_fiducial.cpp
  src/gui/actions/move_lift.cpp
  src/gui/actions/move_model.cpp
  src/gui/actions/move_vertex.cpp
  src/gui/actions/polygon_remove_vertices.cpp
  src/gui/actions/polygon_add_vertex.cpp
  src/gui/actions/rotate_model.cpp
  src/gui/add_param_dialog.cpp
  src/gui/building.cpp
  src/gui/door.cpp
  src/gui/building_dialog.cpp
  src/gui/constraint.cpp
  src/gui/coordinate_system.cpp
  src/gui/delete_dialog.cpp
  src/gui/feature.cpp
  src/gui/edge.cpp
  src/gui/editor.cpp
  src/gui/editor_model.cpp
  src/gui/fiducial.cpp
  src/gui/graph.cpp
  src/gui/layer.cpp
  src/gui/layer_dialog.cpp
  src/gui/layer_table.cpp
  src/gui/level.cpp
  src/gui/level_dialog.cpp
  src/gui/level_table.cpp
  src/gui/lift.cpp
  src/gui/lift_dialog.cpp
  src/gui/lift_door.cpp
  src/gui/lift_table.cpp
  src/gui/map_tile_cache.cpp
  src/gui/map_view.cpp
  src/gui/model.cpp
  src/gui/model_dialog.cpp
  src/gui/param.cpp
  src/gui/polygon.cpp
  src/gui/preferences_dialog.cpp
  src/gui/preferences_keys.cpp
  src/gui/rendering_options.cpp
  src/gui/table_list.cpp
  src/gui/traffic_table.cpp
  src/gui/traffic_map.cpp
  src/gui/transform.cpp
  src/gui/vertex.cpp
  src/gui/yaml_utils.cpp

  #crowd_sim related
  src/gui/multi_select_combo_box.cpp
  src/crowd_sim/agent_group.cpp
  src/crowd_sim/agent_group_table.cpp
  src/crowd_sim/agent_profile.cpp
  src/crowd_sim/agent_profile_table.cpp
  src/crowd_sim/condition.cpp
  src/crowd_sim/condition_dialog.cpp
  src/crowd_sim/crowd_sim_dialog.cpp
  src/crowd_sim/crowd_sim_editor_table.cpp
  src/crowd_sim/crowd_sim_impl.cpp
  src/crowd_sim/crowd_sim_table_base.cpp
  src/crowd_sim/goal_set.cpp
  src/crowd_sim/goal_set_table.cpp
  src/crowd_sim/model_type.cpp
  src/crowd_sim/model_type_table.cpp
  src/crowd_sim/state.cpp
  src/crowd_sim/state_table.cpp
  src/crowd_sim/to_state_dialog.cpp
  src/crowd_sim/to_state_table.cpp
  src/crowd_sim/transition.cpp
  src/crowd_sim/transition_table.cpp
)

add_library(
  gui_lib STATIC
  ${gui_sources}
  include/crowd_sim/crowd_sim_editor_table.h
  include/gui/delete_dialog.h
  include/gui/editor.h
  include/gui/layer_dialog.h
  include/gui/layer_table.h
  include/gui/level_table.h
  include/gui/lift_dialog.h
  include/gui/map_view.h
  include/gui/model_dialog.h
  include/gui/table_list.h)

target_link_libraries(gui_lib PUBLIC
  ament_index_cpp::ament_index_cpp
  ceres
  Eigen3::Eigen
  Qt5::Widgets
  Qt5::Concurrent
  Qt5::Network
  proj
  yaml-cpp
  rmf_utils::rmf_utils
  ${ament_index_cpp_LIBRARIES}
)

add_executable(traffic_editor resources/resource.qrc
                              src/gui/main.cpp)

target_link_libraries(traffic_editor PUBLIC gui_lib)

set_property(TARGET traffic_editor PROPERTY ENABLE_EXPORTS 1)

install(TARGETS traffic_editor
  LIBRARY DESTINATION lib/${PROJECT_NAME}
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY plugins
  DESTINATION include/traffic_editor
)

install(DIRECTORY include/gui/
  DESTINATION include/traffic_editor)

install(DIRECTORY include/crowd_sim
  DESTINATION include/traffic_editor)

if (BUILD_TESTING)
  find_package(ament_cmake_uncrustify REQUIRED)

  add_subdirectory(test)

  find_file(uncrustify_config_file
    NAMES "rmf_code_style.cfg"
    PATHS "${rmf_utils_DIR}/../../../share/rmf_utils/")

  ament_uncrustify(
    ARGN include src
    CONFIG_FILE ${uncrustify_config_file}
    MAX_LINE_LENGTH 80
    LANGUAGE CPP
  )
endif()

ament_export_include_directories(include include/traffic_editor)

ament_package()
