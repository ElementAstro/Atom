// async_logger.cpp
/*
 * async_logger.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-5-6

Description: Enhanced High-Performance Asynchronous Logger Implementation
using C++20/23 Coroutines with optimized performance

**************************************************/

#include "async_logger.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <coroutine>
#include <format>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <shared_mutex>
#include <stop_token>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <sdkddkver.h>
#define WIN32_LEAN_AND_MEAN
#include <windef.h>
#include <windows.h>

#undef ERROR
#endif

#include "atom/memory/memory_pool.hpp"
#include "atom/type/json.hpp"

namespace atom::log {

using json = nlohmann::json;
struct LoggerMemoryPool {
    static constexpr size_t BLOCK_SIZE = 8192;    // 8KB blocks
    static constexpr size_t MAX_BLOCKS = 2048;    // Max 16MB total
    static constexpr size_t INITIAL_BLOCKS = 16;  // 预分配块提高启动性能

    // 线程安全的单例访问
    static LoggerMemoryPool& instance() {
        static LoggerMemoryPool pool;
        return pool;
    }

    // 获取内存资源
    std::pmr::memory_resource* resource() { return &adapter_resource_; }

    // 销毁时输出内存使用统计信息
    ~LoggerMemoryPool() {
        try {
            // 仅在调试模式下输出统计信息
#ifdef _DEBUG
            auto [allocated, total] = memory_pool_.get_stats();
            std::cerr << std::format(
                "LoggerMemoryPool stats: Allocated blocks: {}, Total blocks: "
                "{}\n",
                allocated, total);
#endif
        } catch (...) {
            // 析构函数不应抛出异常
        }
    }

private:
    // 使用现有的内存池实现
    atom::memory::MemoryPool<BLOCK_SIZE, MAX_BLOCKS> memory_pool_;

    // 适配器：将 MemoryPool 接口转换为 std::pmr::memory_resource
    class MemoryPoolResource : public std::pmr::memory_resource {
    public:
        explicit MemoryPoolResource(
            atom::memory::MemoryPool<BLOCK_SIZE, MAX_BLOCKS>& pool)
            : pool_(pool) {}

    private:
        void* do_allocate(size_t bytes, size_t alignment) override {
            // 如果请求大小超过块大小，使用标准分配器
            if (bytes > BLOCK_SIZE) {
                return std::pmr::new_delete_resource()->allocate(bytes,
                                                                 alignment);
            }
            return pool_.allocate();
        }

        void do_deallocate(void* ptr, size_t bytes, size_t alignment) override {
            // 如果大小超过块大小，使用标准释放
            if (bytes > BLOCK_SIZE) {
                std::pmr::new_delete_resource()->deallocate(ptr, bytes,
                                                            alignment);
                return;
            }
            pool_.deallocate(ptr);
        }

        bool do_is_equal(const memory_resource& other) const noexcept override {
            return this == &other;
        }

        atom::memory::MemoryPool<BLOCK_SIZE, MAX_BLOCKS>& pool_;
    };

    // 内存资源适配器
    MemoryPoolResource adapter_resource_{memory_pool_};

    // 单例的私有构造函数
    LoggerMemoryPool() {
        // 预分配块以减少初始运行时的分配延迟
        std::vector<void*> blocks;
        blocks.reserve(INITIAL_BLOCKS);

        try {
            for (size_t i = 0; i < INITIAL_BLOCKS; ++i) {
                blocks.push_back(memory_pool_.allocate());
            }

            // 释放预分配的块
            for (void* block : blocks) {
                memory_pool_.deallocate(block);
            }
        } catch (...) {
            // 预分配失败，但这不是致命错误
            // 清理任何已分配的块
            for (void* block : blocks) {
                if (block) {
                    memory_pool_.deallocate(block);
                }
            }
        }
    }

