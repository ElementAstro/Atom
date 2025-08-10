cmake_minimum_required(VERSION 3.20)

project(atom_secret.test)

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

file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)
# Exclude CMake compiler ID files
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*CMakeCXXCompilerId\\.cpp$")

# Only create executable if there are source files
if(TEST_SOURCES)
    add_executable(${PROJECT_NAME} ${TEST_SOURCES})

    find_package(OpenSSL REQUIRED)
    target_link_libraries(${PROJECT_NAME} gtest gtest_main gmock gmock_main atom-secret atom-error loguru OpenSSL::SSL OpenSSL::Crypto)

    # Register tests with CTest
    add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
else()
    message(STATUS "No test sources found for ${PROJECT_NAME}, skipping target creation")
endif()
