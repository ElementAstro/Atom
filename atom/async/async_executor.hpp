/*
 * async_executor.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-24

Description: Advanced async task executor with thread pooling

**************************************************/

#ifndef ATOM_ASYNC_ASYNC_EXECUTOR_HPP
#define ATOM_ASYNC_ASYNC_EXECUTOR_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <coroutine>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <semaphore>
#include <source_location>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// 平台特定优化
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#define ATOM_PLATFORM_WINDOWS 1
#define WIN32_LEAN_AND_MEAN
#elif defined(__APPLE__)
#include <dispatch/dispatch.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#define ATOM_PLATFORM_MACOS 1
#elif defined(__linux__)
#include <pthread.h>
#include <sched.h>
#define ATOM_PLATFORM_LINUX 1
#endif

// 添加编译器特定优化
#if defined(__GNUC__) || defined(__clang__)
#define ATOM_LIKELY(x) __builtin_expect(!!(x), 1)
#define ATOM_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define ATOM_FORCE_INLINE __attribute__((always_inline)) inline
#define ATOM_NO_INLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define ATOM_LIKELY(x) (x)
#define ATOM_UNLIKELY(x) (x)
#define ATOM_FORCE_INLINE __forceinline
#define ATOM_NO_INLINE __declspec(noinline)
#else
#define ATOM_LIKELY(x) (x)
#define ATOM_UNLIKELY(x) (x)
#define ATOM_FORCE_INLINE inline
#define ATOM_NO_INLINE
#endif

// 缓存行大小定义 - 用于避免假共享
#ifndef ATOM_CACHE_LINE_SIZE
#if defined(ATOM_PLATFORM_WINDOWS)
#define ATOM_CACHE_LINE_SIZE 64
#elif defined(ATOM_PLATFORM_MACOS)
#define ATOM_CACHE_LINE_SIZE 128
#else
#define ATOM_CACHE_LINE_SIZE 64
#endif
#endif

// 对齐到缓存行的宏
#define ATOM_CACHELINE_ALIGN alignas(ATOM_CACHE_LINE_SIZE)

namespace atom::async {

// 前置声明
class AsyncExecutor;

// C++20异常类增强版本，带源码位置信息
class ExecutorException : public std::runtime_error {
public:
    explicit ExecutorException(
        const std::string& msg,
        const std::source_location& loc = std::source_location::current())
        : std::runtime_error(msg + " at " + loc.file_name() + ":" +
                             std::to_string(loc.line()) + " in " +
                             loc.function_name()) {}
};

// 任务异常处理机制增强
class TaskException : public ExecutorException {
public:
    explicit TaskException(
        const std::string& msg,
        const std::source_location& loc = std::source_location::current())
        : ExecutorException(msg, loc) {}
};

// C++20 协程任务类型，包含续体（continuation）和错误处理
template <typename R>
class Task;

// Task<void> specialization for coroutines
template <>
class Task<void> {
public:
    struct promise_type {
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void unhandled_exception() { exception_ = std::current_exception(); }
        void return_void() {}
        
        Task<void> get_return_object() {
            return Task<void>{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::exception_ptr exception_{};
    };

    using handle_type = std::coroutine_handle<promise_type>;

    Task(handle_type h) : handle_(h) {}
    ~Task() {
        if (handle_ && handle_.done()) {
            handle_.destroy();
        }
    }

    Task(Task&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_)
                handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    bool is_ready() const noexcept { return handle_.done(); }

    void get() {
        handle_.resume();
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
    }

    struct Awaiter {
        handle_type handle;
        bool await_ready() const noexcept { return handle.done(); }
        void await_suspend(std::coroutine_handle<> h) noexcept { h.resume(); }
        void await_resume() {
            if (handle.promise().exception_) {
                std::rethrow_exception(handle.promise().exception_);
            }
        }
    };

    auto operator co_await() noexcept { return Awaiter{handle_}; }

private:
    handle_type handle_{};
        std::exception_ptr exception_{};
    };
    
