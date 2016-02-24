# Require a recent version of cmake
cmake_minimum_required(VERSION 2.8.4 FATAL_ERROR)

include(CheckCXXCompilerFlag)

if(NOT DEFINED HPX_BINARY_ROOT)
  set(HPX_BINARY_ROOT $ENV{HPX_BINARY_ROOT})
endif()

if(NOT DEFINED HPX_ROOT)
  set(HPX_ROOT $ENV{HPX_ROOT})
endif()

if(NOT DEFINED BOOST_ROOT)
  set(BOOST_ROOT $ENV{BOOST_ROOT})
endif()

find_package(Boost 1.57.0 QUIET
             COMPONENTS
             chrono
             date_time
             filesystem
             program_options
             regex
             system
             thread
             context
             random
             atomic)

if(HPX_ROOT AND BOOST_ROOT)
set(HPX_FOUND TRUE)
# Find HPX include path.
find_path(HPX_INCLUDE_DIR hpx/hpx.hpp
          PATHS ${HPX_ROOT}
          PATH_SUFFIXES include
          NO_DEFAULT_PATH)

# Find Boost libraries provided by HPX.
if(IS_DIRECTORY ${HPX_INCLUDE_DIR}/hpx/external)
  set(HPX_INCLUDE_EXTERNAL_DIRS ${HPX_INCLUDE_DIR}/hpx/external)
else()
  set(HPX_INCLUDE_EXTERNAL_DIRS ${HPX_BINARY_ROOT})
  foreach(ext asio atomic boostbook cache lockfree)
    if(NOT IS_DIRECTORY ${Boost_INCLUDE_DIR}/boost/${ext})
      if(IS_DIRECTORY ${HPX_ROOT}/external/${ext})
        list(APPEND HPX_INCLUDE_EXTERNAL_DIRS ${HPX_ROOT}/external/${ext})
      endif()
    endif()
  endforeach()
endif()

# Set HPX library path.
if(IS_DIRECTORY ${HPX_ROOT}/lib)
  set(HPX_LIBRARY_DIR ${HPX_ROOT}/lib)
else()
  set(HPX_LIBRARY_DIR ${HPX_BINARY_ROOT}/lib)
endif()

# Add HPX libraries in the project
foreach(lib hpx hpx_init hpx_iostreams)
  string(TOUPPER ${lib} lib_U)
  find_library(${lib_U}_LIBRARY ${lib}
               PATHS ${HPX_LIBRARY_DIR}
               PATH_SUFFIXES hpx
               NO_DEFAULT_PATH)
 endforeach()

# Add Boost libraries in the project
foreach(lib
        CHRONO
        DATE_TIME
        FILESYSTEM
        PROGRAM_OPTIONS
        REGEX
        SYSTEM
        THREAD
        CONTEXT RANDOM
        ATOMIC)
  if(Boost_${lib}_LIBRARY_DEBUG AND Boost_${lib}_LIBRARY_RELEASE)
    list(APPEND BOOST_DEPENDENCIES_LIBRARIES
         ${Boost_${lib}_LIBRARY_DEBUG}
         ${Boost_${lib}_LIBRARY_RELEASE}
        )
  else()
    list(APPEND BOOST_DEPENDENCIES_LIBRARIES ${Boost_${lib}_LIBRARY_RELEASE})
  endif()
endforeach()

# Add additionnal flag for static (or dynamic) link with Boost
if(NOT Boost_USE_STATIC_LIBS)
  set(HPX_DEFINITIONS "${HPX_DEFINITIONS} -DBOOST_ALL_DYN_LINK")
endif()

# Add needed flags depending on the environment
if(NOT MSVC)
  check_cxx_compiler_flag(-std=c++14 HPX_WITH_CXX14)
  if(HPX_WITH_CXX14)
    set(CXX_FLAG -std=c++14)
  else()
    # ... otherwise try -std=c++0y
    check_cxx_compiler_flag(-std=c++0y HPX_WITH_CXX0Y)
    if(HPX_WITH_CXX0Y)
      set(CXX_FLAG -std=c++0y)
    else()
      # ... otherwise try -std=c++11
      check_cxx_compiler_flag(-std=c++11 HPX_WITH_CXX11)
      if(HPX_WITH_CXX11)
        set(CXX_FLAG -std=c++11)
      else()
        # ... otherwise try -std=c++0x
        check_cxx_compiler_flag(-std=c++0x HPX_WITH_CXX0X)
        if(HPX_WITH_CXX0X)
          set(CXX_FLAG -std=c++0x)
        endif()
      endif()
    endif()
  endif()
  set(HPX_DEFINITIONS "${HPX_DEFINITIONS} -fexceptions ${CXX_FLAG} -include hpx/config.hpp")
else()
  set(HPX_DEFINITIONS "${HPX_DEFINITIONS} /FIhpx/config.hpp /FIwinsock2.h")
endif()

# Gather include paths libraries and definitions
set(HPX_INCLUDE_DIRS
    ${HPX_INCLUDE_DIR}
    ${HPX_INCLUDE_EXTERNAL_DIRS}
    ${Boost_INCLUDE_DIR})

set(HPX_LIBRARIES
    ${HPX_LIBRARY}
    ${HPX_INIT_LIBRARY}
    ${HPX_IOSTREAMS_LIBRARY}
    ${BOOST_DEPENDENCIES_LIBRARIES}
    dl
    rt)

else()
  set(HPX_FOUND FALSE)
endif()

