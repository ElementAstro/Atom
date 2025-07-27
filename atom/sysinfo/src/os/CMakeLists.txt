# CMakeLists.txt for atom/sysinfo/os module

cmake_minimum_required(VERSION 3.15)

# Set OS module source files
set(OS_SOURCES
    os.cpp
    common.cpp
)

# Create the OS module library
add_library(atom_sysinfo_os STATIC ${OS_SOURCES})

# Set include directories
target_include_directories(atom_sysinfo_os
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
        ${CMAKE_CURRENT_SOURCE_DIR}/../../..
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link dependencies
target_link_libraries(atom_sysinfo_os
    PUBLIC
        loguru
)

# Platform-specific library linking
if(WIN32)
    target_link_libraries(atom_sysinfo_os PRIVATE pdh)
elseif(UNIX AND NOT APPLE)
    # Linux-specific libraries
    find_package(Threads REQUIRED)
    target_link_libraries(atom_sysinfo_os PRIVATE ${CMAKE_THREAD_LIBS_INIT})
elseif(APPLE)
    # macOS-specific libraries
    target_link_libraries(atom_sysinfo_os PRIVATE
        "-framework CoreFoundation"
        "-framework SystemConfiguration"
    )
endif()

# Set compilation features
target_compile_features(atom_sysinfo_os PUBLIC cxx_std_20)

# Install rules
install(TARGETS atom_sysinfo_os
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
        os.hpp
        common.hpp
        $<$<PLATFORM_ID:Windows>:windows.hpp>
        $<$<PLATFORM_ID:Linux>:linux.hpp>
        $<$<PLATFORM_ID:Darwin>:macos.hpp>
    DESTINATION include/atom/sysinfo/os
)
