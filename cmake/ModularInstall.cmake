# =============================================================================
# ModularInstall.cmake - Advanced modular installation system for Atom library
# =============================================================================
# This module provides comprehensive modular installation capabilities
# including: - Component-based installation - Dependency resolution with
# topological sorting - Conflict detection - Package validation - Metadata
# generation (JSON)
#
# Main functions: atom_register_component()           - Register a component
# with metadata atom_resolve_component_dependencies() - Resolve all dependencies
# atom_validate_component_dependencies() - Validate dependency graph
# atom_install_component()            - Install a component
# atom_setup_modular_installation()   - Setup the installation system
# atom_print_installation_summary()   - Print installation status
#
# Options: ATOM_INSTALL_MODULAR              - Enable modular installation
# ATOM_INSTALL_COMPONENT_PACKAGES   - Create separate packages
# ATOM_INSTALL_DEVELOPMENT_FILES    - Install headers and CMake configs
# ATOM_INSTALL_DOCUMENTATION        - Install documentation
# ATOM_INSTALL_EXAMPLES             - Install examples
#
# Author: Max Qian License: GPL3
# =============================================================================

include_guard(GLOBAL)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# =============================================================================
# Modular Installation Configuration
# =============================================================================

# Component installation options
option(ATOM_INSTALL_MODULAR "Enable modular installation" ON)
option(ATOM_INSTALL_COMPONENT_PACKAGES
       "Create separate packages for each component" OFF)
option(ATOM_INSTALL_DEVELOPMENT_FILES
       "Install development files (headers, CMake configs)" ON)
option(ATOM_INSTALL_DOCUMENTATION "Install documentation" ON)
option(ATOM_INSTALL_EXAMPLES "Install examples" OFF)

# Installation paths
set(ATOM_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR}/atom)
set(ATOM_INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR})
set(ATOM_INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR})
set(ATOM_INSTALL_DATADIR ${CMAKE_INSTALL_DATADIR}/atom)
set(ATOM_INSTALL_DOCDIR ${CMAKE_INSTALL_DOCDIR}/atom)
set(ATOM_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/atom)

# Component metadata
set(ATOM_COMPONENT_METADATA_FILE "${CMAKE_BINARY_DIR}/atom_components.json")

# =============================================================================
# Component Registration System
# =============================================================================

# Global component registry
set_property(GLOBAL PROPERTY ATOM_REGISTERED_COMPONENTS "")
set_property(GLOBAL PROPERTY ATOM_COMPONENT_DEPENDENCIES "")
set_property(GLOBAL PROPERTY ATOM_COMPONENT_DESCRIPTIONS "")

# Function to register a component
function(atom_register_component COMPONENT_NAME)
  set(options REQUIRED)
  set(oneValueArgs DESCRIPTION VERSION)
  set(multiValueArgs DEPENDS PROVIDES CONFLICTS)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}"
                        ${ARGN})

  # Validate component name
  if(NOT COMPONENT_NAME MATCHES "^[a-zA-Z][a-zA-Z0-9_-]*$")
    message(FATAL_ERROR "Invalid component name: ${COMPONENT_NAME}")
  endif()

  # Register component globally
  get_property(REGISTERED_COMPONENTS GLOBAL PROPERTY ATOM_REGISTERED_COMPONENTS)
  if(COMPONENT_NAME IN_LIST REGISTERED_COMPONENTS)
    message(WARNING "Component ${COMPONENT_NAME} is already registered")
    return()
  endif()

  list(APPEND REGISTERED_COMPONENTS ${COMPONENT_NAME})
  set_property(GLOBAL PROPERTY ATOM_REGISTERED_COMPONENTS
                               ${REGISTERED_COMPONENTS})

  # Store component metadata
  set_property(GLOBAL PROPERTY ATOM_COMPONENT_${COMPONENT_NAME}_DESCRIPTION
                               "${ARG_DESCRIPTION}")
  set_property(GLOBAL PROPERTY ATOM_COMPONENT_${COMPONENT_NAME}_VERSION
                               "${ARG_VERSION}")
  set_property(GLOBAL PROPERTY ATOM_COMPONENT_${COMPONENT_NAME}_DEPENDS
                               "${ARG_DEPENDS}")
  set_property(GLOBAL PROPERTY ATOM_COMPONENT_${COMPONENT_NAME}_PROVIDES
                               "${ARG_PROVIDES}")
  set_property(GLOBAL PROPERTY ATOM_COMPONENT_${COMPONENT_NAME}_CONFLICTS
                               "${ARG_CONFLICTS}")
  set_property(GLOBAL PROPERTY ATOM_COMPONENT_${COMPONENT_NAME}_REQUIRED
                               "${ARG_REQUIRED}")

  message(STATUS "Registered component: ${COMPONENT_NAME}")
  if(ARG_DEPENDS)
    message(STATUS "  Dependencies: ${ARG_DEPENDS}")
  endif()
