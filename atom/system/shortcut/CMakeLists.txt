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
find_package(spdlog REQUIRED)

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
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link against required libraries
target_link_libraries(shortcut_detector
    PRIVATE
        psapi
        spdlog::spdlog
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
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(shortcut_detector_static
    PRIVATE
        psapi
        spdlog::spdlog
)

# Example executable
add_executable(shortcut_detector_example
    examples/shortcut_detector_example.cpp
)

target_link_libraries(shortcut_detector_example
    PRIVATE
        shortcut_detector
)

# Create test executable
add_executable(test_shortcut_detector test_shortcut_detector.cpp)

# Link the test against the static library
target_link_libraries(test_shortcut_detector
    PRIVATE
        shortcut_detector_static
        psapi
        user32
        spdlog::spdlog
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
        include/detector.h
        include/shortcut.h
        include/factory.h
        include/tatus.h
    DESTINATION include/shortcut_detector
)

install(
    EXPORT shortcut_detector-config
    NAMESPACE shortcut_detector::
    DESTINATION lib/cmake/shortcut_detector
)
