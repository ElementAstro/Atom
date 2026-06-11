/**
 * @file scheduling/crontab.hpp
 * @brief Backwards compatibility header for cron-like scheduling.
 *
 * The crontab implementation now lives in the modular `atom/system/crontab/`
 * tree (CronManager, CronJob, schedulers, monitoring, security, etc.). This
 * header is kept so existing `#include "atom/system/scheduling/crontab.hpp"`
 * call sites continue to resolve to the consolidated implementation.
 */

#ifndef ATOM_SYSTEM_SCHEDULING_CRONTAB_HPP
#define ATOM_SYSTEM_SCHEDULING_CRONTAB_HPP

#include "atom/system/crontab/cron_job.hpp"
#include "atom/system/crontab/cron_manager.hpp"
#include "atom/system/crontab/cron_validation.hpp"

#endif  // ATOM_SYSTEM_SCHEDULING_CRONTAB_HPP