    // 禁止复制和移动
    LoggerMemoryPool(const LoggerMemoryPool&) = delete;
    LoggerMemoryPool& operator=(const LoggerMemoryPool&) = delete;
    LoggerMemoryPool(LoggerMemoryPool&&) = delete;
    LoggerMemoryPool& operator=(LoggerMemoryPool&&) = delete;
};

// 用于日志任务的缓存行对齐节点
struct alignas(64) LogTaskNode {  // 缓存行对齐以避免伪共享
    LogLevel level;
    std::string message;
    std::source_location location;
    std::coroutine_handle<> continuation;
    std::atomic<LogTaskNode*> next{nullptr};
    std::chrono::steady_clock::time_point timestamp;  // 用于计算延迟

    LogTaskNode(LogLevel lvl, std::string msg, const std::source_location& loc,
                std::coroutine_handle<> cont)
        : level(lvl),
          message(std::move(msg)),
          location(loc),
          continuation(cont),
          timestamp(std::chrono::steady_clock::now()) {}
};

// 改进的无锁MPSC（多生产者单消费者）队列，用于高吞吐量
class LockFreeTaskQueue {
public:
    LockFreeTaskQueue(size_t capacity = 10000)
        : capacity_(capacity),
          current_size_(0),
          head_(new LogTaskNode(LogLevel::INFO, "",
                                std::source_location::current(), nullptr)),
          dropped_messages_(0),
          max_size_(0) {
        tail_.store(head_.load(std::memory_order_relaxed),
                    std::memory_order_relaxed);
    }

    ~LockFreeTaskQueue() {
        // 清理剩余节点
        LogTaskNode* current = head_.load(std::memory_order_relaxed);
        while (current) {
            LogTaskNode* next = current->next.load(std::memory_order_relaxed);
            delete current;
            current = next;
        }
    }

    // 生产者的线程安全入队操作
    bool enqueue(LogLevel level, std::string message,
                 const std::source_location& location,
                 std::coroutine_handle<> continuation) {
        // 检查队列容量
        size_t current = current_size_.load(std::memory_order_acquire);
        if (current >= capacity_) {
            dropped_messages_.fetch_add(1, std::memory_order_relaxed);
            // 对于严重级别的消息，我们始终尝试记录它们，即使队列已满
            if (level < LogLevel::ERROR) {
                return false;  // 队列已满，拒绝消息
            }
            // 否则继续尝试记录严重错误
        }

        // 创建新节点
        LogTaskNode* node =
            new LogTaskNode(level, std::move(message), location, continuation);

        // 添加到队列，确保内存顺序保证
        LogTaskNode* prev = head_.exchange(node, std::memory_order_acq_rel);
        prev->next.store(node, std::memory_order_release);

        // 增加大小计数器
        size_t new_size =
            current_size_.fetch_add(1, std::memory_order_relaxed) + 1;

        // 更新最大大小统计信息
        size_t expected_max = max_size_.load(std::memory_order_relaxed);
        while (new_size > expected_max &&
               !max_size_.compare_exchange_weak(expected_max, new_size,
                                                std::memory_order_relaxed)) {
            // 继续尝试
        }

        return true;
    }

    // 消费者的线程安全出队操作
    bool dequeue(LogLevel& level, std::string& message,
                 std::source_location& location,
                 std::coroutine_handle<>& continuation,
                 std::chrono::nanoseconds& latency) {
        LogTaskNode* tail = tail_.load(std::memory_order_relaxed);
        LogTaskNode* next = tail->next.load(std::memory_order_acquire);

        if (!next) {
            return false;  // 队列为空
        }

        // 保存任务数据
        level = next->level;
        message = std::move(next->message);
        location = next->location;
        continuation = next->continuation;

        // 计算从创建到消费的延迟
        latency = std::chrono::steady_clock::now() - next->timestamp;

        // 更新尾部并释放旧节点
        tail_.store(next, std::memory_order_release);
        delete tail;

        // 减少大小计数器
        current_size_.fetch_sub(1, std::memory_order_relaxed);

        return true;
    }

    // 检查队列是否为空
    [[nodiscard]] bool empty() const {
        LogTaskNode* tail = tail_.load(std::memory_order_relaxed);
        LogTaskNode* next = tail->next.load(std::memory_order_acquire);
        return next == nullptr;
    }

    // 获取当前队列大小
    [[nodiscard]] size_t size() const noexcept {
        return current_size_.load(std::memory_order_relaxed);
    }

