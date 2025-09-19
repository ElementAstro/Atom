# Enhanced Connection Examples Build Configuration
cmake_minimum_required(VERSION 3.10)

# Get subdirectory name (for executable prefix)
get_filename_component(SUBDIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# Enhanced build options for connection examples
option(ATOM_EXAMPLE_CONNECTION_BUILD_ALL "Build all connection examples" ON)
option(ATOM_EXAMPLE_CONNECTION_ENABLE_TESTS "Enable connection example tests" OFF)
option(ATOM_EXAMPLE_CONNECTION_VERBOSE "Enable verbose output for connection examples" OFF)

# Get all CPP files in the current directory
file(GLOB CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

# Enhanced dependency detection
find_package(Threads REQUIRED)

# Define example categories and their specific dependencies
set(TCP_EXAMPLES "tcpclient;async_tcpclient;sockethub;async_sockethub")
set(UDP_EXAMPLES "udpclient;udpserver;async_udpclient;async_udpserver")
set(FIFO_EXAMPLES "fifoclient;fifoserver;async_fifoclient;async_fifoserver")
set(SSH_EXAMPLES "sshclient;sshserver")
set(TTY_EXAMPLES "ttybase")

# Create executable for each CPP file
foreach(CPP_FILE ${CPP_FILES})
    # Get filename without extension
    get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)

    # Construct executable name (subdirectory_filename)
    set(EXECUTABLE_NAME ${SUBDIR_NAME}_${EXAMPLE_NAME})

    # Configuration option to control individual example builds
    string(TOUPPER ${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
    option(ATOM_EXAMPLE_CONNECTION_${EXAMPLE_NAME_UPPER} "Build connection example: ${EXAMPLE_NAME}" ${ATOM_EXAMPLE_CONNECTION_BUILD_ALL})

    # Conditionally add executable
    if(ATOM_EXAMPLE_CONNECTION_${EXAMPLE_NAME_UPPER})
        add_executable(${EXECUTABLE_NAME} ${CPP_FILE})

        # Base dependencies for all connection examples
        target_link_libraries(${EXECUTABLE_NAME}
            atom-connection
            atom-error
            Threads::Threads
        )
        
        # Add OpenSSL for SSL-enabled examples
        if("${EXAMPLE_NAME}" MATCHES ".*sockethub.*" OR "${EXAMPLE_NAME}" MATCHES ".*tcp.*")
            if(OPENSSL_FOUND)
                target_link_libraries(${EXECUTABLE_NAME} 
                    OpenSSL::SSL 
                    OpenSSL::Crypto
                )
            else()
                # Fallback to direct library linking
                target_link_libraries(${EXECUTABLE_NAME} 
                    ssl 
                    crypto
                )
            endif()
        endif()

        # Set C++ standard and compile features
        target_compile_features(${EXECUTABLE_NAME} PRIVATE cxx_std_20)

        # Enhanced compile options for better debugging and optimization
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            target_compile_options(${EXECUTABLE_NAME} PRIVATE
                -Wall -Wextra -Wpedantic
                $<$<CONFIG:Debug>:-g -O0 -fsanitize=address>
                $<$<CONFIG:Release>:-O3 -DNDEBUG>
            )
            if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
                target_compile_options(${EXECUTABLE_NAME} PRIVATE -fconcepts)
            endif()
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            target_compile_options(${EXECUTABLE_NAME} PRIVATE
                /W4 /permissive-
                $<$<CONFIG:Debug>:/Od /Zi>
                $<$<CONFIG:Release>:/O2 /DNDEBUG>
            )
        endif()

        # Category-specific dependencies
        if(EXAMPLE_NAME IN_LIST TCP_EXAMPLES)
            # TCP examples may need additional networking libraries on some platforms
            if(WIN32)
                target_link_libraries(${EXECUTABLE_NAME} ws2_32 wsock32)
            endif()
        endif()

        if(EXAMPLE_NAME IN_LIST UDP_EXAMPLES)
            # UDP examples may need additional networking libraries on some platforms
            if(WIN32)
                target_link_libraries(${EXECUTABLE_NAME} ws2_32 wsock32)
            endif()
        endif()

        if(EXAMPLE_NAME IN_LIST FIFO_EXAMPLES)
            # FIFO examples are primarily for Unix-like systems
            if(UNIX)
                target_compile_definitions(${EXECUTABLE_NAME} PRIVATE UNIX_FIFO_SUPPORT)
            endif()
        endif()

        # SSH examples need additional dependencies
        if(EXAMPLE_NAME IN_LIST SSH_EXAMPLES)
            if(ATOM_USE_SSH)
                target_link_libraries(${EXECUTABLE_NAME} ${LIBSSH_LIBRARIES})
                target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ATOM_USE_SSH)
            else()
                message(STATUS "SSH support not enabled, ${EXAMPLE_NAME} may have limited functionality")
            endif()
        endif()

        if(EXAMPLE_NAME IN_LIST TTY_EXAMPLES)
            # TTY examples may need platform-specific libraries
            if(WIN32)
                target_link_libraries(${EXECUTABLE_NAME} setupapi)
            elseif(UNIX)
                target_compile_definitions(${EXECUTABLE_NAME} PRIVATE UNIX_TTY_SUPPORT)
            endif()
        endif()

        # Enhanced runtime path settings
        if(UNIX AND NOT APPLE)
            set_target_properties(${EXECUTABLE_NAME} PROPERTIES
                INSTALL_RPATH "$ORIGIN/../lib"
                BUILD_WITH_INSTALL_RPATH TRUE
            )
        endif()

        # Set IDE folder grouping with enhanced organization
        if(EXAMPLE_NAME IN_LIST TCP_EXAMPLES)
            set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER "Examples/Connection/TCP")
        elseif(EXAMPLE_NAME IN_LIST UDP_EXAMPLES)
            set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER "Examples/Connection/UDP")
        elseif(EXAMPLE_NAME IN_LIST FIFO_EXAMPLES)
            set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER "Examples/Connection/FIFO")
        elseif(EXAMPLE_NAME IN_LIST SSH_EXAMPLES)
            set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER "Examples/Connection/SSH")
        elseif(EXAMPLE_NAME IN_LIST TTY_EXAMPLES)
            set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER "Examples/Connection/TTY")
        else()
            set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER "Examples/Connection/Other")
        endif()

        # Add custom target for running the example (optional)
        if(ATOM_EXAMPLE_CONNECTION_ENABLE_TESTS)
            add_custom_target(run_${EXECUTABLE_NAME}
                COMMAND ${EXECUTABLE_NAME}
                DEPENDS ${EXECUTABLE_NAME}
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                COMMENT "Running connection example: ${EXAMPLE_NAME}"
            )
        endif()

        # Verbose output for debugging build issues
        if(ATOM_EXAMPLE_CONNECTION_VERBOSE)
            message(STATUS "Configured connection example: ${EXECUTABLE_NAME}")
            message(STATUS "  Source file: ${CPP_FILE}")
            message(STATUS "  Category: ${EXAMPLE_NAME}")
        endif()
    endif()
