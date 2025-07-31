cmake_minimum_required(VERSION 3.20)

project(atom_ioconnection.test)

find_package(GTest QUIET)

# Only include TTY tests for now since other connection classes have missing implementations
set(TEST_SOURCES ${PROJECT_SOURCE_DIR}/ttybase.cpp)

add_executable(${PROJECT_NAME} ${TEST_SOURCES})

target_link_libraries(${PROJECT_NAME} gtest gtest_main gmock gmock_main atom-connection atom-error loguru)
