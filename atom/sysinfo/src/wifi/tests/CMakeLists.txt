# WiFi Module Tests CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

# Find required packages
find_package(GTest REQUIRED)
find_package(GMock REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${CMAKE_SOURCE_DIR})

# Define test sources
set(WIFI_TEST_SOURCES
    test_wifi_basic.cpp
    test_wifi_advanced.cpp
)

# Create test executable
add_executable(wifi_tests ${WIFI_TEST_SOURCES})

# Link libraries
target_link_libraries(wifi_tests
    PRIVATE
    atom_sysinfo_wifi
    GTest::GTest
    GTest::Main
    GMock::GMock
    GMock::Main
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(wifi_tests PRIVATE ws2_32 iphlpapi wlanapi pdh icmp)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(wifi_tests PRIVATE pthread)
elseif(APPLE)
    target_link_libraries(wifi_tests PRIVATE 
        "-framework CoreFoundation"
        "-framework SystemConfiguration"
    )
endif()

# Set C++ standard
target_compile_features(wifi_tests PRIVATE cxx_std_20)

# Compiler-specific options
if(MSVC)
    target_compile_options(wifi_tests PRIVATE /W4)
else()
    target_compile_options(wifi_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Add test discovery
include(GoogleTest)
gtest_discover_tests(wifi_tests)

# Create individual test targets for easier debugging
add_executable(wifi_basic_tests test_wifi_basic.cpp)
target_link_libraries(wifi_basic_tests
    PRIVATE
    atom_sysinfo_wifi
    GTest::GTest
    GTest::Main
)

add_executable(wifi_advanced_tests test_wifi_advanced.cpp)
target_link_libraries(wifi_advanced_tests
    PRIVATE
    atom_sysinfo_wifi
    GTest::GTest
    GTest::Main
    GMock::GMock
    GMock::Main
)

# Platform-specific libraries for individual tests
if(WIN32)
    target_link_libraries(wifi_basic_tests PRIVATE ws2_32 iphlpapi wlanapi pdh icmp)
    target_link_libraries(wifi_advanced_tests PRIVATE ws2_32 iphlpapi wlanapi pdh icmp)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(wifi_basic_tests PRIVATE pthread)
    target_link_libraries(wifi_advanced_tests PRIVATE pthread)
elseif(APPLE)
    target_link_libraries(wifi_basic_tests PRIVATE 
        "-framework CoreFoundation"
        "-framework SystemConfiguration"
    )
    target_link_libraries(wifi_advanced_tests PRIVATE 
        "-framework CoreFoundation"
        "-framework SystemConfiguration"
    )
endif()

# Set C++ standard for individual tests
target_compile_features(wifi_basic_tests PRIVATE cxx_std_20)
target_compile_features(wifi_advanced_tests PRIVATE cxx_std_20)

# Compiler-specific options for individual tests
if(MSVC)
    target_compile_options(wifi_basic_tests PRIVATE /W4)
    target_compile_options(wifi_advanced_tests PRIVATE /W4)
else()
    target_compile_options(wifi_basic_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(wifi_advanced_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Add individual test discovery
gtest_discover_tests(wifi_basic_tests)
gtest_discover_tests(wifi_advanced_tests)

# Create a custom target to run all WiFi tests
add_custom_target(run_wifi_tests
    COMMAND wifi_tests
    DEPENDS wifi_tests
    COMMENT "Running WiFi module tests"
)

# Create custom targets for individual test suites
add_custom_target(run_wifi_basic_tests
    COMMAND wifi_basic_tests
    DEPENDS wifi_basic_tests
    COMMENT "Running basic WiFi tests"
)

add_custom_target(run_wifi_advanced_tests
    COMMAND wifi_advanced_tests
    DEPENDS wifi_advanced_tests
    COMMENT "Running advanced WiFi tests"
)

# Performance test target (runs tests with timing)
add_custom_target(run_wifi_performance_tests
    COMMAND wifi_tests --gtest_filter="*Performance*"
    DEPENDS wifi_tests
    COMMENT "Running WiFi performance tests"
)

# Memory test target (if valgrind is available)
find_program(VALGRIND_PROGRAM valgrind)
if(VALGRIND_PROGRAM)
    add_custom_target(run_wifi_memory_tests
        COMMAND ${VALGRIND_PROGRAM} --tool=memcheck --leak-check=full --show-leak-kinds=all ./wifi_tests
        DEPENDS wifi_tests
        COMMENT "Running WiFi tests with memory checking"
    )
endif()

# Coverage target (if gcov is available)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX)
    find_program(GCOV_PROGRAM gcov)
    find_program(LCOV_PROGRAM lcov)
    find_program(GENHTML_PROGRAM genhtml)
    
    if(GCOV_PROGRAM AND LCOV_PROGRAM AND GENHTML_PROGRAM)
        target_compile_options(wifi_tests PRIVATE --coverage)
        target_link_options(wifi_tests PRIVATE --coverage)
        
        add_custom_target(wifi_coverage
            COMMAND ${LCOV_PROGRAM} --directory . --zerocounters
            COMMAND wifi_tests
            COMMAND ${LCOV_PROGRAM} --directory . --capture --output-file coverage.info
            COMMAND ${LCOV_PROGRAM} --remove coverage.info '/usr/*' --output-file coverage.info
            COMMAND ${LCOV_PROGRAM} --remove coverage.info '*/tests/*' --output-file coverage.info
            COMMAND ${GENHTML_PROGRAM} coverage.info --output-directory coverage_html
            DEPENDS wifi_tests
            COMMENT "Generating WiFi test coverage report"
        )
    endif()
endif()

# Install test executables (optional)
if(INSTALL_TESTS)
    install(TARGETS wifi_tests wifi_basic_tests wifi_advanced_tests
        RUNTIME DESTINATION bin/tests
    )
endif()

# Test configuration file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/test_config.ini.in
    ${CMAKE_CURRENT_BINARY_DIR}/test_config.ini
    @ONLY
)

message(STATUS "WiFi module tests configured successfully")
message(STATUS "Available test targets:")
message(STATUS "  - wifi_tests: Run all WiFi tests")
message(STATUS "  - wifi_basic_tests: Run basic WiFi tests")
message(STATUS "  - wifi_advanced_tests: Run advanced WiFi tests")
message(STATUS "  - run_wifi_tests: Custom target to run all tests")
message(STATUS "  - run_wifi_basic_tests: Custom target to run basic tests")
message(STATUS "  - run_wifi_advanced_tests: Custom target to run advanced tests")
message(STATUS "  - run_wifi_performance_tests: Run performance-related tests")

if(VALGRIND_PROGRAM)
    message(STATUS "  - run_wifi_memory_tests: Run tests with memory checking")
endif()

if(GCOV_PROGRAM AND LCOV_PROGRAM AND GENHTML_PROGRAM AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "  - wifi_coverage: Generate test coverage report")
endif()
