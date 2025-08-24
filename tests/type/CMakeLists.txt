cmake_minimum_required(VERSION 3.20)

project(atom_iotype.test)

find_package(GTest QUIET)

file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)
# Remove demo/example files that have their own main() functions
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/test_args_main.cpp)

add_executable(${PROJECT_NAME} ${TEST_SOURCES})

target_link_libraries(${PROJECT_NAME} gtest gtest_main gmock gmock_main atom-error atom-type loguru)
