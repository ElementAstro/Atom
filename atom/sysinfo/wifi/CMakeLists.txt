# CMakeLists.txt for atom/sysinfo/wifi module

# Define the source files
set(WIFI_SOURCES
    wifi.cpp
    common.cpp
)

# Add platform-specific source files
if(WIN32)
    list(APPEND WIFI_SOURCES windows.cpp)
elseif(UNIX AND NOT APPLE)
    list(APPEND WIFI_SOURCES linux.cpp)
elseif(APPLE)
    list(APPEND WIFI_SOURCES macos.cpp)
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
install(FILES wifi.hpp common.hpp
    DESTINATION include/atom/sysinfo/wifi
)

# Install platform-specific headers
if(WIN32)
    install(FILES windows.hpp DESTINATION include/atom/sysinfo/wifi)
elseif(UNIX AND NOT APPLE)
    install(FILES linux.hpp DESTINATION include/atom/sysinfo/wifi)
elseif(APPLE)
    install(FILES macos.hpp DESTINATION include/atom/sysinfo/wifi)
endif()
