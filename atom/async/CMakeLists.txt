cmake_minimum_required(VERSION 3.20)
project(
  atom-async
  VERSION 1.0.0
  LANGUAGES C CXX)

# Sources
set(SOURCES
    # Core files
    core/promise.cpp

    # Threading files
    threading/lock.cpp

    # Synchronization files
    sync/limiter.cpp

    # Utility files
    utils/timer.cpp
)

# Headers
set(HEADERS
    # Backwards compatibility headers (in root)
    async.hpp
    async_executor.hpp
    daemon.hpp
    eventstack.hpp
    future.hpp
    generator.hpp
    limiter.hpp
    lock.hpp
    lodash.hpp
    message_bus.hpp
    message_queue.hpp
    packaged_task.hpp
    parallel.hpp
    pool.hpp
    promise.hpp
    queue.hpp
    safetype.hpp
    slot.hpp
    thread_wrapper.hpp
    threadlocal.hpp
    timer.hpp
    trigger.hpp

    # Actual implementation headers (in subdirectories)
    core/async.hpp
    core/future.hpp
    core/promise.hpp
    core/promise_awaiter.hpp
    core/promise_fwd.hpp
    core/promise_impl.hpp
    core/promise_utils.hpp
    core/promise_void_impl.hpp

    threading/lock.hpp
    threading/thread_wrapper.hpp
    threading/threadlocal.hpp

    messaging/eventstack.hpp
    messaging/message_bus.hpp
    messaging/message_queue.hpp
    messaging/queue.hpp

    execution/async_executor.hpp
    execution/packaged_task.hpp
    execution/parallel.hpp
    execution/pool.hpp

    sync/limiter.hpp
    sync/safetype.hpp
    sync/slot.hpp
    sync/trigger.hpp

    utils/daemon.hpp
    utils/generator.hpp
    utils/lodash.hpp
    utils/timer.hpp
)

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

# Register this module to the global target list
get_property(ATOM_MODULE_TARGETS GLOBAL PROPERTY ATOM_MODULE_TARGETS)
list(APPEND ATOM_MODULE_TARGETS ${PROJECT_NAME})
set_property(GLOBAL PROPERTY ATOM_MODULE_TARGETS "${ATOM_MODULE_TARGETS}")
