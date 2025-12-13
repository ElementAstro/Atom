# FIFO Examples CMakeLists.txt

set(FIFO_EXAMPLES
    fifoclient_example
    fifoserver_example
    async_fifoclient_example
    async_fifoserver_example
)

foreach(example ${FIFO_EXAMPLES})
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} PRIVATE atom-connection)
    target_include_directories(${example} PRIVATE ${CMAKE_SOURCE_DIR})
    set_target_properties(${example} PROPERTIES
        FOLDER "Examples/Connection/FIFO"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/examples/connection/fifo"
    )
endforeach()
