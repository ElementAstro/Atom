cmake_minimum_required(VERSION 3.14)
project(atom_python_bindings)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find dependencies
find_package(
  Python
  COMPONENTS Interpreter Development
  REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

# Set output directories
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# Automatically detect all subdirectories as module types
file(
  GLOB MODULE_DIRS
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/*)
set(MODULE_TYPES "")
foreach(dir ${MODULE_DIRS})
  if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${dir})
    # Ignore some common non-module directories
    if(NOT dir STREQUAL "build"
       AND NOT dir STREQUAL "tests"
       AND NOT dir STREQUAL ".git"
       AND NOT dir STREQUAL "cmake")
      list(APPEND MODULE_TYPES ${dir})
      message(STATUS "Found module type: ${dir}")
    endif()
  endif()
endforeach()

# Create source file list variables for each module type
foreach(type ${MODULE_TYPES})
  set(${type}_SOURCES
      ""
      CACHE INTERNAL "")
  set(${type}_FOUND
      FALSE
      CACHE INTERNAL "")
endforeach()

# Function to collect source files for a module
function(collect_sources_for_module module_type dir)
  # Find all cpp files
  file(GLOB cpp_files "${dir}/*.cpp")
  if(NOT cpp_files)
    message(STATUS "No cpp files found in ${dir}, skipping.")
    return()
  endif()

  # Add source files to the list
  set(${module_type}_SOURCES
      ${${module_type}_SOURCES} ${cpp_files}
      CACHE INTERNAL "")
  set(${module_type}_FOUND
      TRUE
      CACHE INTERNAL "")
endfunction()

# Recursively scan directories
function(scan_directories dir)
  # Get all items in the current directory
  file(
    GLOB items
    RELATIVE ${dir}
    ${dir}/*)

  foreach(item ${items})
    # Full path
    set(item_path "${dir}/${item}")

    # If it's a directory, process it further
    if(IS_DIRECTORY ${item_path})
      # Check if it's one of our module types of interest
      list(FIND MODULE_TYPES ${item} type_index)
      if(type_index GREATER -1)
        # If it's a module type of interest, collect source files
        collect_sources_for_module(${item} ${item_path})
      else()
        # Otherwise, continue scanning recursively
        scan_directories(${item_path})
      endif()
    endif()
  endforeach()
endfunction()

scan_directories(${CMAKE_CURRENT_SOURCE_DIR})

foreach(type ${MODULE_TYPES})
  if(${type}_FOUND)
    # Create Python module
    pybind11_add_module(atom_${type} ${${type}_SOURCES})

    # Set include directories
    target_include_directories(
      atom_${type} PRIVATE ${CMAKE_SOURCE_DIR}/.. # Atom root directory
    )

    if(TARGET atom-${type})
      target_link_libraries(atom_${type} PRIVATE atom-${type})
    endif()

    # Set output name
    set_target_properties(
      atom_${type}
      PROPERTIES PREFIX "" # Remove lib prefix
                 SUFFIX "${PYTHON_MODULE_EXTENSION}" # Use correct extension
    )

    # Install rules
    install(
      TARGETS atom_${type}
      LIBRARY DESTINATION python/${type}
      RUNTIME DESTINATION python/${type})

    # Handle __init__.py
    set(init_py_dir "${CMAKE_CURRENT_SOURCE_DIR}/${type}")
    set(init_py_path "${init_py_dir}/__init__.py")

    # Ensure directory exists
    if(NOT EXISTS ${init_py_dir})
      file(MAKE_DIRECTORY ${init_py_dir})
    endif()

    # If __init__.py doesn't exist, create it
    if(NOT EXISTS ${init_py_path})
      file(WRITE ${init_py_path}
           "# Auto-generated __init__.py for ${type} module")
    endif()

    install(FILES ${init_py_path} DESTINATION python/${type})
  endif()
endforeach()

# Add test support (optional)
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()
