cmake_minimum_required(VERSION 3.15)
project(ShortcutDetector VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Find and link spdlog
find_package(spdlog QUIET)
if(spdlog_FOUND)
    message(STATUS "spdlog found - enhanced logging enabled in shortcut detector")
else()
    message(STATUS "spdlog not found - using standard logging in shortcut detector")
endif()

# Create shared library
add_library(shortcut_detector SHARED
    detector.cpp
    detector_impl.cpp
    shortcut.cpp
    factory.cpp
    win32_utils.cpp
)

# Set include directories
target_include_directories(shortcut_detector
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link against required libraries
set(SHORTCUT_LIBS psapi)
if(spdlog_FOUND)
    list(APPEND SHORTCUT_LIBS spdlog::spdlog)
endif()

target_link_libraries(shortcut_detector
    PRIVATE
        ${SHORTCUT_LIBS}
)

# Create static library version
add_library(shortcut_detector_static STATIC
    detector.cpp
    detector_impl.cpp
    shortcut.cpp
    factory.cpp
    win32_utils.cpp
)

target_include_directories(shortcut_detector_static
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(shortcut_detector_static
    PRIVATE
        ${SHORTCUT_LIBS}
)

# Create test executable
add_executable(test_shortcut_detector test_shortcut_detector.cpp)

# Link the test against the static library
set(TEST_SHORTCUT_LIBS shortcut_detector_static psapi user32)
if(spdlog_FOUND)
    list(APPEND TEST_SHORTCUT_LIBS spdlog::spdlog)
endif()

target_link_libraries(test_shortcut_detector
    PRIVATE
        ${TEST_SHORTCUT_LIBS}
)

# Set include directories for test
target_include_directories(test_shortcut_detector
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Installation rules
install(
    TARGETS shortcut_detector shortcut_detector_static
    EXPORT shortcut_detector-config
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(
    FILES
        detector.hpp
        shortcut.h
        factory.h
        status.h
    DESTINATION include/shortcut_detector
)

install(
    EXPORT shortcut_detector-config
    NAMESPACE shortcut_detector::
    DESTINATION lib/cmake/shortcut_detector
)
