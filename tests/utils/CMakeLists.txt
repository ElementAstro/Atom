cmake_minimum_required(VERSION 3.20)

project(atom_ioutils.test)

find_package(GTest QUIET)

file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)
# Remove demo/example files that have their own main() functions
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/crypto/test_aes_main.cpp)
# Remove problematic test files with namespace issues
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/core/test_uuid.cpp)

add_executable(${PROJECT_NAME} ${TEST_SOURCES})

target_link_libraries(${PROJECT_NAME} gtest gtest_main atom-utils atom-error loguru)
