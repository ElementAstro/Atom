cmake_minimum_required(VERSION 3.20)

project(atom_iosearch.test)

file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)
# Remove standalone test files that have their own main function
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/test_optimizations_simple.cpp)

add_executable(${PROJECT_NAME} ${TEST_SOURCES})

target_link_libraries(${PROJECT_NAME} gtest gtest_main gmock gmock_main atom-search atom-error loguru)
