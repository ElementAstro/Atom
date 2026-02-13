# CMakeLists.txt for Atom-Secret Core Examples

set(CORE_EXAMPLE_SOURCES error_codes_example.cpp utils_example.cpp)

foreach(example_source ${CORE_EXAMPLE_SOURCES})
  get_filename_component(example_name ${example_source} NAME_WE)
  set(example_target "secret_${example_name}")

  add_executable(${example_target} ${example_source})
  target_link_libraries(${example_target} PRIVATE atom::secret)
  target_compile_features(${example_target} PRIVATE cxx_std_20)

  set_property(TARGET ${example_target} PROPERTY FOLDER "Examples/Secret/Core")
endforeach()
