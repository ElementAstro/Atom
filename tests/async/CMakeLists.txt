cmake_minimum_required(VERSION 3.20)

project(atom_async.test)

find_package(GTest QUIET)

file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)
list(REMOVE_ITEM TEST_SOURCES ${PROJECT_SOURCE_DIR}/test_async_main.cpp)

add_executable(${PROJECT_NAME} ${TEST_SOURCES})

target_link_libraries(${PROJECT_NAME} gtest gtest_main atom-async atom-error loguru)

# Link Windows socket library if on Windows
if(WIN32)
    target_link_libraries(${PROJECT_NAME} ws2_32)
endif()
