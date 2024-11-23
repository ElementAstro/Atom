#include "atom/connection/async_fifoclient.hpp"

#include <iostream>
#include <thread>

using namespace atom::async::connection;

int main() {
    // Create a FifoClient object with the specified FIFO path
    FifoClient fifoClient("/tmp/my_fifo");

    // Check if the FIFO is open
    if (fifoClient.isOpen()) {
        std::cout << "FIFO is open" << std::endl;
    } else {
        std::cout << "FIFO is not open" << std::endl;
    }

    // Write data to the FIFO with a timeout
    bool writeSuccess =
        fifoClient.write("Hello, FIFO!", std::chrono::milliseconds(500));
    if (writeSuccess) {
        std::cout << "Data written to FIFO successfully" << std::endl;
    } else {
        std::cout << "Failed to write data to FIFO" << std::endl;
    }

    // Read data from the FIFO with a timeout
    auto data = fifoClient.read(std::chrono::milliseconds(500));
    if (data) {
        std::cout << "Data read from FIFO: " << *data << std::endl;
    } else {
        std::cout << "Failed to read data from FIFO" << std::endl;
    }

    // Close the FIFO
    fifoClient.close();
    std::cout << "FIFO closed" << std::endl;

    // Check if the FIFO is open after closing
    if (fifoClient.isOpen()) {
        std::cout << "FIFO is still open" << std::endl;
    } else {
        std::cout << "FIFO is closed" << std::endl;
    }

    return 0;
}