endforeach()

# Add a custom target to build all connection examples
add_custom_target(connection_examples_all
    COMMENT "Building all connection examples"
)

# Make all connection examples depend on this target
foreach(CPP_FILE ${CPP_FILES})
    get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)
    set(EXECUTABLE_NAME ${SUBDIR_NAME}_${EXAMPLE_NAME})
    string(TOUPPER ${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)

    if(ATOM_EXAMPLE_CONNECTION_${EXAMPLE_NAME_UPPER})
        add_dependencies(connection_examples_all ${EXECUTABLE_NAME})
    endif()
endforeach()

# Print summary of enabled examples
message(STATUS "Connection Examples Configuration Summary:")
message(STATUS "  Build all examples: ${ATOM_EXAMPLE_CONNECTION_BUILD_ALL}")
message(STATUS "  Enable tests: ${ATOM_EXAMPLE_CONNECTION_ENABLE_TESTS}")
message(STATUS "  Verbose output: ${ATOM_EXAMPLE_CONNECTION_VERBOSE}")

set(ENABLED_EXAMPLES "")
foreach(CPP_FILE ${CPP_FILES})
    get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)
    string(TOUPPER ${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
    if(ATOM_EXAMPLE_CONNECTION_${EXAMPLE_NAME_UPPER})
        list(APPEND ENABLED_EXAMPLES ${EXAMPLE_NAME})
    endif()
endforeach()

if(ENABLED_EXAMPLES)
    message(STATUS "  Enabled examples: ${ENABLED_EXAMPLES}")
else()
    message(STATUS "  No connection examples enabled")
endif()
