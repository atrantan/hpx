cmake_minimum_required(VERSION 2.8.4)
set(tests
    "foreach"
   )

foreach(test ${tests})
    set(sources ${test}.cpp)
    add_executable(${test}_test ${sources})
    target_link_libraries (${test}_test ${HPX_LIBRARIES} hpx_remote_promise)
endforeach()
