cmake_minimum_required(VERSION 3.20)
project(modern_log VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

add_library(modern_log
    core/context.cpp
    filters/filter.cpp
    filters/builtin_filters.cpp
    sampling/sampler.cpp
    events/event_system.cpp
    utils/structured_data.cpp
    utils/timer.cpp
    utils/archiver.cpp
    logger/logger.cpp
    logger/manager.cpp
)

target_include_directories(modern_log
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(modern_log
    PUBLIC
        spdlog::spdlog
        fmt::fmt
)

target_compile_features(modern_log
    PUBLIC
        cxx_std_23
)

add_executable(modern_log_example examples/basic_usage.cpp)
target_link_libraries(modern_log_example modern_log)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)

install(TARGETS modern_log
    EXPORT modern_log_targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT modern_log_targets
    FILE modern_log_targets.cmake
    NAMESPACE modern_log::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/modern_log
)