#include "atom/system/env.hpp"

#include <iostream>

using namespace atom::utils;

int main(int argc, char** argv) {
    // Create an Env object with command-line arguments
    Env env(argc, argv);

    // Add a key-value pair to the environment variables
    env.add("MY_VAR", "123");
    std::cout << "Added MY_VAR=123" << std::endl;

    // Check if a key exists in the environment variables
    bool hasVar = env.has("MY_VAR");
    std::cout << "Has MY_VAR: " << std::boolalpha << hasVar << std::endl;

    // Get the value associated with a key
    std::string value = env.get("MY_VAR", "default");
    std::cout << "Value of MY_VAR: " << value << std::endl;

    // Delete a key-value pair from the environment variables
    env.del("MY_VAR");
    std::cout << "Deleted MY_VAR" << std::endl;

    // Set the value of an environment variable
    bool setResult = env.setEnv("NEW_VAR", "456");
    std::cout << "Set NEW_VAR=456: " << std::boolalpha << setResult
              << std::endl;

    // Get the value of an environment variable
    std::string newValue = env.getEnv("NEW_VAR", "default");
    std::cout << "Value of NEW_VAR: " << newValue << std::endl;

    // Unset an environment variable
    env.unsetEnv("NEW_VAR");
    std::cout << "Unset NEW_VAR" << std::endl;

    // List all environment variables
    auto variables = Env::listVariables();
    std::cout << "Environment variables:" << std::endl;
    for (const auto& var : variables) {
        std::cout << var << std::endl;
    }

#if ATOM_ENABLE_DEBUG
    // Print all environment variables
    Env::printAllVariables();
#endif

    // Get the current environment variables
    auto envVars = Env::Environ();
    std::cout << "Current environment variables:" << std::endl;
    for (const auto& [key, value] : envVars) {
        std::cout << key << "=" << value << std::endl;
    }

    // Create a shared pointer to an Env object
    auto sharedEnv = Env::createShared(argc, argv);
    sharedEnv->add("SHARED_VAR", "789");
    std::cout << "Added SHARED_VAR=789 to shared Env" << std::endl;

    return 0;
}
