# SSH Examples CMakeLists.txt

# Check if libssh is available
find_package(libssh QUIET)

if(libssh_FOUND OR TARGET ssh)
  set(SSH_EXAMPLES sshclient_example sshserver_example)

  foreach(example ${SSH_EXAMPLES})
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} PRIVATE atom-connection)
    target_include_directories(${example} PRIVATE ${CMAKE_SOURCE_DIR})
    set_target_properties(
      ${example}
      PROPERTIES FOLDER "Examples/Connection/SSH"
                 RUNTIME_OUTPUT_DIRECTORY
                 "${CMAKE_BINARY_DIR}/bin/examples/connection/ssh")
  endforeach()
else()
  message(STATUS "libssh not found - SSH examples will not be built")
endif()