    // 获取队列容量
    [[nodiscard]] size_t capacity() const noexcept { return capacity_; }

    // 设置队列容量
    void capacity(size_t new_capacity) noexcept { capacity_ = new_capacity; }

    // 获取队列统计信息
    [[nodiscard]] auto getStatistics() const noexcept {
        struct Stats {
            size_t current_size;
            size_t max_size;
            size_t capacity;
            size_t dropped_messages;
        };

        return Stats{current_size_.load(std::memory_order_relaxed),
                     max_size_.load(std::memory_order_relaxed), capacity_,
                     dropped_messages_.load(std::memory_order_relaxed)};
    }

private:
    size_t capacity_;                             // 队列容量
    std::atomic<size_t> current_size_;            // 当前队列大小
    alignas(64) std::atomic<LogTaskNode*> head_;  // 缓存行对齐
    alignas(64) std::atomic<LogTaskNode*> tail_;  // 缓存行对齐
    std::atomic<size_t> dropped_messages_;        // 被丢弃的消息数
    std::atomic<size_t> max_size_;                // 历史最大队列大小
};

// 异步日志器实现类
class AsyncLogger::AsyncLoggerImpl {
    struct Statistics {
        explicit Statistics(AsyncLoggerImpl& parent) : parent_(parent) {}

        std::atomic<uint64_t> messages_processed{0};
        std::atomic<uint64_t> errors_occurred{0};
        std::atomic<uint64_t> flush_operations{0};
        std::atomic<uint64_t> total_latency_ns{0};
        std::atomic<uint64_t> max_latency_ns{0};

        AsyncLoggerImpl& parent_;

        void reset() {
            messages_processed = 0;
            errors_occurred = 0;
            flush_operations = 0;
            total_latency_ns = 0;
            max_latency_ns = 0;
        }

        [[nodiscard]] json toJson() const {
            json j;
            j["messages_processed"] = messages_processed.load();
            j["errors_occurred"] = errors_occurred.load();
            j["flush_operations"] = flush_operations.load();

            uint64_t processed = messages_processed.load();
            if (processed > 0) {
                j["avg_latency_us"] =
                    static_cast<double>(total_latency_ns.load()) / processed /
                    1000.0;
                j["max_latency_us"] =
                    static_cast<double>(max_latency_ns.load()) / 1000.0;

                auto queue_stats = parent_.task_queue_.getStatistics();
                j["queue"] = {
                    {"current_size", queue_stats.current_size},
                    {"max_size", queue_stats.max_size},
                    {"capacity", queue_stats.capacity},
                    {"dropped_messages", queue_stats.dropped_messages}};
            }

            return j;
        }
    };

public:
    AsyncLoggerImpl(const AsyncLoggerConfig& config)
        : logger_(std::make_shared<Logger>(config.file_name, config.min_level,
                                           config.max_file_size,
                                           config.max_files)),
          task_queue_(config.queue_capacity),
          shutdown_(false),
          active_workers_(0),
          flush_interval_(config.flush_interval),
          stats_(*this) {
        // 初始化工作线程池
        initWorkers(config.thread_pool_size);

        // 启动自动刷新线程（如果间隔 > 0）
        if (flush_interval_.count() > 0) {
            startAutoFlushThread();
        }
    }

    AsyncLoggerImpl(const fs::path& file_name, LogLevel min_level,
                    size_t max_file_size, int max_files,
                    size_t thread_pool_size)
        : logger_(std::make_shared<Logger>(file_name, min_level, max_file_size,
                                           max_files)),
          shutdown_(false),
          active_workers_(0),
          stats_(*this) {
        // 初始化工作线程池
        initWorkers(thread_pool_size);
    }

    ~AsyncLoggerImpl() {
        // 标记关闭并通知所有线程
        shutdown_.store(true, std::memory_order_release);

        // 停止自动刷新线程
        if (auto_flush_thread_.joinable()) {
            auto_flush_thread_.request_stop();
            auto_flush_thread_.join();
        }

        cv_.notify_all();

        // 等待所有线程完成
        for (auto& worker : workers_) {
            worker.request_stop();
            // jthread 将自动 join
        }

        // 确保所有刷新等待者都被通知
        std::unique_lock lock(flush_mutex_);
        for (auto& handle : flush_points_) {
            if (handle)
                handle.resume();
        }
        flush_points_.clear();
    }

