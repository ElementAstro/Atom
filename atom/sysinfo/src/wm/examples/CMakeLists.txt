# Window Manager Examples CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

# Example executables
set(EXAMPLES
    basic_system_info
    window_management
    theme_monitoring
)

# Create executables for each example
foreach(EXAMPLE ${EXAMPLES})
    add_executable(${EXAMPLE} ${EXAMPLE}.cpp)
    
    target_link_libraries(${EXAMPLE}
        PRIVATE
            atom_sysinfo_wm
            spdlog::spdlog
    )
    
    target_include_directories(${EXAMPLE}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    
    # Compiler options
    if(MSVC)
        target_compile_options(${EXAMPLE} PRIVATE /W4)
    else()
        target_compile_options(${EXAMPLE} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    
    # Set output directory
    set_target_properties(${EXAMPLE} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
    )
endforeach()

# Install examples (optional)
option(INSTALL_WM_EXAMPLES "Install window manager examples" OFF)
if(INSTALL_WM_EXAMPLES)
    install(TARGETS ${EXAMPLES}
        RUNTIME DESTINATION bin/examples
    )
    
    # Install source files as documentation
    install(FILES
        basic_system_info.cpp
        window_management.cpp
        theme_monitoring.cpp
        DESTINATION share/doc/atom_sysinfo_wm/examples
    )
endif()
