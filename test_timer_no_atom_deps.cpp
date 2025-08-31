#include <iostream>
#include <chrono>
#include <thread>
#include <functional>
#include <future>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>

// Completely isolated Timer implementation without any Atom dependencies
class IsolatedTimer {
private:
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_paused{false};
    std::mutex m_mutex;
    std::condition_variable m_cond;
    std::jthread m_thread;
    
    struct Task {
        std::function<void()> func;
        std::chrono::steady_clock::time_point executeTime;
        
        bool operator<(const Task& other) const {
            return executeTime > other.executeTime; // Min heap
        }
    };
    
    std::priority_queue<Task> m_taskQueue;
    
    void run() {
        while (!m_stop.load()) {
            std::unique_lock<std::mutex> lock(m_mutex);
            
            if (m_taskQueue.empty()) {
                m_cond.wait(lock, [this] { return !m_taskQueue.empty() || m_stop.load(); });
                continue;
            }
            
            auto now = std::chrono::steady_clock::now();
            auto& nextTask = const_cast<Task&>(m_taskQueue.top());
            
            if (nextTask.executeTime <= now) {
                auto task = std::move(nextTask.func);
                m_taskQueue.pop();
                lock.unlock();
                
                try {
                    task();
                } catch (...) {
                    // Suppress task exceptions
                }
            } else {
                auto waitTime = nextTask.executeTime - now;
                m_cond.wait_for(lock, waitTime);
            }
        }
    }
    
    void ensureThreadStarted() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_thread.joinable() && !m_stop.load()) {
            m_thread = std::jthread([this] { run(); });
        }
    }

public:
    IsolatedTimer() = default;
    
    ~IsolatedTimer() {
        stop();
    }
    
    template<typename Function>
    auto setTimeout(Function&& func, unsigned int delay) -> std::future<void> {
        auto task = std::make_shared<std::packaged_task<void()>>(
            std::forward<Function>(func)
        );
        
        std::future<void> result = task->get_future();
        
        ensureThreadStarted();
        
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_taskQueue.emplace(Task{
                [task]() { (*task)(); },
                std::chrono::steady_clock::now() + std::chrono::milliseconds(delay)
            });
        }
        
        m_cond.notify_one();
        return result;
    }
    
    void stop() {
        m_stop.store(true);
        m_cond.notify_all();
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }
};

void testTask() {
    std::cout << "Isolated timer task executed successfully!" << std::endl;
}

int main() {
    std::cout << "=== ISOLATED TIMER TEST ===" << std::endl;
    
    try {
        std::cout << "Step 1: Creating isolated timer..." << std::endl;
        IsolatedTimer timer;
        
        std::cout << "Step 2: Setting timeout..." << std::endl;
        auto future = timer.setTimeout(testTask, 1000);
        
        std::cout << "Step 3: Waiting for task..." << std::endl;
        future.wait();
        
        std::cout << "Step 4: Test completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "ERROR: Exception caught: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "ERROR: Unknown exception caught!" << std::endl;
        return 1;
    }
}
