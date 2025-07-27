cmake_minimum_required(VERSION 3.16)

# Find required packages for testing
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    # Download and build GoogleTest if not found
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50f33f9142a5612210725c0e7b1bc9.zip
    )
    
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# Test sources
set(TEST_SOURCES
    test_sn_basic.cpp
    test_sn_comprehensive.cpp
    test_sn_backward_compatibility.cpp
    test_sn_utils.cpp
)

# Create test executable
add_executable(atom_sysinfo_sn_tests ${TEST_SOURCES})

# Link libraries
target_link_libraries(atom_sysinfo_sn_tests
    PRIVATE
        atom_sysinfo_sn
        gtest_main
        gtest
)

# Set test properties
set_target_properties(atom_sysinfo_sn_tests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Include directories
target_include_directories(atom_sysinfo_sn_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../
)

# Discover tests
include(GoogleTest)
gtest_discover_tests(atom_sysinfo_sn_tests)

# Add custom test target
add_custom_target(run_sn_tests
    COMMAND atom_sysinfo_sn_tests
    DEPENDS atom_sysinfo_sn_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running atom_sysinfo_sn tests"
)

# Platform-specific test configuration
if(WIN32)
    # Windows-specific test configuration
    target_compile_definitions(atom_sysinfo_sn_tests PRIVATE _WIN32_WINNT=0x0601)
else()
    # Linux-specific test configuration
    target_compile_options(atom_sysinfo_sn_tests PRIVATE -pthread)
endif()

# Add performance tests if requested
option(BUILD_PERFORMANCE_TESTS "Build performance tests" OFF)
if(BUILD_PERFORMANCE_TESTS)
    add_executable(atom_sysinfo_sn_perf_tests test_sn_performance.cpp)
    target_link_libraries(atom_sysinfo_sn_perf_tests
        PRIVATE
            atom_sysinfo_sn
            gtest_main
            gtest
    )
    set_target_properties(atom_sysinfo_sn_perf_tests PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
endif()

# Test data files
configure_file(test_data.json ${CMAKE_CURRENT_BINARY_DIR}/test_data.json COPYONLY)
