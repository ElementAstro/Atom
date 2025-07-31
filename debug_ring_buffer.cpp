#include <iostream>
#include "atom/memory/ring.hpp"

int main() {
    std::cout << "=== Ring Buffer Debug ===\n";
    
    // Create ring buffer with lock-free reads enabled
    atom::memory::RingBufferConfig config;
    config.enable_lock_free_reads = true;
    
    atom::memory::RingBuffer<int> buffer(10, config);
    
    std::cout << "Initial state:\n";
    std::cout << "  sizeLockFree(): " << buffer.sizeLockFree() << "\n";
    std::cout << "  size(): " << buffer.size() << "\n";
    std::cout << "  emptyLockFree(): " << buffer.emptyLockFree() << "\n";
    std::cout << "  empty(): " << buffer.empty() << "\n";
    
    std::cout << "\nPushing 3 items...\n";
    buffer.push(1);
    std::cout << "After push(1): sizeLockFree() = " << buffer.sizeLockFree() << ", size() = " << buffer.size() << "\n";
    
    buffer.push(2);
    std::cout << "After push(2): sizeLockFree() = " << buffer.sizeLockFree() << ", size() = " << buffer.size() << "\n";
    
    buffer.push(3);
    std::cout << "After push(3): sizeLockFree() = " << buffer.sizeLockFree() << ", size() = " << buffer.size() << "\n";
    
    std::cout << "\nFinal state:\n";
    std::cout << "  sizeLockFree(): " << buffer.sizeLockFree() << "\n";
    std::cout << "  size(): " << buffer.size() << "\n";
    std::cout << "  emptyLockFree(): " << buffer.emptyLockFree() << "\n";
    std::cout << "  empty(): " << buffer.empty() << "\n";
    
    return 0;
}
