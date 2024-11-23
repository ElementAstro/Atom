#include "atom/memory/memory.hpp"

#include <iostream>

int main() {
    // Create a MemoryPool for int with a block size of 4096 bytes
    MemoryPool<int> pool;

    // Allocate memory for 10 integers
    int* p1 = pool.allocate(10);
    std::cout << "Allocated memory for 10 integers." << std::endl;

    // Deallocate memory for 10 integers
    pool.deallocate(p1, 10);
    std::cout << "Deallocated memory for 10 integers." << std::endl;

    // Allocate memory for 20 integers
    int* p2 = pool.allocate(20);
    std::cout << "Allocated memory for 20 integers." << std::endl;

    // Get the total memory allocated by the pool
    size_t totalAllocated = pool.getTotalAllocated();
    std::cout << "Total memory allocated: " << totalAllocated << " bytes."
              << std::endl;

    // Get the total memory available in the pool
    size_t totalAvailable = pool.getTotalAvailable();
    std::cout << "Total memory available: " << totalAvailable << " bytes."
              << std::endl;

    // Reset the memory pool, freeing all allocated memory
    pool.reset();
    std::cout << "Memory pool reset." << std::endl;

    // Verify the pool is empty after reset
    totalAllocated = pool.getTotalAllocated();
    totalAvailable = pool.getTotalAvailable();
    std::cout << "Total memory allocated after reset: " << totalAllocated
              << " bytes." << std::endl;
    std::cout << "Total memory available after reset: " << totalAvailable
              << " bytes." << std::endl;

    return 0;
}