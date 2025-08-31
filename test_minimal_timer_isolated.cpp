#include <iostream>
#include <chrono>
#include <thread>
#include <functional>
#include <future>

// Minimal Timer implementation without any Atom dependencies
class MinimalTimer {
public:
    MinimalTimer() = default;
    
    template<typename Function>
    auto setTimeout(Function&& func, unsigned int delay) -> std::future<void> {
        auto task = std::make_shared<std::packaged_task<void()>>(
            std::forward<Function>(func)
        );
        
        std::future<void> result = task->get_future();
        
        std::thread([task, delay]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            (*task)();
        }).detach();
        
        return result;
    }
};

void testTask() {
    std::cout << "Timer task executed successfully!" << std::endl;
}

int main() {
    std::cout << "=== MINIMAL TIMER TEST ===" << std::endl;
    
    try {
        std::cout << "Step 1: Creating minimal timer..." << std::endl;
        MinimalTimer timer;
        
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
