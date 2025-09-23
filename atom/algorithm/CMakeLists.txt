cmake_minimum_required(VERSION 3.20)
project(
  atom-algorithm
  VERSION 1.0.0
  LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenSSL package
find_package(OpenSSL REQUIRED)

# Find TBB package (optional for now due to vcpkg network issues)
find_package(TBB QUIET)

# Get dependencies from module_dependencies.cmake
if(NOT DEFINED ATOM_ALGORITHM_DEPENDS)
  set(ATOM_ALGORITHM_DEPENDS atom-error)
endif()

# Verify if dependency modules are built
foreach(dep ${ATOM_ALGORITHM_DEPENDS})
  string(REPLACE "atom-" "ATOM_BUILD_" dep_var_name ${dep})
  string(TOUPPER ${dep_var_name} dep_var_name)
  if(NOT DEFINED ${dep_var_name} OR NOT ${dep_var_name})
    message(
      WARNING
        "Module ${PROJECT_NAME} depends on ${dep}, but that module is not enabled for building"
    )
    # Auto dependency building can be added here if needed
  endif()
endforeach()

# Sources and Headers
set(SOURCES
    # Core files
    core/algorithm.cpp
    core/opencl_utils.cpp

    # Crypto files
    crypto/md5.cpp
    crypto/sha1.cpp
    crypto/blowfish.cpp
    crypto/tea.cpp

    # Hash files
    hash/mhash.cpp

    # Math files
    math/math.cpp
    math/fraction.cpp
    math/bignumber.cpp
    math/gpu_math.cpp

    # Compression files
    compression/huffman.cpp
    compression/matrix_compress.cpp

    # Signal processing files
    signal/convolve.cpp

    # Optimization files
    optimization/pathfinding.cpp

    # Encoding files
    encoding/base.cpp

    # Graphics files
    graphics/flood.cpp

    # Utils files
    utils/fnmatch.cpp
)

set(HEADERS
    # Backwards compatibility headers (in root)
    algorithm.hpp
    annealing.hpp
    base.hpp
    bignumber.hpp
    blowfish.hpp
    convolve.hpp
    error_calibration.hpp
    flood.hpp
    fnmatch.hpp
    fraction.hpp
    hash.hpp
    huffman.hpp
    math.hpp
    matrix.hpp
    matrix_compress.hpp
    md5.hpp
    mhash.hpp
    pathfinding.hpp
    perlin.hpp
    rust_numeric.hpp
    sha1.hpp
    snowflake.hpp
    tea.hpp
    weight.hpp

    # Actual implementation headers (in subdirectories)
    core/algorithm.hpp
    core/rust_numeric.hpp
    core/simd_utils.hpp
    core/opencl_utils.hpp

    crypto/md5.hpp
    crypto/sha1.hpp
    crypto/blowfish.hpp
    crypto/tea.hpp

    hash/hash.hpp
    hash/mhash.hpp

    math/math.hpp
    math/matrix.hpp
    math/fraction.hpp
    math/bignumber.hpp
    math/statistics.hpp
    math/numerical.hpp
    math/gpu_math.hpp

    compression/huffman.hpp
    compression/matrix_compress.hpp

    signal/convolve.hpp

    optimization/annealing.hpp
    optimization/pathfinding.hpp

    encoding/base.hpp

    graphics/flood.hpp
    graphics/perlin.hpp
    graphics/simplex.hpp
    graphics/image_ops.hpp

    utils/error_calibration.hpp
    utils/fnmatch.hpp
    utils/snowflake.hpp
    utils/weight.hpp
    utils/uuid.hpp
)

set(LIBS ${ATOM_ALGORITHM_DEPENDS})

# Add OpenSSL to the list of libraries
list(APPEND LIBS OpenSSL::SSL OpenSSL::Crypto loguru)
if(TBB_FOUND)
    list(APPEND LIBS TBB::tbb)
endif()

# Build object library
add_library(${PROJECT_NAME}_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

target_link_libraries(${PROJECT_NAME}_object PRIVATE ${LIBS})

# Build static library
add_library(${PROJECT_NAME} STATIC)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_object ${LIBS}
                                              ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Add OpenSSL include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${OPENSSL_INCLUDE_DIR})

set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR}
             OUTPUT_NAME ${PROJECT_NAME})

install(TARGETS ${PROJECT_NAME} ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Register this module to the global target list
get_property(ATOM_MODULE_TARGETS GLOBAL PROPERTY ATOM_MODULE_TARGETS)
list(APPEND ATOM_MODULE_TARGETS ${PROJECT_NAME})
set_property(GLOBAL PROPERTY ATOM_MODULE_TARGETS "${ATOM_MODULE_TARGETS}")
