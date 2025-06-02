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

# Link against required Windows libraries
target_link_libraries(shortcut_detector
    PRIVATE
        psapi
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
)

# Example executable
add_executable(shortcut_detector_example
    examples/shortcut_detector_example.cpp
)

target_link_libraries(shortcut_detector_example
    PRIVATE
        shortcut_detector
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