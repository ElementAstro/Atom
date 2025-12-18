# TCP Examples CMakeLists.txt

set(TCP_EXAMPLES tcpclient_example async_tcpclient_example)

foreach(example ${TCP_EXAMPLES})
  add_executable(${example} ${example}.cpp)
  target_link_libraries(${example} PRIVATE atom-connection)
  target_include_directories(${example} PRIVATE ${CMAKE_SOURCE_DIR})
  set_target_properties(
    ${example}
    PROPERTIES FOLDER "Examples/Connection/TCP"
               RUNTIME_OUTPUT_DIRECTORY
               "${CMAKE_BINARY_DIR}/bin/examples/connection/tcp")
endforeach()
