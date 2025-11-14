# Example directory: sysinfo
cmake_minimum_required(VERSION 3.10)

# Get subdirectory name (for executable prefix)
get_filename_component(SUBDIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# Option: Build all sysinfo examples
option(ATOM_EXAMPLE_SYSINFO_BUILD_ALL "Build all sysinfo examples" ON)

# Function to add examples from a directory
function(add_sysinfo_examples SUBDIR FOLDER_SUFFIX)
  # Get all CPP files in the subdirectory
  file(GLOB CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR}/*.cpp)

  # Create executable for each CPP file
  foreach(CPP_FILE ${CPP_FILES})
    # Get filename without extension
    get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)

    # Construct executable name (subdir_category_filename)
    set(EXECUTABLE_NAME ${SUBDIR_NAME}_${SUBDIR}_${EXAMPLE_NAME})

    # Configure option to control individual example builds
    string(TOUPPER ${SUBDIR}_${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
    option(ATOM_EXAMPLE_SYSINFO_${EXAMPLE_NAME_UPPER}
           "Build sysinfo example: ${SUBDIR}/${EXAMPLE_NAME}"
           ${ATOM_EXAMPLE_SYSINFO_BUILD_ALL})

    # Conditionally add executable
    if(ATOM_EXAMPLE_SYSINFO_${EXAMPLE_NAME_UPPER})
      add_executable(${EXECUTABLE_NAME} ${CPP_FILE})
      target_link_libraries(${EXECUTABLE_NAME} atom)

      # Set IDE folder grouping
      set_property(TARGET ${EXECUTABLE_NAME}
                   PROPERTY FOLDER "Examples/Sysinfo/${FOLDER_SUFFIX}")
    endif()
  endforeach()
endfunction()

# Add examples from subdirectories
add_sysinfo_examples("hardware" "Hardware")
add_sysinfo_examples("info" "Info")
add_sysinfo_examples("network" "Network")
add_sysinfo_examples("storage" "Storage")
add_sysinfo_examples("utils" "Utils")

# Add examples from root sysinfo directory (general examples)
file(GLOB ROOT_CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

foreach(CPP_FILE ${ROOT_CPP_FILES})
  # Get filename without extension
  get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)

  # Construct executable name (subdir_filename)
  set(EXECUTABLE_NAME ${SUBDIR_NAME}_${EXAMPLE_NAME})

  # Configure option to control individual example builds
  string(TOUPPER ${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
  option(ATOM_EXAMPLE_SYSINFO_${EXAMPLE_NAME_UPPER}
         "Build sysinfo example: ${EXAMPLE_NAME}"
         ${ATOM_EXAMPLE_SYSINFO_BUILD_ALL})

  # Conditionally add executable
  if(ATOM_EXAMPLE_SYSINFO_${EXAMPLE_NAME_UPPER})
    add_executable(${EXECUTABLE_NAME} ${CPP_FILE})
    target_link_libraries(${EXECUTABLE_NAME} atom)

    # Set IDE folder grouping
    set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER "Examples/Sysinfo")
  endif()
endforeach()
