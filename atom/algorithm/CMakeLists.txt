cmake_minimum_required(VERSION 3.21)
project(atom-algorithm VERSION 1.0.0 LANGUAGES C CXX)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# Find OpenSSL package
find_package(OpenSSL REQUIRED)

# Find TBB package (optional for now due to vcpkg network issues)
find_package(TBB QUIET)

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

# Create library target
add_library(atom-algorithm STATIC ${SOURCES} ${HEADERS})

# Configure module using standardized function
atom_configure_module(atom-algorithm)

# Link module-specific dependencies
target_link_libraries(atom-algorithm PRIVATE ${LIBS} ${CMAKE_THREAD_LIBS_INIT})

# Add OpenSSL include directories
target_include_directories(atom-algorithm PRIVATE ${OPENSSL_INCLUDE_DIR})

# Install headers
install(FILES ${HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/algorithm
)
