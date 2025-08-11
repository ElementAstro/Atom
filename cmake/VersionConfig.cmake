# cmake/VersionConfig.cmake

# Configure version from Git
configure_version_from_git(
    OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/atom_version.h"
    VERSION_VARIABLE ATOM_VERSION
    PREFIX "ATOM"
)

# Update project version with Git version
if(DEFINED ATOM_VERSION)
    set(PROJECT_VERSION ${ATOM_VERSION})
    message(STATUS "Using Git-derived version: ${PROJECT_VERSION}")
else()
    message(STATUS "Using default project version: ${PROJECT_VERSION}")
endif()

# Pass version information as definitions to all targets
add_compile_definitions(
    ATOM_VERSION="${PROJECT_VERSION}"
    ATOM_VERSION_STRING="${PROJECT_VERSION}"
)

# Ensure the generated version header is included in builds
include_directories(${CMAKE_CURRENT_BINARY_DIR}) # For atom_version.h

# Generate a version information file for runtime access
# Ensure your version_info.h.in file exists in cmake/
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version_info.h.in")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version_info.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/atom_version_info.h"
        @ONLY
    )
else()
    message(WARNING "cmake/version_info.h.in not found. Skipping generation of atom_version_info.h.")
endif()

message(STATUS "Atom project version configured to: ${PROJECT_VERSION}")
