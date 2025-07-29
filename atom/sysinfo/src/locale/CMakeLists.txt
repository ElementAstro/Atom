# CMakeLists.txt for Locale Module
cmake_minimum_required(VERSION 3.16)

project(atom_sysinfo_locale
    VERSION 1.0.0
    DESCRIPTION "Advanced cross-platform locale management system"
    LANGUAGES CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(LOCALE_BUILD_EXAMPLES "Build locale examples" ON)
option(LOCALE_BUILD_TESTS "Build locale tests" ON)
option(LOCALE_ENABLE_VALIDATION "Enable locale validation features" ON)
option(LOCALE_ENABLE_CACHING "Enable caching features" ON)
option(LOCALE_ENABLE_PERFORMANCE_MONITORING "Enable performance monitoring" ON)

# Find required packages
find_package(spdlog REQUIRED)

# Platform-specific dependencies
if(WIN32)
    # Windows-specific libraries are linked automatically
elseif(APPLE)
    find_library(CORE_FOUNDATION CoreFoundation REQUIRED)
    find_library(FOUNDATION Foundation REQUIRED)
else()
    # Linux/Unix - no additional libraries required for basic functionality
endif()

# Define the locale library
set(LOCALE_SOURCES
    common.cpp
    locale.cpp
    config.cpp
)

# Add platform-specific sources
if(WIN32)
    list(APPEND LOCALE_SOURCES platform/windows.cpp)
elseif(UNIX AND NOT APPLE)
    list(APPEND LOCALE_SOURCES platform/linux.cpp)
elseif(APPLE)
    list(APPEND LOCALE_SOURCES platform/macos.cpp)
endif()

# Add validation sources if enabled
if(LOCALE_ENABLE_VALIDATION)
    list(APPEND LOCALE_SOURCES validator.cpp)
endif()

# Create the locale library
add_library(atom_sysinfo_locale ${LOCALE_SOURCES})

# Set target properties
set_target_properties(atom_sysinfo_locale PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

# Include directories
target_include_directories(atom_sysinfo_locale
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(atom_sysinfo_locale
    PUBLIC
        spdlog::spdlog
)

# Platform-specific linking
if(APPLE)
    target_link_libraries(atom_sysinfo_locale
        PRIVATE
            ${CORE_FOUNDATION}
            ${FOUNDATION}
    )
endif()

# Compiler-specific options
target_compile_options(atom_sysinfo_locale
    PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Preprocessor definitions
target_compile_definitions(atom_sysinfo_locale
    PRIVATE
        $<$<BOOL:${LOCALE_ENABLE_VALIDATION}>:LOCALE_ENABLE_VALIDATION>
        $<$<BOOL:${LOCALE_ENABLE_CACHING}>:LOCALE_ENABLE_CACHING>
        $<$<BOOL:${LOCALE_ENABLE_PERFORMANCE_MONITORING}>:LOCALE_ENABLE_PERFORMANCE_MONITORING>
        $<$<CONFIG:Debug>:ATOM_ENABLE_DEBUG>
)

# Examples
if(LOCALE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(LOCALE_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
include(GNUInstallDirs)

# Install the library
install(TARGETS atom_sysinfo_locale
    EXPORT atom_sysinfo_locale_targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers
install(FILES
    common.hpp
    locale.hpp
    config.hpp
    validator.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/sysinfo/locale
)

# Platform-specific headers
if(WIN32)
    install(FILES windows.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/sysinfo/locale
    )
elseif(APPLE)
    install(FILES macos.hpp linux.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/sysinfo/locale
    )
else()
    install(FILES linux.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/sysinfo/locale
    )
endif()

# Install documentation
install(FILES README.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

# Export targets
install(EXPORT atom_sysinfo_locale_targets
    FILE atom_sysinfo_locale_targets.cmake
    NAMESPACE atom::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/atom_sysinfo_locale
)

# Create config file
include(CMakePackageConfigHelpers)

# Skip config file generation if template doesn't exist
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/atom_sysinfo_locale_config.cmake.in")
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/atom_sysinfo_locale_config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_locale_config.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/atom_sysinfo_locale
    )
endif()

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_locale_config_version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_locale_config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_locale_config_version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/atom_sysinfo_locale
)

# Create pkg-config file if template exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/atom_sysinfo_locale.pc.in")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/atom_sysinfo_locale.pc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_locale.pc"
        @ONLY
    )
endif()

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_locale.pc"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# Print configuration summary
message(STATUS "")
message(STATUS "Locale Module Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build examples: ${LOCALE_BUILD_EXAMPLES}")
message(STATUS "  Build tests: ${LOCALE_BUILD_TESTS}")
message(STATUS "  Enable validation: ${LOCALE_ENABLE_VALIDATION}")
message(STATUS "  Enable caching: ${LOCALE_ENABLE_CACHING}")
message(STATUS "  Enable performance monitoring: ${LOCALE_ENABLE_PERFORMANCE_MONITORING}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
