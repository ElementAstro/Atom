#include <iostream>
#include <thread>
#include <chrono>
#include "atom/async/timer.hpp"

void simpleTask() {
    std::cout << "Simple task executed!" << std::endl;
}

int main() {
    try {
        std::cout << "Creating timer..." << std::endl;
        atom::async::Timer timer;
        std::cout << "Timer created successfully!" << std::endl;

        std::cout << "About to call setTimeout with delay 1000..." << std::endl;

        // Test with explicit unsigned int cast
        unsigned int delay = 1000;
        std::cout << "Delay variable value: " << delay << std::endl;

        auto future = timer.setTimeout(simpleTask, delay);
        std::cout << "setTimeout called successfully!" << std::endl;

        std::cout << "Waiting for task completion..." << std::endl;
        future.wait();

        std::cout << "Test completed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        return 1;
    }
}
