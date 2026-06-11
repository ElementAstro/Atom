# CMakeLists.txt for Atom-System This project is licensed under the terms of the
# GPL3 license.
#
# Project Name: Atom-System Description: A collection of useful system functions
# Author: Max Qian License: GPL3

cmake_minimum_required(VERSION 3.21)
project(
  atom-system
  VERSION 1.0.0
  LANGUAGES C CXX)

# Include standardized module configuration
include(${CMAKE_SOURCE_DIR}/cmake/ModuleDependencies.cmake)

find_package(PkgConfig QUIET)

if(PKG_CONFIG_FOUND)
  pkg_check_modules(LIBUSB QUIET libusb-1.0)

  if(LIBUSB_FOUND)
    message(STATUS "Found libusb-1.0: ${LIBUSB_VERSION}")
  else()
    message(STATUS "libusb-1.0 not found - USB device support will be disabled")
  endif()
else()
  message(STATUS "PkgConfig not found - USB device support will be disabled")
endif()

# Sources and Headers
set(SOURCES
    # Core files
    core/priority.cpp
    # Process management
    process/pidwatcher.cpp
    process/process_manager.cpp
    process/process.cpp
    # Command execution - modular implementation (replaces the flat
    # process/command.cpp; process/command.hpp now forwards to command/).
    command/executor.cpp
    command/async_executor.cpp
    command/process_manager.cpp
    command/history.cpp
    command/utils.cpp
    command/validation.cpp
    command/config.cpp
    command/cache.cpp
    command/security.cpp
    command/statistics.cpp
    command/rate_limiter.cpp
    command/resource_monitor.cpp
    command/thread_pool.cpp
    # Crontab facade (depends on the command subsystem above)
    crontab/cron_system.cpp
    # Hardware
    hardware/device.cpp
    hardware/gpio.cpp
    hardware/voltage.cpp
    # Power management
    power/power.cpp
    # System information
    info/env.cpp
    info/software.cpp
    info/stat.cpp
    info/user.cpp
    # Environment - modular components layered behind the info/env.cpp Env
    # facade (caching, async, encryption, config, scoped, persistence).
    # env_example.cpp is a standalone demo (has main()) and is excluded.
    env/env_core.cpp
    env/env_config.cpp
    env/env_cache.cpp
    env/env_async.cpp
    env/env_manager.cpp
    env/env_file_io.cpp
    env/env_path.cpp
    env/env_persistent.cpp
    env/env_scoped.cpp
    env/env_system.cpp
    env/env_utils.cpp
    # Registry
    registry/lregistry.cpp
    registry/wregistry.cpp
    # Network
    network/network_manager.cpp
    network/virtual_network.cpp
    # Storage
    storage/storage.cpp
    # Signals
    signals/signal.cpp
    # Debug
    debug/crash_quotes.cpp
    debug/crash.cpp
    debug/nodebugger.cpp
    # Scheduling (crontab) - modular implementation. cron_system.cpp (the facade
    # tying crontab to the command subsystem) is intentionally omitted until the
    # command subsystem is adopted into the build.
    crontab/cron_job.cpp
    crontab/cron_config.cpp
    crontab/cron_cache.cpp
    crontab/cron_validation.cpp
    crontab/cron_storage.cpp
    crontab/cron_thread_pool.cpp
    crontab/cron_security.cpp
    crontab/cron_monitor.cpp
    crontab/cron_scheduler.cpp
    crontab/cron_manager.cpp
    crontab/cron_manager_batch.cpp
    crontab/cron_manager_query.cpp
    # Clipboard (cross-platform core; platform backend appended below)
    clipboard/clipboard.cpp
    clipboard/clipboard_text.cpp
    clipboard/clipboard_data.cpp
    clipboard/clipboard_image.cpp
    clipboard/clipboard_query.cpp
    clipboard/clipboard_monitor.cpp
    clipboard/clipboard_cache.cpp
    clipboard/clipboard_async.cpp)

# Platform-specific sources
if(WIN32)
  list(APPEND SOURCES clipboard/platform/clipboard_windows.cpp
       hardware/voltage_windows.cpp)
elseif(UNIX AND NOT APPLE)
  list(APPEND SOURCES clipboard/platform/clipboard_linux.cpp
       hardware/voltage_linux.cpp)
elseif(APPLE)
  list(APPEND SOURCES clipboard/platform/clipboard_macos.cpp)
endif()

