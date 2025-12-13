# UDP Examples CMakeLists.txt

set(UDP_EXAMPLES
    udpclient_example
    udpserver_example
)

foreach(example ${UDP_EXAMPLES})
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} PRIVATE atom-connection)
    target_include_directories(${example} PRIVATE ${CMAKE_SOURCE_DIR})
    set_target_properties(${example} PROPERTIES
        FOLDER "Examples/Connection/UDP"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/examples/connection/udp"
    )
endforeach()
