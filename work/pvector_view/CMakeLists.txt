  # Require a recent version of cmake
cmake_minimum_required(VERSION 2.8.4 FATAL_ERROR)

include_directories(include)
add_subdirectory(tests/unit)
add_subdirectory(tests/performance)
