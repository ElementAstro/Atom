// async_logger.cpp
/*
 * async_logger.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-5-4

Description: High-Performance Asynchronous Logger Implementation using C++20
Coroutines

**************************************************/

#include "async_logger.hpp"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace atom::log {

// Memory pool for efficient task allocation
struct LoggerMemoryPool {
    static constexpr size_t BLOCK_SIZE = 4096;  // 4KB blocks
    static constexpr size_t MAX_BLOCKS = 1024;  // Max 4MB total

    // Thread-safe singleton access
    static LoggerMemoryPool& instance() {
        static LoggerMemoryPool pool;
        return pool;
    }

    // Get memory resource
    std::pmr::memory_resource* resource() { return &pool_resource_; }

private:
    // Monotonic buffer resource backed by a synchronized pool
    std::pmr::synchronized_pool_resource pool_resource_{
        std::pmr::pool_options{BLOCK_SIZE, MAX_BLOCKS},
        std::pmr::new_delete_resource()};

    // Private constructor for singleton
    LoggerMemoryPool() = default;
};

// Lock-free queue node for log tasks
struct alignas(64) LogTaskNode {  // Cache line alignment
    LogLevel level;
    std::string message;
    std::source_location location;
    std::coroutine_handle<> continuation;
    std::atomic<LogTaskNode*> next{nullptr};

    LogTaskNode(LogLevel lvl, std::string msg, const std::source_location& loc,
                std::coroutine_handle<> cont)
        : level(lvl),
          message(std::move(msg)),
          location(loc),
          continuation(cont) {}
};

// Lock-free MPSC (Multiple Producer Single Consumer) queue for high throughput
class LockFreeTaskQueue {
public:
    LockFreeTaskQueue()
        : head_(new LogTaskNode(LogLevel::INFO, "",
                                std::source_location::current(), nullptr)) {
        tail_.store(head_.load(std::memory_order_relaxed),
                    std::memory_order_relaxed);
    }

    ~LockFreeTaskQueue() {
        // Clean up remaining nodes
        LogTaskNode* current = head_.load(std::memory_order_relaxed);
        while (current) {
            LogTaskNode* next = current->next.load(std::memory_order_relaxed);
            delete current;
            current = next;
        }
    }

    // Thread-safe enqueue operation for producers
    void enqueue(LogLevel level, std::string message,
                 const std::source_location& location,
                 std::coroutine_handle<> continuation) {
        // Create new node
        LogTaskNode* node =
            new LogTaskNode(level, std::move(message), location, continuation);

        // Add to queue with memory ordering guarantees
        LogTaskNode* prev = head_.exchange(node, std::memory_order_acq_rel);
        prev->next.store(node, std::memory_order_release);
    }

    // Thread-safe dequeue operation for consumer
    bool dequeue(LogLevel& level, std::string& message,
                 std::source_location& location,
                 std::coroutine_handle<>& continuation) {
        LogTaskNode* tail = tail_.load(std::memory_order_relaxed);
        LogTaskNode* next = tail->next.load(std::memory_order_acquire);

        if (!next) {
            return false;  // Queue is empty
        }

        // Save task data
        level = next->level;
        message = std::move(next->message);
        location = next->location;
        continuation = next->continuation;

        // Update tail and free the old node
        tail_.store(next, std::memory_order_release);
        delete tail;
        return true;
    }

    // Check if queue is empty
    bool empty() const {
        LogTaskNode* tail = tail_.load(std::memory_order_relaxed);
        LogTaskNode* next = tail->next.load(std::memory_order_acquire);
        return next == nullptr;
    }

private:
    alignas(64) std::atomic<LogTaskNode*> head_;  // Cache line alignment
    alignas(64) std::atomic<LogTaskNode*> tail_;  // Cache line alignment
};

