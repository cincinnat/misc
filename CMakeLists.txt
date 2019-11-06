cmake_minimum_required(VERSION 3.12)
project(misc)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fdiagnostics-color=always")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()
message(STATUS "Build type = '${CMAKE_BUILD_TYPE}'")

find_package(Threads)

link_libraries(
    ${CMAKE_THREAD_LIBS_INIT}
)
include_directories(
    "${CMAKE_SOURCE_DIR}"
)

add_subdirectory(src)

option(BUILD_TESTS "Build the tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
