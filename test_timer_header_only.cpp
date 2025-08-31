#include <iostream>

// Include the timer header at global scope, not inside main()
#include "atom/async/timer.hpp"

int main() {
    std::cout << "=== HEADER INCLUSION TEST ===" << std::endl;

    try {
        std::cout << "Step 1: Basic cout test..." << std::endl;
        std::cout << "Step 2: Timer header included successfully!" << std::endl;
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
