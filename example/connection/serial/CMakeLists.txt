# Serial Examples CMakeLists.txt

set(SERIAL_EXAMPLES ttybase_example)

foreach(example ${SERIAL_EXAMPLES})
  add_executable(${example} ${example}.cpp)
  target_link_libraries(${example} PRIVATE atom-connection)
  target_include_directories(${example} PRIVATE ${CMAKE_SOURCE_DIR})
  set_target_properties(
    ${example}
    PROPERTIES FOLDER "Examples/Connection/Serial"
               RUNTIME_OUTPUT_DIRECTORY
               "${CMAKE_BINARY_DIR}/bin/examples/connection/serial")
endforeach()
