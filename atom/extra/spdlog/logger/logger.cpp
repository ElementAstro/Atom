#include "logger.h"

#include <stacktrace>

namespace modern_log {

Logger::Logger(std::shared_ptr<spdlog::logger> logger,
               LogEventSystem* event_system)
    : logger_(std::move(logger)),
      filter_(std::make_unique<LogFilter>()),
      sampler_(std::make_unique<LogSampler>()),
      event_system_(event_system) {
    if (event_system_) {
        emit_event(LogEvent::logger_created, logger_->name());
    }
}

void Logger::log_structured(Level level, const StructuredData& data) {
    if (!should_log_internal(level))
        return;

    auto json_str = data.to_json();
    log_internal(level, std::format("STRUCTURED: {}", json_str));
}

void Logger::log_exception(Level level, const std::exception& ex,
                           std::string_view context) {
    if (!should_log_internal(level))
        return;

    std::string message = std::format("Exception: {}", ex.what());

    if (!context.empty()) {
        message = std::format("{} | Context: {}", message, context);
    }

    try {
        auto trace = std::stacktrace::current();
        message += "\nStack trace:\n";
        for (const auto& entry : trace) {
            message += std::format("  {}\n", std::to_string(entry));
        }
    } catch (...) {
    }

    log_internal(level, message);
}

bool Logger::should_log_internal(Level level) const {
    if (!should_log(level)) {
        return false;
    }

    if (!sampler_->should_sample()) {
        stats_.sampled_logs.fetch_add(1);
        return false;
    }

    return true;
}

void Logger::log_internal(Level level, const std::string& message) {
    try {
        if (!filter_->should_log(message, level, context_)) {
            stats_.filtered_logs.fetch_add(1);
            return;
        }

        std::string enhanced_message = message;
        if (!context_.empty()) {
            enhanced_message = enrich_message_with_context(message, context_);
        }

        logger_->log(static_cast<spdlog::level::level_enum>(level),
                     enhanced_message);
        stats_.total_logs.fetch_add(1);

    } catch (...) {
        stats_.failed_logs.fetch_add(1);
        emit_event(LogEvent::error_occurred, "Log write failed");
    }
}

std::string Logger::enrich_message_with_context(const std::string& message,
                                                const LogContext& ctx) const {
    if (ctx.empty()) {
        return message;
    }

    std::string enriched = message;

    std::string context_str;
    if (!ctx.user_id().empty()) {
        context_str += std::format("user={} ", ctx.user_id());
    }
    if (!ctx.session_id().empty()) {
        context_str += std::format("session={} ", ctx.session_id());
    }
    if (!ctx.trace_id().empty()) {
        context_str += std::format("trace={} ", ctx.trace_id());
    }
    if (!ctx.request_id().empty()) {
        context_str += std::format("request={} ", ctx.request_id());
    }

    if (!context_str.empty()) {
        enriched = std::format("[{}] {}", context_str, message);
    }

    return enriched;
}

void Logger::emit_event(LogEvent event, const std::any& data) {
    if (event_system_) {
        event_system_->emit(event, data);
    }
}

}  // namespace modern_log