endfunction()

# Function to resolve component dependencies
function(atom_resolve_component_dependencies COMPONENT_LIST OUTPUT_VAR)
  set(RESOLVED_COMPONENTS ${COMPONENT_LIST})
  set(PROCESSING_QUEUE ${COMPONENT_LIST})

  while(PROCESSING_QUEUE)
    list(POP_FRONT PROCESSING_QUEUE CURRENT_COMPONENT)

    # Get dependencies for current component
    get_property(COMPONENT_DEPS GLOBAL
                 PROPERTY ATOM_COMPONENT_${CURRENT_COMPONENT}_DEPENDS)

    foreach(DEP ${COMPONENT_DEPS})
      if(NOT DEP IN_LIST RESOLVED_COMPONENTS)
        list(APPEND RESOLVED_COMPONENTS ${DEP})
        list(APPEND PROCESSING_QUEUE ${DEP})
      endif()
    endforeach()
  endwhile()

  # Remove duplicates and sort
  list(REMOVE_DUPLICATES RESOLVED_COMPONENTS)

  # Topological sort based on dependencies
  atom_topological_sort("${RESOLVED_COMPONENTS}" SORTED_COMPONENTS)

  set(${OUTPUT_VAR}
      ${SORTED_COMPONENTS}
      PARENT_SCOPE)
endfunction()

# Function to perform topological sort of components
function(atom_topological_sort COMPONENT_LIST OUTPUT_VAR)
  set(SORTED_COMPONENTS "")
  set(REMAINING_COMPONENTS ${COMPONENT_LIST})

  while(REMAINING_COMPONENTS)
    set(READY_COMPONENTS "")

    # Find components with no unresolved dependencies
    foreach(COMPONENT ${REMAINING_COMPONENTS})
      get_property(COMPONENT_DEPS GLOBAL
                   PROPERTY ATOM_COMPONENT_${COMPONENT}_DEPENDS)

      set(HAS_UNRESOLVED_DEPS FALSE)
      foreach(DEP ${COMPONENT_DEPS})
        if(DEP IN_LIST REMAINING_COMPONENTS)
          set(HAS_UNRESOLVED_DEPS TRUE)
          break()
        endif()
      endforeach()

      if(NOT HAS_UNRESOLVED_DEPS)
        list(APPEND READY_COMPONENTS ${COMPONENT})
      endif()
    endforeach()

    if(NOT READY_COMPONENTS)
      message(
        FATAL_ERROR
          "Circular dependency detected in components: ${REMAINING_COMPONENTS}")
    endif()

    # Add ready components to sorted list
    list(APPEND SORTED_COMPONENTS ${READY_COMPONENTS})

    # Remove ready components from remaining list
    foreach(READY_COMPONENT ${READY_COMPONENTS})
      list(REMOVE_ITEM REMAINING_COMPONENTS ${READY_COMPONENT})
    endforeach()
  endwhile()

  set(${OUTPUT_VAR}
      ${SORTED_COMPONENTS}
      PARENT_SCOPE)
endfunction()

