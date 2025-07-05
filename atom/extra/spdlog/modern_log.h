#pragma once

// **主入口头文件**
#include "core/concepts.h"
#include "core/types.h"
#include "core/context.h"
#include "core/error.h"
#include "filters/filter.h"
#include "filters/builtin_filters.h"
#include "sampling/sampler.h"
#include "events/event_system.h"
#include "utils/structured_data.h"
#include "utils/timer.h"
#include "utils/archiver.h"
#include "logger/logger.h"
#include "logger/manager.h"

// **便利宏定义**
#define LOG_TRACE(...) modern_log::LogManager::default_logger().trace(__VA_ARGS__)
#define LOG_DEBUG(...) modern_log::LogManager::default_logger().debug(__VA_ARGS__)
#define LOG_INFO(...)  modern_log::LogManager::default_logger().info(__VA_ARGS__)
#define LOG_WARN(...)  modern_log::LogManager::default_logger().warn(__VA_ARGS__)
#define LOG_ERROR(...) modern_log::LogManager::default_logger().error(__VA_ARGS__)
#define LOG_CRITICAL(...) modern_log::LogManager::default_logger().critical(__VA_ARGS__)

#define LOG_TIME_SCOPE(name) auto _timer = modern_log::LogManager::default_logger().time_scope(name)

#define LOG_WITH_CONTEXT(ctx) modern_log::LogManager::default_logger().with_context(ctx)
