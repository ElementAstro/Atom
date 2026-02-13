# CMakeLists.txt for Dependency Injection library Integrated with main Atom
# build system Header-only library

# Header files
set(INJECTION_HEADERS
    all.hpp
    binding.hpp
    common.hpp
    container.hpp
    inject.hpp
    resolver.hpp
    test_all.hpp)

# Create interface library (header-only)
add_library(atom-extra-injection INTERFACE)

# Set target properties
target_include_directories(atom-extra-injection
                           INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Set target properties for consistency
target_compile_features(atom-extra-injection INTERFACE cxx_std_20)

# Installation rules (optional) install(TARGETS atom-extra-injection DESTINATION
# lib) install(FILES ${INJECTION_HEADERS} DESTINATION
# include/atom/extra/injection)
