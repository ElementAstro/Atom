cmake_minimum_required(VERSION 3.21)
project(
  atom-log
  VERSION 1.0.0
  LANGUAGES C CXX)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# Define atom-log sources
set(ATOM_LOG_SOURCES async_logger.cpp atomlog.cpp log_manager.cpp logger.cpp
                     mmap_logger.cpp)

# Define atom-log headers
set(ATOM_LOG_HEADERS async_logger.hpp atomlog.hpp log_manager.hpp logger.hpp
                     mmap_logger.hpp)

# Create atom-log library
add_library(atom-log SHARED ${ATOM_LOG_SOURCES} ${ATOM_LOG_HEADERS})
add_library(atom::log ALIAS atom-log)

# Find and link spdlog (required)
find_package(spdlog REQUIRED)
message(STATUS "spdlog found - linking to atom-log")

# Compression support (required for mmap_logger compression path)
find_package(ZLIB REQUIRED)

# Find and link fmt if available (required for spdlog)
find_package(fmt QUIET)
if(fmt_FOUND)
  message(STATUS "fmt found - linking to atom-log")
endif()

# Configure module using standardized function
atom_configure_module(atom-log)

# Link module-specific dependencies Use header-only spdlog to avoid linking
# issues with MinGW
target_link_libraries(
  atom-log
  PUBLIC spdlog::spdlog atom-error atom-utils ZLIB::ZLIB
         $<$<TARGET_EXISTS:ZLIB::ZLIB>:ZLIB::ZLIB>
         $<$<TARGET_EXISTS:fmt::fmt>:fmt::fmt>)

# Configure spdlog options Using header-only spdlog for better compatibility
target_compile_definitions(atom-log PUBLIC SPDLOG_FMT_EXTERNAL)

# Install library target
install(
  TARGETS atom-log
  EXPORT atom-log-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime)

# Install export targets
install(
  EXPORT atom-log-targets
  FILE atom-logTargets.cmake
  NAMESPACE atom::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/atom
  COMPONENT development)

# Install headers
install(FILES ${ATOM_LOG_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/log)

message(STATUS "atom-log library configured with spdlog")
