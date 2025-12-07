# System Examples - Organized by Module
cmake_minimum_required(VERSION 3.21)

# Get subdirectory name (for executable prefix)
get_filename_component(SUBDIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# Option to build all system examples
option(ATOM_EXAMPLE_SYSTEM_BUILD_ALL "Build all system examples" ON)

# Define all subdirectories that mirror atom/system structure
set(SYSTEM_SUBDIRS
    clipboard
    core
    debug
    hardware
    info
    network
    power
    process
    registry
    scheduling
    shortcut
    signals
    storage)

# Function to add examples from a subdirectory
function(add_examples_from_subdir SUBDIR_PATH FOLDER_NAME)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR_PATH})
    # Get all CPP files in the subdirectory
    file(GLOB SUBDIR_CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR_PATH}/*.cpp)

    foreach(CPP_FILE ${SUBDIR_CPP_FILES})
      # Get filename without extension
      get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)

      # Construct executable name (system_subdir_filename)
      set(EXECUTABLE_NAME ${SUBDIR_NAME}_${SUBDIR_PATH}_${EXAMPLE_NAME})

      # Configure option to control individual example builds
      string(TOUPPER ${SUBDIR_PATH}_${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
      option(ATOM_EXAMPLE_SYSTEM_${EXAMPLE_NAME_UPPER}
             "Build system/${SUBDIR_PATH} example: ${EXAMPLE_NAME}"
             ${ATOM_EXAMPLE_SYSTEM_BUILD_ALL})

      # Conditionally add executable
      if(ATOM_EXAMPLE_SYSTEM_${EXAMPLE_NAME_UPPER})
        add_executable(${EXECUTABLE_NAME} ${CPP_FILE})
        target_link_libraries(${EXECUTABLE_NAME} atom)

        # Set IDE folder grouping
        set_property(TARGET ${EXECUTABLE_NAME}
                     PROPERTY FOLDER "Examples/System/${FOLDER_NAME}")

        message(STATUS "Added example: ${EXECUTABLE_NAME}")
      endif()
    endforeach()
  endif()
endfunction()

# Add examples from each subdirectory
add_examples_from_subdir(clipboard "Clipboard")
add_examples_from_subdir(core "Core")
add_examples_from_subdir(debug "Debug")
add_examples_from_subdir(hardware "Hardware")
add_examples_from_subdir(info "Info")
add_examples_from_subdir(network "Network")
add_examples_from_subdir(power "Power")
add_examples_from_subdir(process "Process")
add_examples_from_subdir(registry "Registry")
add_examples_from_subdir(scheduling "Scheduling")
add_examples_from_subdir(shortcut "Shortcut")
add_examples_from_subdir(signals "Signals")
add_examples_from_subdir(storage "Storage")

# Handle any remaining CPP files in the root directory (for backward
# compatibility)
file(GLOB ROOT_CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

foreach(CPP_FILE ${ROOT_CPP_FILES})
  # Get filename without extension
  get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)

  # Construct executable name
  set(EXECUTABLE_NAME ${SUBDIR_NAME}_${EXAMPLE_NAME})

  # Configure option
  string(TOUPPER ${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
  option(ATOM_EXAMPLE_SYSTEM_${EXAMPLE_NAME_UPPER}
         "Build system example: ${EXAMPLE_NAME}"
         ${ATOM_EXAMPLE_SYSTEM_BUILD_ALL})

  # Conditionally add executable
  if(ATOM_EXAMPLE_SYSTEM_${EXAMPLE_NAME_UPPER})
    add_executable(${EXECUTABLE_NAME} ${CPP_FILE})
    target_link_libraries(${EXECUTABLE_NAME} atom)

    # Set IDE folder grouping
    set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER "Examples/System")

    message(STATUS "Added root example: ${EXECUTABLE_NAME}")
  endif()
endforeach()
