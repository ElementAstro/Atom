cmake_minimum_required(VERSION 3.15)

# Set GPU module source files
set(GPU_SOURCES
    gpu.cpp
    common.cpp
)

# Add platform-specific sources
if(WIN32)
    list(APPEND GPU_SOURCES platform/windows.cpp)
elseif(UNIX AND NOT APPLE)
    list(APPEND GPU_SOURCES platform/linux.cpp)
elseif(APPLE)
    list(APPEND GPU_SOURCES platform/macos.cpp)
endif()

# Create GPU module library
add_library(atom_sysinfo_gpu STATIC ${GPU_SOURCES})

# Set include directories
target_include_directories(atom_sysinfo_gpu
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
        ${CMAKE_CURRENT_SOURCE_DIR}/../../..
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link dependency libraries
target_link_libraries(atom_sysinfo_gpu
    PUBLIC
        loguru
)

# Platform-specific library linking
if(WIN32)
    target_link_libraries(atom_sysinfo_gpu PRIVATE
        setupapi
        wbemuuid
        ole32
        oleaut32
        gdi32
        user32
    )
elseif(UNIX AND NOT APPLE)
    # Linux-specific libraries
    find_package(Threads REQUIRED)
    find_package(X11)
    target_link_libraries(atom_sysinfo_gpu PRIVATE ${CMAKE_THREAD_LIBS_INIT})

    if(X11_FOUND)
        target_link_libraries(atom_sysinfo_gpu PRIVATE ${X11_LIBRARIES})
        if(X11_Xrandr_FOUND)
            target_link_libraries(atom_sysinfo_gpu PRIVATE ${X11_Xrandr_LIB})
        endif()
    endif()

    # Optional: Link with DRM libraries if available
    find_library(DRM_LIBRARY drm)
    if(DRM_LIBRARY)
        target_link_libraries(atom_sysinfo_gpu PRIVATE ${DRM_LIBRARY})
    endif()

elseif(APPLE)
    # macOS-specific frameworks
    target_link_libraries(atom_sysinfo_gpu PRIVATE
        "-framework CoreFoundation"
        "-framework IOKit"
        "-framework CoreGraphics"
        "-framework Metal"
        "-framework MetalKit"
    )
endif()

# Set compile features
target_compile_features(atom_sysinfo_gpu PUBLIC cxx_std_20)

# Set compile definitions
target_compile_definitions(atom_sysinfo_gpu PRIVATE
    $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
)

# Set compile options
if(MSVC)
    target_compile_options(atom_sysinfo_gpu PRIVATE /W4)
else()
    target_compile_options(atom_sysinfo_gpu PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Installation rules
install(TARGETS atom_sysinfo_gpu
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
        gpu.hpp
        common.hpp
        $<$<PLATFORM_ID:Windows>:windows.hpp>
        $<$<PLATFORM_ID:Linux>:linux.hpp>
        $<$<PLATFORM_ID:Darwin>:macos.hpp>
    DESTINATION include/atom/sysinfo/gpu
)

# Export removed to avoid dependency issues with loguru
# Individual modules should be used directly rather than exported