    // 异步记录消息，返回协程任务
    Task<void> log(LogLevel level, std::string message,
                   const std::source_location& location,
                   std::coroutine_handle<> continuation) {
        // 如果关闭中，则拒绝新任务
        if (shutdown_.load(std::memory_order_acquire)) {
            // 如果存在等待的协程，立即恢复以避免挂起
            if (continuation)
                continuation.resume();
            throw ShutdownException("Logger is shutting down");
        }

        // 将任务添加到无锁队列
        if (!task_queue_.enqueue(level, std::move(message), location,
                                 continuation)) {
            // 队列已满
            if (continuation) {
                continuation.resume();
            }
            throw QueueFullException("Logging queue is full, message dropped");
        }

        // 通知工作线程处理新任务
        cv_.notify_one();

        // 协程完成
        co_return;
    }

    // 等待所有挂起的任务完成
    Task<void> flush() {
        // 如果队列为空且没有活动处理，则立即返回
        if (task_queue_.empty() &&
            active_workers_.load(std::memory_order_acquire) == 0) {
            co_return;
        }

        // 如果已关闭则返回
        if (shutdown_.load(std::memory_order_acquire)) {
            co_return;
        }

        stats_.flush_operations.fetch_add(1, std::memory_order_relaxed);

        // 创建特殊的刷新点，挂起当前协程直到所有先前的任务完成
        {
            std::unique_lock lock(flush_mutex_);
            flush_points_.push_back(std::coroutine_handle<>::from_address(
                std::noop_coroutine().address()));
        }

        // 等待操作完成（协程将由上面存储的延续恢复）
        co_await std::suspend_always{};

        co_return;
    }

    // 设置日志级别
    void setLevel(LogLevel level) {
        std::shared_lock read_lock(logger_mutex_);
        logger_->setLevel(level);
    }

    // 设置线程名
    void setThreadName(const String& name) {
        std::shared_lock read_lock(logger_mutex_);
        logger_->setThreadName(name);
    }

    // 设置底层日志器
    void setUnderlyingLogger(std::shared_ptr<Logger> logger) {
        if (logger) {
            std::unique_lock write_lock(logger_mutex_);
            logger_ = std::move(logger);
        }
    }

    // 启用或禁用系统日志记录
    void enableSystemLogging(bool enable) {
        std::shared_lock read_lock(logger_mutex_);
        logger_->enableSystemLogging(enable);
    }

    // 设置自动刷新间隔
    void setAutoFlushInterval(std::chrono::milliseconds interval) {
        if (interval == flush_interval_) {
            return;  // 没有变化
        }

        flush_interval_ = interval;

        // 如果之前自动刷新被禁用，现在启用它
        if (interval.count() > 0 && !auto_flush_thread_.joinable()) {
            startAutoFlushThread();
        }
        // 如果现在禁用自动刷新，停止线程
        else if (interval.count() == 0 && auto_flush_thread_.joinable()) {
            auto_flush_thread_.request_stop();
            auto_flush_thread_.join();
        }
    }

    // 设置队列容量
    void setQueueCapacity(size_t capacity) { task_queue_.capacity(capacity); }

    // 获取日志统计信息
    [[nodiscard]] std::string getStatistics() const {
        json j = stats_.toJson();
        return j.dump(2);
    }

    // 等待所有日志处理完成，带超时
    [[nodiscard]] bool waitForCompletion(std::chrono::milliseconds timeout) {
        auto start = std::chrono::steady_clock::now();

        while (task_queue_.size() > 0) {
            // 检查超时
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed >= timeout) {
                return false;
            }

            // 通知工作线程处理队列
            cv_.notify_all();

            // 短暂休眠，避免忙等
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        return true;
    }

private:
    // 具有读写器锁的底层日志器，提高并发性
    std::shared_ptr<Logger> logger_;
    std::shared_mutex logger_mutex_;  // 读写锁

