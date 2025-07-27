cmake_minimum_required(VERSION 3.20)
project(atom_sysinfo_cpu VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(spdlog REQUIRED)

# Define the library
set(CPU_SOURCES
    cpu.cpp
    common.cpp
)

set(CPU_HEADERS
    cpu.hpp
    common.hpp
)

# Platform-specific sources
if(WIN32)
    list(APPEND CPU_SOURCES platform/windows.cpp)
elseif(APPLE)
    list(APPEND CPU_SOURCES platform/macos.cpp)
elseif(UNIX)
    list(APPEND CPU_SOURCES platform/linux.cpp)
    if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
        list(APPEND CPU_SOURCES platform/freebsd.cpp)
    endif()
endif()

# Create the library
add_library(${PROJECT_NAME} STATIC ${CPU_SOURCES} ${CPU_HEADERS})

# Set target properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME ${PROJECT_NAME}
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Include directories
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        spdlog::spdlog
    PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE pdh)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE pthread)
endif()

# Compiler-specific options
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

# Installation
install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${CPU_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/sysinfo/cpu
)

# Export targets
install(EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE atom::sysinfo::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Skip config file generation if template doesn't exist
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}Config.cmake.in")
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}Config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    )
endif()

# Install config files if they exist
if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake")
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    )
endif()
