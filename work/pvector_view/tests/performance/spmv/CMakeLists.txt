cmake_minimum_required(VERSION 2.8.4)

include_directories(include)

# mkl libraries
set(mkl_lib_dir
    "/opt/intel/mkl/lib/intel64"
    "/opt/intel/lib/intel64"
   )

set(mkl_include_dir
    "/opt/intel/mkl/include"
   )

set(mkl_seq_lib_name
    mkl_intel_lp64
    mkl_core
    mkl_sequential
    pthread
    m
    dl
   )

# set(mkl_threaded_lib_name
#     mkl_intel_lp64
#     mkl_core
#     mkl_gnu_thread
#     pthread
#     m
#     dl
#    )

# Create library for Matrix Market utilities
add_library(mmio SHARED mmio.cpp)

include_directories(${mkl_include_dir})

set(tests
    pvector_view_spmv
    pvector_view_spmv_with_tasks
    hpx_threaded_spmv
   )

foreach(test ${tests})
    set(sources ${test}.cpp)
    add_executable(${test}_test ${sources})
    set_target_properties(${test}_test PROPERTIES COMPILE_FLAGS "-m64")
    target_link_libraries (${test}_test  ${mkl_seq_lib_name} ${HPX_LIBRARIES} hpx_partitioned_vector hpx_remote_promise mmio)
endforeach()

# set(test mkl_threaded_spmv)
# link_directories(${mkl_lib_dir})
# set(sources ${test}.cpp)
# add_executable(${test}_test ${sources})
# set_target_properties(${test}_test PROPERTIES COMPILE_FLAGS "-fopenmp -m64")
# target_link_libraries (${test}_test ${HPX_LIBRARIES} ${mkl_threaded_lib_name} -fopenmp  mmio)
