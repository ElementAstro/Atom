# BIOS Information Module CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

project(atom_sysinfo_bios VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(spdlog REQUIRED)

# Define source files
set(BIOS_SOURCES
    common.cpp
    bios.cpp
)

# Define header files
set(BIOS_HEADERS
    common.hpp
    bios.hpp
)

# Add platform-specific sources and headers
if(WIN32)
    list(APPEND BIOS_SOURCES platform/windows.cpp)
    list(APPEND BIOS_HEADERS platform/windows.hpp)
elseif(UNIX AND NOT APPLE)
    list(APPEND BIOS_SOURCES platform/linux.cpp)
    list(APPEND BIOS_HEADERS platform/linux.hpp)
elseif(APPLE)
    list(APPEND BIOS_SOURCES platform/macos.cpp)
    list(APPEND BIOS_HEADERS platform/macos.hpp)
endif()

# Create the library
add_library(atom_sysinfo_bios STATIC ${BIOS_SOURCES} ${BIOS_HEADERS})

# Set target properties
set_target_properties(atom_sysinfo_bios PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Include directories
target_include_directories(atom_sysinfo_bios
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(atom_sysinfo_bios
    PUBLIC
        spdlog::spdlog
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(atom_sysinfo_bios
        PRIVATE
            wbemuuid
            ole32
            oleaut32
    )
elseif(UNIX AND NOT APPLE)
    # Linux-specific libraries if needed
    target_link_libraries(atom_sysinfo_bios
        PRIVATE
            # Add any Linux-specific libraries here
    )
elseif(APPLE)
    # macOS-specific frameworks
    target_link_libraries(atom_sysinfo_bios
        PRIVATE
            "-framework CoreFoundation"
            "-framework IOKit"
    )
endif()

# Compiler-specific options
if(MSVC)
    target_compile_options(atom_sysinfo_bios PRIVATE /W4)
else()
    target_compile_options(atom_sysinfo_bios PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Define preprocessor macros
target_compile_definitions(atom_sysinfo_bios
    PRIVATE
        $<$<CONFIG:Debug>:DEBUG>
        $<$<CONFIG:Release>:NDEBUG>
)

# Installation rules
install(TARGETS atom_sysinfo_bios
    EXPORT atom_sysinfo_bios_targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(FILES ${BIOS_HEADERS}
    DESTINATION include/atom/sysinfo/bios
)

# Export targets
install(EXPORT atom_sysinfo_bios_targets
    FILE atom_sysinfo_bios_targets.cmake
    NAMESPACE atom::
    DESTINATION lib/cmake/atom_sysinfo_bios
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    atom_sysinfo_bios_config_version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/atom_sysinfo_bios_config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_bios_config.cmake
    INSTALL_DESTINATION lib/cmake/atom_sysinfo_bios
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_bios_config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_bios_config_version.cmake
    DESTINATION lib/cmake/atom_sysinfo_bios
)

# Optional: Add tests
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# Optional: Add examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Default to building examples for demonstration
if(NOT DEFINED BUILD_EXAMPLES)
    set(BUILD_EXAMPLES ON)
    add_subdirectory(examples)
endif()
