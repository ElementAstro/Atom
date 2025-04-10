/*
 * timer.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-14

Description: Timer class for C++

**************************************************/

#include "timer.hpp"
#include <cstddef>
#include <stdexcept>

#include "atom/error/exception.hpp"

namespace atom::async {

TimerTask::TimerTask(std::function<void()> func, unsigned int delay,
                     int repeatCount, int priority) noexcept(false)
    : m_func(func),
      m_delay(delay),
      m_repeatCount(repeatCount),
      m_priority(priority) {
    
    if (!func) {
        throw std::invalid_argument("TimerTask: Function cannot be null");
    }
    
    if (delay == 0) {
        throw std::invalid_argument("TimerTask: Delay must be greater than 0");
    }
    
    if (repeatCount < -1) {
        throw std::invalid_argument("TimerTask: RepeatCount must be >= -1");
    }
    
    m_nextExecutionTime =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(m_delay);
}

auto TimerTask::operator<(const TimerTask &other) const noexcept -> bool {
    if (m_priority != other.m_priority) {
        return m_priority > other.m_priority;
    }
    return m_nextExecutionTime > other.m_nextExecutionTime;
}

void TimerTask::run() noexcept(false) {
    if (!m_func) {
        throw std::logic_error("TimerTask::run: Function is null");
    }
    
    try {
        m_func();
    } catch (const std::exception &e) {
        THROW_RUNTIME_ERROR("Failed to run timer task: ", e.what());
    } catch (...) {
        THROW_RUNTIME_ERROR("Failed to run timer task: Unknown error");
    }
    
    if (m_repeatCount > 0) {
        --m_repeatCount;
        if (m_repeatCount > 0) {
            m_nextExecutionTime = std::chrono::steady_clock::now() +
                                  std::chrono::milliseconds(m_delay);
        }
    }
}

auto TimerTask::getNextExecutionTime() const noexcept -> std::chrono::steady_clock::time_point {
    return m_nextExecutionTime;
}

void Timer::validateTaskParams(unsigned int delay, int repeatCount) noexcept(false) {
    if (delay == 0) {
        throw std::invalid_argument("Timer: Delay must be greater than 0");
    }
    
    if (repeatCount < -1) {
        throw std::invalid_argument("Timer: RepeatCount must be >= -1");
    }
}

Timer::Timer() noexcept(false) {
    try {
        m_thread = std::jthread(&Timer::run, this);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to create timer thread: ") + e.what());
    }
}

Timer::~Timer() noexcept {
    try {
        stop();
        // std::jthread automatically joins in destructor - no need to manually join
    } catch (...) {
        // Suppress exceptions in destructor
    }
}

void Timer::cancelAllTasks() noexcept {
    try {
#ifdef ATOM_USE_BOOST_LOCKFREE
        m_taskContainer.clear();
        m_hasCurrentTask.store(false, std::memory_order_release);
#else
        std::scoped_lock lock(m_mutex);
        m_taskQueue = std::priority_queue<TimerTask>();
#endif
        m_cond.notify_all();
    } catch (...) {
        // Ensure noexcept guarantee
    }
}

void Timer::pause() noexcept {
    m_paused.store(true, std::memory_order_release);
}

void Timer::resume() noexcept {
    m_paused.store(false, std::memory_order_release);
    m_cond.notify_all();
}

void Timer::stop() noexcept {
    m_stop.store(true, std::memory_order_release);
    m_cond.notify_all();
}

auto Timer::now() const noexcept -> std::chrono::steady_clock::time_point {
    return std::chrono::steady_clock::now();
}

void Timer::run() noexcept {
    try {
        while (!m_stop.load(std::memory_order_acquire)) {
#ifdef ATOM_USE_BOOST_LOCKFREE
            // Boost.lockfree implementation
            
            // Handle current task first if there is one
            if (m_hasCurrentTask.load(std::memory_order_acquire)) {
                auto now = std::chrono::steady_clock::now();
                
                if (!m_paused.load(std::memory_order_acquire) && 
                    now >= m_currentTask.getNextExecutionTime()) {
                    // Clear current task flag before processing to allow new tasks to be set as current
                    m_hasCurrentTask.store(false, std::memory_order_release);
                    
                    // Execute the task
                    try {
                        m_currentTask.run();
                        
                        // Re-add the task if it needs to repeat
                        if (m_currentTask.m_repeatCount > 0) {
                            m_taskContainer.push(m_currentTask);
                        }
                        
                        // Execute callback if defined
                        std::function<void()> callback;
                        {
                            std::scoped_lock callbackLock(m_mutex);
                            callback = m_callback;
                        }
                        
                        if (callback) {
                            try {
                                callback();
                            } catch (...) {
                                // Suppress callback exceptions
                            }
                        }
                    } catch (...) {
                        // Suppress exceptions to keep the timer running
                    }
                } else {
                    // Wait until task execution time or until conditions change
                    std::unique_lock lock(m_mutex);
                    auto waitTime = m_paused.load(std::memory_order_acquire) ? 
                        std::chrono::milliseconds(100) : 
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            m_currentTask.getNextExecutionTime() - now);
                    
                    m_cond.wait_for(lock, waitTime, [this]() {
                        return m_stop.load(std::memory_order_acquire) || 
                               !m_hasCurrentTask.load(std::memory_order_acquire);
                    });
                }
            } else {
                // Try to get a new task from the queue
                TimerTask newTask;
                if (!m_paused.load(std::memory_order_acquire) && m_taskContainer.pop(newTask)) {
                    m_currentTask = newTask;
                    m_hasCurrentTask.store(true, std::memory_order_release);
                } else {
                    // No tasks available or paused, wait for a short time
                    std::unique_lock lock(m_mutex);
                    m_cond.wait_for(lock, std::chrono::milliseconds(100), [this]() {
                        return m_stop.load(std::memory_order_acquire) || 
                               !m_paused.load(std::memory_order_acquire);
                    });
                }
            }
#else
            // Original implementation with priority queue
            std::unique_lock lock(m_mutex);
            
            auto waitPredicate = [this]() {
                return m_stop.load(std::memory_order_acquire) || 
                       !m_paused.load(std::memory_order_acquire) || 
                       !m_taskQueue.empty();
            };
            
            m_cond.wait(lock, waitPredicate);
            
            if (m_stop.load(std::memory_order_acquire)) {
                break;
            }
            
            if (m_paused.load(std::memory_order_acquire) || m_taskQueue.empty()) {
                continue;
            }
            
            TimerTask task = m_taskQueue.top();
            auto now = std::chrono::steady_clock::now();
            
            if (now >= task.getNextExecutionTime()) {
                m_taskQueue.pop();
                lock.unlock();
                
                try {
                    task.run();
                    
                    if (task.m_repeatCount > 0) {
                        std::scoped_lock innerLock(m_mutex);
                        m_taskQueue.emplace(task.m_func, task.m_delay,
                                           task.m_repeatCount, task.m_priority);
                    }
                    
                    // Execute callback if defined
                    std::function<void()> callback;
                    {
                        std::scoped_lock callbackLock(m_mutex);
                        callback = m_callback;
                    }
                    
                    if (callback) {
                        try {
                            callback();
                        } catch (...) {
                            // Suppress callback exceptions
                        }
                    }
                } catch (...) {
                    // Suppress exceptions to keep the timer running
                }
            } else {
                m_cond.wait_until(lock, task.getNextExecutionTime());
            }
#endif
        }
    } catch (...) {
        // Ensure run() maintains noexcept guarantee
    }
}

auto Timer::getTaskCount() const noexcept -> size_t {
    try {
#ifdef ATOM_USE_BOOST_LOCKFREE
        // For lockfree implementation, we can't get an exact count easily
        // since it's not part of the boost::lockfree::queue API.
        // We only report if there's at least one task (current task)
        return m_hasCurrentTask.load(std::memory_order_acquire) ? 1 : 0;
#else
        std::scoped_lock lock(m_mutex);
        return m_taskQueue.size();
#endif
    } catch (...) {
        // Ensure noexcept guarantee
        return 0;
    }
}

void Timer::wait() noexcept {
    try {
#ifdef ATOM_USE_BOOST_LOCKFREE
        // For lockfree implementation, we wait until there are no tasks
        // and current task is processed
        while (!m_stop.load(std::memory_order_acquire)) {
            if (m_taskContainer.empty() && !m_hasCurrentTask.load(std::memory_order_acquire)) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
#else
        std::unique_lock lock(m_mutex);
        m_cond.wait(lock, [this]() { return m_taskQueue.empty(); });
#endif
    } catch (...) {
        // Ensure noexcept guarantee
    }
}

}  // namespace atom::async
