  # Require a recent version of cmake
cmake_minimum_required(VERSION 2.8.4 FATAL_ERROR)

add_executable(numa_domains numa_domains.cpp)
target_link_libraries (numa_domains ${HPX_LIBRARIES})