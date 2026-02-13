cmake_minimum_required(VERSION 3.21)
project(
  atom-algorithm
  VERSION 1.0.0
  LANGUAGES C CXX)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

# Find OpenSSL package (optional for all builds) First check if targets already
# exist from parent scope
if(TARGET OpenSSL::SSL AND TARGET OpenSSL::Crypto)
  set(OpenSSL_FOUND TRUE)
  message(STATUS "OpenSSL targets found from parent scope")
else()
  find_package(OpenSSL QUIET)
  if(NOT OpenSSL_FOUND)
    message(
      STATUS "OpenSSL not found - some cryptographic features will be disabled")
  endif()
endif()

# Find TBB package (optional for now due to vcpkg network issues)
find_package(TBB QUIET)

# Sources and Headers
set(SOURCES
    # Core files
    core/kmp.cpp
    core/boyer_moore.cpp
    core/opencl_utils.cpp
    # Crypto files
    crypto/md5.cpp
    crypto/sha1.cpp
    crypto/blowfish.cpp
    crypto/tea.cpp
    crypto/tea_common.cpp
    crypto/xtea.cpp
    crypto/xxtea.cpp
    crypto/xor_cipher.cpp
    # Hash files
    hash/hex_utils.cpp
    hash/keccak.cpp
    hash/hash_context.cpp
    hash/minhash.cpp
    # Math files
    math/safe_math.cpp
    math/bit_ops.cpp
    math/number_theory.cpp
    math/random.cpp
    math/math_memory.cpp
    math/parallel_math.cpp
    math/fraction.cpp
    math/bignumber.cpp
    math/gpu_math.cpp
    # Compression files
    compression/huffman.cpp
    compression/huffman_optimized.cpp
    compression/matrix_compress.cpp
    compression/matrix_compress_parallel.cpp
    compression/matrix_compress_utils.cpp
    # Signal processing files
    signal/convolution_2d.cpp
    signal/dft.cpp
    signal/gaussian_filter.cpp
    # Optimization files
    optimization/pathfinding/heuristics.cpp
    optimization/pathfinding/grid_map.cpp
    optimization/pathfinding/pathfinder.cpp
    # Encoding files
    encoding/base64.cpp
    encoding/base32.cpp
    encoding/hex.cpp
    encoding/url.cpp
    # Graphics files
    graphics/flood.cpp
    # Utils files
    utils/fnmatch.cpp)