    // 高性能任务队列
    LockFreeTaskQueue task_queue_;

    // 线程协调
    std::condition_variable_any cv_;
    std::atomic<bool> shutdown_{false};
    std::atomic<size_t> active_workers_{0};  // 跟踪活动处理线程

    // 工作线程池
    std::vector<std::jthread> workers_;

    // 刷新点，带专用互斥锁（很少发生争用）
    std::vector<std::coroutine_handle<>> flush_points_;
    std::mutex flush_mutex_;

    // 自动刷新配置
    std::chrono::milliseconds flush_interval_{0};  // 0表示无自动刷新
    std::jthread auto_flush_thread_;

    // 统计信息
    mutable Statistics stats_;

    // 临时互斥锁用于条件变量等待
    std::mutex temp_mutex_;

    // 初始化工作线程池
    void initWorkers(size_t thread_count) {
        workers_.reserve(thread_count);  // 预分配vector空间

        for (size_t i = 0; i < thread_count; ++i) {
            workers_.emplace_back([this, i](std::stop_token stoken) {
// 设置线程名以便调试
#ifdef __linux__
                pthread_setname_np(pthread_self(),
                                   std::format("log_worker_{}", i).c_str());
#elif defined(_WIN32)
                // Windows上的线程名设置需要异常处理
                try {
                    HANDLE thread = GetCurrentThread();
                    if (thread) {
                        SetThreadDescription(
                            thread,
                            std::wstring(std::format(L"log_worker_{}", i))
                                .c_str());
                    }
                } catch (...) {
                }
#endif

                // 增加活动工作线程计数
                active_workers_.fetch_add(1, std::memory_order_relaxed);
                workerLoop(stoken);
                // 减少活动工作线程计数
                active_workers_.fetch_sub(1, std::memory_order_relaxed);
            });
        }
    }

    // 启动自动刷新线程
    void startAutoFlushThread() {
        auto_flush_thread_ = std::jthread([this](std::stop_token stoken) {
#if defined(__linux__) || defined(__APPLE__)
            pthread_setname_np(pthread_self(), "log_auto_flush");
#elif defined(_WIN32)
            try {
                HANDLE thread = GetCurrentThread();
                if (thread) {
                    SetThreadDescription(thread, L"log_auto_flush");
                }
            } catch (...) {
            }
#endif

            while (!stoken.stop_requested()) {
                std::this_thread::sleep_for(flush_interval_);

                // 如果不是空的，执行刷新
                if (!task_queue_.empty()) {
                    try {
                        logger_->flush();
                    } catch (...) {
                        // 忽略刷新期间的异常
                    }
                }
            }
        });
    }

    // 工作线程循环
    void workerLoop(std::stop_token stoken) {
        // 任务处理变量
        LogLevel level;
        std::string message;
        std::source_location location;
        std::coroutine_handle<> continuation;
        std::chrono::nanoseconds latency;

        while (!stoken.stop_requested() &&
               !shutdown_.load(std::memory_order_relaxed)) {
            bool got_task = false;
            std::vector<std::coroutine_handle<>> flush_waiters;

            // 尝试获取任务
            got_task = task_queue_.dequeue(level, message, location,
                                           continuation, latency);

            if (!got_task) {
                // 无可用任务，等待新任务或关闭信号
                std::unique_lock<std::mutex> temp_lock(temp_mutex_);
                cv_.wait_for(
                    temp_lock, std::chrono::milliseconds(100), [this, &stoken] {
                        // 再次检查任务队列以避免竞争条件
                        return !task_queue_.empty() ||
                               shutdown_.load(std::memory_order_relaxed) ||
                               stoken.stop_requested();
                    });

                // 醒来后重试
                continue;
            }

            // 处理获取的任务
            processLogTask(level, std::move(message), location, continuation,
                           latency);

            // 检查是否应处理刷新点
            if (task_queue_.empty() &&
                active_workers_.load(std::memory_order_acquire) == 1) {
                // 该线程是最后一个活动的工作线程，且队列为空，
                // 安全处理刷新点
                std::unique_lock lock(flush_mutex_);
                if (!flush_points_.empty()) {
                    flush_waiters = std::move(flush_points_);
                    flush_points_.clear();
                }
            }

            // 恢复所有等待刷新完成的协程
            for (auto& handle : flush_waiters) {
                if (handle)
                    handle.resume();
            }
        }
    }

