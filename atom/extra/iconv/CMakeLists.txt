# CMakeLists.txt for iconv C++ wrapper
# Integrated with main Atom build system

# Header files
set(ICONV_HEADERS
    iconv_cpp.hpp
)

# Test source files (optional)
set(ICONV_TEST_SOURCES
    test_iconv_cpp.cpp
)

# Create interface library (header-only)
add_library(atom-extra-iconv INTERFACE)

# Set target properties
target_include_directories(atom-extra-iconv INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Set target properties for consistency
target_compile_features(atom-extra-iconv INTERFACE cxx_std_20)

# Find and link iconv library (optional)
find_package(Iconv QUIET)
if(Iconv_FOUND)
    target_link_libraries(atom-extra-iconv INTERFACE Iconv::Iconv)
    message(STATUS "Found Iconv, linking to atom-extra-iconv")
else()
    message(STATUS "Iconv not found, atom-extra-iconv may have limited functionality")
endif()

# Optional: Create test executable
if(BUILD_TESTING)
    add_executable(atom-extra-iconv-test ${ICONV_TEST_SOURCES})
    target_link_libraries(atom-extra-iconv-test atom-extra-iconv)
endif()

# Installation rules (optional)
# install(TARGETS atom-extra-iconv DESTINATION lib)
# install(FILES ${ICONV_HEADERS} DESTINATION include/atom/extra/iconv)
