cmake_minimum_required(VERSION 3.23)
project(atom-asio-advanced VERSION 1.0.0 LANGUAGES CXX)

# Set C++23 standard for cutting-edge features
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Advanced compiler flags for maximum performance
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
        -Wall -Wextra -Wpedantic -Werror
        -O3 -march=native -mtune=native
        -ffast-math -funroll-loops -flto
        -fomit-frame-pointer -finline-functions
        -pthread -fcoroutines
        # Advanced optimization flags
        -fno-semantic-interposition
        -fdevirtualize-at-ltrans
        -fipa-pta -floop-nest-optimize
        -ftree-vectorize -fvect-cost-model=dynamic
    )
    add_link_options(-flto -fuse-linker-plugin)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic -Werror
        -O3 -march=native -mtune=native
        -ffast-math -funroll-loops -flto
        -fomit-frame-pointer -finline-functions
        -pthread -fcoroutines-ts
        # Clang-specific optimizations
        -fvectorize -fslp-vectorize
        -fforce-enable-int128
    )
    add_link_options(-flto)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(
        /W4 /WX /O2 /Oi /Ot /GL /arch:AVX2
        /fp:fast /Qpar /Qvec-report:2
    )
    add_link_options(/LTCG /OPT:REF /OPT:ICF)
endif()

# Enable advanced concurrency and performance features
add_compile_definitions(
    ATOM_ASIO_ENABLE_ADVANCED_CONCURRENCY=1
    ATOM_ASIO_ENABLE_LOCK_FREE=1
    ATOM_ASIO_ENABLE_PERFORMANCE_MONITORING=1
    ATOM_HAS_SPDLOG=1
    ATOM_USE_WORK_STEALING_POOL=1
    ATOM_ENABLE_NUMA_AWARENESS=1
)

# Find required dependencies
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Find ASIO (standalone or Boost)
find_path(ASIO_INCLUDE_DIR NAMES asio.hpp PATH_SUFFIXES asio)
if(ASIO_INCLUDE_DIR)
    set(ASIO_STANDALONE TRUE)
    add_compile_definitions(ASIO_STANDALONE)
    message(STATUS "Using standalone ASIO")
else()
    find_package(Boost REQUIRED COMPONENTS system)
    set(ASIO_STANDALONE FALSE)
    add_compile_definitions(USE_BOOST_ASIO)
    message(STATUS "Using Boost.ASIO")
endif()

# Find spdlog
find_package(spdlog REQUIRED)

# Find OpenSSL for SSL/TLS support
find_package(OpenSSL REQUIRED)
add_compile_definitions(USE_SSL)

# Find nlohmann_json for JSON support
find_package(nlohmann_json REQUIRED)

# Optional: Find NUMA library for NUMA awareness
find_library(NUMA_LIBRARY numa)
if(NUMA_LIBRARY)
    add_compile_definitions(ATOM_HAS_NUMA=1)
    message(STATUS "NUMA support enabled")
endif()

# Source files for the advanced ASIO library
set(ASIO_SOURCES
    # Core concurrency framework
    concurrency/lockfree_queue.hpp
    concurrency/adaptive_spinlock.hpp
    concurrency/work_stealing_pool.hpp
    concurrency/performance_monitor.hpp
    concurrency/memory_manager.hpp
    concurrency/concurrency.hpp
    concurrency/concurrency.cpp

    # Enhanced MQTT implementation
    mqtt/client.cpp
    mqtt/client.hpp
    mqtt/packet.cpp
    mqtt/packet.hpp
    mqtt/protocol.hpp
    mqtt/types.hpp

    # Enhanced SSE implementation
    sse/event.cpp
    sse/event.hpp
    sse/event_store.cpp
    sse/event_store.hpp
    sse/sse.hpp
    sse/server/auth_service.cpp
    sse/server/auth_service.hpp
    sse/server/connection.cpp
    sse/server/connection.hpp
    sse/server/event_queue.cpp
    sse/server/event_queue.hpp
    sse/server/event_store.cpp
    sse/server/event_store.hpp
    sse/server/http_request.cpp
    sse/server/http_request.hpp
    sse/server/metrics.cpp
    sse/server/metrics.hpp
    sse/server/server.cpp
    sse/server/server.hpp
    sse/server/server_config.cpp
    sse/server/server_config.hpp

    # Core compatibility layer
    asio_compatibility.hpp
)

