# Virtual Module Tests CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

# Find required packages
find_package(GTest REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${CMAKE_SOURCE_DIR})

# Define test sources
set(VIRTUAL_TEST_SOURCES
    test_virtual_basic.cpp
)

# Create test executable
add_executable(virtual_tests ${VIRTUAL_TEST_SOURCES})

# Link libraries
target_link_libraries(virtual_tests
    PRIVATE
    atom_sysinfo_virtual
    GTest::GTest
    GTest::Main
)

# Set C++ standard
target_compile_features(virtual_tests PUBLIC cxx_std_20)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(virtual_tests
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(virtual_tests
        PRIVATE
            /W4
            /wd4100  # unreferenced formal parameter
    )
endif()

# Add test discovery
include(GoogleTest)
gtest_discover_tests(virtual_tests)

# Create custom target to run tests
add_custom_target(run_virtual_tests
    COMMAND virtual_tests
    DEPENDS virtual_tests
    COMMENT "Running virtual module tests"
)

# Performance test target (runs tests with timing)
add_custom_target(run_virtual_performance_tests
    COMMAND virtual_tests --gtest_filter="*Performance*"
    DEPENDS virtual_tests
    COMMENT "Running virtual performance tests"
)

# Memory test target (if valgrind is available)
find_program(VALGRIND_PROGRAM valgrind)
if(VALGRIND_PROGRAM)
    add_custom_target(run_virtual_memory_tests
        COMMAND ${VALGRIND_PROGRAM} --tool=memcheck --leak-check=full --show-leak-kinds=all ./virtual_tests
        DEPENDS virtual_tests
        COMMENT "Running virtual tests with memory checking"
    )
endif()

# Coverage target (if gcov is available)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX)
    find_program(GCOV_PROGRAM gcov)
    find_program(LCOV_PROGRAM lcov)
    find_program(GENHTML_PROGRAM genhtml)
    
    if(GCOV_PROGRAM AND LCOV_PROGRAM AND GENHTML_PROGRAM)
        # Add coverage flags
        target_compile_options(virtual_tests PRIVATE --coverage)
        target_link_options(virtual_tests PRIVATE --coverage)
        
        add_custom_target(virtual_coverage
            COMMAND ${LCOV_PROGRAM} --directory . --zerocounters
            COMMAND virtual_tests
            COMMAND ${LCOV_PROGRAM} --directory . --capture --output-file virtual_coverage.info
            COMMAND ${LCOV_PROGRAM} --remove virtual_coverage.info '/usr/*' --output-file virtual_coverage.info
            COMMAND ${GENHTML_PROGRAM} virtual_coverage.info --output-directory virtual_coverage_html
            DEPENDS virtual_tests
            COMMENT "Generating virtual module coverage report"
        )
    endif()
endif()

message(STATUS "Virtual module tests configured successfully")
message(STATUS "Available test targets:")
message(STATUS "  - virtual_tests: Run all virtual tests")
message(STATUS "  - run_virtual_tests: Custom target to run all tests")
message(STATUS "  - run_virtual_performance_tests: Run performance-related tests")

if(VALGRIND_PROGRAM)
    message(STATUS "  - run_virtual_memory_tests: Run tests with memory checking")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX)
    if(GCOV_PROGRAM AND LCOV_PROGRAM AND GENHTML_PROGRAM)
        message(STATUS "  - virtual_coverage: Generate coverage report")
    endif()
endif()