    using handle_type = std::coroutine_handle<promise_type>;
    
    Task(handle_type h) : handle_(h) {}
    ~Task() {
        if (handle_ && handle_.done()) {
            handle_.destroy();
        }
    }
    
    Task(Task&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_)
                handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    
    bool is_ready() const noexcept { return handle_.done(); }
    
    void get_result() {
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
    }
    
    struct Awaiter {
        handle_type handle;
        
        bool await_ready() const noexcept { return handle.done(); }
        
        std::coroutine_handle<> await_suspend(
            std::coroutine_handle<> h) noexcept {
            continuation = h;
            return handle;
        }
        
        void await_resume() {
            if (handle.promise().exception_) {
                std::rethrow_exception(handle.promise().exception_);
            }
        }
        
        std::coroutine_handle<> continuation = nullptr;
    };
    
    Awaiter operator co_await() noexcept { return Awaiter{handle_}; }
    
private:
    handle_type handle_{};
};

// Generic type implementation
template <typename R>
class Task {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void unhandled_exception() { exception_ = std::current_exception(); }

        template <typename T>
            requires std::convertible_to<T, R>
        void return_value(T&& value) {
            result_ = std::forward<T>(value);
        }

        Task get_return_object() {
            return Task{handle_type::from_promise(*this)};
        }

        R result_{};
        std::exception_ptr exception_{};
    };

    Task(handle_type h) : handle_(h) {}
    ~Task() {
        if (handle_ && handle_.done()) {
            handle_.destroy();
        }
    }

    Task(Task&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_)
                handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    bool is_ready() const noexcept { return handle_.done(); }

    R get_result() {
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
        return std::move(handle_.promise().result_);
    }

    // 协程等待器支持
    struct Awaiter {
        handle_type handle;

        bool await_ready() const noexcept { return handle.done(); }

        std::coroutine_handle<> await_suspend(
            std::coroutine_handle<> h) noexcept {
            // 存储续体
            continuation = h;
            return handle;
        }

        R await_resume() {
            if (handle.promise().exception_) {
                std::rethrow_exception(handle.promise().exception_);
            }
            return std::move(handle.promise().result_);
        }

        std::coroutine_handle<> continuation = nullptr;
    };

    Awaiter operator co_await() noexcept { return Awaiter{handle_}; }

private:
    handle_type handle_{};
};

/**
 * @brief 异步执行器 - 高性能线程池实现
 *
 * 实现高效的任务调度和执行，支持任务优先级，协程和未来/承诺
 */
class AsyncExecutor {
public:
    // 任务优先级
    enum class Priority { Low = 0, Normal = 50, High = 100, Critical = 200 };

    // 线程池配置选项
    struct Configuration {
        size_t minThreads = 4;            // 最小线程数
        size_t maxThreads = 16;           // 最大线程数
        size_t queueSizePerThread = 128;  // 每线程队列大小
        std::chrono::milliseconds threadIdleTimeout =
            std::chrono::seconds(30);  // 空闲线程超时
        bool setPriority = false;      // 是否设置线程优先级
        int threadPriority = 0;        // 线程优先级，依赖于平台
        bool pinThreads = false;       // 是否绑定线程到CPU核心
        bool useWorkStealing = true;   // 是否启用工作窃取算法
        std::chrono::milliseconds statInterval =
            std::chrono::seconds(10);  // 统计信息收集间隔
    };

    /**
     * @brief 创建具有指定配置的异步执行器
     * @param config 线程池配置
     */
    explicit AsyncExecutor(Configuration config);

    /**
     * @brief 禁止拷贝构造
     */
    AsyncExecutor(const AsyncExecutor&) = delete;
    AsyncExecutor& operator=(const AsyncExecutor&) = delete;

    /**
     * @brief 支持移动构造
     */
    AsyncExecutor(AsyncExecutor&& other) noexcept;
    AsyncExecutor& operator=(AsyncExecutor&& other) noexcept;

