# CMakeLists.txt for Boost utility headers
# Integrated with main Atom build system
# Header-only library

# Header files
set(BOOST_HEADERS
    charconv.hpp
    locale.hpp
    math.hpp
    regex.hpp
    system.hpp
    uuid.hpp
)

# Create interface library (header-only)
add_library(atom-extra-boost INTERFACE)

# Set target properties
target_include_directories(atom-extra-boost INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Set target properties for consistency
target_compile_features(atom-extra-boost INTERFACE cxx_std_20)

# Installation rules (optional)
# install(TARGETS atom-extra-boost DESTINATION lib)
# install(FILES ${BOOST_HEADERS} DESTINATION include/atom/extra/boost)