set(HEADERS
    # Barrel export header
    index.hpp
    # Algorithm exception hierarchy
    algorithm_exception.hpp
    # Implementation headers (in subdirectories)
    core/algorithm.hpp
    core/kmp.hpp
    core/bloom_filter.hpp
    core/boyer_moore.hpp
    core/rust_numeric.hpp
    core/rust_types.hpp
    core/rust_error.hpp
    core/rust_result.hpp
    core/rust_option.hpp
    core/rust_range.hpp
    core/rust_int_methods.hpp
    core/rust_float_methods.hpp
    core/rust_iter.hpp
    core/simd_utils.hpp
    core/opencl_utils.hpp
    core/hex_utils.hpp
    crypto/md5.hpp
    crypto/sha1.hpp
    crypto/blowfish.hpp
    crypto/tea.hpp
    crypto/tea_common.hpp
    crypto/xtea.hpp
    crypto/xxtea.hpp
    crypto/crypto_utils.hpp
    crypto/xor_cipher.hpp
    hash/hash.hpp
    hash/hash_base.hpp
    hash/hash_compute.hpp
    hash/hex_utils.hpp
    hash/keccak.hpp
    hash/hash_context.hpp
    hash/minhash.hpp
    hash/mhash.hpp
    math/math.hpp
    math/math_concepts.hpp
    math/safe_math.hpp
    math/bit_ops.hpp
    math/number_theory.hpp
    math/random.hpp
    math/math_memory.hpp
    math/parallel_math.hpp
    math/matrix.hpp
    math/fraction.hpp
    math/bignumber.hpp
    math/statistics.hpp
    math/numerical.hpp
    math/gpu_math.hpp
    math/linear_solver.hpp
    math/levenberg_marquardt.hpp
    compression/huffman.hpp
    compression/huffman_optimized.hpp
    compression/matrix_compress.hpp
    compression/matrix_compress_parallel.hpp
    compression/matrix_compress_utils.hpp
    signal/convolve.hpp
    signal/convolve_common.hpp
    signal/convolution_1d.hpp
    signal/convolution_2d.hpp
    signal/convolution_filters.hpp
    signal/dft.hpp
    signal/frequency_domain.hpp
    signal/gaussian_filter.hpp
    optimization/annealing.hpp
    optimization/annealing/annealing_concept.hpp
    optimization/annealing/cooling_schedule.hpp
    optimization/annealing/simulated_annealing.hpp
    optimization/annealing/tsp.hpp
    optimization/pathfinding.hpp
    optimization/pathfinding/point.hpp
    optimization/pathfinding/graph.hpp
    optimization/pathfinding/heuristics.hpp
    optimization/pathfinding/grid_map.hpp
    optimization/pathfinding/pathfinder.hpp
    encoding/base.hpp
    encoding/detail.hpp
    encoding/base64.hpp
    encoding/base32.hpp
    encoding/hex.hpp
    encoding/url.hpp
    encoding/xor_cipher.hpp
    graphics/flood.hpp
    graphics/noise_base.hpp
    graphics/perlin.hpp
    graphics/simplex.hpp
    graphics/image_ops.hpp
    utils/async_calibration.hpp
    utils/error_calibration.hpp
    utils/fnmatch.hpp
    utils/levenberg_marquardt.hpp
    utils/linear_solver.hpp
    utils/snowflake.hpp
    utils/snowflake_exception.hpp
    utils/uuid.hpp
    utils/weight.hpp
    utils/weight_collection.hpp
    utils/weight_common.hpp
    utils/weight_sampler.hpp
    utils/weight_selector.hpp
    utils/weight_strategy.hpp
    # Common utilities
    common/concepts.hpp
    common/endian.hpp
    common/hex.hpp
    common/parallel.hpp)

set(LIBS ${ATOM_ALGORITHM_DEPENDS})

# Add OpenSSL to the list of libraries (if available)
if(OpenSSL_FOUND)
  list(APPEND LIBS OpenSSL::SSL OpenSSL::Crypto)
endif()

# Add spdlog for logging
find_package(spdlog QUIET)
if(spdlog_FOUND)
  list(APPEND LIBS spdlog::spdlog)
endif()

if(TBB_FOUND)
  list(APPEND LIBS TBB::tbb)
endif()

# Create library target
add_library(atom-algorithm STATIC ${SOURCES} ${HEADERS})
add_library(atom::algorithm ALIAS atom-algorithm)

# Configure module using standardized function
atom_configure_module(atom-algorithm)

# Link module-specific dependencies Use PUBLIC for static library so consumers
# get transitive dependencies
target_link_libraries(atom-algorithm PUBLIC ${LIBS} ${CMAKE_THREAD_LIBS_INIT})

# Define OpenSSL availability for conditional compilation
if(OpenSSL_FOUND)
  target_compile_definitions(atom-algorithm PRIVATE ATOM_HAS_OPENSSL=1)
else()
  target_compile_definitions(atom-algorithm PRIVATE ATOM_HAS_OPENSSL=0)
endif()

# Install library target
install(
  TARGETS atom-algorithm
  EXPORT atom-algorithm-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime)

# Install export targets
install(
  EXPORT atom-algorithm-targets
  FILE atom-algorithmTargets.cmake
  NAMESPACE atom::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/atom
  COMPONENT development)

# Install headers preserving subdirectory structure
foreach(_header ${HEADERS})
  get_filename_component(_dir ${_header} DIRECTORY)
  install(FILES ${_header}
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/algorithm/${_dir})
endforeach()
