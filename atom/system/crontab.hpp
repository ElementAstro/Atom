#ifndef CRONTAB_HPP
#define CRONTAB_HPP

// Main header that includes all crontab components
// This maintains the same interface as the original crontab.hpp

#include "crontab/cron_job.hpp"
#include "crontab/cron_manager.hpp"
#include "crontab/cron_validation.hpp"

// Re-export types for backward compatibility
using CronJob = ::CronJob;
using CronValidationResult = ::CronValidationResult;
using CronManager = ::CronManager;

#endif  // CRONTAB_HPP