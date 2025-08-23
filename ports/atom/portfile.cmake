# vcpkg portfile for Atom library
# This file defines how to build and install the Atom library using vcpkg

vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

# Get source code from GitHub
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO ElementAstro/Atom
    REF v${VERSION}
    SHA512 0  # This will be automatically updated by vcpkg
    HEAD_REF main
    PATCHES
        fix-cmake-config.patch  # Optional patch file
)

# Check for required system dependencies
if(VCPKG_TARGET_IS_LINUX)
    message(STATUS "Linux build detected - checking for system dependencies")
endif()

# Configure build options based on features
set(ATOM_BUILD_OPTIONS)

# Core build options
list(APPEND ATOM_BUILD_OPTIONS
    -DATOM_BUILD_EXAMPLES=OFF
    -DATOM_BUILD_TESTS=OFF
    -DATOM_BUILD_DOCS=OFF
    -DATOM_BUILD_PYTHON_BINDINGS=OFF
)

# Feature-based options
if("python" IN_LIST FEATURES)
    list(APPEND ATOM_BUILD_OPTIONS -DATOM_BUILD_PYTHON_BINDINGS=ON)
endif()

if("examples" IN_LIST FEATURES)
    list(APPEND ATOM_BUILD_OPTIONS -DATOM_BUILD_EXAMPLES=ON)
endif()

if("tests" IN_LIST FEATURES)
    list(APPEND ATOM_BUILD_OPTIONS -DATOM_BUILD_TESTS=ON)
endif()

if("docs" IN_LIST FEATURES)
    list(APPEND ATOM_BUILD_OPTIONS -DATOM_BUILD_DOCS=ON)
endif()

# Boost features
if("boost-lockfree" IN_LIST FEATURES)
    list(APPEND ATOM_BUILD_OPTIONS -DATOM_USE_BOOST_LOCKFREE=ON)
endif()

if("boost-graph" IN_LIST FEATURES)
    list(APPEND ATOM_BUILD_OPTIONS -DATOM_USE_BOOST_GRAPH=ON)
endif()

if("boost-intrusive" IN_LIST FEATURES)
    list(APPEND ATOM_BUILD_OPTIONS -DATOM_USE_BOOST_INTRUSIVE=ON)
endif()

# Optional features
if("cfitsio" IN_LIST FEATURES)
    list(APPEND ATOM_BUILD_OPTIONS -DATOM_USE_CFITSIO=ON)
endif()

if("ssh" IN_LIST FEATURES)
    list(APPEND ATOM_BUILD_OPTIONS -DATOM_USE_SSH=ON)
endif()

# Configure CMake
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${ATOM_BUILD_OPTIONS}
        -DUSE_VCPKG=ON
        -DCMAKE_DISABLE_FIND_PACKAGE_Git=ON
    OPTIONS_DEBUG
        -DCMAKE_DEBUG_POSTFIX=d
)

# Build the project
vcpkg_cmake_build()

# Run tests if enabled
if("tests" IN_LIST FEATURES)
    vcpkg_cmake_build(TARGET test)
endif()

# Install the project
vcpkg_cmake_install()

# Fix CMake config files
vcpkg_cmake_config_fixup(
    PACKAGE_NAME atom
    CONFIG_PATH lib/cmake/atom
)

# Fix pkg-config files
vcpkg_fixup_pkgconfig()

# Remove debug includes (they're the same as release)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

# Remove debug share directory if it exists
if(EXISTS "${CURRENT_PACKAGES_DIR}/debug/share")
    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
endif()

# Handle copyright
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

# Create usage file
file(WRITE "${CURRENT_PACKAGES_DIR}/share/${PORT}/usage" [[
The package atom provides CMake targets:

    find_package(atom CONFIG REQUIRED)
    target_link_libraries(main PRIVATE atom::atom)

Available modules:
- atom::algorithm - Algorithm utilities
- atom::async - Asynchronous programming utilities  
- atom::components - Component system
- atom::connection - Network and IPC connections
- atom::containers - Container utilities
- atom::error - Error handling
- atom::image - Image processing (requires cfitsio feature)
- atom::io - Input/output utilities
- atom::log - Logging utilities
- atom::memory - Memory management utilities
- atom::meta - Metaprogramming utilities
- atom::search - Search algorithms
- atom::secret - Cryptographic utilities
- atom::serial - Serial communication
- atom::sysinfo - System information
- atom::system - System utilities
- atom::type - Type utilities
- atom::utils - General utilities
- atom::web - Web utilities

For Python bindings, enable the 'python' feature.
]])

# Validate installation
file(GLOB ATOM_HEADERS "${CURRENT_PACKAGES_DIR}/include/atom/*.hpp")
if(NOT ATOM_HEADERS)
    message(FATAL_ERROR "No Atom headers found in installation")
endif()

file(GLOB ATOM_LIBRARIES 
    "${CURRENT_PACKAGES_DIR}/lib/libatom*.a"
    "${CURRENT_PACKAGES_DIR}/lib/atom*.lib"
)
if(NOT ATOM_LIBRARIES)
    message(FATAL_ERROR "No Atom libraries found in installation")
endif()

# Check for CMake config files
if(NOT EXISTS "${CURRENT_PACKAGES_DIR}/share/atom/atomConfig.cmake")
    message(FATAL_ERROR "CMake config file not found")
endif()

message(STATUS "Atom library installation completed successfully")
message(STATUS "Headers: ${ATOM_HEADERS}")
message(STATUS "Libraries: ${ATOM_LIBRARIES}")

# Feature summary
message(STATUS "Atom features enabled:")
foreach(feature IN LISTS FEATURES)
    message(STATUS "  - ${feature}")
endforeach()

if(NOT FEATURES)
    message(STATUS "  - core (default)")
endif()
