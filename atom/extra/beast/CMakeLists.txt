# CMakeLists.txt for Beast HTTP/WebSocket library
# Integrated with main Atom build system

# Set CMake policy for Boost module
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

# Find required dependencies
find_package(Boost QUIET COMPONENTS system)

# Try different ways to find nlohmann_json
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(NLOHMANN_JSON QUIET nlohmann_json)
    endif()
endif()

# Check if we have the required dependencies
set(BEAST_DEPS_FOUND FALSE)
if(Boost_FOUND AND (nlohmann_json_FOUND OR NLOHMANN_JSON_FOUND))
    set(BEAST_DEPS_FOUND TRUE)
endif()

# Only build if dependencies are available
if(BEAST_DEPS_FOUND)
    # Source files
    set(BEAST_SOURCES
        http.cpp
        ws.cpp
    )

    # Header files
    set(BEAST_HEADERS
        http.hpp
        http_utils.hpp
        ws.hpp
    )

    # Create library
    add_library(atom-extra-beast ${BEAST_SOURCES} ${BEAST_HEADERS})

    # Set target properties
    target_include_directories(atom-extra-beast PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

    # Set target properties for consistency
    set_target_properties(atom-extra-beast PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    # Link dependencies
    target_link_libraries(atom-extra-beast PUBLIC Boost::system)

    # Link fmt and spdlog libraries for logging compatibility
    find_package(fmt REQUIRED)
    find_package(spdlog REQUIRED)
    target_link_libraries(atom-extra-beast PUBLIC fmt::fmt spdlog::spdlog)

    if(nlohmann_json_FOUND)
        target_link_libraries(atom-extra-beast PUBLIC nlohmann_json::nlohmann_json)
        message(STATUS "atom-extra-beast: Built with Boost and nlohmann_json (CMake)")
    elseif(NLOHMANN_JSON_FOUND)
        target_link_libraries(atom-extra-beast PUBLIC ${NLOHMANN_JSON_LIBRARIES})
        target_include_directories(atom-extra-beast PUBLIC ${NLOHMANN_JSON_INCLUDE_DIRS})
        message(STATUS "atom-extra-beast: Built with Boost and nlohmann_json (pkg-config)")
    endif()

    # Installation rules (optional)
    # install(TARGETS atom-extra-beast DESTINATION lib)
    # install(FILES ${BEAST_HEADERS} DESTINATION include/atom/extra/beast)
else()
    message(STATUS "atom-extra-beast: Skipped due to missing dependencies (Boost::system or nlohmann_json)")
endif()
