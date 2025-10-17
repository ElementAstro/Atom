#include "atom/error/stacktrace.hpp"

#include <iostream>

void functionThatThrows() {
    // Create a StackTrace object to capture the current stack trace
    atom::error::StackTrace stackTrace;

    // Convert the stack trace to a string and print it
    std::cout << "Captured Stack Trace:\n"
              << stackTrace.toString() << std::endl;

    // Throw an exception to demonstrate stack trace capture
    throw std::runtime_error("An error occurred");
}

int main() {
    try {
        functionThatThrows();
    } catch (const std::exception &e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;

        // Create another StackTrace object to capture the stack trace at the
        // catch block
        atom::error::StackTrace stackTrace;
        std::cout << "Stack Trace at catch block:\n"
                  << stackTrace.toString() << std::endl;
    }

    return 0;
}
