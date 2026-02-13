# CMakeLists.txt for Locale Examples
cmake_minimum_required(VERSION 3.16)

# Define example executables
set(LOCALE_EXAMPLES basic_usage advanced_formatting validation_demo
                    configuration_example)

# Create executables for each example
foreach(EXAMPLE ${LOCALE_EXAMPLES})
  add_executable(locale_${EXAMPLE} ${EXAMPLE}.cpp)

  # Link with the locale library
  target_link_libraries(locale_${EXAMPLE} PRIVATE atom_sysinfo_locale)

  # Set target properties
  set_target_properties(
    locale_${EXAMPLE}
    PROPERTIES CXX_STANDARD 20
               CXX_STANDARD_REQUIRED ON
               CXX_EXTENSIONS OFF)

  # Compiler-specific options
  target_compile_options(
    locale_${EXAMPLE}
    PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
            $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
            $<$<CXX_COMPILER_ID:MSVC>:/W4>)
endforeach()

# Optional: Install examples
if(LOCALE_INSTALL_EXAMPLES)
  install(TARGETS ${LOCALE_EXAMPLES}
          RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/locale_examples)

  # Install source files for reference
  install(FILES basic_usage.cpp advanced_formatting.cpp validation_demo.cpp
                configuration_example.cpp
          DESTINATION ${CMAKE_INSTALL_DOCDIR}/examples)
endif()

# Add custom target to run all examples
add_custom_target(run_locale_examples COMMENT "Running all locale examples")

foreach(EXAMPLE ${LOCALE_EXAMPLES})
  add_custom_target(
    run_locale_${EXAMPLE}
    COMMAND locale_${EXAMPLE}
    DEPENDS locale_${EXAMPLE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running locale ${EXAMPLE} example")

  add_dependencies(run_locale_examples run_locale_${EXAMPLE})
endforeach()

message(
  STATUS "Configured ${CMAKE_CURRENT_LIST_DIR} with ${LOCALE_EXAMPLES} examples"
)
