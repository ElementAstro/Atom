cmake_minimum_required(VERSION 3.23)
project(ConcurrentPugiXML VERSION 2.0.0 LANGUAGES CXX)

# Set C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific optimizations
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -O3 -march=native -mtune=native)
    add_compile_options(-ffast-math -funroll-loops -flto)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -O3 -march=native -mtune=native)
    add_compile_options(-ffast-math -funroll-loops -flto)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4 /O2 /GL /arch:AVX2)
    add_link_options(/LTCG)
endif()

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(spdlog REQUIRED)
find_package(Threads REQUIRED)

# Find pugixml
pkg_check_modules(PUGIXML REQUIRED pugixml)

# Optional: Find Google Test for testing
find_package(GTest QUIET)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${PUGIXML_INCLUDE_DIRS})

# Define the concurrent XML library
add_library(concurrent_pugixml INTERFACE)

target_include_directories(concurrent_pugixml INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(concurrent_pugixml INTERFACE
    spdlog::spdlog
    Threads::Threads
    ${PUGIXML_LIBRARIES}
)

target_compile_features(concurrent_pugixml INTERFACE cxx_std_23)

# Add compile definitions for optimization
target_compile_definitions(concurrent_pugixml INTERFACE
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG
    PUGIXML_HEADER_ONLY=1
)

# Platform-specific optimizations
if(WIN32)
    target_compile_definitions(concurrent_pugixml INTERFACE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0A00  # Windows 10
    )
elseif(UNIX)
    target_compile_definitions(concurrent_pugixml INTERFACE
        _GNU_SOURCE
        _POSIX_C_SOURCE=200809L
    )
endif()

# Example executable
add_executable(concurrent_xml_example
    examples/concurrent_example.cpp
)

target_link_libraries(concurrent_xml_example
    concurrent_pugixml
)

# Performance benchmark executable
add_executable(concurrent_xml_benchmark
    examples/performance_benchmark.cpp
)

target_link_libraries(concurrent_xml_benchmark
    concurrent_pugixml
)

# Tests (if Google Test is available)
if(GTest_FOUND)
    enable_testing()
    
    add_executable(concurrent_xml_tests
        tests/concurrent_tests.hpp
        tests/test_main.cpp
    )
    
    target_link_libraries(concurrent_xml_tests
        concurrent_pugixml
        GTest::gtest
        GTest::gtest_main
    )
    
    # Add individual test cases
    add_test(NAME ThreadSafeNodeOperations 
             COMMAND concurrent_xml_tests --gtest_filter=ConcurrentXmlTest.ThreadSafeNodeOperations)
    add_test(NAME LockFreePoolPerformance 
             COMMAND concurrent_xml_tests --gtest_filter=ConcurrentXmlTest.LockFreePoolPerformance)
    add_test(NAME ParallelProcessing 
             COMMAND concurrent_xml_tests --gtest_filter=ConcurrentXmlTest.ParallelProcessing)
    add_test(NAME QueryEnginePerformance 
             COMMAND concurrent_xml_tests --gtest_filter=ConcurrentXmlTest.QueryEnginePerformance)
    add_test(NAME ThreadSafeBuilders 
             COMMAND concurrent_xml_tests --gtest_filter=ConcurrentXmlTest.ThreadSafeBuilders)
    add_test(NAME HighConcurrencyStressTest 
             COMMAND concurrent_xml_tests --gtest_filter=ConcurrentXmlTest.HighConcurrencyStressTest)
    add_test(NAME MemoryPoolBenchmark 
             COMMAND concurrent_xml_tests --gtest_filter=ConcurrentXmlTest.MemoryPoolBenchmark)
    
    # Set test properties
    set_tests_properties(HighConcurrencyStressTest PROPERTIES TIMEOUT 300)
    set_tests_properties(MemoryPoolBenchmark PROPERTIES TIMEOUT 120)
endif()

# Documentation target (if Doxygen is available)
find_package(Doxygen QUIET)
if(Doxygen_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
endif()

# Installation
include(GNUInstallDirs)

install(TARGETS concurrent_pugixml
    EXPORT ConcurrentPugiXMLTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/concurrent_pugixml
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT ConcurrentPugiXMLTargets
    FILE ConcurrentPugiXMLTargets.cmake
    NAMESPACE ConcurrentPugiXML::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ConcurrentPugiXML
)

# Create package config file
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ConcurrentPugiXMLConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ConcurrentPugiXMLConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ConcurrentPugiXML
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ConcurrentPugiXMLConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ConcurrentPugiXMLConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ConcurrentPugiXMLConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ConcurrentPugiXML
)

# CPack configuration for packaging
set(CPACK_PACKAGE_NAME "ConcurrentPugiXML")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-performance concurrent XML library based on pugixml")
set(CPACK_PACKAGE_VENDOR "Atom Project")
set(CPACK_PACKAGE_CONTACT "atom@example.com")

if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ;DragNDrop")
else()
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
endif()

include(CPack)

# Print configuration summary
message(STATUS "=== ConcurrentPugiXML Configuration Summary ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "spdlog Found: ${spdlog_FOUND}")
message(STATUS "PugiXML Found: ${PUGIXML_FOUND}")
message(STATUS "Google Test Found: ${GTest_FOUND}")
message(STATUS "Doxygen Found: ${Doxygen_FOUND}")
message(STATUS "==============================================")

# Performance optimization hints
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Performance optimizations enabled:")
    message(STATUS "  - Native CPU optimizations: ON")
    message(STATUS "  - Link-time optimization: ON")
    message(STATUS "  - Fast math: ON")
    message(STATUS "  - Loop unrolling: ON")
endif()

# Thread safety verification
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(concurrent_pugixml INTERFACE
        -fsanitize=thread
        $<$<CONFIG:Debug>:-fsanitize=address>
        $<$<CONFIG:Debug>:-fsanitize=undefined>
    )
    target_link_options(concurrent_pugixml INTERFACE
        -fsanitize=thread
        $<$<CONFIG:Debug>:-fsanitize=address>
        $<$<CONFIG:Debug>:-fsanitize=undefined>
    )
endif()

# Add custom targets for development
add_custom_target(format
    COMMAND find ${CMAKE_CURRENT_SOURCE_DIR} -name "*.hpp" -o -name "*.cpp" | xargs clang-format -i
    COMMENT "Formatting source code"
)

add_custom_target(lint
    COMMAND find ${CMAKE_CURRENT_SOURCE_DIR} -name "*.hpp" -o -name "*.cpp" | xargs clang-tidy
    COMMENT "Running static analysis"
)

add_custom_target(benchmark
    COMMAND $<TARGET_FILE:concurrent_xml_benchmark>
    DEPENDS concurrent_xml_benchmark
    COMMENT "Running performance benchmarks"
)

# Export compile commands for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
