# CMakeLists.txt for modern logging library (spdlog wrapper)
# Integrated with main Atom build system

# Source files
set(SPDLOG_SOURCES
    core/context.cpp
    filters/filter.cpp
    filters/builtin_filters.cpp
    sampling/sampler.cpp
    events/event_system.cpp
    utils/structured_data.cpp
    utils/timer.cpp
    utils/archiver.cpp
    logger/logger.cpp
    logger/manager.cpp
)

# Header files (main header)
set(SPDLOG_HEADERS
    modern_log.h
)

# Create library
add_library(atom-extra-spdlog ${SPDLOG_SOURCES} ${SPDLOG_HEADERS})

# Set target properties
target_include_directories(atom-extra-spdlog PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Set target properties for consistency
set_target_properties(atom-extra-spdlog PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Find and link required packages (optional)
find_package(spdlog QUIET)
find_package(fmt QUIET)

if(spdlog_FOUND AND fmt_FOUND)
    target_link_libraries(atom-extra-spdlog
        PUBLIC
            spdlog::spdlog
            fmt::fmt
    )
    message(STATUS "Found spdlog and fmt, linking to atom-extra-spdlog")
else()
    message(STATUS "spdlog or fmt not found, atom-extra-spdlog may have limited functionality")
endif()

# Installation rules (optional)
# install(TARGETS atom-extra-spdlog DESTINATION lib)
# install(FILES ${SPDLOG_HEADERS} DESTINATION include/atom/extra/spdlog)