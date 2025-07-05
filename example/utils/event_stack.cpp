#include "atom/utils/error_stack.hpp"

#include <iostream>
#include <memory>

using namespace atom::error;

int main() {
    // Create a shared pointer to an ErrorStack object
    auto errorStackShared = ErrorStack::createShared();

    // Create a unique pointer to an ErrorStack object
    auto errorStackUnique = ErrorStack::createUnique();

    // Insert a new error into the error stack
    errorStackShared->insertError("Error message 1", "Module1", "Function1", 10,
                                  "file1.cpp");
    errorStackShared->insertError("Error message 2", "Module2", "Function2", 20,
                                  "file2.cpp");

    // Set the modules to be filtered out while printing the error stack
    errorStackShared->setFilteredModules({"Module1"});

    // Clear the list of filtered modules
    errorStackShared->clearFilteredModules();

    // Print the filtered error stack to the standard output
    errorStackShared->printFilteredErrorStack();

    // Get a vector of errors filtered by a specific module
    auto filteredErrors =
        errorStackShared->getFilteredErrorsByModule("Module2");
    std::cout << "Filtered errors for Module2: " << std::endl;
    for (const auto& error : filteredErrors) {
        std::cout << error << std::endl;
    }

    // Get a string containing the compressed errors in the stack
    std::string compressedErrors = errorStackShared->getCompressedErrors();
    std::cout << "Compressed errors: " << compressedErrors << std::endl;

    return 0;
}
