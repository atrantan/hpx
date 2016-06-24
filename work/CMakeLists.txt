# Require a recent version of cmake
cmake_minimum_required(VERSION 2.8.4 FATAL_ERROR)

project(my_project)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

link_directories(${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}")

find_package(Hpx REQUIRED)

if (HPX_FOUND)
  set(CMAKE_CXX_FLAGS "${HPX_DEFINITIONS}")
  include_directories(${HPX_INCLUDE_DIRS})
  link_directories(${HPX_LIBRARY_DIR})

include_directories(make_action/include)
include_directories(utility/include)
include_directories(stencil_view/include)
include_directories(spmd_block/include)
include_directories(remote_promise/include)
include_directories(barrier_algorithms/include)

# Add programs here
add_subdirectory(remote_promise)
add_subdirectory(spmd_block)
add_subdirectory(pvector_view)
add_subdirectory(make_action)
# add_subdirectory(stencil_view)
add_subdirectory(barrier_algorithms)


add_custom_target(
    run_tests_and_plot_results
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMAND python ${CMAKE_SOURCE_DIR}/run.py ${CMAKE_SOURCE_DIR}
    COMMAND pdflatex ${CMAKE_SOURCE_DIR}/plot_results.tex
)

add_custom_target(
    run_tests_and_plot_barrier_results
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMAND python ${CMAKE_SOURCE_DIR}/run_barrier_test.py
    COMMAND pdflatex ${CMAKE_SOURCE_DIR}/plot_barrier_results.tex
)

endif()
