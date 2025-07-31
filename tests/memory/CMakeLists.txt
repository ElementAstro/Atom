cmake_minimum_required(VERSION 3.20)

project(atom_memory.test)

find_package(GTest QUIET)

file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)
# Remove the comprehensive test runner to avoid multiple main functions
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/run_comprehensive_tests.cpp)

add_executable(${PROJECT_NAME} ${TEST_SOURCES})

target_link_libraries(${PROJECT_NAME} gtest gtest_main gmock gmock_main atom-error loguru)

# Register tests with CTest
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