    /**
     * @brief 析构函数 - 停止所有线程
     */
    ~AsyncExecutor();

    /**
     * @brief 启动线程池
     */
    void start();

    /**
     * @brief 停止线程池
     */
    void stop();

    /**
     * @brief 检查线程池是否正在运行
     */
    [[nodiscard]] bool isRunning() const noexcept {
        return m_isRunning.load(std::memory_order_acquire);
    }

    /**
     * @brief 获取活动线程数量
     */
    [[nodiscard]] size_t getActiveThreadCount() const noexcept {
        return m_activeThreads.load(std::memory_order_relaxed);
    }

    /**
     * @brief 获取当前等待中的任务数量
     */
    [[nodiscard]] size_t getPendingTaskCount() const noexcept {
        return m_pendingTasks.load(std::memory_order_relaxed);
    }

    /**
     * @brief 获取已完成任务数量
     */
    [[nodiscard]] size_t getCompletedTaskCount() const noexcept {
        return m_completedTasks.load(std::memory_order_relaxed);
    }

    /**
     * @brief 在后台执行任意可调用对象，无返回值版本
     *
     * @param func 可调用对象
     * @param priority 任务优先级
     */
    template <typename Func>
        requires std::invocable<Func> &&
                 std::same_as<void, std::invoke_result_t<Func>>
    void execute(Func&& func, Priority priority = Priority::Normal) {
        if (!isRunning()) {
            throw ExecutorException("Executor is not running");
        }

        enqueueTask(createWrappedTask(std::forward<Func>(func)),
                    static_cast<int>(priority));
    }

    /**
     * @brief 在后台执行任意可调用对象，有返回值版本，使用std::future
     *
     * @param func 可调用对象
     * @param priority 任务优先级
     * @return std::future<ResultT> 异步结果
     */
    template <typename Func>
        requires std::invocable<Func> &&
                 (!std::same_as<void, std::invoke_result_t<Func>>)
    auto execute(Func&& func, Priority priority = Priority::Normal)
        -> std::future<std::invoke_result_t<Func>> {
        if (!isRunning()) {
            throw ExecutorException("Executor is not running");
        }

        using ResultT = std::invoke_result_t<Func>;
        auto promise = std::make_shared<std::promise<ResultT>>();
        auto future = promise->get_future();

        auto wrappedTask = [func = std::forward<Func>(func),
                            promise = std::move(promise)]() mutable {
            try {
                if constexpr (std::is_same_v<ResultT, void>) {
                    func();
                    promise->set_value();
                } else {
                    promise->set_value(func());
                }
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        };

        enqueueTask(std::move(wrappedTask), static_cast<int>(priority));

        return future;
    }

    /**
     * @brief 使用C++20协程执行异步任务
     *
     * @param func 可调用对象
     * @param priority 任务优先级
     * @return Task<ResultT> 协程任务对象
     */
    template <typename Func>
        requires std::invocable<Func>
    auto executeAsTask(Func&& func, Priority priority = Priority::Normal) {
        using ResultT = std::invoke_result_t<Func>;
        using TaskType = Task<ResultT>;

        struct Awaitable {
            std::future<ResultT> future;
            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<> h) noexcept { }
            ResultT await_resume() { return future.get(); }
        };

        if constexpr (std::is_same_v<ResultT, void>) {
            co_await Awaitable{this->execute(std::forward<Func>(func), priority)};
            co_return;
        } else {
            co_return co_await Awaitable{this->execute(std::forward<Func>(func), priority)};
        }
                co_await std::suspend_always{};
                co_return future.get();  // 获取结果或传播异常
            }();
        }
    }

    /**
     * @brief 将任务提交到全局线程池实例
     *
     * @param func 可调用对象
     * @param priority 任务优先级
     * @return 任务结果的future
     */
    template <typename Func>
    static auto submit(Func&& func, Priority priority = Priority::Normal) {
        return getInstance().execute(std::forward<Func>(func), priority);
    }

    /**
     * @brief 获取全局线程池实例的引用
     * @return AsyncExecutor& 全局线程池引用
     */
    static AsyncExecutor& getInstance() {
        static AsyncExecutor instance{Configuration{}};
        return instance;
    }

