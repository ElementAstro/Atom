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
    # Ignore build directories, hidden directories, and other non-module
    # directories Exclude patterns: build*, .*, cmake, tests, dist, logs,
    # __pycache__
    if(NOT dir MATCHES "^build"
       AND NOT dir MATCHES "^\\."
       AND NOT dir STREQUAL "tests"
       AND NOT dir STREQUAL "cmake"
       AND NOT dir STREQUAL "dist"
       AND NOT dir STREQUAL "logs"
       AND NOT dir STREQUAL "__pycache__"
       AND NOT dir MATCHES "^Release"
       AND NOT dir MATCHES "^Debug"
       AND NOT dir MATCHES "^RelWithDebInfo"
       AND NOT dir MATCHES "^MinSizeRel")
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
  # Find all cpp files recursively (to support subdirectory organization)
  file(GLOB_RECURSE cpp_files "${dir}/*.cpp")
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

  message(STATUS "Collected ${module_type} sources: ${cpp_files}")
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

    # Link to corresponding C++ library if it exists
    if(TARGET atom-${type})
      target_link_libraries(atom_${type} PRIVATE atom-${type})
      message(STATUS "Linking atom_${type} to atom-${type}")
    else()
      message(WARNING "Target atom-${type} not found for Python module ${type}")
    endif()

    # Module-specific dependency handling Web module: requires web-address
    # component and utils library
    if("${type}" STREQUAL "web")
      if(TARGET atom-web-address)
        target_link_libraries(atom_${type} PRIVATE atom-web-address)
        message(STATUS "Linking atom_web to atom-web-address")
      else()
        message(
          WARNING
            "atom-web-address target not found, web module may have limited functionality"
        )
      endif()

      if(TARGET atom-utils)
        target_link_libraries(atom_${type} PRIVATE atom-utils)
        message(STATUS "Linking atom_web to atom-utils")
      else()
        message(
          WARNING
            "atom-utils target not found, web module may have limited functionality"
        )
      endif()
    endif()

    # Connection module: Windows-specific socket library
    if("${type}" STREQUAL "connection" AND WIN32)
      target_link_libraries(atom_${type} PRIVATE mswsock)
      message(STATUS "Linking atom_connection to mswsock on Windows")
    endif()

    # Algorithm module: requires type library
    if("${type}" STREQUAL "algorithm")
      if(TARGET atom-type)
        target_link_libraries(atom_${type} PRIVATE atom-type)
        message(STATUS "Linking atom_algorithm to atom-type")
      else()
        message(
          WARNING
            "atom-type target not found, algorithm module may have limited functionality"
        )
      endif()
    endif()

    # Image module: requires OpenCV and image processing dependencies
    if("${type}" STREQUAL "image")
      find_package(OpenCV QUIET)
      if(OpenCV_FOUND)
        target_link_libraries(atom_${type} PRIVATE ${OpenCV_LIBS})
        message(STATUS "Linking atom_image to OpenCV")
      else()
        message(
          WARNING
            "OpenCV not found, image module may have limited functionality")
      endif()
    endif()

    # IO module: may require filesystem library
    if("${type}" STREQUAL "io")
      if(TARGET atom-utils)
        target_link_libraries(atom_${type} PRIVATE atom-utils)
        message(STATUS "Linking atom_io to atom-utils")
      endif()
    endif()

    # System/Sysinfo modules: may require platform-specific libraries
    if("${type}" STREQUAL "system" OR "${type}" STREQUAL "sysinfo")
      if(WIN32)
        target_link_libraries(atom_${type} PRIVATE pdh)
        message(STATUS "Linking atom_${type} to pdh on Windows")
      endif()
    endif()

    # Link spdlog and error handling (common for all modules)
    find_package(spdlog QUIET)
    if(spdlog_FOUND)
      target_link_libraries(atom_${type} PRIVATE spdlog::spdlog atom-error)
      message(STATUS "Linking atom_${type} to spdlog and atom-error")
    else()
      target_link_libraries(atom_${type} PRIVATE atom-error)
      message(STATUS "Linking atom_${type} to atom-error (spdlog not found)")
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

    # Only create __init__.py if it doesn't exist (don't overwrite user-created
    # files)
    if(NOT EXISTS ${init_py_path})
      message(
        WARNING
          "Creating auto-generated __init__.py for ${type} module. Consider creating a proper one manually."
      )
      file(WRITE ${init_py_path}
           "# Auto-generated __init__.py for ${type} module\n")
    else()
      message(STATUS "Using existing __init__.py for ${type} module")
    endif()

    # Install __init__.py
    install(FILES ${init_py_path} DESTINATION python/${type})

    # Install subdirectory __init__.py files if they exist (for nested module
    # structures)
    file(GLOB_RECURSE submodule_init_files "${init_py_dir}/**/__init__.py")
    foreach(init_file ${submodule_init_files})
      file(RELATIVE_PATH rel_path ${init_py_dir} ${init_file})
      get_filename_component(rel_dir ${rel_path} DIRECTORY)
      if(rel_dir)
        install(FILES ${init_file} DESTINATION python/${type}/${rel_dir})
        message(STATUS "Found submodule __init__.py: ${rel_path}")
      endif()
    endforeach()
  endif()
endforeach()

# Add test support (optional)
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()