# Create the advanced ASIO library
add_library(atom-asio-advanced STATIC ${ASIO_SOURCES})

# Set target properties
set_target_properties(atom-asio-advanced PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

# Include directories
target_include_directories(atom-asio-advanced
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(atom-asio-advanced
    PUBLIC
        Threads::Threads
        spdlog::spdlog
        OpenSSL::SSL
        OpenSSL::Crypto
        nlohmann_json::nlohmann_json
)

# Add ASIO include directories
if(ASIO_STANDALONE)
    target_include_directories(atom-asio-advanced PUBLIC ${ASIO_INCLUDE_DIR})
else()
    target_link_libraries(atom-asio-advanced PUBLIC Boost::system)
    target_include_directories(atom-asio-advanced PUBLIC ${Boost_INCLUDE_DIRS})
endif()

# Add NUMA library if available
if(NUMA_LIBRARY)
    target_link_libraries(atom-asio-advanced PRIVATE ${NUMA_LIBRARY})
endif()

# Compiler-specific optimizations
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "12.0")
    target_compile_options(atom-asio-advanced PRIVATE
        -fanalyzer
        -Wanalyzer-too-complex
    )
endif()

# Enable LTO for release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set_property(TARGET atom-asio-advanced PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Create test executable (optional)
option(ATOM_ASIO_BUILD_TESTS "Build ASIO tests" OFF)
if(ATOM_ASIO_BUILD_TESTS)
    find_package(GTest REQUIRED)

    add_executable(atom-asio-tests
        mqtt/test_client.hpp
        mqtt/test_packet.hpp
        mqtt/test_protocol.hpp
        mqtt/test_types.hpp
    )

    target_link_libraries(atom-asio-tests
        PRIVATE
            atom-asio-advanced
            GTest::gtest_main
    )

    # Enable testing
    enable_testing()
    add_test(NAME AsioTests COMMAND atom-asio-tests)
endif()

# Create benchmark executable (optional)
option(ATOM_ASIO_BUILD_BENCHMARKS "Build ASIO benchmarks" OFF)
if(ATOM_ASIO_BUILD_BENCHMARKS)
    find_package(benchmark REQUIRED)

    add_executable(atom-asio-benchmarks
        benchmarks/mqtt_benchmark.cpp
        benchmarks/sse_benchmark.cpp
        benchmarks/concurrency_benchmark.cpp
    )

    target_link_libraries(atom-asio-benchmarks
        PRIVATE
            atom-asio-advanced
            benchmark::benchmark
    )
endif()

# Installation
install(TARGETS atom-asio-advanced
    EXPORT atom-asio-advanced-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY .
    DESTINATION include/atom/extra/asio
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT atom-asio-advanced-targets
    FILE atom-asio-advanced-targets.cmake
    NAMESPACE atom::
    DESTINATION lib/cmake/atom-asio-advanced
)

# Create package config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    atom-asio-advanced-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/atom-asio-advanced-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/atom-asio-advanced-config.cmake
    INSTALL_DESTINATION lib/cmake/atom-asio-advanced
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/atom-asio-advanced-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/atom-asio-advanced-config-version.cmake
    DESTINATION lib/cmake/atom-asio-advanced
)

# Print configuration summary
message(STATUS "=== Atom ASIO Advanced Configuration ===")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "ASIO: ${ASIO_STANDALONE}")
message(STATUS "SSL Support: ${OpenSSL_FOUND}")
message(STATUS "NUMA Support: ${NUMA_LIBRARY}")
message(STATUS "Tests: ${ATOM_ASIO_BUILD_TESTS}")
message(STATUS "Benchmarks: ${ATOM_ASIO_BUILD_BENCHMARKS}")
message(STATUS "=========================================")