// Asynchronous logger implementation class
class AsyncLogger::AsyncLoggerImpl {
public:
    AsyncLoggerImpl(const fs::path& file_name, LogLevel min_level,
                    size_t max_file_size, int max_files,
                    size_t thread_pool_size)
        : logger_(std::make_shared<Logger>(file_name, min_level, max_file_size,
                                           max_files)),
          shutdown_(false),
          active_workers_(0) {
        // Initialize worker thread pool
        workers_.reserve(thread_pool_size);  // Pre-allocate vector space
        for (size_t i = 0; i < thread_pool_size; ++i) {
            workers_.emplace_back([this](std::stop_token stoken) {
                // Increment active worker count
                active_workers_.fetch_add(1, std::memory_order_relaxed);
                workerLoop(stoken);
                // Decrement active worker count
                active_workers_.fetch_sub(1, std::memory_order_relaxed);
            });
        }
    }

    ~AsyncLoggerImpl() {
        // Mark shutdown and notify all threads
        shutdown_.store(true, std::memory_order_release);
        cv_.notify_all();

        // Wait for all threads to complete
        for (auto& worker : workers_) {
            worker.request_stop();
            // jthread will automatically join
        }

        // Ensure all flush waiters are notified
        std::unique_lock lock(flush_mutex_);
        for (auto& handle : flush_points_) {
            if (handle)
                handle.resume();
        }
        flush_points_.clear();
    }

    // Log message asynchronously, returns a coroutine task
    Task<void> log(LogLevel level, std::string message,
                   const std::source_location& location,
                   std::coroutine_handle<> continuation) {
        // Reject new tasks if shutting down
        if (shutdown_.load(std::memory_order_acquire)) {
            // Resume immediately if there's a waiting coroutine to avoid
            // hanging
            if (continuation)
                continuation.resume();
            co_return;
        }

        // Add task to lock-free queue
        task_queue_.enqueue(level, std::move(message), location, continuation);

        // Notify a worker thread to process the new task
        cv_.notify_one();

        // Coroutine complete
        co_return;
    }

    // Wait for all pending tasks to complete
    Task<void> flush() {
        // Return immediately if queue is empty and no active processing
        if (task_queue_.empty() &&
            active_workers_.load(std::memory_order_acquire) == 0) {
            co_return;
        }

        // Return if already shutting down
        if (shutdown_.load(std::memory_order_acquire)) {
            co_return;
        }

        // Create a special flush point, suspending current coroutine until
        // all previous tasks complete
        {
            std::unique_lock lock(flush_mutex_);
            flush_points_.push_back(std::coroutine_handle<>::from_address(
                std::noop_coroutine().address()));
        }

        // Wait for operation to complete (coroutine will be resumed by the
        // continuation stored above)
        co_await std::suspend_always{};

        co_return;
    }

    // Set the log level
    void setLevel(LogLevel level) {
        std::shared_lock read_lock(logger_mutex_);
        logger_->setLevel(level);
    }

    // Set the thread name
    void setThreadName(const String& name) {
        std::shared_lock read_lock(logger_mutex_);
        logger_->setThreadName(name);
    }

    // Set the underlying logger
    void setUnderlyingLogger(std::shared_ptr<Logger> logger) {
        if (logger) {
            std::unique_lock write_lock(logger_mutex_);
            logger_ = std::move(logger);
        }
    }

    // Enable or disable system logging
    void enableSystemLogging(bool enable) {
        std::shared_lock read_lock(logger_mutex_);
        logger_->enableSystemLogging(enable);
    }

private:
    // Underlying logger with reader-writer lock for better concurrency
    std::shared_ptr<Logger> logger_;
    std::shared_mutex logger_mutex_;  // Reader-writer lock

    // High-performance task queue
    LockFreeTaskQueue task_queue_;

    // Thread coordination
    std::condition_variable_any cv_;
    std::atomic<bool> shutdown_{false};
    std::atomic<size_t> active_workers_{
        0};  // Track actively processing threads

    // Worker thread pool
    std::vector<std::jthread> workers_;

    // Flush points with dedicated mutex (rarely contended)
    std::vector<std::coroutine_handle<>> flush_points_;
    std::mutex flush_mutex_;

