#include <iostream>
#include "atom/memory/memory_pool.hpp"

int main() {
    std::cout << "Testing lock-free memory pool..." << std::endl;

    try {
        // Test lock-free pool like in AllocationStrategies test
        atom::memory::MemoryPool<64, 1024, true> pool;
        std::cout << "Lock-free memory pool created successfully" << std::endl;

        std::vector<void*> ptrs;

        // Allocate several blocks
        for (int i = 0; i < 10; ++i) {
            std::cout << "Allocating block " << i << "..." << std::endl;
            void* ptr = pool.allocate();
            std::cout << "Block " << i << " allocated: " << ptr << std::endl;
            if (ptr == nullptr) {
                std::cout << "ERROR: Allocation " << i << " returned null!" << std::endl;
                return 1;
            }
            ptrs.push_back(ptr);
        }

        std::cout << "All allocations successful" << std::endl;

        // Deallocate every other block to create fragmentation
        for (size_t i = 1; i < ptrs.size(); i += 2) {
            std::cout << "Deallocating block " << i << ": " << ptrs[i] << std::endl;
            pool.deallocate(ptrs[i]);
        }

        // Try to allocate again
        std::cout << "Allocating new block..." << std::endl;
        void* new_ptr = pool.allocate();
        std::cout << "New block allocated: " << new_ptr << std::endl;
        if (new_ptr == nullptr) {
            std::cout << "ERROR: New allocation returned null!" << std::endl;
            return 1;
        }

        // Cleanup
        for (size_t i = 0; i < ptrs.size(); i += 2) {
            pool.deallocate(ptrs[i]);
        }
        pool.deallocate(new_ptr);

        std::cout << "Lock-free test completed successfully" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "Exception caught in lock-free test: " << e.what() << std::endl;
        return 1;
    }

    // Test the second part of AllocationStrategies test
    std::cout << "\nTesting non-lock-free memory pool..." << std::endl;
    try {
        atom::memory::MemoryPool<128, 512, false> pool;
        std::cout << "Non-lock-free memory pool created successfully" << std::endl;

        // Test exactly like the AllocationStrategies test
        std::vector<void*> ptrs;

        // Allocate several blocks
        for (int i = 0; i < 10; ++i) {
            std::cout << "Allocating non-lock-free block " << i << "..." << std::endl;
            void* ptr = pool.allocate();
            std::cout << "Non-lock-free block " << i << " allocated: " << ptr << std::endl;
            if (ptr == nullptr) {
                std::cout << "ERROR: Non-lock-free allocation " << i << " returned null!" << std::endl;
                return 1;
            }
            ptrs.push_back(ptr);
        }

        std::cout << "All non-lock-free allocations successful" << std::endl;

        // Deallocate every other block to create fragmentation
        for (size_t i = 1; i < ptrs.size(); i += 2) {
            std::cout << "Deallocating non-lock-free block " << i << ": " << ptrs[i] << std::endl;
            pool.deallocate(ptrs[i]);
        }

        // Try to allocate again
        std::cout << "Allocating new non-lock-free block..." << std::endl;
        void* new_ptr = pool.allocate();
        std::cout << "New non-lock-free block allocated: " << new_ptr << std::endl;
        if (new_ptr == nullptr) {
            std::cout << "ERROR: New non-lock-free allocation returned null!" << std::endl;
            return 1;
        }

        // Cleanup
        for (size_t i = 0; i < ptrs.size(); i += 2) {
            pool.deallocate(ptrs[i]);
        }
        pool.deallocate(new_ptr);

        std::cout << "Non-lock-free test completed successfully" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "Exception caught in non-lock-free test: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\nAll tests completed successfully!" << std::endl;
    return 0;
}