# Function to validate component dependencies
function(atom_validate_component_dependencies)
  get_property(REGISTERED_COMPONENTS GLOBAL PROPERTY ATOM_REGISTERED_COMPONENTS)

  foreach(COMPONENT ${REGISTERED_COMPONENTS})
    get_property(COMPONENT_DEPS GLOBAL
                 PROPERTY ATOM_COMPONENT_${COMPONENT}_DEPENDS)

    foreach(DEP ${COMPONENT_DEPS})
      if(NOT DEP IN_LIST REGISTERED_COMPONENTS)
        message(
          FATAL_ERROR
            "Component ${COMPONENT} depends on unregistered component: ${DEP}")
      endif()
    endforeach()

    # Check for conflicts
    get_property(COMPONENT_CONFLICTS GLOBAL
                 PROPERTY ATOM_COMPONENT_${COMPONENT}_CONFLICTS)
    foreach(CONFLICT ${COMPONENT_CONFLICTS})
      if(CONFLICT IN_LIST REGISTERED_COMPONENTS)
        message(
          WARNING
            "Component ${COMPONENT} conflicts with registered component: ${CONFLICT}"
        )
      endif()
    endforeach()
  endforeach()

  message(STATUS "Component dependency validation completed")
endfunction()

# =============================================================================
# Installation Functions
# =============================================================================

# Function to install a component
function(atom_install_component COMPONENT_NAME)
  set(options OPTIONAL)
  set(oneValueArgs DESTINATION)
  set(multiValueArgs TARGETS HEADERS CMAKE_CONFIGS)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}"
                        ${ARGN})

  # Validate component is registered
  get_property(REGISTERED_COMPONENTS GLOBAL PROPERTY ATOM_REGISTERED_COMPONENTS)
  if(NOT COMPONENT_NAME IN_LIST REGISTERED_COMPONENTS)
    if(ARG_OPTIONAL)
      message(STATUS "Optional component ${COMPONENT_NAME} not found, skipping")
      return()
    else()
      message(FATAL_ERROR "Component ${COMPONENT_NAME} is not registered")
    endif()
  endif()

  message(STATUS "Installing component: ${COMPONENT_NAME}")

  # Install targets
  if(ARG_TARGETS)
    foreach(TARGET ${ARG_TARGETS})
      if(TARGET ${TARGET})
        install(
          TARGETS ${TARGET}
          EXPORT atom-${COMPONENT_NAME}-targets
          LIBRARY DESTINATION ${ATOM_INSTALL_LIBDIR}
          ARCHIVE DESTINATION ${ATOM_INSTALL_LIBDIR}
          RUNTIME DESTINATION ${ATOM_INSTALL_BINDIR}
          INCLUDES
          DESTINATION ${ATOM_INSTALL_INCLUDEDIR}
          COMPONENT ${COMPONENT_NAME})
      endif()
    endforeach()

    # Install export targets
    install(
      EXPORT atom-${COMPONENT_NAME}-targets
      FILE atom-${COMPONENT_NAME}-targets.cmake
      NAMESPACE atom::
      DESTINATION ${ATOM_INSTALL_CMAKEDIR}
      COMPONENT ${COMPONENT_NAME})
  endif()

  # Install headers
  if(ARG_HEADERS)
    foreach(HEADER_DIR ${ARG_HEADERS})
      if(EXISTS ${HEADER_DIR})
        install(
          DIRECTORY ${HEADER_DIR}/
          DESTINATION ${ATOM_INSTALL_INCLUDEDIR}/${COMPONENT_NAME}
          FILES_MATCHING
          PATTERN "*.hpp"
          PATTERN "*.h"
          COMPONENT ${COMPONENT_NAME})
      endif()
    endforeach()
  endif()

  # Install CMake configuration files
  if(ARG_CMAKE_CONFIGS)
    foreach(CMAKE_CONFIG ${ARG_CMAKE_CONFIGS})
      if(EXISTS ${CMAKE_CONFIG})
        install(
          FILES ${CMAKE_CONFIG}
          DESTINATION ${ATOM_INSTALL_CMAKEDIR}
          COMPONENT ${COMPONENT_NAME})
      endif()
    endforeach()
  endif()

  # Generate component configuration file
  atom_generate_component_config(${COMPONENT_NAME})
endfunction()