    // Worker thread loop
    void workerLoop(std::stop_token stoken) {
        // Task processing variables
        LogLevel level;
        std::string message;
        std::source_location location;
        std::coroutine_handle<> continuation;

        while (!stoken.stop_requested() &&
               !shutdown_.load(std::memory_order_relaxed)) {
            bool got_task = false;
            std::vector<std::coroutine_handle<>> flush_waiters;

            // Try to get a task
            got_task =
                task_queue_.dequeue(level, message, location, continuation);

            if (!got_task) {
                // No task available, wait for new tasks or shutdown signal
                std::unique_lock<std::mutex> temp_lock(temp_mutex_);
                cv_.wait(temp_lock, stoken, [this, &stoken] {
                    // Check task queue again to avoid race conditions
                    return !task_queue_.empty() ||
                           shutdown_.load(std::memory_order_relaxed) ||
                           stoken.stop_requested();
                });

                // Try again after waking up
                continue;
            }

            // Process the acquired task
            processLogTask(level, std::move(message), location, continuation);

            // Check if we should process flush points
            if (task_queue_.empty() &&
                active_workers_.load(std::memory_order_acquire) == 1) {
                // This thread is the last active worker and queue is empty,
                // safe to process flush points
                std::unique_lock lock(flush_mutex_);
                if (!flush_points_.empty()) {
                    flush_waiters = std::move(flush_points_);
                    flush_points_.clear();
                }
            }

            // Resume all coroutines waiting for flush completion
            for (auto& handle : flush_waiters) {
                if (handle)
                    handle.resume();
            }
        }
    }

    // Temporary mutex used for condition variable waiting
    std::mutex temp_mutex_;

    // Process a log task with appropriate error handling
    void processLogTask(LogLevel level, std::string message,
                        const std::source_location& location,
                        std::coroutine_handle<> continuation) {
        try {
            // Process log task
            std::shared_lock read_lock(logger_mutex_);

            // Use public API based on log level
            switch (level) {
                case LogLevel::TRACE:
                    logger_->trace(String(message), location);
                    break;
                case LogLevel::DEBUG:
                    logger_->debug(String(message), location);
                    break;
                case LogLevel::INFO:
                    logger_->info(String(message), location);
                    break;
                case LogLevel::WARN:
                    logger_->warn(String(message), location);
                    break;
                case LogLevel::ERROR:
                    logger_->error(String(message), location);
                    break;
                case LogLevel::CRITICAL:
                    logger_->critical(String(message), location);
                    break;
                case LogLevel::OFF:
                    // No logging when level is OFF
                    break;
            }
        } catch (...) {
            // Log the exception
            try {
                std::shared_lock read_lock(logger_mutex_);
                logger_->error("Exception occurred during log processing",
                               std::source_location::current());
            } catch (...) {
                // Ignore nested exceptions
            }
        }

        // Resume waiting coroutine if there is one, regardless of exceptions
        if (continuation)
            continuation.resume();
    }
};

// AsyncLogger class method implementations

AsyncLogger::AsyncLogger(const fs::path& file_name, LogLevel min_level,
                         size_t max_file_size, int max_files,
                         size_t thread_pool_size)
    : impl_(std::make_unique<AsyncLoggerImpl>(
          file_name, min_level, max_file_size, max_files, thread_pool_size)) {}

AsyncLogger::~AsyncLogger() = default;

void AsyncLogger::setLevel(LogLevel level) { impl_->setLevel(level); }

void AsyncLogger::setThreadName(const String& name) {
    impl_->setThreadName(name);
}

Task<void> AsyncLogger::flush() { return impl_->flush(); }

void AsyncLogger::setUnderlyingLogger(std::shared_ptr<Logger> logger) {
    impl_->setUnderlyingLogger(std::move(logger));
}

void AsyncLogger::enableSystemLogging(bool enable) {
    impl_->enableSystemLogging(enable);
}

Task<void> AsyncLogger::logAsync(LogLevel level, std::string msg,
                                 const std::source_location& location) {
    struct Awaiter {
        AsyncLoggerImpl* impl;
        LogLevel level;
        std::string message;
        std::source_location location;

        bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h) {
            // Pass current coroutine handle to resume when task completes
            impl->log(level, std::move(message), location, h);
        }

        void await_resume() const noexcept {}
    };

    // Return awaiter object which will handle logging via impl_ and resume
    // coroutine when done
    co_await Awaiter{impl_.get(), level, std::move(msg), location};
}

}  // namespace atom::log