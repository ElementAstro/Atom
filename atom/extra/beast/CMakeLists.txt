# 不再设置cmake_minimum_required和project，由主CMake统一

# 源文件
set(BEAST_SOURCES
    http.cpp
    ws.cpp
    concurrency_primitives.cpp
    connection_pool.cpp
    performance_monitor.cpp
)

set(BEAST_HEADERS
    http.hpp
    http_utils.hpp
    ws.hpp
    concurrency_primitives.hpp
    connection_pool.hpp
    performance_monitor.hpp
    lock_free_queue.hpp
    memory_pool.hpp
)

add_library(beast ${BEAST_SOURCES} ${BEAST_HEADERS})
target_include_directories(beast PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Link required libraries for advanced concurrency features
target_link_libraries(beast PRIVATE pthread)

# Optional: Build example and tests
option(BUILD_BEAST_EXAMPLES "Build Beast examples" OFF)
option(BUILD_BEAST_TESTS "Build Beast tests" OFF)

if(BUILD_BEAST_EXAMPLES)
    add_executable(beast_example example_advanced_concurrency.cpp)
    target_link_libraries(beast_example PRIVATE beast spdlog::spdlog)
    target_compile_features(beast_example PRIVATE cxx_std_20)
endif()

if(BUILD_BEAST_TESTS)
    find_package(GTest REQUIRED)
    add_executable(beast_tests test_concurrency.cpp)
    target_link_libraries(beast_tests PRIVATE beast GTest::gtest GTest::gtest_main spdlog::spdlog)
    target_compile_features(beast_tests PRIVATE cxx_std_20)

    # Enable testing
    enable_testing()
    add_test(NAME BeastConcurrencyTests COMMAND beast_tests)
endif()

# Compiler-specific optimizations for high performance
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(beast PRIVATE
        -O3                    # Maximum optimization
        -march=native          # Use native CPU instructions
        -mtune=native          # Tune for native CPU
        -flto                  # Link-time optimization
        -fno-omit-frame-pointer # Better profiling
    )
endif()

# 可选: 安装规则
# install(TARGETS beast DESTINATION lib)
# install(FILES ${BEAST_HEADERS} DESTINATION include/beast)