# Function to generate component configuration file
function(atom_generate_component_config COMPONENT_NAME)
  get_property(COMPONENT_VERSION GLOBAL
               PROPERTY ATOM_COMPONENT_${COMPONENT_NAME}_VERSION)
  get_property(COMPONENT_DESCRIPTION GLOBAL
               PROPERTY ATOM_COMPONENT_${COMPONENT_NAME}_DESCRIPTION)
  get_property(COMPONENT_DEPENDS GLOBAL
               PROPERTY ATOM_COMPONENT_${COMPONENT_NAME}_DEPENDS)

  set(CONFIG_FILE "${CMAKE_BINARY_DIR}/atom-${COMPONENT_NAME}-config.cmake")

  file(
    WRITE ${CONFIG_FILE}
    "# Configuration file for atom-${COMPONENT_NAME}\n"
    "# Generated by CMake\n\n"
    "set(atom-${COMPONENT_NAME}_VERSION \"${COMPONENT_VERSION}\")\n"
    "set(atom-${COMPONENT_NAME}_DESCRIPTION \"${COMPONENT_DESCRIPTION}\")\n"
    "set(atom-${COMPONENT_NAME}_DEPENDS \"${COMPONENT_DEPENDS}\")\n\n")

  # Add dependency finding
  if(COMPONENT_DEPENDS)
    file(APPEND ${CONFIG_FILE} "# Find dependencies\n")
    foreach(DEP ${COMPONENT_DEPENDS})
      file(APPEND ${CONFIG_FILE} "find_package(atom-${DEP} REQUIRED)\n")
    endforeach()
    file(APPEND ${CONFIG_FILE} "\n")
  endif()

  # Include targets file
  file(
    APPEND ${CONFIG_FILE}
    "# Include targets\n"
    "include(\"\${CMAKE_CURRENT_LIST_DIR}/atom-${COMPONENT_NAME}-targets.cmake\")\n\n"
    "# Component found\n"
    "set(atom-${COMPONENT_NAME}_FOUND TRUE)\n")

  # Install the configuration file
  install(
    FILES ${CONFIG_FILE}
    DESTINATION ${ATOM_INSTALL_CMAKEDIR}
    COMPONENT ${COMPONENT_NAME})
endfunction()

