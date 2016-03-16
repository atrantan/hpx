cmake_minimum_required(VERSION 2.8.4)

include_directories(include)

# mkl libraries
# set(mkl_lib_dir
#     "/opt/intel/mkl/lib/intel64"
#     "/opt/intel/lib/intel64"
#    )

# set(mkl_include_dir
#     "/opt/intel/mkl/include"
#    )

# set(mkl_lib_name
#     mkl_intel_lp64
#     mkl_sequential
#     mkl_core
#     m
#     pthread
#    )

# include_directories(${mkl_include_dir})
# link_directories(${mkl_lib_dir})

# Create library for Matrix Market utilities
add_library(mmio SHARED mmio.cpp)

set(tests
    pvector_view_spmv
   )

foreach(test ${tests})
    set(sources ${test}.cpp)
    add_executable(${test}_test ${sources})
    target_link_libraries (${test}_test  ${HPX_LIBRARIES} hpx_partitioned_vector hpx_spmd_block mmio)
endforeach()
