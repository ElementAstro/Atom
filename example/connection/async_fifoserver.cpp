#include "atom/connection/async_fifoserver.hpp"

#include <iostream>
#include <thread>

using namespace atom::async::connection;

int main() {
    // Create a FifoServer object with the specified FIFO path
    FifoServer fifoServer("/tmp/my_fifo");

    // Start the server to listen for messages
    fifoServer.start();
    std::cout << "FIFO server started" << std::endl;

    // Check if the server is running
    if (fifoServer.isRunning()) {
        std::cout << "FIFO server is running" << std::endl;
    } else {
        std::cout << "FIFO server is not running" << std::endl;
    }

    // Simulate some work by sleeping for a few seconds
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Stop the server
    fifoServer.stop();
    std::cout << "FIFO server stopped" << std::endl;

    // Check if the server is running after stopping
    if (fifoServer.isRunning()) {
        std::cout << "FIFO server is still running" << std::endl;
    } else {
        std::cout << "FIFO server is not running" << std::endl;
    }

    return 0;
}