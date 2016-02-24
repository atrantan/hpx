#  Copyright (c) 2016 Antoine Tran Tan
#
#  Distributed under the Boost Software License, Version 1.0. (See accompanying
#  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

# Require a recent version of cmake
cmake_minimum_required(VERSION 2.8.4 FATAL_ERROR)

set(tests
     spmd_block_creation
   )

foreach(test ${tests})
    set(sources ${test}.cpp)
    add_executable(${test}_test ${sources})
    target_link_libraries (${test}_test ${HPX_LIBRARIES} hpx_spmd_block)
    add_custom_target(tests.unit.${test})
endforeach()
