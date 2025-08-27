#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <memory>
#include <sstream>
#include <string>
#include <cassert>

// Include the headers for the classes we're testing
#include "atom/async/timer.hpp"
#include "atom/async/sync/slot.hpp"

using namespace std::chrono_literals;
using namespace atom::async;

// Helper function to capture output
std::string captureOutput(std::function<void()> func) {
    std::ostringstream oss;
    std::streambuf* orig = std::cout.rdbuf();
    std::cout.rdbuf(oss.rdbuf());
    
    func();
    
    std::cout.rdbuf(orig);
    return oss.str();
}

// Timer test
void testTimer() {
    std::cout << "Testing Timer..." << std::endl;

    atom::async::Timer timer;
    std::atomic<int> funcCalls{0};

    timer.setInterval([&funcCalls]() { funcCalls.fetch_add(1); }, 50, 5, 0);

    // Wait for all executions to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    std::cout << "Timer test: Expected 5 calls, got " << funcCalls.load() << std::endl;
    assert(funcCalls.load() == 5);
    std::cout << "✅ Timer test PASSED" << std::endl;
}

// Signal test
void testScopedSignal() {
    std::cout << "Testing ScopedSignal..." << std::endl;

    ScopedSignal<int> dynamicSignal;
    auto slot = std::make_shared<std::function<void(int)>>(
        [](int x) { std::cout << "Dynamic Signal: " << x << '\n'; });

    std::string output1 = captureOutput([&]() {
        dynamicSignal.connect(slot);
        dynamicSignal.emit(500);
    });

    std::cout << "First emit output: '" << output1 << "'" << std::endl;
    assert(output1 == "Dynamic Signal: 500\n");

    // ScopedSignal automatically disconnects when slot goes out of scope
    slot.reset(); // Manually reset to simulate scope exit

    std::string output2 = captureOutput([&]() {
        dynamicSignal.emit(600);  // Should not be called
    });

    std::cout << "Second emit output: '" << output2 << "'" << std::endl;
    assert(output2 == "");
    std::cout << "✅ ScopedSignal test PASSED" << std::endl;
}

int main() {
    std::cout << "Running test fixes verification..." << std::endl;

    try {
        testTimer();
        testScopedSignal();
        std::cout << "\n🎉 All tests PASSED!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cout << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "\n❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}
