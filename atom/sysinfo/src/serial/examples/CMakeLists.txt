cmake_minimum_required(VERSION 3.16)

# Example executables
set(EXAMPLES basic_usage comprehensive_info backward_compatibility
             advanced_features performance_test)

# Create example executables
foreach(EXAMPLE ${EXAMPLES})
  add_executable(${EXAMPLE} ${EXAMPLE}.cpp)

  target_link_libraries(${EXAMPLE} PRIVATE atom_sysinfo_sn)

  set_target_properties(${EXAMPLE} PROPERTIES CXX_STANDARD 20
                                              CXX_STANDARD_REQUIRED ON)

  # Include directories
  target_include_directories(${EXAMPLE} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../)
endforeach()

# Platform-specific configuration
if(WIN32)
  foreach(EXAMPLE ${EXAMPLES})
    target_compile_definitions(${EXAMPLE} PRIVATE _WIN32_WINNT=0x0601)
  endforeach()
endif()

# Install examples (optional)
option(INSTALL_EXAMPLES "Install example executables" OFF)
if(INSTALL_EXAMPLES)
  install(TARGETS ${EXAMPLES}
          RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples)

  # Install source files for reference
  install(
    FILES basic_usage.cpp comprehensive_info.cpp backward_compatibility.cpp
          advanced_features.cpp performance_test.cpp
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/examples)
endif()

# Add custom target to build all examples
add_custom_target(
  examples
  DEPENDS ${EXAMPLES}
  COMMENT "Building all examples")

# Add custom target to run all examples
add_custom_target(
  run_examples
  COMMAND echo "Running basic_usage example:"
  COMMAND basic_usage
  COMMAND echo "\\nRunning comprehensive_info example:"
  COMMAND comprehensive_info
  COMMAND echo "\\nRunning backward_compatibility example:"
  COMMAND backward_compatibility
  COMMAND echo "\\nRunning advanced_features example:"
  COMMAND advanced_features
  DEPENDS ${EXAMPLES}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMENT "Running all examples")
