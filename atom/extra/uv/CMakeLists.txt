# CMakeLists.txt for libuv extensions Integrated with main Atom build system

# Source files
set(UV_SOURCES message_bus.cpp subprocess.cpp)

# Header files
set(UV_HEADERS coro.hpp message_bus.hpp subprocess.hpp)

# Create library
add_library(atom-extra-uv ${UV_SOURCES} ${UV_HEADERS})

# Set target properties
target_include_directories(atom-extra-uv PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Set target properties for consistency (C++23 required for std::expected)
set_target_properties(
  atom-extra-uv
  PROPERTIES CXX_STANDARD 23
             CXX_STANDARD_REQUIRED ON
             CXX_EXTENSIONS OFF)

# Find and link required libraries
find_package(Threads REQUIRED)
target_link_libraries(atom-extra-uv PUBLIC Threads::Threads)

# Find and link libuv library (prefer CMake config packages from vcpkg)
find_package(unofficial-libuv CONFIG QUIET)
if(TARGET unofficial-libuv::uv)
  target_link_libraries(atom-extra-uv PUBLIC unofficial-libuv::uv)
  message(STATUS "Found libuv via CMake config (unofficial-libuv::uv)")
else()
  # Fallback to pkg-config or manual discovery
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(LIBUV QUIET libuv)
  endif()

  if(LIBUV_FOUND)
    target_include_directories(atom-extra-uv PUBLIC ${LIBUV_INCLUDE_DIRS})
    target_link_libraries(atom-extra-uv PUBLIC ${LIBUV_LIBRARIES})
    target_compile_options(atom-extra-uv PUBLIC ${LIBUV_CFLAGS_OTHER})
    message(STATUS "Found libuv via pkg-config, linking to atom-extra-uv")
  else()
    # Try to find libuv using find_library as fallback
    find_library(LIBUV_LIBRARY NAMES uv libuv)
    find_path(LIBUV_INCLUDE_DIR NAMES uv.h)

    if(LIBUV_LIBRARY AND LIBUV_INCLUDE_DIR)
      target_include_directories(atom-extra-uv PUBLIC ${LIBUV_INCLUDE_DIR})
      target_link_libraries(atom-extra-uv PUBLIC ${LIBUV_LIBRARY})
      message(STATUS "Found libuv library: ${LIBUV_LIBRARY}")
    else()
      message(WARNING "libuv not found - atom-extra-uv may have linking issues")
      message(STATUS "Please install libuv development package:")
      message(STATUS "  Ubuntu/Debian: sudo apt-get install libuv1-dev")
      message(STATUS "  CentOS/RHEL: sudo yum install libuv-devel")
      message(STATUS "  Windows: vcpkg install libuv")
    endif()
  endif()
endif()

# Installation rules (optional) install(TARGETS atom-extra-uv DESTINATION lib)
# install(FILES ${UV_HEADERS} DESTINATION include/atom/extra/uv)
