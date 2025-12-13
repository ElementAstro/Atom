# Shared Examples CMakeLists.txt

set(SHARED_EXAMPLES
    sockethub_example
    async_sockethub_example
)

foreach(example ${SHARED_EXAMPLES})
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} PRIVATE atom-connection)
    target_include_directories(${example} PRIVATE ${CMAKE_SOURCE_DIR})
    set_target_properties(${example} PROPERTIES
        FOLDER "Examples/Connection/Shared"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/examples/connection/shared"
    )
endforeach()
