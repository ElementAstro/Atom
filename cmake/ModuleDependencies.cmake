# ModuleDependencies.cmake
# Helper functions for module-specific dependency management

# Function to setup common dependencies for a module
function(atom_setup_module_dependencies module_name)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs REQUIRED_DEPS OPTIONAL_DEPS SYSTEM_LIBS)
    cmake_parse_arguments(AMD "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    string(TOUPPER ${module_name} MODULE_UPPER)

    # Link required dependencies
    foreach(dep ${AMD_REQUIRED_DEPS})
        if(TARGET ${dep})
            target_link_libraries(${module_name} PUBLIC ${dep})
        else()
            message(WARNING "Required dependency ${dep} not found for module ${module_name}")
        endif()
    endforeach()

    # Link optional dependencies
    foreach(dep ${AMD_OPTIONAL_DEPS})
        if(TARGET ${dep})
            target_link_libraries(${module_name} PUBLIC ${dep})
            message(STATUS "Optional dependency ${dep} linked to ${module_name}")
        else()
            message(STATUS "Optional dependency ${dep} not available for ${module_name}")
        endif()
    endforeach()

    # Link system libraries
    foreach(lib ${AMD_SYSTEM_LIBS})
        target_link_libraries(${module_name} PUBLIC ${lib})
    endforeach()
endfunction()

# Function to configure a module with standardized settings
function(atom_configure_module module_name)
    set(options HEADER_ONLY)
    set(oneValueArgs)
    set(multiValueArgs DEPENDENCIES OPTIONAL_DEPENDENCIES)
    cmake_parse_arguments(ACM "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(is_header_only ${ACM_HEADER_ONLY})

    if(is_header_only)
        if(NOT TARGET ${module_name})
            add_library(${module_name} INTERFACE)
        endif()
        set(include_scope INTERFACE)
        set(link_scope INTERFACE)
        set(feature_scope INTERFACE)
        set(option_scope INTERFACE)
    else()
        if(NOT TARGET ${module_name})
            message(FATAL_ERROR "atom_configure_module called for target ${module_name} before it was created")
        endif()
        set(include_scope PUBLIC)
        set(link_scope PUBLIC)
        set(feature_scope PUBLIC)
        set(option_scope PRIVATE)
    endif()

    # Set standard include directories
    target_include_directories(${module_name} ${include_scope}
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/atom/${module_name}>
    )

    # Link required dependencies
    foreach(dep ${ACM_DEPENDENCIES})
        if(TARGET ${dep})
            target_link_libraries(${module_name} ${link_scope} ${dep})
        else()
            message(WARNING "Required dependency ${dep} not found for module ${module_name}")
        endif()
    endforeach()

    # Link optional dependencies
    foreach(dep ${ACM_OPTIONAL_DEPENDENCIES})
        if(TARGET ${dep})
            target_link_libraries(${module_name} ${link_scope} ${dep})
            message(STATUS "Optional dependency ${dep} linked to ${module_name}")
        else()
            message(STATUS "Optional dependency ${dep} not available for ${module_name}")
        endif()
    endforeach()

    # All modules depend on error handling
    if(TARGET atom-error AND NOT ${module_name} STREQUAL "atom-error")
        target_link_libraries(${module_name} ${link_scope} atom-error)
    endif()

    # Most modules need threading
    find_package(Threads REQUIRED)
    target_link_libraries(${module_name} ${link_scope} Threads::Threads)

    # Platform-specific libraries
    if(WIN32)
        target_link_libraries(${module_name} ${link_scope} ws2_32 wsock32)
    endif()

    # Apply common compile features/options
    target_compile_features(${module_name} ${feature_scope} cxx_std_20)

    if(MSVC)
        target_compile_options(${module_name} ${option_scope} /W4)
    else()
        target_compile_options(${module_name} ${option_scope} -Wall -Wextra -Wpedantic)
    endif()

    # Set common include directories for installation for non-header-only targets are already handled above

    # Add to global module registry (ensure uniqueness)
    get_property(ATOM_MODULE_TARGETS GLOBAL PROPERTY ATOM_MODULE_TARGETS)
    if(NOT ATOM_MODULE_TARGETS)
        set(ATOM_MODULE_TARGETS)
    endif()
    list(APPEND ATOM_MODULE_TARGETS ${module_name})
    list(REMOVE_DUPLICATES ATOM_MODULE_TARGETS)
    set_property(GLOBAL PROPERTY ATOM_MODULE_TARGETS "${ATOM_MODULE_TARGETS}")
endfunction()

# Function to setup standard Atom module dependencies (legacy compatibility)
function(atom_setup_standard_dependencies module_name)
    # All modules depend on error handling
    if(TARGET atom-error)
        target_link_libraries(${module_name} PUBLIC atom-error)
    endif()

    # Most modules need threading
    find_package(Threads REQUIRED)
    target_link_libraries(${module_name} PUBLIC Threads::Threads)

    # Platform-specific libraries
    if(WIN32)
        # Windows-specific libraries that many modules need
        target_link_libraries(${module_name} PUBLIC ws2_32 wsock32)
    endif()

    # Add to global module registry
    get_property(ATOM_MODULE_TARGETS GLOBAL PROPERTY ATOM_MODULE_TARGETS)
    list(APPEND ATOM_MODULE_TARGETS ${module_name})
    set_property(GLOBAL PROPERTY ATOM_MODULE_TARGETS "${ATOM_MODULE_TARGETS}")
endfunction()

# Function to automatically resolve and enable module dependencies
function(atom_auto_resolve_dependencies MODULE_NAME)
    # Get the dependency list for this module
    string(TOUPPER ${MODULE_NAME} MODULE_UPPER)
    string(REPLACE "-" "_" MODULE_UPPER ${MODULE_UPPER})

    if(DEFINED ATOM_${MODULE_UPPER}_DEPENDS)
        foreach(dep ${ATOM_${MODULE_UPPER}_DEPENDS})
            string(REPLACE "atom-" "ATOM_BUILD_" dep_var_name ${dep})
            string(TOUPPER ${dep_var_name} dep_var_name)

            if(NOT DEFINED ${dep_var_name} OR NOT ${dep_var_name})
                message(STATUS "Auto-enabling dependency ${dep} for ${MODULE_NAME}")
                set(${dep_var_name} ON CACHE BOOL "Auto-enabled dependency for ${MODULE_NAME}" FORCE)

                # Recursively resolve dependencies of dependencies
                atom_auto_resolve_dependencies(${dep})
            endif()
        endforeach()
    endif()
endfunction()

# Function to validate module dependencies are satisfied
function(atom_validate_module_dependencies MODULE_NAME)
    string(TOUPPER ${MODULE_NAME} MODULE_UPPER)
    string(REPLACE "-" "_" MODULE_UPPER ${MODULE_UPPER})

    if(DEFINED ATOM_${MODULE_UPPER}_DEPENDS)
        foreach(dep ${ATOM_${MODULE_UPPER}_DEPENDS})
            string(REPLACE "atom-" "ATOM_BUILD_" dep_var_name ${dep})
            string(TOUPPER ${dep_var_name} dep_var_name)

            if(NOT DEFINED ${dep_var_name} OR NOT ${dep_var_name})
                message(FATAL_ERROR "Module ${MODULE_NAME} depends on ${dep}, but that module is not enabled for building. Enable it with -D${dep_var_name}=ON")
            endif()
        endforeach()
    endif()
endfunction()

# Function to find and setup TBB (Intel Threading Building Blocks)
function(atom_find_tbb module_name)
    find_package(TBB QUIET)
    if(TBB_FOUND)
        target_link_libraries(${module_name} PUBLIC TBB::tbb)
        message(STATUS "TBB linked to ${module_name}")
    else()
        message(STATUS "TBB not found for ${module_name} - parallel algorithms may be limited")
    endif()
endfunction()

# Function to setup logging dependencies
function(atom_setup_logging_deps module_name)
    # Try to find loguru
    find_library(LOGURU_LIBRARY
        NAMES loguru
        PATHS
            /usr/lib
            /usr/local/lib
            /mingw64/lib
            ${CMAKE_PREFIX_PATH}/lib
    )

    if(LOGURU_LIBRARY)
        target_link_libraries(${module_name} PUBLIC ${LOGURU_LIBRARY})
        message(STATUS "Loguru linked to ${module_name}")
    else()
        message(STATUS "Loguru not found for ${module_name} - using fallback logging")
    endif()
endfunction()

# Function to setup XML dependencies
function(atom_setup_xml_deps module_name)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(TINYXML2 QUIET tinyxml2)
        if(TINYXML2_FOUND)
            target_include_directories(${module_name} PRIVATE ${TINYXML2_INCLUDE_DIRS})
            target_link_libraries(${module_name} PUBLIC ${TINYXML2_LIBRARIES})
            message(STATUS "TinyXML2 linked to ${module_name}")
            return()
        endif()
    endif()

    # Fallback to find_package
    find_package(tinyxml2 QUIET)
    if(tinyxml2_FOUND)
        target_link_libraries(${module_name} PUBLIC tinyxml2::tinyxml2)
        message(STATUS "TinyXML2 (via find_package) linked to ${module_name}")
    else()
        message(STATUS "TinyXML2 not found for ${module_name} - XML features may be limited")
    endif()
endfunction()

# Function to setup compression dependencies
function(atom_setup_compression_deps module_name)
    if(ZLIB_FOUND)
        target_link_libraries(${module_name} PUBLIC ZLIB::ZLIB)
    endif()

    # Try to find additional compression libraries
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBZIPPP QUIET libzippp)
        if(LIBZIPPP_FOUND)
            target_include_directories(${module_name} PRIVATE ${LIBZIPPP_INCLUDE_DIRS})
            target_link_libraries(${module_name} PUBLIC ${LIBZIPPP_LIBRARIES})
            message(STATUS "libzippp linked to ${module_name}")
        endif()
    endif()
endfunction()

# Function to setup database dependencies
function(atom_setup_database_deps module_name)
    if(SQLite3_FOUND)
        target_link_libraries(${module_name} PUBLIC SQLite::SQLite3)
        message(STATUS "SQLite3 linked to ${module_name}")
    else()
        message(STATUS "SQLite3 not available for ${module_name} - database features limited")
    endif()
endfunction()

# Function to setup networking dependencies
function(atom_setup_networking_deps module_name)
    if(ASIO_FOUND)
        target_include_directories(${module_name} PRIVATE ${ASIO_INCLUDE_DIR})
        target_compile_definitions(${module_name} PRIVATE ASIO_STANDALONE)
        message(STATUS "Asio linked to ${module_name}")
    endif()

    if(OpenSSL_FOUND)
        target_link_libraries(${module_name} PUBLIC OpenSSL::SSL OpenSSL::Crypto)
        message(STATUS "OpenSSL linked to ${module_name}")
    endif()

    # Platform-specific networking libraries
    if(WIN32)
        target_link_libraries(${module_name} PUBLIC ws2_32 wsock32 iphlpapi)
    endif()
endfunction()

# Function to setup formatting dependencies
function(atom_setup_formatting_deps module_name)
    if(fmt_FOUND)
        target_link_libraries(${module_name} PUBLIC fmt::fmt)
        message(STATUS "fmt linked to ${module_name}")
    else()
        message(STATUS "fmt not available for ${module_name} - using standard formatting")
    endif()
endfunction()

# Function to setup crypto dependencies
function(atom_setup_crypto_deps module_name)
    if(OpenSSL_FOUND)
        target_link_libraries(${module_name} PUBLIC OpenSSL::SSL OpenSSL::Crypto)
        message(STATUS "OpenSSL crypto linked to ${module_name}")
    else()
        message(WARNING "OpenSSL not available for ${module_name} - crypto features will be limited")
    endif()
endfunction()

# Function to setup test dependencies
function(atom_setup_test_deps test_name)
    if(ATOM_BUILD_TESTS)
        if(GTEST_FOUND)
            if(TARGET GTest::gtest)
                target_link_libraries(${test_name} PRIVATE GTest::gtest GTest::gtest_main)
            elseif(GTEST_LIBRARIES)
                target_include_directories(${test_name} PRIVATE ${GTEST_INCLUDE_DIRS})
                target_link_libraries(${test_name} PRIVATE ${GTEST_LIBRARIES})
            endif()
            message(STATUS "GTest linked to ${test_name}")
        else()
            message(WARNING "GTest not available - ${test_name} may not build")
        endif()
    endif()
endfunction()

# Macro removed; function implementation above handles module configuration
