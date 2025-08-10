cmake_minimum_required(VERSION 3.20)

project(atom_connection.test)

find_package(GTest QUIET)

if(NOT GTEST_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.11.0
  )
  FetchContent_MakeAvailable(googletest)
  include(GoogleTest)
else()
  include(GoogleTest)
endif()

# Include all connection tests
set(TEST_SOURCES
    ${PROJECT_SOURCE_DIR}/ttybase.cpp
    ${PROJECT_SOURCE_DIR}/async_fifoclient.cpp
    ${PROJECT_SOURCE_DIR}/async_fifoserver.cpp
    ${PROJECT_SOURCE_DIR}/async_tcpclient.cpp
    ${PROJECT_SOURCE_DIR}/async_udpclient.cpp
    ${PROJECT_SOURCE_DIR}/async_udpserver.cpp
    ${PROJECT_SOURCE_DIR}/async_sockethub.cpp
    ${PROJECT_SOURCE_DIR}/async_integration_tests.cpp
    ${PROJECT_SOURCE_DIR}/async_performance_tests.cpp
)

# Only create executable if there are source files
if(TEST_SOURCES)
    add_executable(${PROJECT_NAME} ${TEST_SOURCES})

    target_link_libraries(${PROJECT_NAME} gtest gtest_main gmock gmock_main atom-connection atom-error loguru)

    # Register tests with CTest
    add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
else()
    message(STATUS "No test sources found for ${PROJECT_NAME}, skipping target creation")
endif()
