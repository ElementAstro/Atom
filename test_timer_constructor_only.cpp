#include <iostream>

// Include the timer header at global scope
#include "atom/async/timer.hpp"

int main() {
    std::cout << "=== TIMER CONSTRUCTOR TEST ===" << std::endl;
    
    try {
        std::cout << "Step 1: About to create Timer object..." << std::endl;
        
        // Just try to create a Timer object, don't call any methods
        atom::async::Timer timer;
        
        std::cout << "Step 2: Timer object created successfully!" << std::endl;
        std::cout << "Step 3: Test completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "ERROR: Exception caught: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "ERROR: Unknown exception caught!" << std::endl;
        return 1;
    }
}