private:
    // 线程池配置
    Configuration m_config;

    // 原子状态变量
    ATOM_CACHELINE_ALIGN std::atomic<bool> m_isRunning{false};
    ATOM_CACHELINE_ALIGN std::atomic<size_t> m_activeThreads{0};
    ATOM_CACHELINE_ALIGN std::atomic<size_t> m_pendingTasks{0};
    ATOM_CACHELINE_ALIGN std::atomic<size_t> m_completedTasks{0};

    // 任务计数信号量 - C++20新特性
    std::counting_semaphore<> m_taskSemaphore{0};

    // 任务类型
    struct Task {
        std::function<void()> func;
        int priority;

        bool operator<(const Task& other) const {
            // 越高优先级的任务在队列中排序越靠前
            return priority < other.priority;
        }
    };

    // 任务队列 - 优先级队列
    std::mutex m_queueMutex;
    std::priority_queue<Task> m_taskQueue;
    std::condition_variable m_condition;

    // 工作线程
    std::vector<std::jthread> m_threads;

    // 统计信息线程
    std::jthread m_statsThread;

    // 使用工作窃取队列优化
    struct WorkStealingQueue {
        std::mutex mutex;
        std::deque<Task> tasks;
    };
    std::vector<std::unique_ptr<WorkStealingQueue>> m_perThreadQueues;

    /**
     * @brief 线程工作循环
     * @param threadId 线程ID
     * @param stoken 停止令牌
     */
    void workerLoop(size_t threadId, std::stop_token stoken);

    /**
     * @brief 设置线程亲和性
     * @param threadId 线程ID
     */
    void setThreadAffinity(size_t threadId);

    /**
     * @brief 设置线程优先级
     * @param threadId 线程ID
     */
    void setThreadPriority(std::thread::native_handle_type handle);

    /**
     * @brief 从队列获取任务
     * @param threadId 当前线程ID
     * @return std::optional<Task> 可选任务
     */
    std::optional<Task> dequeueTask(size_t threadId);

    /**
     * @brief 尝试从其他线程窃取任务
     * @param currentId 当前线程ID
     * @return std::optional<Task> 可选任务
     */
    std::optional<Task> stealTask(size_t currentId);

    /**
     * @brief 将任务添加到队列
     * @param task 任务函数
     * @param priority 优先级
     */
    void enqueueTask(std::function<void()> task, int priority);

    /**
     * @brief 包装任务以添加异常处理和性能统计
     * @param func 原始函数
     * @return std::function<void()> 包装后的任务
     */
    template <typename Func>
    auto createWrappedTask(Func&& func) {
        return [this, func = std::forward<Func>(func)]() {
            // 增加活动线程计数
            m_activeThreads.fetch_add(1, std::memory_order_relaxed);

            // 捕获任务开始时间 - 用于性能监控
            auto startTime = std::chrono::high_resolution_clock::now();

            try {
                // 执行实际任务
                func();

                // 更新完成任务计数
                m_completedTasks.fetch_add(1, std::memory_order_relaxed);
            } catch (...) {
                // 处理任务异常 - 在实际应用中可能需要日志记录
                m_completedTasks.fetch_add(1, std::memory_order_relaxed);

                // 重新抛出异常或记录
                // throw TaskException("Task execution failed with exception");
            }

            // 计算任务执行时间
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    endTime - startTime);

            // 在实际应用中这里可以记录任务执行时间用于性能分析

            // 减少活动线程计数
            m_activeThreads.fetch_sub(1, std::memory_order_relaxed);
        };
    }

    /**
     * @brief 统计信息收集线程
     * @param stoken 停止令牌
     */
    void statsLoop(std::stop_token stoken);
};

}  // namespace atom::async

#endif  // ATOM_ASYNC_ASYNC_EXECUTOR_HPP
