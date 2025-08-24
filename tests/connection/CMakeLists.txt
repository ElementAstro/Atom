cmake_minimum_required(VERSION 3.20)

project(atom_ioconnection.test)

find_package(GTest QUIET)

file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)
# Remove build directory files and files with their own main() functions
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/build/.*")
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/udpclient.cpp)

add_executable(${PROJECT_NAME} ${TEST_SOURCES})

target_link_libraries(${PROJECT_NAME} gtest gtest_main atom-connection atom-error loguru)
