# CMakeLists.txt for Enhanced OS Module Tests

cmake_minimum_required(VERSION 3.15)

project(atom_sysinfo_bios_os_tests
    VERSION 1.0.0
    DESCRIPTION "Tests for Enhanced Operating System Information Module"
    LANGUAGES CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# Test source files
set(TEST_SOURCES
    test_enhanced_os.cpp
)

# Create test executable
add_executable(test_enhanced_os ${TEST_SOURCES})

# Set target properties
set_target_properties(test_enhanced_os PROPERTIES
    OUTPUT_NAME "test_enhanced_os"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# Include directories
target_include_directories(test_enhanced_os PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../..
)

# Link libraries
target_link_libraries(test_enhanced_os PRIVATE
    atom_sysinfo_bios_os
    GTest::GTest
    GTest::Main
    Threads::Threads
)

# Platform-specific linking
if(WIN32)
    target_link_libraries(test_enhanced_os PRIVATE
        wbemuuid ole32 oleaut32 ws2_32 iphlpapi
        netapi32 advapi32 kernel32 user32 psapi pdh
    )
elseif(UNIX AND NOT APPLE)
    target_link_libraries(test_enhanced_os PRIVATE
        pthread dl
    )
elseif(APPLE)
    find_library(FOUNDATION_FRAMEWORK Foundation)
    find_library(IOKIT_FRAMEWORK IOKit)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    find_library(SYSTEMCONFIGURATION_FRAMEWORK SystemConfiguration)

    target_link_libraries(test_enhanced_os PRIVATE
        ${FOUNDATION_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
        ${COREFOUNDATION_FRAMEWORK}
        ${SYSTEMCONFIGURATION_FRAMEWORK}
    )
endif()

# Compiler-specific options
if(MSVC)
    target_compile_options(test_enhanced_os PRIVATE
        /W4 /WX- /permissive-
        /DNOMINMAX /DWIN32_LEAN_AND_MEAN
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_enhanced_os PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter -Wno-missing-field-initializers
    )
endif()

# Platform-specific definitions
if(WIN32)
    target_compile_definitions(test_enhanced_os PRIVATE
        _WIN32_WINNT=0x0601
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
    )
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(test_enhanced_os PRIVATE
        _GNU_SOURCE
        _DEFAULT_SOURCE
    )
elseif(APPLE)
    target_compile_definitions(test_enhanced_os PRIVATE
        _DARWIN_C_SOURCE
    )
endif()

# Enable testing
enable_testing()

# Add test cases
add_test(NAME EnhancedOSBasicTests COMMAND test_enhanced_os --gtest_filter="EnhancedOSTest.GetEnhancedOSInfo:EnhancedOSTest.GetPerformanceMetrics:EnhancedOSTest.GetSecurityInfo:EnhancedOSTest.GetNetworkConfiguration")
add_test(NAME EnhancedOSMonitoringTests COMMAND test_enhanced_os --gtest_filter="EnhancedOSTest.MonitoringStartStop:EnhancedOSTest.MonitoringCallbacks")
add_test(NAME EnhancedOSLegacyTests COMMAND test_enhanced_os --gtest_filter="EnhancedOSTest.LegacyCompatibility")
add_test(NAME EnhancedOSUtilityTests COMMAND test_enhanced_os --gtest_filter="EnhancedOSTest.OSTypeConversion:EnhancedOSTest.OSArchitectureConversion:EnhancedOSTest.OSTypeDetection:EnhancedOSTest.OSArchitectureDetection")
add_test(NAME EnhancedOSStressTests COMMAND test_enhanced_os --gtest_filter="EnhancedOSTest.RepeatedInfoRetrieval:EnhancedOSTest.ConcurrentAccess")
add_test(NAME EnhancedOSErrorHandlingTests COMMAND test_enhanced_os --gtest_filter="EnhancedOSTest.InvalidMonitoringConfig:EnhancedOSTest.DoubleMonitoringStart")

# Platform-specific tests
if(UNIX AND NOT APPLE)
    add_test(NAME EnhancedOSLinuxTests COMMAND test_enhanced_os --gtest_filter="EnhancedOSTest.LinuxSpecificFeatures")
elseif(WIN32)
    add_test(NAME EnhancedOSWindowsTests COMMAND test_enhanced_os --gtest_filter="EnhancedOSTest.WindowsSpecificFeatures")
elseif(APPLE)
    add_test(NAME EnhancedOSMacOSTests COMMAND test_enhanced_os --gtest_filter="EnhancedOSTest.MacOSSpecificFeatures")
endif()

# Set test properties
set_tests_properties(EnhancedOSBasicTests PROPERTIES
    TIMEOUT 30
    LABELS "basic;unit"
)

set_tests_properties(EnhancedOSMonitoringTests PROPERTIES
    TIMEOUT 60
    LABELS "monitoring;integration"
)

set_tests_properties(EnhancedOSLegacyTests PROPERTIES
    TIMEOUT 30
    LABELS "legacy;compatibility"
)

set_tests_properties(EnhancedOSUtilityTests PROPERTIES
    TIMEOUT 15
    LABELS "utility;unit"
)

set_tests_properties(EnhancedOSStressTests PROPERTIES
    TIMEOUT 120
    LABELS "stress;performance"
)

set_tests_properties(EnhancedOSErrorHandlingTests PROPERTIES
    TIMEOUT 30
    LABELS "error;robustness"
)

# Platform-specific test properties
if(UNIX AND NOT APPLE)
    set_tests_properties(EnhancedOSLinuxTests PROPERTIES
        TIMEOUT 45
        LABELS "linux;platform"
    )
elseif(WIN32)
    set_tests_properties(EnhancedOSWindowsTests PROPERTIES
        TIMEOUT 45
        LABELS "windows;platform"
    )
elseif(APPLE)
    set_tests_properties(EnhancedOSMacOSTests PROPERTIES
        TIMEOUT 45
        LABELS "macos;platform"
    )
endif()

# Custom test targets
add_custom_target(run_basic_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L "basic"
    DEPENDS test_enhanced_os
    COMMENT "Running basic Enhanced OS tests"
)

add_custom_target(run_monitoring_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L "monitoring"
    DEPENDS test_enhanced_os
    COMMENT "Running monitoring Enhanced OS tests"
)

add_custom_target(run_platform_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L "platform"
    DEPENDS test_enhanced_os
    COMMENT "Running platform-specific Enhanced OS tests"
)

add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_enhanced_os
    COMMENT "Running all Enhanced OS tests"
)

# Coverage target (if gcov/lcov is available)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    find_program(GCOV_EXECUTABLE gcov)
    find_program(LCOV_EXECUTABLE lcov)
    find_program(GENHTML_EXECUTABLE genhtml)

    if(GCOV_EXECUTABLE AND LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
        target_compile_options(test_enhanced_os PRIVATE --coverage)
        target_link_options(test_enhanced_os PRIVATE --coverage)

        add_custom_target(coverage
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
            COMMAND ${LCOV_EXECUTABLE} --capture --directory . --output-file coverage.info
            COMMAND ${LCOV_EXECUTABLE} --remove coverage.info '/usr/*' --output-file coverage.info
            COMMAND ${LCOV_EXECUTABLE} --list coverage.info
            COMMAND ${GENHTML_EXECUTABLE} coverage.info --output-directory coverage_html
            DEPENDS test_enhanced_os
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating test coverage report"
        )
    endif()
endif()

# Valgrind target (if valgrind is available)
find_program(VALGRIND_EXECUTABLE valgrind)
if(VALGRIND_EXECUTABLE AND UNIX)
    add_custom_target(valgrind_tests
        COMMAND ${VALGRIND_EXECUTABLE} --tool=memcheck --leak-check=full --show-leak-kinds=all
                --track-origins=yes --verbose --error-exitcode=1
                $<TARGET_FILE:test_enhanced_os>
        DEPENDS test_enhanced_os
        COMMENT "Running Enhanced OS tests with Valgrind"
    )
endif()

# Print test configuration
message(STATUS "Enhanced OS Module Tests Configuration:")
message(STATUS "  Test executable: test_enhanced_os")
message(STATUS "  GTest found: ${GTest_FOUND}")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")

if(GCOV_EXECUTABLE AND LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
    message(STATUS "  Coverage support: Available")
else()
    message(STATUS "  Coverage support: Not available")
endif()

if(VALGRIND_EXECUTABLE AND UNIX)
    message(STATUS "  Valgrind support: Available")
else()
    message(STATUS "  Valgrind support: Not available")
endif()
