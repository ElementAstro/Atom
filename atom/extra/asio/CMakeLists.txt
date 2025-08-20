# CMakeLists.txt for ASIO networking library extensions
# Integrated with main Atom build system

# Find required dependencies
find_package(Threads REQUIRED)

# Try to find ASIO (standalone or Boost.Asio)
find_package(asio QUIET)
find_package(Boost QUIET COMPONENTS system)

# Debug output (can be removed in production)
# message(STATUS "asio_FOUND: ${asio_FOUND}")
# message(STATUS "Asio_FOUND: ${Asio_FOUND}")
# message(STATUS "Boost_FOUND: ${Boost_FOUND}")

# Only build if ASIO is available
if(asio_FOUND OR Asio_FOUND OR Boost_FOUND)
    # MQTT client source files
    set(MQTT_SOURCES
        mqtt/client.cpp
        mqtt/packet.cpp
    )

    # MQTT client header files
    set(MQTT_HEADERS
        mqtt/client.hpp
        mqtt/packet.hpp
        mqtt/protocol.hpp
        mqtt/test_client.hpp
        mqtt/test_packet.hpp
        mqtt/test_protocol.hpp
        mqtt/test_types.hpp
        mqtt/types.hpp
    )

    # SSE (Server-Sent Events) source files
    set(SSE_SOURCES
        sse/event.cpp
        sse/event_store.cpp
    )

    # SSE header files
    set(SSE_HEADERS
        sse/event.hpp
        sse/event_store.hpp
        sse/sse.hpp
    )

    # Main header files
    set(ASIO_HEADERS
        asio_compatibility.hpp
    )

    # Combine all sources and headers
    set(ASIO_ALL_SOURCES ${MQTT_SOURCES} ${SSE_SOURCES})
    set(ASIO_ALL_HEADERS ${MQTT_HEADERS} ${SSE_HEADERS} ${ASIO_HEADERS})

    # Create library
    add_library(atom-extra-asio ${ASIO_ALL_SOURCES} ${ASIO_ALL_HEADERS})

    # Set target properties
    target_include_directories(atom-extra-asio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

    # Set target properties for consistency (C++23 required for std::expected)
    set_target_properties(atom-extra-asio PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    # Link dependencies
    target_link_libraries(atom-extra-asio PUBLIC Threads::Threads)

    if(asio_FOUND)
        target_link_libraries(atom-extra-asio PUBLIC asio::asio)
        message(STATUS "atom-extra-asio: Built with standalone ASIO (asio_FOUND)")
    elseif(Asio_FOUND)
        # Handle case where the package is found but with different variable name
        if(TARGET asio::asio)
            target_link_libraries(atom-extra-asio PUBLIC asio::asio)
        endif()
        message(STATUS "atom-extra-asio: Built with standalone ASIO (Asio_FOUND)")
    elseif(Boost_FOUND)
        target_link_libraries(atom-extra-asio PUBLIC Boost::system)
        message(STATUS "atom-extra-asio: Built with Boost.Asio")
    endif()

    # Installation rules (optional)
    # install(TARGETS atom-extra-asio DESTINATION lib)
    # install(FILES ${ASIO_ALL_HEADERS} DESTINATION include/atom/extra/asio)
else()
    message(STATUS "atom-extra-asio: Skipped due to missing ASIO dependencies")
endif()
