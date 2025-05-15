cmake_minimum_required(VERSION 3.20)
project(
  atom-async
  VERSION 1.0.0
  LANGUAGES C CXX)

# Sources
set(SOURCES daemon.cpp limiter.cpp lock.cpp timer.cpp)

# Headers
set(HEADERS
    async.hpp
    daemon.hpp
    eventstack.hpp
    limiter.hpp
    lock.hpp
    message_bus.hpp
    message_queue.hpp
    pool.hpp
    queue.hpp
    safetype.hpp
    thread_wrapper.hpp
    timer.hpp
    trigger.hpp)

set(LIBS loguru atom-utils ${CMAKE_THREAD_LIBS_INIT})

# Build Object Library
add_library(${PROJECT_NAME}_object OBJECT ${SOURCES} ${HEADERS})
set_property(TARGET ${PROJECT_NAME}_object PROPERTY POSITION_INDEPENDENT_CODE 1)

target_link_libraries(${PROJECT_NAME}_object PRIVATE ${LIBS})

# Build Static Library
add_library(${PROJECT_NAME} STATIC)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_object ${LIBS})
target_include_directories(${PROJECT_NAME} PUBLIC .)

set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR}
             OUTPUT_NAME ${PROJECT_NAME})

install(TARGETS ${PROJECT_NAME} ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
