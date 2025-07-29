# WiFi Module Examples CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${CMAKE_SOURCE_DIR})

# Create demo executable
add_executable(wifi_demo wifi_demo.cpp)

# Link libraries
target_link_libraries(wifi_demo
    PRIVATE
    atom_sysinfo_wifi
    loguru
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(wifi_demo PRIVATE ws2_32 iphlpapi wlanapi pdh icmp)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(wifi_demo PRIVATE pthread)
elseif(APPLE)
    target_link_libraries(wifi_demo PRIVATE 
        "-framework CoreFoundation"
        "-framework SystemConfiguration"
    )
endif()

# Set C++ standard
target_compile_features(wifi_demo PRIVATE cxx_std_20)

# Compiler-specific options
if(MSVC)
    target_compile_options(wifi_demo PRIVATE /W4)
else()
    target_compile_options(wifi_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install demo executable (optional)
if(INSTALL_EXAMPLES)
    install(TARGETS wifi_demo
        RUNTIME DESTINATION bin/examples
    )
endif()

# Create a custom target to run the demo
add_custom_target(run_wifi_demo
    COMMAND wifi_demo
    DEPENDS wifi_demo
    COMMENT "Running WiFi module demonstration"
)

message(STATUS "WiFi module examples configured successfully")
message(STATUS "Available example targets:")
message(STATUS "  - wifi_demo: WiFi module demonstration program")
message(STATUS "  - run_wifi_demo: Custom target to run the demo")
