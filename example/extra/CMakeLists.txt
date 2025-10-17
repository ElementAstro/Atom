# Examples directory: extra
cmake_minimum_required(VERSION 3.10)

# Get subdirectory name (for executable prefix)
get_filename_component(SUBDIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# Optional: Add extra example-specific options
option(ATOM_EXAMPLE_EXTRA_BUILD_ALL "Build all extra examples" ON)

# Function to create examples from a directory
function(create_examples_from_directory DIR_NAME)
    # Get all CPP files in the directory
    file(GLOB CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/${DIR_NAME}/*.cpp)

    foreach(CPP_FILE ${CPP_FILES})
        # Get filename without extension
        get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)

        # Construct executable name (subdir_dirname_filename)
        set(EXECUTABLE_NAME ${SUBDIR_NAME}_${DIR_NAME}_${EXAMPLE_NAME})

        # Configuration option to control each example build individually
        string(TOUPPER ${DIR_NAME}_${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
        option(ATOM_EXAMPLE_EXTRA_${EXAMPLE_NAME_UPPER} "Build extra example: ${DIR_NAME}/${EXAMPLE_NAME}" ${ATOM_EXAMPLE_EXTRA_BUILD_ALL})

        # Conditionally add executable
        if(ATOM_EXAMPLE_EXTRA_${EXAMPLE_NAME_UPPER})
            add_executable(${EXECUTABLE_NAME} ${CPP_FILE})
            target_link_libraries(${EXECUTABLE_NAME} atom)

            # Set IDE folder grouping
            set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER "Examples/Extra/${DIR_NAME}")
        endif()
    endforeach()
endfunction()

# Get all CPP files in root directory
file(GLOB CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

# Create executables for root directory files
foreach(CPP_FILE ${CPP_FILES})
    # Get filename without extension
    get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)

    # Construct executable name (subdir_filename)
    set(EXECUTABLE_NAME ${SUBDIR_NAME}_${EXAMPLE_NAME})

    # Configuration option to control each example build individually
    string(TOUPPER ${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
    option(ATOM_EXAMPLE_EXTRA_${EXAMPLE_NAME_UPPER} "Build extra example: ${EXAMPLE_NAME}" ${ATOM_EXAMPLE_EXTRA_BUILD_ALL})

    # Conditionally add executable
    if(ATOM_EXAMPLE_EXTRA_${EXAMPLE_NAME_UPPER})
        add_executable(${EXECUTABLE_NAME} ${CPP_FILE})
        target_link_libraries(${EXECUTABLE_NAME} atom)

        # Set IDE folder grouping
        set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER "Examples/Extra")
    endif()
endforeach()

# Create examples for each subdirectory
create_examples_from_directory("asio")
create_examples_from_directory("beast")
# create_examples_from_directory("boost")  # Disabled: Boost not available
create_examples_from_directory("curl")
create_examples_from_directory("dotenv")
create_examples_from_directory("iconv")
create_examples_from_directory("inicpp")
create_examples_from_directory("injection")
create_examples_from_directory("pugixml")
create_examples_from_directory("spdlog")
create_examples_from_directory("uv")
