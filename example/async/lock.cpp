#include "atom/async/lock.hpp"

#include <iostream>
#include <thread>
#include <vector>

using namespace atom::async;

// Example function to demonstrate Spinlock usage
void spinlockExample(Spinlock& spinlock, int& counter) {
    for (int i = 0; i < 1000; ++i) {
        ScopedLock<Spinlock> lock(spinlock);
        ++counter;
    }
}

// Example function to demonstrate TicketSpinlock usage
void ticketSpinlockExample(TicketSpinlock& ticketSpinlock, int& counter) {
    for (int i = 0; i < 1000; ++i) {
        TicketSpinlock::LockGuard lock(ticketSpinlock);
        ++counter;
    }
}

// Example function to demonstrate UnfairSpinlock usage
void unfairSpinlockExample(UnfairSpinlock& unfairSpinlock, int& counter) {
    for (int i = 0; i < 1000; ++i) {
        ScopedUnfairLock<UnfairSpinlock> lock(unfairSpinlock);
        ++counter;
    }
}

int main() {
    // Spinlock example
    Spinlock spinlock;
    int spinlockCounter = 0;
    std::vector<std::thread> spinlockThreads;
    for (int i = 0; i < 10; ++i) {
        spinlockThreads.emplace_back(spinlockExample, std::ref(spinlock),
                                     std::ref(spinlockCounter));
    }
    for (auto& thread : spinlockThreads) {
        thread.join();
    }
    std::cout << "Spinlock counter: " << spinlockCounter << std::endl;

    // TicketSpinlock example
    TicketSpinlock ticketSpinlock;
    int ticketSpinlockCounter = 0;
    std::vector<std::thread> ticketSpinlockThreads;
    for (int i = 0; i < 10; ++i) {
        ticketSpinlockThreads.emplace_back(ticketSpinlockExample,
                                           std::ref(ticketSpinlock),
                                           std::ref(ticketSpinlockCounter));
    }
    for (auto& thread : ticketSpinlockThreads) {
        thread.join();
    }
    std::cout << "TicketSpinlock counter: " << ticketSpinlockCounter
              << std::endl;

    // UnfairSpinlock example
    UnfairSpinlock unfairSpinlock;
    int unfairSpinlockCounter = 0;
    std::vector<std::thread> unfairSpinlockThreads;
    for (int i = 0; i < 10; ++i) {
        unfairSpinlockThreads.emplace_back(unfairSpinlockExample,
                                           std::ref(unfairSpinlock),
                                           std::ref(unfairSpinlockCounter));
    }
    for (auto& thread : unfairSpinlockThreads) {
        thread.join();
    }
    std::cout << "UnfairSpinlock counter: " << unfairSpinlockCounter
              << std::endl;

    return 0;
}