    // 用适当错误处理处理日志任务
    void processLogTask(LogLevel level, std::string message,
                        const std::source_location& location,
                        std::coroutine_handle<> continuation,
                        const std::chrono::nanoseconds& latency) {
        try {
            // 更新处理的消息数
            stats_.messages_processed.fetch_add(1, std::memory_order_relaxed);

            // 更新延迟统计
            uint64_t latency_ns = latency.count();
            stats_.total_latency_ns.fetch_add(latency_ns,
                                              std::memory_order_relaxed);

            // 更新最大延迟
            uint64_t current_max =
                stats_.max_latency_ns.load(std::memory_order_relaxed);
            while (latency_ns > current_max &&
                   !stats_.max_latency_ns.compare_exchange_weak(
                       current_max, latency_ns, std::memory_order_relaxed)) {
                // 继续尝试
            }

            // 处理日志任务
            std::shared_lock read_lock(logger_mutex_);

            // 基于日志级别使用公共API
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
                    // 级别为OFF时不记录
                    break;
            }
        } catch (const std::exception& e) {
            // 记录异常
            try {
                stats_.errors_occurred.fetch_add(1, std::memory_order_relaxed);
                std::shared_lock read_lock(logger_mutex_);
                logger_->error(
                    std::format("Exception during log processing: {}",
                                e.what()),
                    std::source_location::current());
            } catch (...) {
                // 忽略嵌套异常
            }
        } catch (...) {
            // 记录未知异常
            try {
                stats_.errors_occurred.fetch_add(1, std::memory_order_relaxed);
                std::shared_lock read_lock(logger_mutex_);
                logger_->error("Unknown exception during log processing",
                               std::source_location::current());
            } catch (...) {
                // 忽略嵌套异常
            }
        }

        // 无论异常如何，如果存在等待的协程，恢复它
        if (continuation)
            continuation.resume();
    }
};

// AsyncLogger类的方法实现

AsyncLogger::AsyncLogger(const AsyncLoggerConfig& config)
    : impl_(std::make_unique<AsyncLoggerImpl>(config)) {}

AsyncLogger::AsyncLogger(const fs::path& file_name, LogLevel min_level,
                         size_t max_file_size, int max_files,
                         size_t thread_pool_size)
    : impl_(std::make_unique<AsyncLoggerImpl>(
          file_name, min_level, max_file_size, max_files, thread_pool_size)) {}

AsyncLogger::~AsyncLogger() = default;

// 实现移动操作
AsyncLogger::AsyncLogger(AsyncLogger&&) noexcept = default;
auto AsyncLogger::operator=(AsyncLogger&&) noexcept -> AsyncLogger& = default;

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

void AsyncLogger::setAutoFlushInterval(std::chrono::milliseconds interval) {
    impl_->setAutoFlushInterval(interval);
}

void AsyncLogger::setQueueCapacity(size_t capacity) {
    impl_->setQueueCapacity(capacity);
}

std::string AsyncLogger::getStatistics() const noexcept {
    try {
        return impl_->getStatistics();
    } catch (...) {
        return R"({"error": "Failed to get statistics"})";
    }
}

bool AsyncLogger::waitForCompletion(
    std::chrono::milliseconds timeout) noexcept {
    try {
        return impl_->waitForCompletion(timeout);
    } catch (...) {
        return false;
    }
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
            try {
                // 传递当前协程句柄，在任务完成时恢复
                impl->log(level, std::move(message), location, h);
            } catch (...) {
                // 发生异常时恢复协程
                h.resume();
                throw;  // 重新抛出异常，将被Task的promise处理
            }
        }

        void await_resume() const noexcept {}
    };

    // 返回awaiter对象，它将通过impl_处理日志记录，并在完成时恢复协程
    co_await Awaiter{impl_.get(), level, std::move(msg), location};
    co_return;
}

}  // namespace atom::log