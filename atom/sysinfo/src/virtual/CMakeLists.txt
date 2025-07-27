# CMakeLists.txt for atom/sysinfo/virtual module

cmake_minimum_required(VERSION 3.16)

# Define the source files
set(VIRTUAL_SOURCES
    virtual.cpp
    common.cpp
    detection.cpp
    hypervisor.cpp
    container.cpp
)

# Define header files
set(VIRTUAL_HEADERS
    virtual.hpp
    common.hpp
    detection.hpp
    hypervisor.hpp
    container.hpp
)

# Create the target library
add_library(atom_sysinfo_virtual STATIC ${VIRTUAL_SOURCES} ${VIRTUAL_HEADERS})

# Set include directories
target_include_directories(atom_sysinfo_virtual
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
        ${CMAKE_CURRENT_SOURCE_DIR}/../../..
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link dependencies
target_link_libraries(atom_sysinfo_virtual
    PUBLIC
        spdlog::spdlog
)

# Platform-specific libraries and settings
if(WIN32)
    # Windows-specific libraries
    target_link_libraries(atom_sysinfo_virtual
        PRIVATE
            wbemuuid
            ole32
            oleaut32
            advapi32
    )

    # Windows-specific compile definitions
    target_compile_definitions(atom_sysinfo_virtual
        PRIVATE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            _WIN32_WINNT=0x0601  # Windows 7 or later
    )

elseif(UNIX AND NOT APPLE)
    # Linux-specific libraries
    target_link_libraries(atom_sysinfo_virtual
        PRIVATE
            ${CMAKE_THREAD_LIBS_INIT}
    )

    # Check for optional libraries
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SYSTEMD QUIET libsystemd)
        if(SYSTEMD_FOUND)
            target_link_libraries(atom_sysinfo_virtual PRIVATE ${SYSTEMD_LIBRARIES})
            target_include_directories(atom_sysinfo_virtual PRIVATE ${SYSTEMD_INCLUDE_DIRS})
            target_compile_definitions(atom_sysinfo_virtual PRIVATE HAVE_SYSTEMD)
        endif()
    endif()

elseif(APPLE)
    # macOS-specific frameworks
    find_library(IOKIT_FRAMEWORK IOKit)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    find_library(SECURITY_FRAMEWORK Security)

    target_link_libraries(atom_sysinfo_virtual
        PRIVATE
            ${IOKIT_FRAMEWORK}
            ${COREFOUNDATION_FRAMEWORK}
            ${SECURITY_FRAMEWORK}
    )

    # Optional: Hypervisor framework (macOS 10.15+)
    find_library(HYPERVISOR_FRAMEWORK Hypervisor)
    if(HYPERVISOR_FRAMEWORK)
        target_link_libraries(atom_sysinfo_virtual PRIVATE ${HYPERVISOR_FRAMEWORK})
        target_compile_definitions(atom_sysinfo_virtual PRIVATE HAVE_HYPERVISOR_FRAMEWORK)
    endif()
endif()

# Set C++ standard
target_compile_features(atom_sysinfo_virtual PUBLIC cxx_std_20)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(atom_sysinfo_virtual
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(atom_sysinfo_virtual
        PRIVATE
            /W4
            /wd4100  # unreferenced formal parameter
            /wd4996  # deprecated functions
    )
endif()

# Set library properties
set_target_properties(atom_sysinfo_virtual
    PROPERTIES
        OUTPUT_NAME "atom_sysinfo_virtual"
        POSITION_INDEPENDENT_CODE ON
)

# Installation rules
install(TARGETS atom_sysinfo_virtual
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers
install(FILES ${VIRTUAL_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/sysinfo/virtual
)

# Optional: Build tests
option(BUILD_VIRTUAL_TESTS "Build virtual module tests" OFF)
if(BUILD_VIRTUAL_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Optional: Build examples
option(BUILD_VIRTUAL_EXAMPLES "Build virtual module examples" OFF)
if(BUILD_VIRTUAL_EXAMPLES)
    add_subdirectory(examples)
endif()

# Create a comprehensive target that builds everything
add_custom_target(virtual_all
    DEPENDS atom_sysinfo_virtual
    COMMENT "Building complete virtual module"
)

if(BUILD_VIRTUAL_TESTS)
    add_dependencies(virtual_all virtual_tests)
endif()

if(BUILD_VIRTUAL_EXAMPLES)
    add_dependencies(virtual_all virtual_examples)
endif()

# Debug information
message(STATUS "Virtual module configuration:")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Sources: ${VIRTUAL_SOURCES}")
message(STATUS "  Headers: ${VIRTUAL_HEADERS}")

if(WIN32)
    message(STATUS "  Windows-specific libraries: wbemuuid, ole32, oleaut32, advapi32")
elseif(UNIX AND NOT APPLE)
    message(STATUS "  Linux-specific libraries: pthread")
    if(SYSTEMD_FOUND)
        message(STATUS "  systemd support: enabled")
    else()
        message(STATUS "  systemd support: disabled")
    endif()
elseif(APPLE)
    message(STATUS "  macOS frameworks: IOKit, CoreFoundation, Security")
    if(HYPERVISOR_FRAMEWORK)
        message(STATUS "  Hypervisor framework: available")
    else()
        message(STATUS "  Hypervisor framework: not available")
    endif()
endif()

# Export targets for use by other projects
export(TARGETS atom_sysinfo_virtual
    FILE "${CMAKE_CURRENT_BINARY_DIR}/atom_sysinfo_virtual-targets.cmake"
)

message(STATUS "Virtual module CMakeLists.txt configured successfully")