# Function to create component metadata file
function(atom_create_component_metadata)
  get_property(REGISTERED_COMPONENTS GLOBAL PROPERTY ATOM_REGISTERED_COMPONENTS)

  file(WRITE ${ATOM_COMPONENT_METADATA_FILE} "{\n")
  file(APPEND ${ATOM_COMPONENT_METADATA_FILE} "  \"components\": {\n")

  list(LENGTH REGISTERED_COMPONENTS COMPONENT_COUNT)
  set(CURRENT_INDEX 0)

  foreach(COMPONENT ${REGISTERED_COMPONENTS})
    math(EXPR CURRENT_INDEX "${CURRENT_INDEX} + 1")

    get_property(COMPONENT_VERSION GLOBAL
                 PROPERTY ATOM_COMPONENT_${COMPONENT}_VERSION)
    get_property(COMPONENT_DESCRIPTION GLOBAL
                 PROPERTY ATOM_COMPONENT_${COMPONENT}_DESCRIPTION)
    get_property(COMPONENT_DEPENDS GLOBAL
                 PROPERTY ATOM_COMPONENT_${COMPONENT}_DEPENDS)
    get_property(COMPONENT_PROVIDES GLOBAL
                 PROPERTY ATOM_COMPONENT_${COMPONENT}_PROVIDES)
    get_property(COMPONENT_CONFLICTS GLOBAL
                 PROPERTY ATOM_COMPONENT_${COMPONENT}_CONFLICTS)

    file(APPEND ${ATOM_COMPONENT_METADATA_FILE} "    \"${COMPONENT}\": {\n")
    file(APPEND ${ATOM_COMPONENT_METADATA_FILE}
         "      \"version\": \"${COMPONENT_VERSION}\",\n")
    file(APPEND ${ATOM_COMPONENT_METADATA_FILE}
         "      \"description\": \"${COMPONENT_DESCRIPTION}\",\n")
    file(APPEND ${ATOM_COMPONENT_METADATA_FILE} "      \"depends\": [")

    if(COMPONENT_DEPENDS)
      list(LENGTH COMPONENT_DEPENDS DEP_COUNT)
      set(DEP_INDEX 0)
      foreach(DEP ${COMPONENT_DEPENDS})
        math(EXPR DEP_INDEX "${DEP_INDEX} + 1")
        file(APPEND ${ATOM_COMPONENT_METADATA_FILE} "\"${DEP}\"")
        if(DEP_INDEX LESS DEP_COUNT)
          file(APPEND ${ATOM_COMPONENT_METADATA_FILE} ", ")
        endif()
      endforeach()
    endif()

    file(APPEND ${ATOM_COMPONENT_METADATA_FILE} "],\n")
    file(APPEND ${ATOM_COMPONENT_METADATA_FILE} "      \"provides\": [")

    if(COMPONENT_PROVIDES)
      list(LENGTH COMPONENT_PROVIDES PROV_COUNT)
      set(PROV_INDEX 0)
      foreach(PROV ${COMPONENT_PROVIDES})
        math(EXPR PROV_INDEX "${PROV_INDEX} + 1")
        file(APPEND ${ATOM_COMPONENT_METADATA_FILE} "\"${PROV}\"")
        if(PROV_INDEX LESS PROV_COUNT)
          file(APPEND ${ATOM_COMPONENT_METADATA_FILE} ", ")
        endif()
      endforeach()
    endif()

    file(APPEND ${ATOM_COMPONENT_METADATA_FILE} "],\n")
    file(APPEND ${ATOM_COMPONENT_METADATA_FILE} "      \"conflicts\": [")

    if(COMPONENT_CONFLICTS)
      list(LENGTH COMPONENT_CONFLICTS CONF_COUNT)
      set(CONF_INDEX 0)
      foreach(CONF ${COMPONENT_CONFLICTS})
        math(EXPR CONF_INDEX "${CONF_INDEX} + 1")
        file(APPEND ${ATOM_COMPONENT_METADATA_FILE} "\"${CONF}\"")
        if(CONF_INDEX LESS CONF_COUNT)
          file(APPEND ${ATOM_COMPONENT_METADATA_FILE} ", ")
        endif()
      endforeach()
    endif()

    file(APPEND ${ATOM_COMPONENT_METADATA_FILE} "]\n")
    file(APPEND ${ATOM_COMPONENT_METADATA_FILE} "    }")

    if(CURRENT_INDEX LESS COMPONENT_COUNT)
      file(APPEND ${ATOM_COMPONENT_METADATA_FILE} ",")
    endif()
    file(APPEND ${ATOM_COMPONENT_METADATA_FILE} "\n")
  endforeach()

  file(APPEND ${ATOM_COMPONENT_METADATA_FILE} "  },\n")
  file(APPEND ${ATOM_COMPONENT_METADATA_FILE} "  \"build_info\": {\n")
  file(APPEND ${ATOM_COMPONENT_METADATA_FILE}
       "    \"cmake_version\": \"${CMAKE_VERSION}\",\n")
  file(APPEND ${ATOM_COMPONENT_METADATA_FILE}
       "    \"build_type\": \"${CMAKE_BUILD_TYPE}\",\n")
  file(APPEND ${ATOM_COMPONENT_METADATA_FILE}
       "    \"compiler\": \"${CMAKE_CXX_COMPILER_ID}\",\n")
  file(APPEND ${ATOM_COMPONENT_METADATA_FILE}
       "    \"platform\": \"${CMAKE_SYSTEM_NAME}\",\n")
  file(APPEND ${ATOM_COMPONENT_METADATA_FILE}
       "    \"architecture\": \"${CMAKE_SYSTEM_PROCESSOR}\"\n")
  file(APPEND ${ATOM_COMPONENT_METADATA_FILE} "  }\n")
  file(APPEND ${ATOM_COMPONENT_METADATA_FILE} "}\n")

  # Install metadata file
  install(
    FILES ${ATOM_COMPONENT_METADATA_FILE}
    DESTINATION ${ATOM_INSTALL_DATADIR}
    COMPONENT metadata)
endfunction()

# Function to setup modular installation
function(atom_setup_modular_installation)
  if(NOT ATOM_INSTALL_MODULAR)
    return()
  endif()

  message(STATUS "Setting up modular installation system")

  # Validate all component dependencies
  atom_validate_component_dependencies()

  # Create component metadata
  atom_create_component_metadata()

  # Create main configuration file
  atom_create_main_config()

  message(STATUS "Modular installation system configured")
