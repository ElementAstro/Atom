cmake_minimum_required(VERSION 3.20)

project(atom_extra.test)

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

# Collect all test sources from subdirectories
file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)

# Only create executable if there are source files
if(TEST_SOURCES)
    add_executable(${PROJECT_NAME} ${TEST_SOURCES})

    # Link with common libraries and external dependencies
    target_link_libraries(${PROJECT_NAME} 
        gtest gtest_main gmock gmock_main 
        atom-error loguru
    )

    # Add conditional linking for external libraries if available
    find_package(Boost QUIET)
    if(Boost_FOUND)
        target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
        target_include_directories(${PROJECT_NAME} PRIVATE ${Boost_INCLUDE_DIRS})
    endif()

    find_package(CURL QUIET)
    if(CURL_FOUND)
        target_link_libraries(${PROJECT_NAME} ${CURL_LIBRARIES})
        target_include_directories(${PROJECT_NAME} PRIVATE ${CURL_INCLUDE_DIRS})
    endif()

    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBUV libuv)
        if(LIBUV_FOUND)
            target_link_libraries(${PROJECT_NAME} ${LIBUV_LIBRARIES})
            target_include_directories(${PROJECT_NAME} PRIVATE ${LIBUV_INCLUDE_DIRS})
        endif()
    endif()

    # Register tests with CTest
    add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
else()
    message(STATUS "No test sources found for ${PROJECT_NAME}, skipping target creation")
endif()
