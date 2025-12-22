/*
 * global_handler.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Global error handler implementation

**************************************************/

#include "global_handler.hpp"

#include <format>
#include <iostream>

#include "../core/error_codes.hpp"

namespace atom::error {

auto GlobalErrorHandler::getInstance() -> GlobalErrorHandler& {
    static GlobalErrorHandler instance;
    return instance;
}

void GlobalErrorHandler::initialize() {
    std::scoped_lock lock(initMutex_);
    if (!initialized_.load()) {
        reporter_ = std::make_unique<ErrorReporter>();
        aggregator_ = std::make_unique<ErrorAggregator>();

        // Add default handler that forwards to aggregator
        reporter_->addHandler("aggregator",
                              [this](std::shared_ptr<ErrorContext> context) {
                                  aggregator_->addError(std::move(context));
                              });

        reporter_->start();
        initialized_.store(true);
    }
}

void GlobalErrorHandler::shutdown() {
    std::scoped_lock lock(initMutex_);
    if (initialized_.load()) {
        globalHandler_ = nullptr;
        if (reporter_) {
            reporter_->stop();
            reporter_.reset();
        }
        aggregator_.reset();
        initialized_.store(false);
    }
}

void GlobalErrorHandler::reportError(std::shared_ptr<ErrorContext> context) {
    if (!initialized_.load()) [[unlikely]] {
        initialize();
    }

    if (reporter_) [[likely]] {
        reporter_->reportError(context);
    }

    if (globalHandler_) {
        try {
            globalHandler_(context);
        } catch (const std::exception& e) {
            std::cerr << std::format("Error in global handler: {}\n", e.what());
        }
    }
}

auto GlobalErrorHandler::getReporter() -> ErrorReporter& {
    if (!initialized_.load()) {
        initialize();
    }
    return *reporter_;
}

auto GlobalErrorHandler::getAggregator() -> ErrorAggregator& {
    if (!initialized_.load()) {
        initialize();
    }
    return *aggregator_;
}

void GlobalErrorHandler::setGlobalHandler(ErrorHandlerCallback handler) {
    globalHandler_ = std::move(handler);
}

void GlobalErrorHandler::setUnhandledExceptionHandler() {
    std::set_terminate([]() {
        GlobalErrorHandler::getInstance().handleUnhandledException();
        std::abort();
    });
}

void GlobalErrorHandler::handleUnhandledException() {
    try {
        auto context =
            ErrorContext::create(static_cast<int>(ErrorCodeBase::Failed),
                                 "Unhandled exception occurred");
        context->addTag("unhandled_exception")
            .setSystemInfo("severity", "fatal");

        reportError(std::move(context));
    } catch (...) {
        std::cerr << "Fatal: Unhandled exception in error handler\n";
    }
}

// ThreadLocalErrorHandler implementation
thread_local std::unique_ptr<ThreadLocalErrorHandler> tlsErrorHandler;

ThreadLocalErrorHandler::ThreadLocalErrorHandler()
    : errorCount_(0), startTime_(std::chrono::steady_clock::now()) {}

ThreadLocalErrorHandler::~ThreadLocalErrorHandler() = default;

void ThreadLocalErrorHandler::setHandler(ErrorHandlerCallback handler) {
    handler_ = std::move(handler);
}

void ThreadLocalErrorHandler::reportError(
    std::shared_ptr<ErrorContext> context) {
    ++errorCount_;

    if (handler_) {
        try {
            handler_(context);
        } catch (const std::exception& e) {
            std::cerr << std::format("Error in thread-local handler: {}\n",
                                     e.what());
        }
    }

    // Also report to global handler
    GlobalErrorHandler::getInstance().reportError(std::move(context));
}

auto ThreadLocalErrorHandler::getStatistics() const
    -> std::unordered_map<std::string, int> {
    std::unordered_map<std::string, int> stats;
    stats.emplace("thread_error_count", errorCount_.load());

    const auto duration = std::chrono::steady_clock::now() - startTime_;
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    stats.emplace("thread_uptime_seconds", static_cast<int>(seconds));

    return stats;
}

}  // namespace atom::error
