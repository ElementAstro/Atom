# CMakeLists.txt for CURL HTTP client library
# Integrated with main Atom build system

# Source files
set(CURL_SOURCES
    cache.cpp
    connection_pool.cpp
    cookie.cpp
    error.cpp
    multi_session.cpp
    multipart.cpp
    rate_limiter.cpp
    request.cpp
    response.cpp
    session.cpp
    session_pool.cpp
    websocket.cpp
)

# Header files
set(CURL_HEADERS
    cache.hpp
    connection_pool.hpp
    cookie.hpp
    error.hpp
    interceptor.hpp
    multi_session.hpp
    multipart.hpp
    rate_limiter.hpp
    request.hpp
    response.hpp
    rest_client.hpp
    session.hpp
    session_pool.hpp
    websocket.hpp
)

# Create library
add_library(atom-extra-curl ${CURL_SOURCES} ${CURL_HEADERS})

# Set target properties
target_include_directories(atom-extra-curl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Set target properties for consistency
set_target_properties(atom-extra-curl PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Find and link CURL library (optional)
find_package(CURL QUIET)
if(CURL_FOUND)
    target_link_libraries(atom-extra-curl PUBLIC CURL::libcurl)
    message(STATUS "Found CURL, linking to atom-extra-curl")
else()
    message(STATUS "CURL not found, atom-extra-curl may have limited functionality")
endif()

# Installation rules (optional)
# install(TARGETS atom-extra-curl DESTINATION lib)
# install(FILES ${CURL_HEADERS} DESTINATION include/atom/extra/curl)
