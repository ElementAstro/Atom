# Network test sources Note: test_network_manager.cpp disabled -
# GetNetworkConnections can hang
set(SYSTEM_NETWORK_TEST_SOURCES
    # ${CMAKE_CURRENT_SOURCE_DIR}/test_network_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_virtual_network.cpp
    PARENT_SCOPE)
