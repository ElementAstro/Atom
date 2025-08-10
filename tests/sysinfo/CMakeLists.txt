cmake_minimum_required(VERSION 3.20)

project(atom_sysinfo.test)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# Find additional dependencies that might be needed
find_package(Threads REQUIRED)

file(GLOB_RECURSE TEST_SOURCES ${PROJECT_SOURCE_DIR}/*.cpp)

# Only create executable if there are source files
if(TEST_SOURCES)
    add_executable(${PROJECT_NAME} ${TEST_SOURCES})

    # Set C++ standard for the target
    target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

    # Link libraries including thread support
    target_link_libraries(${PROJECT_NAME}
        gtest
        gtest_main
        gmock
        gmock_main
        atom-sysinfo
        atom-error
        loguru
        Threads::Threads
    )

    # Compiler-specific options
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${PROJECT_NAME}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wno-unused-parameter
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${PROJECT_NAME}
            PRIVATE
                /W4
                /wd4100  # unreferenced formal parameter
        )
    endif()

    # Register tests with CTest and enable test discovery
    include(GoogleTest)
    gtest_discover_tests(${PROJECT_NAME})

    # Also add the traditional test registration for compatibility
    add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

    # Add individual test targets for easier debugging
    set(INDIVIDUAL_TESTS
        battery
        bios
        cpu
        disk
        gpu
        locale
        memory
        os
        sn
        sysinfo_printer
        virtual
        wifi
        wm
        test_enhanced_features
    )

    foreach(test_name ${INDIVIDUAL_TESTS})
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${test_name}.cpp)
            add_executable(${test_name}_test ${test_name}.cpp)
            target_compile_features(${test_name}_test PRIVATE cxx_std_20)
            target_link_libraries(${test_name}_test
                gtest
                gtest_main
                gmock
                gmock_main
                atom-sysinfo
                atom-error
                loguru
                Threads::Threads
            )

            # Apply same compiler options
            if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
                target_compile_options(${test_name}_test
                    PRIVATE
                        -Wall
                        -Wextra
                        -Wpedantic
                        -Wno-unused-parameter
                )
            elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
                target_compile_options(${test_name}_test
                    PRIVATE
                        /W4
                        /wd4100
                )
            endif()

            # Register individual test
            gtest_discover_tests(${test_name}_test)
            add_test(NAME ${test_name}_individual COMMAND ${test_name}_test)
        endif()
    endforeach()

    # Create a custom target to run all tests
    add_custom_target(run_all_sysinfo_tests
        COMMAND ${PROJECT_NAME}
        DEPENDS ${PROJECT_NAME}
        COMMENT "Running all sysinfo tests"
    )

    # Create custom targets for test categories
    add_custom_target(run_basic_tests
        COMMAND ${PROJECT_NAME} --gtest_filter="*Basic*:*GetCpu*:*GetMemory*:*GetDisk*"
        DEPENDS ${PROJECT_NAME}
        COMMENT "Running basic sysinfo tests"
    )

    add_custom_target(run_advanced_tests
        COMMAND ${PROJECT_NAME} --gtest_filter="*Advanced*:*Enhanced*:*Performance*"
        DEPENDS ${PROJECT_NAME}
        COMMENT "Running advanced sysinfo tests"
    )

    add_custom_target(run_stress_tests
        COMMAND ${PROJECT_NAME} --gtest_filter="*Concurrent*:*Stress*:*Performance*"
        DEPENDS ${PROJECT_NAME}
        COMMENT "Running stress tests"
    )

else()
    message(STATUS "No test sources found for ${PROJECT_NAME}, skipping target creation")
endif()
