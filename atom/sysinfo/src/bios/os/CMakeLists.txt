# CMakeLists.txt for Enhanced OS Module in BIOS folder Enhanced Operating System
# Information Module with comprehensive features

cmake_minimum_required(VERSION 3.15)

project(
  atom_sysinfo_bios_os
  VERSION 1.0.0
  DESCRIPTION "Enhanced Operating System Information Module"
  LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enhanced OS module source files
set(ENHANCED_OS_SOURCES common.cpp os.cpp)

# Add platform-specific source files
if(WIN32)
  list(APPEND ENHANCED_OS_SOURCES windows.cpp)
  message(STATUS "Adding Windows-specific OS implementation")
elseif(UNIX AND NOT APPLE)
  list(APPEND ENHANCED_OS_SOURCES linux.cpp)
  message(STATUS "Adding Linux-specific OS implementation")
elseif(APPLE)
  list(APPEND ENHANCED_OS_SOURCES macos.cpp)
  message(STATUS "Adding macOS-specific OS implementation")
endif()

# Enhanced OS module header files
set(ENHANCED_OS_HEADERS common.hpp os.hpp)

# Add platform-specific header files
if(WIN32)
  list(APPEND ENHANCED_OS_HEADERS windows.hpp)
elseif(UNIX AND NOT APPLE)
  list(APPEND ENHANCED_OS_HEADERS linux.hpp)
elseif(APPLE)
  list(APPEND ENHANCED_OS_HEADERS macos.hpp)
endif()

# Create the enhanced OS module library
add_library(atom_sysinfo_bios_os STATIC ${ENHANCED_OS_SOURCES})

# Set target properties
set_target_properties(
  atom_sysinfo_bios_os
  PROPERTIES VERSION ${PROJECT_VERSION}
             SOVERSION 1
             OUTPUT_NAME "atom_sysinfo_bios_os"
             ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
             LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
             RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Set include directories
target_include_directories(
  atom_sysinfo_bios_os
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../..>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
         $<INSTALL_INTERFACE:include>
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Compiler-specific options
target_compile_features(atom_sysinfo_bios_os PUBLIC cxx_std_20)

# Add compiler flags for enhanced features
if(MSVC)
  target_compile_options(
    atom_sysinfo_bios_os PRIVATE /W4 /WX- /permissive- /Zc:__cplusplus
                                 /DNOMINMAX /DWIN32_LEAN_AND_MEAN)
  # Link Windows-specific libraries
  target_link_libraries(
    atom_sysinfo_bios_os
    PRIVATE wbemuuid
            ole32
            oleaut32
            ws2_32
            iphlpapi
            netapi32
            advapi32
            kernel32
            user32
            psapi
            pdh)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(
    atom_sysinfo_bios_os
    PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter
            -Wno-missing-field-initializers)

  # Linux-specific libraries
  if(UNIX AND NOT APPLE)
    target_link_libraries(atom_sysinfo_bios_os PRIVATE pthread dl)
  endif()

  # macOS-specific libraries
  if(APPLE)
    find_library(FOUNDATION_FRAMEWORK Foundation)
    find_library(IOKIT_FRAMEWORK IOKit)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    find_library(SYSTEMCONFIGURATION_FRAMEWORK SystemConfiguration)

    target_link_libraries(
      atom_sysinfo_bios_os
      PRIVATE ${FOUNDATION_FRAMEWORK} ${IOKIT_FRAMEWORK}
              ${COREFOUNDATION_FRAMEWORK} ${SYSTEMCONFIGURATION_FRAMEWORK})
  endif()
endif()

# Find and link required dependencies
find_package(spdlog REQUIRED)
target_link_libraries(atom_sysinfo_bios_os PUBLIC spdlog::spdlog)

# Optional: Find and link additional dependencies
find_package(Threads REQUIRED)
target_link_libraries(atom_sysinfo_bios_os PRIVATE Threads::Threads)

# Platform-specific definitions
if(WIN32)
  target_compile_definitions(
    atom_sysinfo_bios_os PRIVATE _WIN32_WINNT=0x0601 # Windows 7 minimum
                                 NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
elseif(UNIX AND NOT APPLE)
  target_compile_definitions(atom_sysinfo_bios_os PRIVATE _GNU_SOURCE
                                                          _DEFAULT_SOURCE)
elseif(APPLE)
  target_compile_definitions(atom_sysinfo_bios_os PRIVATE _DARWIN_C_SOURCE)
endif()

# Add custom target for generating documentation
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
  set(DOXYGEN_PROJECT_NAME "Enhanced OS Module")
  set(DOXYGEN_PROJECT_BRIEF "Enhanced Operating System Information Module")
  set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs")
  set(DOXYGEN_EXTRACT_ALL YES)
  set(DOXYGEN_GENERATE_HTML YES)
  set(DOXYGEN_GENERATE_XML YES)

  doxygen_add_docs(atom_sysinfo_bios_os_docs ${CMAKE_CURRENT_SOURCE_DIR}
                   COMMENT "Generating Enhanced OS Module documentation")
endif()

# Installation rules
install(
  TARGETS atom_sysinfo_bios_os
  EXPORT atom_sysinfo_bios_os_targets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES
  DESTINATION include)

install(FILES ${ENHANCED_OS_HEADERS} DESTINATION include/atom/sysinfo/bios/os)

# Export configuration
install(
  EXPORT atom_sysinfo_bios_os_targets
  FILE atom_sysinfo_bios_os_targets.cmake
  NAMESPACE atom::
  DESTINATION lib/cmake/atom_sysinfo_bios_os)

# Create config file
include(CMakePackageConfigHelpers)
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/atom_sysinfo_bios_os_config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_bios_os_config.cmake"
  INSTALL_DESTINATION lib/cmake/atom_sysinfo_bios_os)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_bios_os_config_version.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_bios_os_config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_bios_os_config_version.cmake"
  DESTINATION lib/cmake/atom_sysinfo_bios_os)

# Add subdirectories for examples and tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples")
  add_subdirectory(examples)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
  add_subdirectory(tests)
endif()

# Print configuration summary
message(STATUS "Enhanced OS Module Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  Sources: ${ENHANCED_OS_SOURCES}")
message(STATUS "  Headers: ${ENHANCED_OS_HEADERS}")

# Add custom target for code formatting (if clang-format is available)
find_program(CLANG_FORMAT_EXECUTABLE clang-format)
if(CLANG_FORMAT_EXECUTABLE)
  add_custom_target(
    format_enhanced_os
    COMMAND ${CLANG_FORMAT_EXECUTABLE} -i ${ENHANCED_OS_SOURCES}
            ${ENHANCED_OS_HEADERS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Formatting Enhanced OS Module source code")
endif()

# Add custom target for static analysis (if cppcheck is available)
find_program(CPPCHECK_EXECUTABLE cppcheck)
if(CPPCHECK_EXECUTABLE)
  add_custom_target(
    cppcheck_enhanced_os
    COMMAND ${CPPCHECK_EXECUTABLE} --enable=all --std=c++20 --verbose --quiet
            --error-exitcode=1 ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running static analysis on Enhanced OS Module")
endif()
