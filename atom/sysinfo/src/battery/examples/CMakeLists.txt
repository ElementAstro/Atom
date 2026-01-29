cmake_minimum_required(VERSION 3.20)

# Find required packages
find_package(spdlog REQUIRED)

# Example programs
set(EXAMPLES
    basic_battery_info
    battery_monitoring
    battery_manager_demo
    power_plan_control
    battery_calibration
    thermal_management
    adaptive_power)

foreach(EXAMPLE ${EXAMPLES})
  add_executable(${EXAMPLE} ${EXAMPLE}.cpp)
  target_link_libraries(${EXAMPLE} PRIVATE atom_sysinfo_battery spdlog::spdlog)
  target_compile_features(${EXAMPLE} PRIVATE cxx_std_20)

  # Set output directory
  set_target_properties(${EXAMPLE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                              ${CMAKE_BINARY_DIR}/examples)
endforeach()

# Install examples
install(TARGETS ${EXAMPLES} RUNTIME DESTINATION bin/examples)
