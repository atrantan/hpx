  # Require a recent version of cmake
cmake_minimum_required(VERSION 2.8.4 FATAL_ERROR)

add_executable(distributed_domains distributed_domains.cpp)
target_link_libraries (distributed_domains ${HPX_LIBRARIES})
