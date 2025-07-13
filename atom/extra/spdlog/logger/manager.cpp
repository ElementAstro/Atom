#include "manager.h"

#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/stdout_sinks.h>

namespace modern_log {

LogManager::LogManager()
    : event_system_(std::make_unique<LogEventSystem>()),
      maintenance_thread_(&LogManager::maintenance_loop, this) {}

LogManager::~LogManager() { shutdown(); }

LogManager& LogManager::instance() {
    static LogManager instance;
    return instance;
}

Result<std::shared_ptr<Logger>> LogManager::create_logger(
    const LogConfig& config) {
    std::unique_lock lock(mutex_);
    if (loggers_.contains(config.name)) {
        return std::unexpected(LogError::invalid_config);
    }
    try {
        std::vector<spdlog::sink_ptr> sinks;
        create_default_sinks(config, sinks);
        if (sinks.empty()) {
            sinks.push_back(std::make_shared<spdlog::sinks::null_sink_mt>());
        }
        std::shared_ptr<spdlog::logger> spdlog_logger;
        if (config.async) {
            setup_async_logging(config);
            spdlog_logger = std::make_shared<spdlog::async_logger>(
                config.name, sinks.begin(), sinks.end(), spdlog::thread_pool(),
                spdlog::async_overflow_policy::block);
        } else {
            spdlog_logger = std::make_shared<spdlog::logger>(
                config.name, sinks.begin(), sinks.end());
        }
        spdlog_logger->set_level(
            static_cast<spdlog::level::level_enum>(config.level));
        spdlog_logger->flush_on(spdlog::level::err);
        spdlog::register_logger(spdlog_logger);
        auto logger =
            std::make_shared<Logger>(spdlog_logger, event_system_.get());
        loggers_[config.name] = logger;
        event_system_->emit(LogEvent::logger_created, config.name);
        return logger;
    } catch (const spdlog::spdlog_ex&) {
        return std::unexpected(LogError::file_creation_failed);
    } catch (...) {
        return std::unexpected(LogError::invalid_config);
    }
}

Result<std::shared_ptr<Logger>> LogManager::get_logger(
    const std::string& name) {
    std::shared_lock lock(mutex_);
    if (auto it = loggers_.find(name); it != loggers_.end()) {
        return it->second;
    }
    return std::unexpected(LogError::logger_not_found);
}

bool LogManager::remove_logger(const std::string& name) {
    std::unique_lock lock(mutex_);
    if (auto it = loggers_.find(name); it != loggers_.end()) {
        event_system_->emit(LogEvent::logger_destroyed, name);
        spdlog::drop(name);
        loggers_.erase(it);
        return true;
    }
    return false;
}

std::vector<std::string> LogManager::get_logger_names() const {
    std::shared_lock lock(mutex_);
    std::vector<std::string> names;
    names.reserve(loggers_.size());
    for (const auto& [name, logger] : loggers_) {
        names.push_back(name);
    }
    return names;
}

size_t LogManager::logger_count() const {
    std::shared_lock lock(mutex_);
    return loggers_.size();
}

void LogManager::flush_all() {
    std::shared_lock lock(mutex_);
    for (const auto& [name, logger] : loggers_) {
        logger->flush();
    }
    event_system_->emit(LogEvent::flush_triggered, std::string("all_loggers"));
}

void LogManager::set_global_level(Level level) {
    std::shared_lock lock(mutex_);
    for (const auto& [name, logger] : loggers_) {
        logger->set_level(level);
    }
}

void LogManager::set_archiver(std::unique_ptr<LogArchiver> archiver) {
    archiver_ = std::move(archiver);
}

Logger& LogManager::default_logger() {
    static std::shared_ptr<Logger> logger = []() {
        LogConfig config{.name = "default",
                         .level = Level::info,
                         .console_output = true,
                         .colored_output = true};
        auto result = instance().create_logger(config);
        if (!result) {
            auto fallback = std::make_shared<spdlog::logger>("fallback");
            return std::make_shared<Logger>(fallback);
        }
        return *result;
    }();
    return *logger;
}

Result<std::shared_ptr<Logger>> LogManager::create_simple_logger(
    const std::string& name, Level level, bool console) {
    LogConfig config{.name = name, .level = level, .console_output = console};
    return instance().create_logger(config);
}

Result<std::shared_ptr<Logger>> LogManager::create_file_logger(
    const std::string& name, const std::string& filename, Level level,
    bool rotating) {
    LogConfig config{.name = name,
                     .level = level,
                     .file_config = LogConfig::FileConfig{.filename = filename,
                                                          .rotating = rotating},
                     .console_output = false};
    return instance().create_logger(config);
}

Result<std::shared_ptr<Logger>> LogManager::create_async_logger(
    const std::string& name, const LogConfig& config) {
    LogConfig async_config = config;
    async_config.name = name;
    async_config.async = true;
    return instance().create_logger(async_config);
}

LogManager::GlobalStats LogManager::get_global_stats() const {
    std::shared_lock lock(mutex_);
    GlobalStats stats{};
    stats.total_loggers = loggers_.size();
    stats.start_time = std::chrono::steady_clock::now();
    size_t total_logs = 0;
    size_t total_errors = 0;
    for (const auto& [name, logger] : loggers_) {
        const auto& logger_stats = logger->get_stats();
        total_logs += logger_stats.total_logs.load();
        total_errors += logger_stats.failed_logs.load();
    }
    stats.total_logs = total_logs;
    stats.total_errors = total_errors;
    auto duration = std::chrono::steady_clock::now() - stats.start_time;
    auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    stats.avg_logs_per_second =
        seconds > 0 ? static_cast<double>(total_logs) / seconds : 0.0;
    return stats;
}

void LogManager::shutdown() {
    if (shutdown_requested_.exchange(true)) {
        return;
    }
    if (maintenance_thread_.joinable()) {
        maintenance_thread_.join();
    }
    flush_all();
    {
        std::unique_lock lock(mutex_);
        loggers_.clear();
    }
    spdlog::shutdown();
}

void LogManager::maintenance_loop() {
    while (!shutdown_requested_.load()) {
        try {
            if (archiver_) {
                archiver_->archive_old_files();
            }
            flush_all();
            std::this_thread::sleep_for(std::chrono::minutes(5));
        } catch (...) {
        }
    }
}

void LogManager::create_default_sinks(const LogConfig& config,
                                      std::vector<spdlog::sink_ptr>& sinks) {
    if (config.console_output) {
        spdlog::sink_ptr console_sink;
        if (config.colored_output) {
            console_sink =
                std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        } else {
            console_sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
        }
        console_sink->set_pattern(config.pattern);
        sinks.push_back(console_sink);
        event_system_->emit(LogEvent::sink_added, std::string("console"));
    }
    if (config.file_config) {
        const auto& file_cfg = *config.file_config;
        spdlog::sink_ptr file_sink;
        auto file_path = std::filesystem::path(file_cfg.filename);
        if (file_path.has_parent_path()) {
            std::filesystem::create_directories(file_path.parent_path());
        }
        if (file_cfg.daily_rotation) {
            file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
                file_cfg.filename, file_cfg.rotation_hour,
                file_cfg.rotation_minute);
        } else if (file_cfg.rotating) {
            file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                file_cfg.filename, file_cfg.max_size, file_cfg.max_files);
        } else {
            file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                file_cfg.filename, true);
        }
        file_sink->set_pattern(config.pattern);
        sinks.push_back(file_sink);
        event_system_->emit(LogEvent::sink_added, file_cfg.filename);
    }
}

void LogManager::setup_async_logging(const LogConfig& config) {
    if (!spdlog::get("async_tp")) {
        spdlog::init_thread_pool(config.async_queue_size,
                                 config.async_thread_count);
    }
}

}  // namespace modern_log
