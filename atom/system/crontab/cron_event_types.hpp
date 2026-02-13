/*
 * cron_event_types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef CRON_EVENT_TYPES_HPP
#define CRON_EVENT_TYPES_HPP

#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>

/**
 * @brief Event types for cron system monitoring
 */
enum class CronEventType {
    JOB_CREATED,
    JOB_UPDATED,
    JOB_DELETED,
    JOB_STARTED,
    JOB_COMPLETED,
    JOB_FAILED,
    JOB_TIMEOUT,
    JOB_SKIPPED,
    SCHEDULER_STARTED,
    SCHEDULER_STOPPED,
    SYSTEM_ERROR,
    SECURITY_VIOLATION,
    RESOURCE_LIMIT_EXCEEDED,
    CACHE_OPERATION,
    HEALTH_CHECK_FAILED
};

/**
 * @brief Cron system event
 */
struct CronEvent {
    CronEventType type;
    std::string jobId;
    std::string userId;
    std::string details;
    std::chrono::steady_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> metadata;

    CronEvent(CronEventType t, const std::string& jid = "",
              const std::string& uid = "", const std::string& d = "")
        : type(t), jobId(jid), userId(uid), details(d),
          timestamp(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Event callback function type
 */
using CronEventCallback = std::function<void(const CronEvent& event)>;

// Forward declaration for CronConfigManager
class CronConfigManager;

/**
 * @brief RAII class for measuring execution time
 */
class CronExecutionTimer {
public:
    explicit CronExecutionTimer(CronEventType type,
                                const std::string& jobId = "")
        : type_(type), jobId_(jobId),
          startTime_(std::chrono::steady_clock::now()) {}

    ~CronExecutionTimer();

private:
    CronEventType type_;
    std::string jobId_;
    std::chrono::steady_clock::time_point startTime_;
};

// Convenience macros for event operations
#define CRON_EMIT_EVENT(type, jobId, userId, details) \
    CronConfigManager::getInstance().emitEvent(       \
        CronEvent(type, jobId, userId, details))

#define CRON_TIMER(type, jobId) CronExecutionTimer timer(type, jobId)

#endif  // CRON_EVENT_TYPES_HPP
