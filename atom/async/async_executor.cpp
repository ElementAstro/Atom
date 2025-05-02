#include "async_executor.hpp"
#include <iostream>
#include <system_error>
#include <thread>

namespace atom::async {

// 构造函数
AsyncExecutor::AsyncExecutor(Configuration config)
    : m_config(std::move(config)),
      // C++20 信号量初始化 - 初始值为0
      m_taskSemaphore(0) {
    // 确保线程数的合理性
    if (m_config.minThreads < 1) m_config.minThreads = 1;
    if (m_config.maxThreads < m_config.minThreads) m_config.maxThreads = m_config.minThreads;
    
    // 为每个线程预先创建任务窃取队列
    if (m_config.useWorkStealing) {
        m_perThreadQueues.reserve(m_config.maxThreads);
        for (size_t i = 0; i < m_config.maxThreads; ++i) {
            m_perThreadQueues.emplace_back(std::make_unique<WorkStealingQueue>());
        }
    }
}

// 移动构造函数
AsyncExecutor::AsyncExecutor(AsyncExecutor&& other) noexcept
    : m_config(std::move(other.m_config)),
      m_isRunning(other.m_isRunning.load(std::memory_order_acquire)),
      m_activeThreads(other.m_activeThreads.load(std::memory_order_relaxed)),
      m_pendingTasks(other.m_pendingTasks.load(std::memory_order_relaxed)),
      m_completedTasks(other.m_completedTasks.load(std::memory_order_relaxed)),
      // C++20 信号量不可复制，但可以移动
      m_taskSemaphore(0) {
    
    std::unique_lock lock1(m_queueMutex, std::defer_lock);
    std::unique_lock lock2(other.m_queueMutex, std::defer_lock);
    std::lock(lock1, lock2); // 避免死锁
    
    m_taskQueue = std::move(other.m_taskQueue);
    m_perThreadQueues = std::move(other.m_perThreadQueues);

    // 注意: m_threads 和 m_statsThread 不是线程安全的移动
    // 我们确保其他实例不再持有这些线程
    other.stop(); // 停止其他实例的线程
    
    if (m_isRunning) {
        start(); // 如果需要，启动当前实例的线程
    }
}

// 移动赋值操作符
AsyncExecutor& AsyncExecutor::operator=(AsyncExecutor&& other) noexcept {
    if (this != &other) {
        stop(); // 停止当前实例的所有线程
        
        m_config = std::move(other.m_config);
        m_isRunning.store(other.m_isRunning.load(std::memory_order_acquire), 
                          std::memory_order_release);
        m_activeThreads.store(other.m_activeThreads.load(std::memory_order_relaxed), 
                            std::memory_order_relaxed);
        m_pendingTasks.store(other.m_pendingTasks.load(std::memory_order_relaxed), 
                           std::memory_order_relaxed);
        m_completedTasks.store(other.m_completedTasks.load(std::memory_order_relaxed), 
                             std::memory_order_relaxed);
        
        std::unique_lock lock1(m_queueMutex, std::defer_lock);
        std::unique_lock lock2(other.m_queueMutex, std::defer_lock);
        std::lock(lock1, lock2); // 避免死锁
        
        m_taskQueue = std::move(other.m_taskQueue);
        m_perThreadQueues = std::move(other.m_perThreadQueues);
        
        // 确保其他实例不再持有线程
        other.stop();
        
        if (m_isRunning) {
            start(); // 如果需要，启动当前实例的线程
        }
    }
    return *this;
}

// 析构函数
AsyncExecutor::~AsyncExecutor() {
    stop();
}

// 启动线程池
void AsyncExecutor::start() {
    if (m_isRunning.exchange(true, std::memory_order_acq_rel)) {
        return; // 已经在运行
    }
    
    try {
        // 创建工作线程
        for (size_t i = 0; i < m_config.minThreads; ++i) {
            m_threads.emplace_back([this, id = i](std::stop_token stoken) {
                workerLoop(id, stoken);
            });
        }
        
        // 启动统计信息收集线程
        if (m_config.statInterval.count() > 0) {
            m_statsThread = std::jthread([this](std::stop_token stoken) {
                statsLoop(stoken);
            });
        }
    } catch (...) {
        // 如果线程创建失败，确保清理资源
        stop();
        throw;
    }
}

// 停止线程池
void AsyncExecutor::stop() {
    if (!m_isRunning.exchange(false, std::memory_order_acq_rel)) {
        return; // 已经停止
    }

    // 使用 C++20 特性 - jthread 自动停止
    m_threads.clear();
    
    if (m_statsThread.joinable()) {
        m_statsThread = {}; // Reset the jthread
    }

    // 清理任务队列
    std::lock_guard lock(m_queueMutex);
    while (!m_taskQueue.empty()) {
        m_taskQueue.pop();
    }

    // 重置计数器
    m_pendingTasks.store(0, std::memory_order_relaxed);
    m_activeThreads.store(0, std::memory_order_relaxed);
}

// 将任务添加到队列
void AsyncExecutor::enqueueTask(std::function<void()> task, int priority) {
    if (!task) {
        throw ExecutorException("Cannot enqueue empty task");
    }
    
    // 增加待处理任务计数
    m_pendingTasks.fetch_add(1, std::memory_order_relaxed);
    
    // 如果启用了工作窃取，尝试分配给最不忙的线程队列
    if (m_config.useWorkStealing && !m_perThreadQueues.empty()) {
        // 找到最短的队列用于负载均衡
        size_t minQueueIndex = 0;
        size_t minQueueSize = SIZE_MAX;
        
        for (size_t i = 0; i < m_perThreadQueues.size(); ++i) {
            auto& queue = *m_perThreadQueues[i];
            std::lock_guard queueLock(queue.mutex);
            if (queue.tasks.size() < minQueueSize) {
                minQueueSize = queue.tasks.size();
                minQueueIndex = i;
                
                // 如果找到空队列，立即使用
                if (minQueueSize == 0) {
                    break;
                }
            }
        }
        
        // 添加任务到选择的队列
        auto& targetQueue = *m_perThreadQueues[minQueueIndex];
        {
            std::lock_guard queueLock(targetQueue.mutex);
            targetQueue.tasks.push_back({std::move(task), priority});
        }
    } else {
        // 使用全局队列
        {
            std::lock_guard lock(m_queueMutex);
            m_taskQueue.push({std::move(task), priority});
        }
    }
    
    // 增加信号量计数，并通知等待的线程
    m_taskSemaphore.release();
    m_condition.notify_one();
}

// 线程工作循环
void AsyncExecutor::workerLoop(size_t threadId, std::stop_token stoken) {
    try {
        // 设置线程亲和性（如果配置启用）
        if (m_config.pinThreads) {
            setThreadAffinity(threadId);
        }

        // 设置线程优先级（如果配置启用）
        if (m_config.setPriority) {
            setThreadPriority(std::this_thread::native_handle());
        }
        
        // 主工作循环
        while (!stoken.stop_requested()) {
            // 尝试获取任务
            auto task = dequeueTask(threadId);
            
            // 如果没有任务，尝试从其他线程窃取
            if (!task && m_config.useWorkStealing) {
                task = stealTask(threadId);
            }
            
            // 如果有任务，执行它
            if (task) {
                try {
                    task->func();
                } catch (...) {
                    // 在实际应用中记录异常
                }
                m_pendingTasks.fetch_sub(1, std::memory_order_relaxed);
            } else {
                // 没有任务，等待信号量或停止信号
                if (!m_taskSemaphore.try_acquire_for(m_config.threadIdleTimeout)) {
                    // 超时，如果当前线程数大于最小线程数，可以退出
                    if (m_threads.size() > m_config.minThreads) {
                        break; // 线程将终止
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        // 记录线程异常并终止
        std::cerr << "Thread " << threadId << " encountered an exception: " 
                  << e.what() << std::endl;
    } catch (...) {
        // 记录未知异常并终止
        std::cerr << "Thread " << threadId << " encountered an unknown exception." << std::endl;
    }
}

// 从队列获取任务
std::optional<AsyncExecutor::Task> AsyncExecutor::dequeueTask(size_t threadId) {
    // 先检查线程特定队列（如果启用了工作窃取）
    if (m_config.useWorkStealing && threadId < m_perThreadQueues.size()) {
        auto& queue = *m_perThreadQueues[threadId];
        std::lock_guard queueLock(queue.mutex);
        
        if (!queue.tasks.empty()) {
            auto task = std::move(queue.tasks.front());
            queue.tasks.pop_front();
            return task;
        }
    }
    
    // 否则从主队列获取
    std::unique_lock lock(m_queueMutex);
    
    if (!m_taskQueue.empty()) {
        auto task = m_taskQueue.top();
        m_taskQueue.pop();
        return task;
    }
    
    return std::nullopt;
}

// 尝试从其他线程窃取任务
std::optional<AsyncExecutor::Task> AsyncExecutor::stealTask(size_t currentId) {
    if (!m_config.useWorkStealing || m_perThreadQueues.empty()) {
        return std::nullopt;
    }
    
    // 从其他线程的队列尾部窃取任务（以减少竞争）
    size_t queueCount = m_perThreadQueues.size();
    size_t startIndex = (currentId + 1) % queueCount; // 从下一个线程开始
    
    for (size_t i = 0; i < queueCount - 1; ++i) {
        size_t index = (startIndex + i) % queueCount;
        auto& queue = *m_perThreadQueues[index];
        
        std::lock_guard queueLock(queue.mutex);
        if (!queue.tasks.empty()) {
            // 从队列尾部窃取（通常是较大的工作单元）
            auto task = std::move(queue.tasks.back());
            queue.tasks.pop_back();
            return task;
        }
    }
    
    return std::nullopt;
}

// 设置线程亲和性
void AsyncExecutor::setThreadAffinity(size_t threadId) {
#if defined(ATOM_PLATFORM_WINDOWS)
    // Windows平台实现
    DWORD_PTR mask = (static_cast<DWORD_PTR>(1) << (threadId % std::thread::hardware_concurrency()));
    SetThreadAffinityMask(GetCurrentThread(), mask);
#elif defined(ATOM_PLATFORM_LINUX)
    // Linux平台实现
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(threadId % std::thread::hardware_concurrency(), &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#elif defined(ATOM_PLATFORM_MACOS)
    // macOS平台实现更复杂，有特殊API
    thread_affinity_policy_data_t policy = { 
        static_cast<integer_t>(threadId % std::thread::hardware_concurrency())
    };
    thread_policy_set(pthread_mach_thread_np(pthread_self()),
                     THREAD_AFFINITY_POLICY,
                     (thread_policy_t)&policy,
                     THREAD_AFFINITY_POLICY_COUNT);
#endif
}

// 设置线程优先级
void AsyncExecutor::setThreadPriority(std::thread::native_handle_type handle) {
#if defined(ATOM_PLATFORM_WINDOWS)
    // Windows平台实现
    int winPriority = THREAD_PRIORITY_NORMAL;
    if (m_config.threadPriority > 0) {
        winPriority = THREAD_PRIORITY_ABOVE_NORMAL;
    } else if (m_config.threadPriority < 0) {
        winPriority = THREAD_PRIORITY_BELOW_NORMAL;
    }
    SetThreadPriority(handle, winPriority);
#elif defined(ATOM_PLATFORM_LINUX)
    // Linux平台实现
    int policy;
    struct sched_param param;
    
    pthread_getschedparam(handle, &policy, &param);
    
    // 调整优先级
    int min_prio = sched_get_priority_min(policy);
    int max_prio = sched_get_priority_max(policy);
    int prio_range = max_prio - min_prio;
    
    // 映射自定义优先级到系统范围
    param.sched_priority = min_prio + ((prio_range * (m_config.threadPriority + 100)) / 200);
    
    pthread_setschedparam(handle, policy, &param);
#elif defined(ATOM_PLATFORM_MACOS)
    // macOS平台实现
    struct sched_param param;
    int policy;
    
    pthread_getschedparam(handle, &policy, &param);
    
    // 调整优先级
    int min_prio = sched_get_priority_min(policy);
    int max_prio = sched_get_priority_max(policy);
    int prio_range = max_prio - min_prio;
    
    // 映射自定义优先级到系统范围
    param.sched_priority = min_prio + ((prio_range * (m_config.threadPriority + 100)) / 200);
    
    pthread_setschedparam(handle, policy, &param);
#endif
}

// 统计信息收集线程
void AsyncExecutor::statsLoop(std::stop_token stoken) {
    while (!stoken.stop_requested()) {
        // 统计信息收集在此实现
        size_t active = m_activeThreads.load(std::memory_order_relaxed);
        size_t pending = m_pendingTasks.load(std::memory_order_relaxed);
        size_t completed = m_completedTasks.load(std::memory_order_relaxed);
        
        // 在实际应用中，可能将这些数据发送到监控系统或日志
        
        // 使用C++20的新特性 jthread 和 stop_token 的条件等待
        std::this_thread::sleep_for(m_config.statInterval);
    }
}

} // namespace atom::async