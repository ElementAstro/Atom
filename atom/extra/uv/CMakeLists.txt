# CMakeLists.txt for libuv extensions
# Integrated with main Atom build system

# Source files
set(UV_SOURCES
    message_bus.cpp
    subprocess.cpp
)

# Header files
set(UV_HEADERS
    coro.hpp
    message_bus.hpp
    subprocess.hpp
)

# Create library
add_library(atom-extra-uv ${UV_SOURCES} ${UV_HEADERS})

# Set target properties
target_include_directories(atom-extra-uv PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Set target properties for consistency (C++23 required for std::expected)
set_target_properties(atom-extra-uv PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Find and link required libraries
find_package(Threads REQUIRED)
target_link_libraries(atom-extra-uv PUBLIC Threads::Threads)

# Installation rules (optional)
# install(TARGETS atom-extra-uv DESTINATION lib)
# install(FILES ${UV_HEADERS} DESTINATION include/atom/extra/uv)
