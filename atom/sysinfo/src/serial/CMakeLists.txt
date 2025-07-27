cmake_minimum_required(VERSION 3.16)
project(atom_sysinfo_sn VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(spdlog REQUIRED)

# Define the library
set(ATOM_SYSINFO_SN_SOURCES
    common.cpp
    sn.cpp
)

set(ATOM_SYSINFO_SN_HEADERS
    common.hpp
    sn.hpp
)

# Create the library
add_library(atom_sysinfo_sn ${ATOM_SYSINFO_SN_SOURCES} ${ATOM_SYSINFO_SN_HEADERS})

# Set target properties
set_target_properties(atom_sysinfo_sn PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Include directories
target_include_directories(atom_sysinfo_sn
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(atom_sysinfo_sn
    PUBLIC
        spdlog::spdlog
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(atom_sysinfo_sn
        PRIVATE
            wbemuuid
            iphlpapi
            ws2_32
    )
    target_compile_definitions(atom_sysinfo_sn PRIVATE _WIN32_WINNT=0x0601)
else()
    # Linux-specific libraries if needed
    find_package(Threads REQUIRED)
    target_link_libraries(atom_sysinfo_sn
        PRIVATE
            Threads::Threads
    )
endif()

# Compiler-specific options
if(MSVC)
    target_compile_options(atom_sysinfo_sn PRIVATE /W4)
else()
    target_compile_options(atom_sysinfo_sn PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Enable testing if this is the main project
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
    
    # Add examples
    option(BUILD_EXAMPLES "Build examples" ON)
    if(BUILD_EXAMPLES)
        add_subdirectory(examples)
    endif()
endif()

# Installation
include(GNUInstallDirs)

# Install the library
install(TARGETS atom_sysinfo_sn
    EXPORT atom_sysinfo_sn_targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers
install(FILES ${ATOM_SYSINFO_SN_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/sysinfo/sn
)

# Install export targets
install(EXPORT atom_sysinfo_sn_targets
    FILE atom_sysinfo_sn_targets.cmake
    NAMESPACE atom::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/atom_sysinfo_sn
)

# Create and install config file
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/atom_sysinfo_sn_config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_sn_config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/atom_sysinfo_sn
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_sn_config_version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_sn_config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_sn_config_version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/atom_sysinfo_sn
)

# Export targets for build tree
export(EXPORT atom_sysinfo_sn_targets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_sn_targets.cmake"
    NAMESPACE atom::
)

# Register package in user package registry
export(PACKAGE atom_sysinfo_sn)

# Print configuration summary
message(STATUS "")
message(STATUS "atom_sysinfo_sn Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Build testing: ${BUILD_TESTING}")
message(STATUS "  Build examples: ${BUILD_EXAMPLES}")
message(STATUS "")
