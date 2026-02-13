# Common utilities for algorithm module Header-only library

set(ATOM_ALGORITHM_COMMON_HEADERS concepts.hpp endian.hpp hex.hpp parallel.hpp)

# Add interface library (header-only)
add_library(atom_algorithm_common INTERFACE)
add_library(atom::algorithm::common ALIAS atom_algorithm_common)

target_include_directories(
  atom_algorithm_common
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
            $<INSTALL_INTERFACE:include>)

target_compile_features(atom_algorithm_common INTERFACE cxx_std_20)

# Install headers
install(FILES ${ATOM_ALGORITHM_COMMON_HEADERS}
        DESTINATION include/atom/algorithm/common)
