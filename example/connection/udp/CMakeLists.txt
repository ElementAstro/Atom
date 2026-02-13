# UDP Examples CMakeLists.txt NOTE: udpserver_example disabled due to incomplete
# type issue with UdpSocketHub::Impl udpclient_example builds successfully

set(UDP_EXAMPLES udpclient_example)

foreach(example ${UDP_EXAMPLES})
  add_executable(${example} ${example}.cpp)
  target_link_libraries(${example} PRIVATE atom-connection)
  target_include_directories(${example} PRIVATE ${CMAKE_SOURCE_DIR})
  set_target_properties(
    ${example}
    PROPERTIES FOLDER "Examples/Connection/UDP"
               RUNTIME_OUTPUT_DIRECTORY
               "${CMAKE_BINARY_DIR}/bin/examples/connection/udp")
endforeach()
