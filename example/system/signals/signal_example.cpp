#include "atom/system/signal.hpp"

#include <csignal>
#include <iostream>

void signalHandler(int signal) {
    std::cout << "Received signal: " << signal << std::endl;
}

void safeSignalHandler(int signal) {
    std::cout << "Safely received signal: " << signal << std::endl;
}

int main() {
    // Get the singleton instance of the SignalHandlerRegistry
    auto& registry = SignalHandlerRegistry::getInstance();

    // Set a signal handler for SIGINT with default priority
    registry.setSignalHandler(SIGINT, signalHandler);
    std::cout << "Set signal handler for SIGINT" << std::endl;

    // Set a signal handler for SIGTERM with higher priority
    registry.setSignalHandler(SIGTERM, signalHandler, 10);
    std::cout << "Set signal handler for SIGTERM with priority 10" << std::endl;

    // Remove the signal handler for SIGINT
    registry.removeSignalHandler(SIGINT, signalHandler);
    std::cout << "Removed signal handler for SIGINT" << std::endl;

    // Set handlers for standard crash signals
    registry.setStandardCrashHandlerSignals(signalHandler);
    std::cout << "Set standard crash signal handlers" << std::endl;

    // Get the singleton instance of the SafeSignalManager
    auto& safeManager = SafeSignalManager::getInstance();

    // Add a safe signal handler for SIGINT with default priority
    safeManager.addSafeSignalHandler(SIGINT, safeSignalHandler);
    std::cout << "Added safe signal handler for SIGINT" << std::endl;

    // Add a safe signal handler for SIGTERM with higher priority
    safeManager.addSafeSignalHandler(SIGTERM, safeSignalHandler, 10);
    std::cout << "Added safe signal handler for SIGTERM with priority 10"
              << std::endl;

    // Remove the safe signal handler for SIGINT
    safeManager.removeSafeSignalHandler(SIGINT, safeSignalHandler);
    std::cout << "Removed safe signal handler for SIGINT" << std::endl;

    // Clear the signal queue
    safeManager.clearSignalQueue();
    std::cout << "Cleared signal queue" << std::endl;

    // Simulate sending a signal
    raise(SIGTERM);

    // Wait for a moment to allow signal processing
    std::this_thread::sleep_for(std::chrono::seconds(1));

    return 0;
}