endfunction()

# Function to create main Atom configuration file
function(atom_create_main_config)
  set(MAIN_CONFIG_FILE "${CMAKE_BINARY_DIR}/atom-config.cmake")

  file(
    WRITE ${MAIN_CONFIG_FILE}
    "# Main configuration file for Atom library\n"
    "# Generated by CMake\n\n"
    "set(ATOM_VERSION \"${PROJECT_VERSION}\")\n"
    "set(ATOM_INSTALL_PREFIX \"${CMAKE_INSTALL_PREFIX}\")\n\n"
    "# Component finding function\n"
    "function(find_atom_component COMPONENT_NAME)\n"
    "    find_package(atom-\${COMPONENT_NAME} REQUIRED)\n"
    "endfunction()\n\n"
    "# Find all requested components\n"
    "if(ATOM_FIND_COMPONENTS)\n"
    "    foreach(COMPONENT \${ATOM_FIND_COMPONENTS})\n"
    "        find_atom_component(\${COMPONENT})\n"
    "    endforeach()\n"
    "endif()\n\n"
    "set(ATOM_FOUND TRUE)\n")

  install(
    FILES ${MAIN_CONFIG_FILE}
    DESTINATION ${ATOM_INSTALL_CMAKEDIR}
    COMPONENT core)
endfunction()

# =============================================================================
# Utility Functions
# =============================================================================

# Function to print installation summary
function(atom_print_installation_summary)
  get_property(REGISTERED_COMPONENTS GLOBAL PROPERTY ATOM_REGISTERED_COMPONENTS)
  list(LENGTH REGISTERED_COMPONENTS COMPONENT_COUNT)

  message(STATUS "")
  message(STATUS "=== Modular Installation Summary ===")
  message(STATUS "Registered components: ${COMPONENT_COUNT}")
  message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
  message(STATUS "")
  message(STATUS "Options:")
  message(STATUS "  Modular install:     ${ATOM_INSTALL_MODULAR}")
  message(STATUS "  Component packages:  ${ATOM_INSTALL_COMPONENT_PACKAGES}")
  message(STATUS "  Development files:   ${ATOM_INSTALL_DEVELOPMENT_FILES}")
  message(STATUS "  Documentation:       ${ATOM_INSTALL_DOCUMENTATION}")
  message(STATUS "  Examples:            ${ATOM_INSTALL_EXAMPLES}")
  message(STATUS "")

  if(REGISTERED_COMPONENTS)
    message(STATUS "Components:")
    foreach(COMPONENT ${REGISTERED_COMPONENTS})
      get_property(COMP_DESC GLOBAL
                   PROPERTY ATOM_COMPONENT_${COMPONENT}_DESCRIPTION)
      get_property(COMP_DEPS GLOBAL
                   PROPERTY ATOM_COMPONENT_${COMPONENT}_DEPENDS)
      if(COMP_DEPS)
        message(STATUS "  - ${COMPONENT}: ${COMP_DESC} [deps: ${COMP_DEPS}]")
      else()
        message(STATUS "  - ${COMPONENT}: ${COMP_DESC}")
      endif()
    endforeach()
  endif()

  message(STATUS "====================================")
  message(STATUS "")
endfunction()

# Function to get list of registered components
function(atom_get_registered_components OUTPUT_VAR)
  get_property(_components GLOBAL PROPERTY ATOM_REGISTERED_COMPONENTS)
  set(${OUTPUT_VAR}
      "${_components}"
      PARENT_SCOPE)
endfunction()

# Function to check if a component is registered
function(atom_is_component_registered COMPONENT_NAME OUTPUT_VAR)
  get_property(_components GLOBAL PROPERTY ATOM_REGISTERED_COMPONENTS)
  if(COMPONENT_NAME IN_LIST _components)
    set(${OUTPUT_VAR}
        TRUE
        PARENT_SCOPE)
  else()
    set(${OUTPUT_VAR}
        FALSE
        PARENT_SCOPE)
  endif()
endfunction()