set(HEADERS
    # Backwards compatibility headers (in root)
    _constant.hpp
    clipboard.hpp
    clipboard.ipp
    clipboard_error.hpp
    command.hpp
    crash_quotes.hpp
    crash.hpp
    crontab.hpp
    device.hpp
    env.hpp
    gpio.hpp
    lregistry.hpp
    network_manager.hpp
    nodebugger.hpp
    pidwatcher.hpp
    platform.hpp
    power.hpp
    priority.hpp
    process.hpp
    process_info.hpp
    process_manager.hpp
    signal.hpp
    signal_monitor.hpp
    signal_utils.hpp
    software.hpp
    stat.hpp
    storage.hpp
    user.hpp
    virtual_network.hpp
    voltage.hpp
    voltage_linux.hpp
    voltage_windows.hpp
    wregistry.hpp
    # Actual implementation headers (in subdirectories)
    core/_constant.hpp
    core/platform.hpp
    core/priority.hpp
    process/command.hpp
    process/pidwatcher.hpp
    process/process.hpp
    process/process_info.hpp
    process/process_manager.hpp
    # Command execution (modular)
    command/command.hpp
    command/types.hpp
    command/executor.hpp
    command/async_executor.hpp
    command/process_manager.hpp
    command/history.hpp
    command/utils.hpp
    command/validation.hpp
    command/config.hpp
    command/cache.hpp
    command/security.hpp
    command/statistics.hpp
    command/rate_limiter.hpp
    command/resource_monitor.hpp
    command/thread_pool.hpp
    crontab/cron_system.hpp
    hardware/device.hpp
    hardware/gpio.hpp
    hardware/voltage.hpp
    hardware/voltage_linux.hpp
    hardware/voltage_windows.hpp
    power/power.hpp
    info/env.hpp
    info/software.hpp
    info/stat.hpp
    info/user.hpp
    # Environment modular components
    env/env_core.hpp
    env/env_config.hpp
    env/env_cache.hpp
    env/env_async.hpp
    env/env_manager.hpp
    env/env_file_io.hpp
    env/env_path.hpp
    env/env_persistent.hpp
    env/env_scoped.hpp
    env/env_system.hpp
    env/env_utils.hpp
    registry/lregistry.hpp
    registry/wregistry.hpp
    network/network_manager.hpp
    network/virtual_network.hpp
    storage/storage.hpp
    clipboard/clipboard.hpp
    clipboard/clipboard_error.hpp
    clipboard/clipboard_types.hpp
    clipboard/clipboard_result.hpp
    clipboard/clipboard_config.hpp
    clipboard/clipboard_utils.hpp
    clipboard/clipboard_impl.hpp
    signals/signal.hpp
    signals/signal_monitor.hpp
    signals/signal_utils.hpp
    debug/crash.hpp
    debug/crash_quotes.hpp
    debug/nodebugger.hpp
    scheduling/crontab.hpp
    # Crontab (modular implementation)
    crontab/cron_job.hpp
    crontab/cron_types.hpp
    crontab/cron_config.hpp
    crontab/cron_cache.hpp
    crontab/cron_validation.hpp
    crontab/cron_storage.hpp
    crontab/cron_thread_pool.hpp
    crontab/cron_security.hpp
    crontab/cron_security_types.hpp
    crontab/cron_monitor.hpp
    crontab/cron_monitor_types.hpp
    crontab/cron_metrics_types.hpp
    crontab/cron_event_types.hpp
    crontab/cron_scheduler.hpp
    crontab/cron_manager.hpp
    crontab/cron_manager_batch.hpp
    crontab/cron_manager_query.hpp
    crontab/cron_manager_types.hpp)

find_package(spdlog QUIET)
set(LIBS ${CMAKE_THREAD_LIBS_INIT} atom-sysinfo atom-meta atom-utils
         ${LIBUSB_LIBRARIES})
if(spdlog_FOUND)
  list(APPEND LIBS spdlog::spdlog)
endif()

# Create library target
add_library(atom-system STATIC ${SOURCES} ${HEADERS})
add_library(atom::system ALIAS atom-system)

# Configure module using standardized function
atom_configure_module(atom-system)

# Link module-specific dependencies
target_link_libraries(atom-system PRIVATE ${LIBS})

# Platform-specific libraries
if(WIN32)
  target_link_libraries(
    atom-system
    PRIVATE pdh
            wlanapi
            userenv
            version
            advapi32
            hid
            setupapi
            user32
            ole32
            oleaut32
            psapi
            dbghelp)
endif()

# Add shortcut subdirectory
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/shortcut/CMakeLists.txt")
  add_subdirectory(shortcut)
endif()

# Install headers
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/system)
