  # Require a recent version of cmake
cmake_minimum_required(VERSION 2.8.4 FATAL_ERROR)

include_directories(include)

add_executable(wait_images wait_images.cpp)
target_link_libraries (wait_images ${HPX_LIBRARIES})
