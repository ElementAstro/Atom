# Test suite for Enhanced Component System
cmake_minimum_required(VERSION 3.16)

project(atom_component_tests)

# Find required packages
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# Include parent directory for component headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# Test executable
add_executable(${PROJECT_NAME}
    test_component_system.cpp
    test_component.cpp
    test_registry.cpp
    test_component_pool.cpp
    test_lifecycle.cpp
    test_serialization.cpp
    test_scripting_api.cpp
    test_variable_manager.cpp
    test_dispatch.cpp
    test_integration.cpp
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    atom_component
    GTest::GTest
    GTest::Main
    Threads::Threads
)

# Compiler features
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

# Compiler flags
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Pass macro definitions from parent
if(ATOM_ENABLE_LUA)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ATOM_ENABLE_LUA=1)
endif()

if(ATOM_ENABLE_PYTHON)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ATOM_ENABLE_PYTHON=1)
endif()

# Enable testing
enable_testing()

# Add test cases
add_test(NAME CoreComponentTests COMMAND ${PROJECT_NAME} --gtest_filter="ComponentTest.*:ComponentErrorTest.*:ComponentPerformanceTest.*")
add_test(NAME RegistryTests COMMAND ${PROJECT_NAME} --gtest_filter="RegistryTest.*:RegistryErrorTest.*:RegistryPerformanceTest.*")
add_test(NAME ComponentPoolTests COMMAND ${PROJECT_NAME} --gtest_filter="ComponentPoolTest.*:SIMDContainerTest.*:ComponentPoolPerformanceTest.*")
add_test(NAME LifecycleTests COMMAND ${PROJECT_NAME} --gtest_filter="LifecycleTest.*:DependencyTest.*")
add_test(NAME SerializationTests COMMAND ${PROJECT_NAME} --gtest_filter="SerializationTest.*:SerializationPerformanceTest.*")
add_test(NAME ScriptingTests COMMAND ${PROJECT_NAME} --gtest_filter="ScriptingAPITest.*:ScriptEngineTest.*:ScriptingPerformanceTest.*")
add_test(NAME VariableManagerTests COMMAND ${PROJECT_NAME} --gtest_filter="VariableManagerTest.*:VariableManagerErrorTest.*")
add_test(NAME DispatchTests COMMAND ${PROJECT_NAME} --gtest_filter="DispatchTest.*:DispatchPerformanceTest.*")
add_test(NAME IntegrationTests COMMAND ${PROJECT_NAME} --gtest_filter="IntegrationTest.*:PerformanceTest.*")
add_test(NAME LegacyTests COMMAND ${PROJECT_NAME} --gtest_filter="ComponentSystemTest.*:MemoryPoolTest.*:ScriptingTest.*:PerformanceBenchmark.*")

# Custom test targets
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running all component system tests"
)

add_custom_target(run_benchmarks
    COMMAND ${PROJECT_NAME} --gtest_filter="*PerformanceTest.*:*PerformanceBenchmark.*"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running performance benchmarks"
)

add_custom_target(run_scripting_tests
    COMMAND ${PROJECT_NAME} --gtest_filter="ScriptingAPITest.*:ScriptEngineTest.*:ScriptingPerformanceTest.*"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running scripting engine tests"
)

add_custom_target(run_core_tests
    COMMAND ${PROJECT_NAME} --gtest_filter="ComponentTest.*:RegistryTest.*:LifecycleTest.*"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running core component tests"
)

add_custom_target(run_integration_tests
    COMMAND ${PROJECT_NAME} --gtest_filter="IntegrationTest.*"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running integration tests"
)

add_custom_target(run_error_tests
    COMMAND ${PROJECT_NAME} --gtest_filter="*ErrorTest.*"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Running error handling tests"
)

# Memory leak detection (if available)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(UNIX AND NOT APPLE)
        add_custom_target(run_tests_valgrind
            COMMAND valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all
                    --track-origins=yes --verbose --log-file=valgrind-out.txt
                    $<TARGET_FILE:${PROJECT_NAME}>
            DEPENDS ${PROJECT_NAME}
            COMMENT "Running tests with Valgrind memory checking"
        )
    endif()
endif()

# Code coverage (if available)
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(${PROJECT_NAME} PRIVATE --coverage)
    target_link_options(${PROJECT_NAME} PRIVATE --coverage)

    add_custom_target(coverage
        COMMAND lcov --directory . --capture --output-file coverage.info
        COMMAND lcov --remove coverage.info '/usr/*' --output-file coverage.info
        COMMAND lcov --list coverage.info
        COMMAND genhtml -o coverage_html coverage.info
        DEPENDS run_tests
        COMMENT "Generating code coverage report"
    )
endif()

# Test data files
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/test_data/sample_component.json
    ${CMAKE_CURRENT_BINARY_DIR}/test_data/sample_component.json
    COPYONLY
)

# Install test executable (optional)
if(INSTALL_TESTS)
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin/tests
    )
endif()
