cmake_minimum_required(VERSION 3.21)
project(atom-test-examples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(nlohmann_json QUIET)
find_package(spdlog QUIET)

# Include directories
include_directories(${CMAKE_SOURCE_DIR})

# ============================================================================
# Core Example Sources
# ============================================================================
set(CORE_SOURCES
    core/test_assertions.cpp
)

# ============================================================================
# Fixtures Example Sources
# ============================================================================
set(FIXTURES_SOURCES
    fixtures/test_fixtures.cpp
    fixtures/test_parameterized.cpp
)

# ============================================================================
# Mocking Example Sources
# ============================================================================
set(MOCKING_SOURCES
    mocking/test_mock.cpp
)

# ============================================================================
# Utilities Example Sources
# ============================================================================
set(UTILITIES_SOURCES
    utilities/test_skip_timeout.cpp
    utilities/test_data_factory.cpp
)

# ============================================================================
# Reporters Example Sources
# ============================================================================
set(REPORTERS_SOURCES
    reporters/test_reporters.cpp
)

# ============================================================================
# Performance Example Sources
# ============================================================================
set(PERFORMANCE_SOURCES
    performance/test_benchmark.cpp
)

# ============================================================================
# All Example Sources
# ============================================================================
set(ALL_EXAMPLE_SOURCES
    ${CORE_SOURCES}
    ${FIXTURES_SOURCES}
    ${MOCKING_SOURCES}
    ${UTILITIES_SOURCES}
    ${REPORTERS_SOURCES}
    ${PERFORMANCE_SOURCES}
)

# Helper function to create example executable
function(add_example_executable TARGET_NAME SOURCES)
    add_executable(${TARGET_NAME} ${SOURCES})
    target_link_libraries(${TARGET_NAME} PRIVATE atom-tests)
    if(nlohmann_json_FOUND)
        target_link_libraries(${TARGET_NAME} PRIVATE nlohmann_json::nlohmann_json)
    endif()
    if(spdlog_FOUND)
        target_link_libraries(${TARGET_NAME} PRIVATE spdlog::spdlog)
    endif()
    add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
endfunction()

# ============================================================================
# Create Category-Specific Example Executables
# ============================================================================

# Core examples
add_example_executable(example_core "${CORE_SOURCES}")
set_tests_properties(example_core PROPERTIES LABELS "examples;core")

# Fixtures examples
add_example_executable(example_fixtures "${FIXTURES_SOURCES}")
set_tests_properties(example_fixtures PROPERTIES LABELS "examples;fixtures")

# Mocking examples
add_example_executable(example_mocking "${MOCKING_SOURCES}")
set_tests_properties(example_mocking PROPERTIES LABELS "examples;mocking")

# Utilities examples
add_example_executable(example_utilities "${UTILITIES_SOURCES}")
set_tests_properties(example_utilities PROPERTIES LABELS "examples;utilities")

# Reporters examples
add_example_executable(example_reporters "${REPORTERS_SOURCES}")
set_tests_properties(example_reporters PROPERTIES LABELS "examples;reporters")

# Performance examples
add_example_executable(example_performance "${PERFORMANCE_SOURCES}")
set_tests_properties(example_performance PROPERTIES LABELS "examples;performance" TIMEOUT 300)

# ============================================================================
# Combined Test Executable (all examples)
# ============================================================================
add_executable(test_all test_main.cpp)
target_link_libraries(test_all PRIVATE atom-tests)
if(nlohmann_json_FOUND)
    target_link_libraries(test_all PRIVATE nlohmann_json::nlohmann_json)
endif()
if(spdlog_FOUND)
    target_link_libraries(test_all PRIVATE spdlog::spdlog)
endif()
add_test(NAME test_all COMMAND test_all)
set_tests_properties(test_all PROPERTIES LABELS "examples;all")

# Enable testing
enable_testing()
