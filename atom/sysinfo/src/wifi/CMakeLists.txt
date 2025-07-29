# CMakeLists.txt for atom/sysinfo/wifi module

# Define the source files
set(WIFI_SOURCES
    wifi.cpp
    common.cpp
    monitor.cpp
    quality.cpp
    error_handler.cpp
    config.cpp
)

# Add platform-specific sources
if(WIN32)
    list(APPEND WIFI_SOURCES platform/windows.cpp)
elseif(UNIX AND NOT APPLE)
    list(APPEND WIFI_SOURCES platform/linux.cpp)
elseif(APPLE)
    list(APPEND WIFI_SOURCES platform/macos.cpp)
endif()

# Create the target library
add_library(atom_sysinfo_wifi ${WIFI_SOURCES})

# Set include directories
target_include_directories(atom_sysinfo_wifi
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

# Add platform-specific link libraries
if(WIN32)
    target_link_libraries(atom_sysinfo_wifi
        PUBLIC
            iphlpapi
            ws2_32
            wlanapi
            pdh
    )
elseif(UNIX AND NOT APPLE)
    target_link_libraries(atom_sysinfo_wifi
        PUBLIC
            pthread
    )
elseif(APPLE)
    find_library(CORE_FOUNDATION CoreFoundation REQUIRED)
    find_library(SYSTEM_CONFIGURATION SystemConfiguration REQUIRED)
    target_link_libraries(atom_sysinfo_wifi
        PUBLIC
            ${CORE_FOUNDATION}
            ${SYSTEM_CONFIGURATION}
    )
endif()

# Set installation destination
install(TARGETS atom_sysinfo_wifi
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install header files
install(FILES wifi.hpp common.hpp monitor.hpp quality.hpp error_handler.hpp config.hpp
    DESTINATION include/atom/sysinfo/wifi
)

# Add subdirectories for tests and examples
option(BUILD_WIFI_TESTS "Build WiFi module tests" OFF)
option(BUILD_WIFI_EXAMPLES "Build WiFi module examples" ON)

if(BUILD_WIFI_TESTS)
    # add_subdirectory(tests)  # Temporarily disabled due to GMock dependency
endif()

if(BUILD_WIFI_EXAMPLES)
    add_subdirectory(examples)
endif()

# Create a comprehensive target that builds everything
add_custom_target(wifi_all
    DEPENDS atom_sysinfo_wifi
    COMMENT "Building complete WiFi module"
)

if(BUILD_WIFI_TESTS)
    # add_dependencies(wifi_all wifi_tests)  # Temporarily disabled
endif()

if(BUILD_WIFI_EXAMPLES)
    add_dependencies(wifi_all wifi_demo)
endif()

# Print configuration summary
message(STATUS "WiFi Module Configuration Summary:")
message(STATUS "  - Library: atom_sysinfo_wifi")
message(STATUS "  - Tests: ${BUILD_WIFI_TESTS}")
message(STATUS "  - Examples: ${BUILD_WIFI_EXAMPLES}")
message(STATUS "  - Platform: ${CMAKE_SYSTEM_NAME}")

if(WIN32)
    message(STATUS "  - Windows libraries: ws2_32, iphlpapi, wlanapi, pdh, icmp")
elseif(APPLE)
    message(STATUS "  - macOS frameworks: CoreFoundation, SystemConfiguration")
elseif(UNIX)
    message(STATUS "  - Linux libraries: pthread")
endif()

# Install platform-specific headers
if(WIN32)
    install(FILES windows.hpp DESTINATION include/atom/sysinfo/wifi)
elseif(UNIX AND NOT APPLE)
    install(FILES linux.hpp DESTINATION include/atom/sysinfo/wifi)
elseif(APPLE)
    install(FILES macos.hpp DESTINATION include/atom/sysinfo/wifi)
endif()
