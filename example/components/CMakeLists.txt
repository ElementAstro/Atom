# Components Examples Directory
cmake_minimum_required(VERSION 3.10)

# Get subdirectory name (for executable prefix)
get_filename_component(SUBDIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# Options for building components examples
option(ATOM_EXAMPLE_COMPONENTS_BUILD_ALL "Build all components examples" ON)

# Specific example categories
option(ATOM_EXAMPLE_COMPONENTS_FOUNDATION "Build foundation examples"
       ${ATOM_EXAMPLE_COMPONENTS_BUILD_ALL})
option(ATOM_EXAMPLE_COMPONENTS_CORE "Build core system examples"
       ${ATOM_EXAMPLE_COMPONENTS_BUILD_ALL})
option(ATOM_EXAMPLE_COMPONENTS_SCRIPTING "Build scripting examples"
       ${ATOM_EXAMPLE_COMPONENTS_BUILD_ALL})
option(ATOM_EXAMPLE_COMPONENTS_ADVANCED "Build advanced feature examples"
       ${ATOM_EXAMPLE_COMPONENTS_BUILD_ALL})
option(ATOM_EXAMPLE_COMPONENTS_INTEGRATION "Build integration examples"
       ${ATOM_EXAMPLE_COMPONENTS_BUILD_ALL})

# Get all CPP files
file(GLOB CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

# Define example categories for better organization
set(FOUNDATION_EXAMPLES component_lifecycle variable_management_example
                        command_dispatch_example)

set(CORE_EXAMPLES registry_example component_pool_example
                  lifecycle_management_example iteration_optimization_example)

set(SCRIPTING_EXAMPLES lua_scripting_example python_scripting_example
                       unified_scripting_example script_sandbox_example)

set(ADVANCED_EXAMPLES serialization_example bindings_example
                      type_conversion_example performance_optimization_example)

set(INTEGRATION_EXAMPLES
    game_entity_system_example plugin_architecture_example hot_reload_example
    integration_example component_registry)

# Function to determine example category
function(get_example_category EXAMPLE_NAME OUT_CATEGORY OUT_OPTION)
  if(${EXAMPLE_NAME} IN_LIST FOUNDATION_EXAMPLES)
    set(${OUT_CATEGORY}
        "Foundation"
        PARENT_SCOPE)
    set(${OUT_OPTION}
        ATOM_EXAMPLE_COMPONENTS_FOUNDATION
        PARENT_SCOPE)
  elseif(${EXAMPLE_NAME} IN_LIST CORE_EXAMPLES)
    set(${OUT_CATEGORY}
        "Core"
        PARENT_SCOPE)
    set(${OUT_OPTION}
        ATOM_EXAMPLE_COMPONENTS_CORE
        PARENT_SCOPE)
  elseif(${EXAMPLE_NAME} IN_LIST SCRIPTING_EXAMPLES)
    set(${OUT_CATEGORY}
        "Scripting"
        PARENT_SCOPE)
    set(${OUT_OPTION}
        ATOM_EXAMPLE_COMPONENTS_SCRIPTING
        PARENT_SCOPE)
  elseif(${EXAMPLE_NAME} IN_LIST ADVANCED_EXAMPLES)
    set(${OUT_CATEGORY}
        "Advanced"
        PARENT_SCOPE)
    set(${OUT_OPTION}
        ATOM_EXAMPLE_COMPONENTS_ADVANCED
        PARENT_SCOPE)
  elseif(${EXAMPLE_NAME} IN_LIST INTEGRATION_EXAMPLES)
    set(${OUT_CATEGORY}
        "Integration"
        PARENT_SCOPE)
    set(${OUT_OPTION}
        ATOM_EXAMPLE_COMPONENTS_INTEGRATION
        PARENT_SCOPE)
  else()
    set(${OUT_CATEGORY}
        "Miscellaneous"
        PARENT_SCOPE)
    set(${OUT_OPTION}
        ATOM_EXAMPLE_COMPONENTS_BUILD_ALL
        PARENT_SCOPE)
  endif()
endfunction()

# Create executables for each CPP file
foreach(CPP_FILE ${CPP_FILES})
  # Get filename without extension
  get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)

  # Construct executable name (subdir_filename)
  set(EXECUTABLE_NAME ${SUBDIR_NAME}_${EXAMPLE_NAME})

  # Get example category and build option
  get_example_category(${EXAMPLE_NAME} EXAMPLE_CATEGORY CATEGORY_OPTION)

  # Configure individual example option
  string(TOUPPER ${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
  option(ATOM_EXAMPLE_COMPONENTS_${EXAMPLE_NAME_UPPER}
         "Build components example: ${EXAMPLE_NAME}" ${${CATEGORY_OPTION}})

  # Conditionally add executable
  if(ATOM_EXAMPLE_COMPONENTS_${EXAMPLE_NAME_UPPER})
    add_executable(${EXECUTABLE_NAME} ${CPP_FILE})
    target_link_libraries(${EXECUTABLE_NAME} atom)

    # Set IDE folder grouping by category
    set_property(TARGET ${EXECUTABLE_NAME}
                 PROPERTY FOLDER "Examples/Components/${EXAMPLE_CATEGORY}")

    # Add compile definitions for optional features
    if(ATOM_ENABLE_LUA)
      target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_ENABLE_LUA=1)
    endif()

    if(ATOM_ENABLE_PYTHON)
      target_compile_definitions(${EXECUTABLE_NAME}
                                 PRIVATE ATOM_ENABLE_PYTHON=1)
    endif()

    message(STATUS "Added ${EXAMPLE_CATEGORY} example: ${EXECUTABLE_NAME}")
  endif()
endforeach()

# Print summary
message(STATUS "Components Examples Configuration:")
message(STATUS "  Foundation Examples: ${ATOM_EXAMPLE_COMPONENTS_FOUNDATION}")
message(STATUS "  Core Examples: ${ATOM_EXAMPLE_COMPONENTS_CORE}")
message(STATUS "  Scripting Examples: ${ATOM_EXAMPLE_COMPONENTS_SCRIPTING}")
message(STATUS "  Advanced Examples: ${ATOM_EXAMPLE_COMPONENTS_ADVANCED}")
message(STATUS "  Integration Examples: ${ATOM_EXAMPLE_COMPONENTS_INTEGRATION}")
