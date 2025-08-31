#include <iostream>
#include <thread>
#include <chrono>

// Include only the necessary headers
#include "atom/async/timer.hpp"

void simpleTask() {
    std::cout << "Task executed successfully!" << std::endl;
}

int main() {
    std::cout << "=== MINIMAL TIMER TEST ===" << std::endl;
    
    try {
        std::cout << "Step 1: Creating Timer object..." << std::endl;
        atom::async::Timer timer;
        std::cout << "Step 2: Timer created successfully!" << std::endl;
        
        std::cout << "Step 3: Calling setTimeout..." << std::endl;
        auto future = timer.setTimeout(simpleTask, 1000u);
        std::cout << "Step 4: setTimeout called successfully!" << std::endl;
        
        std::cout << "Step 5: Waiting for task completion..." << std::endl;
        future.wait();
        std::cout << "Step 6: Task completed!" << std::endl;
        
        std::cout << "Step 7: Test completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "ERROR: Exception caught: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "ERROR: Unknown exception caught!" << std::endl;
        return 1;
    }
}
