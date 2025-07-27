# Window Manager Module CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

# Project configuration
project(atom_sysinfo_wm
    VERSION 1.0.0
    DESCRIPTION "Cross-platform window manager and system information library"
    LANGUAGES CXX
)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(spdlog REQUIRED)

# Platform-specific libraries
if(WIN32)
    set(PLATFORM_LIBS dwmapi shell32 psapi)
elseif(UNIX AND NOT APPLE)
    # Linux - X11 libraries are optional
    find_package(X11)
    if(X11_FOUND)
        set(PLATFORM_LIBS ${X11_LIBRARIES})
        add_definitions(-DHAVE_X11)
    endif()
elseif(APPLE)
    # macOS - Core Graphics and other frameworks
    find_library(CORE_GRAPHICS_FRAMEWORK CoreGraphics)
    find_library(CORE_FOUNDATION_FRAMEWORK CoreFoundation)
    find_library(APPLICATION_SERVICES_FRAMEWORK ApplicationServices)
    set(PLATFORM_LIBS 
        ${CORE_GRAPHICS_FRAMEWORK}
        ${CORE_FOUNDATION_FRAMEWORK}
        ${APPLICATION_SERVICES_FRAMEWORK}
    )
endif()

# Source files
set(WM_SOURCES
    common.cpp
    wm.cpp
)

# Header files
set(WM_HEADERS
    common.hpp
    wm.hpp
)

# Create the library
add_library(atom_sysinfo_wm STATIC ${WM_SOURCES} ${WM_HEADERS})

# Include directories
target_include_directories(atom_sysinfo_wm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(atom_sysinfo_wm
    PUBLIC
        spdlog::spdlog
    PRIVATE
        ${PLATFORM_LIBS}
)

# Compiler-specific options
if(MSVC)
    target_compile_options(atom_sysinfo_wm PRIVATE /W4)
else()
    target_compile_options(atom_sysinfo_wm PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Platform-specific compile definitions
if(WIN32)
    target_compile_definitions(atom_sysinfo_wm PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

# Examples (optional)
option(BUILD_WM_EXAMPLES "Build window manager examples" OFF)
if(BUILD_WM_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests (optional)
option(BUILD_WM_TESTS "Build window manager tests" OFF)
if(BUILD_WM_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
install(TARGETS atom_sysinfo_wm
    EXPORT atom_sysinfo_wm_targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(FILES ${WM_HEADERS}
    DESTINATION include/atom/sysinfo/wm
)

install(EXPORT atom_sysinfo_wm_targets
    FILE atom_sysinfo_wm_targets.cmake
    NAMESPACE atom::
    DESTINATION lib/cmake/atom_sysinfo_wm
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    atom_sysinfo_wm_config_version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/atom_sysinfo_wm_config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_wm_config.cmake
    INSTALL_DESTINATION lib/cmake/atom_sysinfo_wm
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_wm_config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_wm_config_version.cmake
    DESTINATION lib/cmake/atom_sysinfo_wm
)

# Export targets for build tree
export(EXPORT atom_sysinfo_wm_targets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_wm_targets.cmake
    NAMESPACE atom::
)

# Package configuration
set(CPACK_PACKAGE_NAME "atom_sysinfo_wm")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
include(CPack)
