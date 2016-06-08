  # Require a recent version of cmake
cmake_minimum_required(VERSION 2.8.4 FATAL_ERROR)

add_library(hpx_spmd_block SHARED spmd_block.cpp)
target_link_libraries (hpx_spmd_block ${HPX_LIBRARIES} hpx_remote_promise -DHPX_COMPONENT_EXPORTS)
