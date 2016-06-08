#  Copyright (c) 2016 Antoine Tran Tan
#
#  Distributed under the Boost Software License, Version 1.0. (See accompanying
#  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

# Require a recent version of cmake
cmake_minimum_required(VERSION 2.8.4 FATAL_ERROR)

set(CMAKE_BUILD_TYPE "Release")

# add_subdirectory(for_each)
add_subdirectory(spmv)
add_subdirectory(transpose)
add_subdirectory(stream)
