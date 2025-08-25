/**
 * @file env_comprehensive.cpp
 * @brief Comprehensive example demonstrating environment variable management
 *
 * This example showcases the complete environment variable functionality
 * available in the Atom System module, including:
 * - Environment variable manipulation and management
 * - Command-line argument processing
 * - Path operations and directory management
 * - System information retrieval
 * - Variable expansion and formatting
 * - Cross-platform environment handling
 * - Advanced environment operations
 *
 * @note Cross-platform compatibility: Windows, Linux, macOS
 * @author Atom Framework
 * @date 2024
 */

#include "atom/system/env.hpp"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace atom::utils;

/**
 * @brief Print a section header
 */
void printSection(const std::string& title) {
    std::cout << "\n=== " << title << " ===" << std::endl;
}

/**
 * @brief Print environment variable information
 */
void printEnvVar(const std::string& key, const std::string& value,
                 bool exists = true) {
    if (exists) {
        std::cout << std::setw(20) << key << " = " << value << std::endl;
    } else {
        std::cout << std::setw(20) << key << " = (not set)" << std::endl;
    }
}

int main(int argc, char** argv) {
    try {
        std::cout << "=== Atom System Environment Management Example ==="
                  << std::endl;
        std::cout
            << "Demonstrating comprehensive environment variable capabilities\n"
            << std::endl;

        // Create an Env object with command-line arguments
        Env env(argc, argv);

        // 1. Basic Environment Variable Operations
        printSection("Basic Environment Variable Operations");

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

        // Verify deletion
        hasVar = env.has("MY_VAR");
        std::cout << "Has MY_VAR after deletion: " << std::boolalpha << hasVar
                  << std::endl;

        // 2. System Environment Variable Operations
        printSection("System Environment Variable Operations");

        // Set the value of an environment variable
        bool setResult = env.setEnv("ATOM_TEST_VAR", "456");
        std::cout << "Set ATOM_TEST_VAR=456: " << std::boolalpha << setResult
                  << std::endl;

        // Get the value of an environment variable
        std::string newValue = env.getEnv("ATOM_TEST_VAR", "default");
        std::cout << "Value of ATOM_TEST_VAR: " << newValue << std::endl;

        // Test different data types
        env.add("INT_VAR", "42");
        env.add("FLOAT_VAR", "3.14159");
        env.add("BOOL_VAR", "true");

        int intValue = env.getAs("INT_VAR", 0);
        double floatValue = env.getAs("FLOAT_VAR", 0.0);
        bool boolValue = env.getAs("BOOL_VAR", false);

        std::cout << "Type conversion examples:" << std::endl;
        std::cout << "  INT_VAR as int: " << intValue << std::endl;
        std::cout << "  FLOAT_VAR as double: " << std::fixed
                  << std::setprecision(5) << floatValue << std::endl;
        std::cout << "  BOOL_VAR as bool: " << std::boolalpha << boolValue
                  << std::endl;

        // 3. Command-line Argument Processing
        printSection("Command-line Argument Processing");

        std::cout << "Program name: " << env.getProgramName() << std::endl;
        std::cout << "Argument count: " << env.getArgc() << std::endl;

        auto allArgs = env.getAllArgs();
        std::cout << "All arguments:" << std::endl;
        for (const auto& [key, value] : allArgs) {
            std::cout << "  " << key << " = " << value << std::endl;
        }

        // 4. System Information
        printSection("System Information");

        std::cout << "System information:" << std::endl;
        std::cout << "  System name: " << Env::getSystemName() << std::endl;
        std::cout << "  Architecture: " << Env::getSystemArch() << std::endl;
        std::cout << "  Current user: " << Env::getCurrentUser() << std::endl;
        std::cout << "  Hostname: " << Env::getHostName() << std::endl;

        // 5. Directory Operations
        printSection("Directory Operations");

        std::cout << "Standard directories:" << std::endl;
        std::cout << "  Home directory: " << Env::getHomeDir() << std::endl;
        std::cout << "  Temp directory: " << Env::getTempDir() << std::endl;
        std::cout << "  Config directory: " << Env::getConfigDir() << std::endl;
        std::cout << "  Data directory: " << Env::getDataDir() << std::endl;

        // 6. Variable Expansion
        printSection("Variable Expansion");

        // Set up test variables for expansion
        env.setEnv("TEST_HOME", Env::getHomeDir());
        env.setEnv("TEST_USER", Env::getCurrentUser());

        std::vector<std::string> testStrings = {
#ifdef _WIN32
            "%TEST_HOME%\\Documents", "Hello %TEST_USER%!", "%PATH%"
#else
            "$TEST_HOME/Documents", "Hello $TEST_USER!", "$PATH"
#endif
        };

        std::cout << "Variable expansion examples:" << std::endl;
        for (const auto& testStr : testStrings) {
            std::string expanded = Env::expandVariables(testStr);
            std::cout << "  Original: " << testStr << std::endl;
            std::cout << "  Expanded: " << expanded.substr(0, 100);
            if (expanded.length() > 100)
                std::cout << "...";
            std::cout << std::endl << std::endl;
        }

        // 7. Environment Variable Listing
        printSection("Environment Variable Listing");

        auto envVars = Env::Environ();
        std::cout << "Total environment variables: " << envVars.size()
                  << std::endl;

        // Show some common environment variables
        std::vector<std::string> commonVars = {
#ifdef _WIN32
            "PATH", "USERPROFILE", "COMPUTERNAME", "OS",
            "PROCESSOR_ARCHITECTURE"
#else
            "PATH", "HOME", "USER", "SHELL", "TERM"
#endif
        };

        std::cout << "\nCommon environment variables:" << std::endl;
        for (const auto& varName : commonVars) {
            auto it = envVars.find(varName);
            if (it != envVars.end()) {
                std::string value = it->second;
                if (value.length() > 50) {
                    value = value.substr(0, 50) + "...";
                }
                printEnvVar(varName, value);
            } else {
                printEnvVar(varName, "", false);
            }
        }

        // 8. Path Operations
        printSection("Path Operations");

        auto pathList = env.getPathList();
        std::cout << "PATH contains " << pathList.size()
                  << " directories:" << std::endl;
        for (size_t i = 0; i < std::min(pathList.size(), size_t(5)); i++) {
            std::cout << "  [" << i << "] " << pathList[i] << std::endl;
        }
        if (pathList.size() > 5) {
            std::cout << "  ... and " << (pathList.size() - 5)
                      << " more directories" << std::endl;
        }

        // Test path operations
        std::string testPath = "/new/test/path";
        env.addToPath(testPath);
        std::cout << "\nAdded '" << testPath << "' to PATH" << std::endl;

        bool inPath = env.isInPath(testPath);
        std::cout << "Is '" << testPath << "' in PATH: " << std::boolalpha
                  << inPath << std::endl;

        env.removeFromPath(testPath);
        std::cout << "Removed '" << testPath << "' from PATH" << std::endl;

        // 9. Optional Values and Error Handling
        printSection("Optional Values and Error Handling");

        // Test optional values
        auto optionalInt = env.getOptional<int>("NONEXISTENT_INT");
        auto optionalString =
            env.getOptional<std::string>("NONEXISTENT_STRING");

        std::cout << "Optional value tests:" << std::endl;
        std::cout << "  Nonexistent int has value: " << std::boolalpha
                  << optionalInt.has_value() << std::endl;
        std::cout << "  Nonexistent string has value: " << std::boolalpha
                  << optionalString.has_value() << std::endl;

        // Test with existing values
        auto existingInt = env.getOptional<int>("INT_VAR");
        if (existingInt.has_value()) {
            std::cout << "  Existing INT_VAR value: " << existingInt.value()
                      << std::endl;
        }

        // 10. Cleanup and Restoration
        printSection("Cleanup and Restoration");

        // Clean up test variables
        env.unsetEnv("ATOM_TEST_VAR");
        env.unsetEnv("TEST_HOME");
        env.unsetEnv("TEST_USER");
        env.del("INT_VAR");
        env.del("FLOAT_VAR");
        env.del("BOOL_VAR");

        std::cout << "Cleaned up test environment variables" << std::endl;

        // Verify cleanup
        bool testVarExists = env.has("ATOM_TEST_VAR");
        std::cout << "ATOM_TEST_VAR exists after cleanup: " << std::boolalpha
                  << testVarExists << std::endl;

        std::cout << "\n=== Environment Management Example Complete ==="
                  << std::endl;
        std::cout << "This example demonstrated:" << std::endl;
        std::cout << "- Basic environment variable operations" << std::endl;
        std::cout << "- System environment variable management" << std::endl;
        std::cout << "- Command-line argument processing" << std::endl;
        std::cout << "- System information retrieval" << std::endl;
        std::cout << "- Directory operations" << std::endl;
        std::cout << "- Variable expansion and formatting" << std::endl;
        std::cout << "- Path manipulation" << std::endl;
        std::cout << "- Optional values and error handling" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
