cmake_minimum_required(VERSION 3.21)
project(
  atom-async
  VERSION 1.0.0
  LANGUAGES C CXX)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

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
    # Execution files
    execution/async_executor.cpp)

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
    utils/timer.hpp)

set(LIBS loguru atom-utils ${CMAKE_THREAD_LIBS_INIT})

# Create library target
add_library(atom-async STATIC ${SOURCES} ${HEADERS})

# Configure module using standardized function
atom_configure_module(atom-async)

# Link module-specific dependencies
target_link_libraries(atom-async PRIVATE ${LIBS})

# Install headers
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/async)
