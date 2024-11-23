#include "atom/async/threadlocal.hpp"

#include <iostream>
#include <thread>

using namespace atom::async;

// Example initializer function
int initializer() { return 42; }

// Example function to demonstrate ThreadLocal usage
void threadFunction(ThreadLocal<int>& threadLocal) {
    std::cout << "Thread ID: " << std::this_thread::get_id()
              << ", Initial Value: " << *threadLocal << std::endl;
    threadLocal.reset(100);
    std::cout << "Thread ID: " << std::this_thread::get_id()
              << ", Updated Value: " << *threadLocal << std::endl;
}

int main() {
    // Create a ThreadLocal object with an initializer
    ThreadLocal<int> threadLocal(initializer);

    // Start multiple threads to demonstrate thread-local storage
    std::thread t1(threadFunction, std::ref(threadLocal));
    std::thread t2(threadFunction, std::ref(threadLocal));
    std::thread t3(threadFunction, std::ref(threadLocal));

    // Wait for all threads to finish
    t1.join();
    t2.join();
    t3.join();

    // Check if the main thread has a value
    if (threadLocal.hasValue()) {
        std::cout << "Main thread value: " << *threadLocal << std::endl;
    } else {
        std::cout << "Main thread has no value" << std::endl;
    }

    // Reset the value in the main thread
    threadLocal.reset(200);
    std::cout << "Main thread updated value: " << *threadLocal << std::endl;

    // Clear the thread-local storage for the main thread
    threadLocal.clear();
    if (threadLocal.hasValue()) {
        std::cout << "Main thread value after clear: " << *threadLocal
                  << std::endl;
    } else {
        std::cout << "Main thread has no value after clear" << std::endl;
    }

    // Execute a function for each thread-local value
    threadLocal.forEach([](int& value) {
        std::cout << "Thread-local value: " << value << std::endl;
    });

    return 0;
}