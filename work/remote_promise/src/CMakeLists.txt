  # Require a recent version of cmake
cmake_minimum_required(VERSION 2.8.4 FATAL_ERROR)

add_library(hpx_remote_promise SHARED remote_promise.cpp)
target_link_libraries (hpx_remote_promise ${HPX_LIBRARIES} -DHPX_COMPONENT_EXPORTS)
