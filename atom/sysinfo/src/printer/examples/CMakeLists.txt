# CMakeLists.txt for sysinfo_printer examples
cmake_minimum_required(VERSION 3.20)

# Example executables
set(EXAMPLE_SOURCES basic_usage.cpp custom_formatting.cpp export_examples.cpp
                    advanced_reports.cpp)

# Create example executables
foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
  get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
  add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE})

  target_link_libraries(${EXAMPLE_NAME} PRIVATE atom_sysinfo_printer
                                                spdlog::spdlog)

  set_target_properties(
    ${EXAMPLE_NAME}
    PROPERTIES CXX_STANDARD 20
               CXX_STANDARD_REQUIRED ON
               CXX_EXTENSIONS OFF)

  # Install examples
  install(TARGETS ${EXAMPLE_NAME} RUNTIME DESTINATION bin/examples)
endforeach()

# Install example source files
install(FILES ${EXAMPLE_SOURCES}
        DESTINATION share/atom/sysinfo_printer/examples)
