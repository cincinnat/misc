project(tests)

find_package(GTest REQUIRED)
include_directories(
    ${GTEST_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

add_executable(tests
    thread_pool.cpp
)
target_link_libraries(tests
    gtest
    gtest_main
    $<TARGET_OBJECTS:misc>
)

gtest_discover_tests(tests)
