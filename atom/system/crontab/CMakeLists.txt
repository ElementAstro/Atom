# CMakeLists.txt for crontab module

set(ATOM_SYSTEM_CRONTAB_SOURCES
    crontab/cron_job.cpp
    crontab/cron_validation.cpp
    crontab/cron_system.cpp
    crontab/cron_storage.cpp
    crontab/cron_manager.cpp
)

set(ATOM_SYSTEM_CRONTAB_HEADERS
    crontab/cron_job.hpp
    crontab/cron_validation.hpp
    crontab/cron_system.hpp
    crontab/cron_storage.hpp
    crontab/cron_manager.hpp
)

# Add sources to parent target
target_sources(atom-static PRIVATE ${ATOM_SYSTEM_CRONTAB_SOURCES})
target_sources(atom-shared PRIVATE ${ATOM_SYSTEM_CRONTAB_SOURCES})

# Install headers
install(FILES ${ATOM_SYSTEM_CRONTAB_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/atom/system/crontab
)
