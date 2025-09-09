cmake_minimum_required(VERSION 3.20)

project(atom_meta.test)

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

# Find all test source files
file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)

# Create separate executables for each test file
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} gtest gtest_main gmock gmock_main atom-meta atom-error atom-utils loguru)

    # Add FFI library for FFI tests
    if(${TEST_NAME} MATCHES ".*ffi.*")
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(FFI REQUIRED libffi)
        target_link_libraries(${TEST_NAME} ${FFI_LIBRARIES})
        target_include_directories(${TEST_NAME} PRIVATE ${FFI_INCLUDE_DIRS})
        # Skip gtest discovery for FFI tests due to runtime dependency issues
        # The test can still be run manually
    else()
        gtest_discover_tests(${TEST_NAME})
    endif()
endforeach()

# Create a meta target that depends on all test executables
add_custom_target(${PROJECT_NAME} DEPENDS)
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_dependencies(${PROJECT_NAME} ${TEST_NAME})
endforeach()
