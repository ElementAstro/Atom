# CMakeLists.txt for Boost utility headers Integrated with main Atom build
# system Header-only library

# Policy: allow legacy FindBoost module without dev warnings in this optional
# component
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()

# Check for Boost dependencies
find_package(Boost QUIET)

if(Boost_FOUND)
  # Header files
  set(BOOST_HEADERS charconv.hpp locale.hpp math.hpp regex.hpp system.hpp
                    uuid.hpp)

  # Create interface library (header-only)
  add_library(atom-extra-boost INTERFACE)

  # Set target properties
  target_include_directories(atom-extra-boost
                             INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_features(atom-extra-boost INTERFACE cxx_std_20)
  target_link_libraries(atom-extra-boost INTERFACE Boost::boost)

  # Installation rules (optional) install(TARGETS atom-extra-boost DESTINATION
  # lib) install(FILES ${BOOST_HEADERS} DESTINATION include/atom/extra/boost)
else()
  message(WARNING "Boost not found - skipping Boost extra module")
endif()
