cmake_minimum_required(VERSION 3.20)

project(atom_error.test)

find_package(GTest QUIET)

file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)

# Only create executable if there are source files
if(TEST_SOURCES)
    add_executable(${PROJECT_NAME} ${TEST_SOURCES})

    target_link_libraries(${PROJECT_NAME} gtest gtest_main gmock gmock_main atom-error loguru)

    # Register tests with CTest
    add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
else()
    message(STATUS "No test sources found for ${PROJECT_NAME}, skipping target creation")
endif()
