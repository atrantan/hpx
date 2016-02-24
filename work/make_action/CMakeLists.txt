  # Require a recent version of cmake
cmake_minimum_required(VERSION 2.8.4 FATAL_ERROR)

include_directories(include)
add_executable(make_action make_action.cpp)
target_link_libraries (make_action ${HPX_LIBRARIES})
