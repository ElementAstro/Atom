cmake_minimum_required(VERSION 3.21)
project(
  atom-search
  VERSION 1.0.0
  LANGUAGES C CXX)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# Sources
set(SOURCES
    # Core search functionality (modular components)
    core/document.cpp
    core/tokenizer.cpp
    core/scoring.cpp
    core/search_engine.cpp
    # Database backends
    database/sqlite.cpp)

# Headers
set(HEADERS
    # Backwards compatibility headers (in root)
    cache.hpp
    lru.hpp
    ttl.hpp
    mysql.hpp
    sqlite.hpp
    search.hpp
    # Core module headers (modular components)
    core/types.hpp
    core/exceptions.hpp
    core/document.hpp
    core/tokenizer.hpp
    core/scoring.hpp
    core/search_engine.hpp
    core/search.hpp
    # Database headers (modular components)
    database/types.hpp
    database/sqlite.hpp
    # Cache headers (modular components)
    cache/types.hpp
    cache/exceptions.hpp
    cache/resource_cache.hpp
    cache/lru_cache.hpp
    cache/ttl_cache.hpp
    cache/cache.hpp)

# Dependencies
find_package(spdlog CONFIG REQUIRED)

if(MSVC)
  find_package(SQLite3 QUIET)
  if(NOT SQLite3_FOUND)
    message(
      WARNING "SQLite3 not found for MSVC - database features will be disabled")
  endif()
else()
  find_package(SQLite3 REQUIRED)
endif()

# Always use compiled spdlog library to avoid ODR violations
set(LIBS spdlog::spdlog ${CMAKE_THREAD_LIBS_INIT})
if(SQLite3_FOUND)
  list(APPEND LIBS ${SQLite3_LIBRARIES})
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(MARIADB libmariadb)
endif()
if(MARIADB_FOUND)
  message(STATUS "Found libmariadb: ${MARIADB_VERSION}")
  list(APPEND SOURCES database/mysql.cpp)
  list(APPEND HEADERS database/mysql.hpp)
  list(APPEND LIBS ${MARIADB_LIBRARIES})
  include_directories(${MARIADB_INCLUDE_DIRS})
else()
  message(STATUS "libmariadb not found, mysql support will be disabled.")
endif()

# Create library target
add_library(atom-search STATIC ${SOURCES} ${HEADERS})
add_library(atom::search ALIAS atom-search)

# Configure module using standardized function
atom_configure_module(atom-search)

# Ensure spdlog is used as compiled library
target_compile_definitions(atom-search PRIVATE SPDLOG_COMPILED_LIB
                                               SPDLOG_FMT_EXTERNAL)

# Link module-specific dependencies
target_link_libraries(atom-search PRIVATE ${LIBS})

# Install headers
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/search)
