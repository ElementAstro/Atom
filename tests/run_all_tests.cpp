// =============================================================================
// Unified Test Runner for Atom Project
// =============================================================================
// This file provides a centralized test execution system that can discover
// and run tests from all modules in the Atom project.
//
// Usage:
//   ./run_all_tests                    - Run all tests from all modules
//   ./run_all_tests --module=error     - Run tests from specific module
//   ./run_all_tests --category=unit    - Run tests by category
//   ./run_all_tests --filter=pattern   - Run tests matching regex pattern
//   ./run_all_tests --list             - List all available tests
//   ./run_all_tests --help             - Show help information
// =============================================================================

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Include the Atom test framework
#include "atom/tests/test.hpp"
#include "atom/tests/test_runner.hpp"

namespace fs = std::filesystem;

// =============================================================================
// Test Discovery and Registration System
// =============================================================================

/**
 * @brief Information about a discovered test module
 */
struct TestModuleInfo {
    std::string name;
    std::string path;
    std::string executable;
    std::vector<std::string> dependencies;
    bool enabled;
    std::string description;
};

/**
 * @brief Global registry of test modules
 */
class TestModuleRegistry {
public:
    static TestModuleRegistry& getInstance() {
        static TestModuleRegistry instance;
        return instance;
    }

    void registerModule(const TestModuleInfo& module) {
        modules_[module.name] = module;
    }

    const std::map<std::string, TestModuleInfo>& getModules() const {
        return modules_;
    }

    std::vector<TestModuleInfo> getEnabledModules() const {
        std::vector<TestModuleInfo> enabled;
        for (const auto& [name, module] : modules_) {
            if (module.enabled) {
                enabled.push_back(module);
            }
        }
        return enabled;
    }

    TestModuleInfo* findModule(const std::string& name) {
        auto it = modules_.find(name);
        return it != modules_.end() ? &it->second : nullptr;
    }

private:
    std::map<std::string, TestModuleInfo> modules_;
};

// =============================================================================
// Command Line Interface
// =============================================================================

/**
 * @brief Command line options for the test runner
 */
struct TestRunnerOptions {
    bool showHelp = false;
    bool listTests = false;
    bool verbose = false;
    bool parallel = false;
    int numThreads = std::thread::hardware_concurrency();
    std::string moduleFilter;
    std::string categoryFilter;
    std::string patternFilter;
    std::string outputFormat;
    std::string outputPath;
    int maxRetries = 0;
    bool failFast = false;
    bool shuffle = false;
    uint64_t randomSeed = 0;
    std::chrono::milliseconds globalTimeout{0};
};

/**
 * @brief Parse command line arguments
 */
TestRunnerOptions parseCommandLine(int argc, char* argv[]) {
    TestRunnerOptions options;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            options.showHelp = true;
        } else if (arg == "--list" || arg == "-l") {
            options.listTests = true;
        } else if (arg == "--verbose" || arg == "-v") {
            options.verbose = true;
        } else if (arg == "--parallel" || arg == "-p") {
            options.parallel = true;
            if (i + 1 < argc) {
                try {
                    options.numThreads = std::stoi(argv[++i]);
                } catch (...) {
                    throw std::invalid_argument(
                        "Invalid thread count for --parallel");
                }
            }
        } else if (arg == "--module" || arg == "-m") {
            if (i + 1 < argc) {
                options.moduleFilter = argv[++i];
            } else {
                throw std::invalid_argument("--module requires a module name");
            }
        } else if (arg == "--category" || arg == "-c") {
            if (i + 1 < argc) {
                options.categoryFilter = argv[++i];
            } else {
                throw std::invalid_argument(
                    "--category requires a category name");
            }
        } else if (arg == "--filter" || arg == "-f") {
            if (i + 1 < argc) {
                options.patternFilter = argv[++i];
            } else {
                throw std::invalid_argument("--filter requires a pattern");
            }
        } else if (arg == "--output-format") {
            if (i + 1 < argc) {
                options.outputFormat = argv[++i];
            } else {
                throw std::invalid_argument(
                    "--output-format requires a format");
            }
        } else if (arg == "--output" || arg == "-o") {
            if (i + 1 < argc) {
                options.outputPath = argv[++i];
            } else {
                throw std::invalid_argument("--output requires a path");
            }
        } else if (arg == "--retry") {
            if (i + 1 < argc) {
                try {
                    options.maxRetries = std::stoi(argv[++i]);
                } catch (...) {
                    throw std::invalid_argument(
                        "Invalid retry count for --retry");
                }
            } else {
                throw std::invalid_argument("--retry requires a number");
            }
        } else if (arg == "--fail-fast") {
            options.failFast = true;
        } else if (arg == "--shuffle") {
            options.shuffle = true;
        } else if (arg == "--seed") {
            if (i + 1 < argc) {
                try {
                    options.randomSeed = std::stoull(argv[++i]);
                } catch (...) {
                    throw std::invalid_argument("Invalid seed for --seed");
                }
            } else {
                throw std::invalid_argument("--seed requires a number");
            }
        } else if (arg == "--timeout") {
            if (i + 1 < argc) {
                try {
                    int timeoutMs = std::stoi(argv[++i]);
                    options.globalTimeout =
                        std::chrono::milliseconds(timeoutMs);
                } catch (...) {
                    throw std::invalid_argument(
                        "Invalid timeout for --timeout");
                }
            } else {
                throw std::invalid_argument("--timeout requires a number");
            }
        } else {
            throw std::invalid_argument(std::string("Unknown argument: ") +
                                        std::string(arg));
        }
    }

    return options;
}

/**
 * @brief Show help information
 */
void showHelp() {
    std::cout << R"(
Atom Unified Test Runner

USAGE:
    run_all_tests [OPTIONS]

OPTIONS:
    -h, --help                    Show this help message
    -l, --list                    List all available test modules
    -v, --verbose                 Enable verbose output
    -p, --parallel [N]            Run tests in parallel (default: hardware concurrency)
    -m, --module <name>           Run tests from specific module only
    -c, --category <name>         Run tests from specific category only
    -f, --filter <pattern>        Run tests matching regex pattern
    --output-format <format>      Output format (json, xml, html, text)
    -o, --output <path>           Output file path
    --retry <N>                   Number of retry attempts for failed tests
    --fail-fast                   Stop testing on first failure
    --shuffle                     Shuffle test execution order
    --seed <N>                    Random seed for shuffling
    --timeout <MS>                Global timeout in milliseconds

EXAMPLES:
    run_all_tests                                    # Run all tests
    run_all_tests --module=error                     # Run error module tests only
    run_all_tests --category=unit                    # Run unit tests only
    run_all_tests --filter=".*socket.*"             # Run tests containing "socket"
    run_all_tests --parallel --verbose              # Run all tests in parallel with verbose output
    run_all_tests --output-format=json --output=report.json  # Export results to JSON

AVAILABLE MODULES:
)";

    auto& registry = TestModuleRegistry::getInstance();
    for (const auto& [name, module] : registry.getModules()) {
        std::cout << "    " << name;
        if (!module.enabled) {
            std::cout << " (disabled)";
        }
        if (!module.description.empty()) {
            std::cout << " - " << module.description;
        }
        std::cout << "\n";
    }

    std::cout << std::endl;
}

/**
 * @brief List all available test modules
 */
void listTests(const TestRunnerOptions& options) {
    auto& registry = TestModuleRegistry::getInstance();

    std::cout << "Available Test Modules:\n";
    std::cout << "========================\n\n";

    for (const auto& [name, module] : registry.getModules()) {
        std::cout << "Module: " << name << "\n";
        std::cout << "  Path: " << module.path << "\n";
        std::cout << "  Executable: " << module.executable << "\n";
        std::cout << "  Status: " << (module.enabled ? "Enabled" : "Disabled")
                  << "\n";

        if (!module.description.empty()) {
            std::cout << "  Description: " << module.description << "\n";
        }

        if (!module.dependencies.empty()) {
            std::cout << "  Dependencies: ";
            for (size_t i = 0; i < module.dependencies.size(); ++i) {
                if (i > 0)
                    std::cout << ", ";
                std::cout << module.dependencies[i];
            }
            std::cout << "\n";
        }

        std::cout << "\n";
    }
}

// =============================================================================
// Test Execution Functions
// =============================================================================

/**
 * @brief Discover test modules from the file system
 */
void discoverTestModules() {
    auto& registry = TestModuleRegistry::getInstance();

    // Known test modules with their descriptions
    std::vector<TestModuleInfo> knownModules = {
        {"algorithm",
         "tests/algorithm",
         "atom_algorithm_tests",
         {"atom-error"},
         true,
         "Mathematical algorithms, cryptography, signal processing"},
        {"async",
         "tests/async",
         "atom_async_tests",
         {"atom-error", "fmt"},
         true,
         "Asynchronous programming primitives and concurrency"},
        {"components",
         "tests/components",
         "atom_components_tests",
         {"atom-error"},
         true,
         "Component system and scripting engines"},
        {"connection",
         "tests/connection",
         "atom_connection_tests",
         {"atom-error"},
         true,
         "Network communication (TCP, UDP, SSH)"},
        {"containers",
         "tests/containers",
         "atom_containers_tests",
         {"atom-error"},
         true,
         "Container data structures"},
        {"error",
         "tests/error",
         "comprehensive_test",
         {},
         true,
         "Comprehensive error handling and stack traces"},
        {"extra",
         "tests/extra",
         "atom_extra_tests",
         {"atom-error"},
         true,
         "Additional utilities and experimental features"},
        {"image",
         "tests/image",
         "atom_image_tests",
         {"atom-error"},
         true,
         "Image processing and computer vision"},
        {"io",
         "tests/io",
         "atom_io_tests",
         {"atom-error"},
         true,
         "Input/output operations and file system utilities"},
        {"log",
         "tests/log",
         "atom_log_tests",
         {"atom-error"},
         true,
         "Logging framework"},
        {"memory",
         "tests/memory",
         "atom_memory_tests",
         {"atom-error"},
         true,
         "Memory management and allocation"},
        {"meta",
         "tests/meta",
         "atom_meta_tests",
         {"atom-error"},
         true,
         "Metaprogramming utilities"},
        {"search",
         "tests/search",
         "atom_search_tests",
         {"atom-error"},
         true,
         "Search algorithms and data structures"},
        {"secret",
         "tests/secret",
         "test_secret",
         {"atom-error"},
         true,
         "Cryptographic operations"},
        {"serial",
         "tests/serial",
         "atom_serial_tests",
         {"atom-error"},
         true,
         "Serial communication"},
        {"sysinfo",
         "tests/sysinfo",
         "atom_sysinfo_tests",
         {"atom-error"},
         true,
         "System information utilities"},
        {"system",
         "tests/system",
         "atom_system_tests",
         {"atom-error"},
         true,
         "System-level integration"},
        {"type",
         "tests/type",
         "atom_type_tests",
         {"atom-error"},
         true,
         "Type system and utilities"},
        {"utils",
         "tests/utils",
         "atom_utils_tests",
         {"atom-error"},
         true,
         "General utility functions"},
        {"web",
         "tests/web",
         "atom_web_tests",
         {"atom-error"},
         true,
         "HTTP client and web utilities"}};

    for (const auto& module : knownModules) {
        // Check if the module actually exists on the filesystem
        if (fs::exists(module.path) && fs::is_directory(module.path)) {
            registry.registerModule(module);
        }
    }
}

/**
 * @brief Configure test runner based on command line options
 */
atom::test::TestRunnerConfig createRunnerConfig(
    const TestRunnerOptions& options) {
    atom::test::TestRunnerConfig config;

    config.withParallel(options.parallel)
        .withThreads(options.numThreads)
        .withRetries(options.maxRetries)
        .withFailFast(options.failFast)
        .withVerboseOutput(options.verbose)
        .withShuffleTests(options.shuffle)
        .withGlobalTimeout(options.globalTimeout);

    if (options.randomSeed != 0) {
        config.withRandomSeed(options.randomSeed);
    }

    if (!options.patternFilter.empty()) {
        config.withFilter(options.patternFilter);
    }

    if (!options.outputFormat.empty()) {
        config.withOutputFormat(options.outputFormat);
    }

    if (!options.outputPath.empty()) {
        config.withOutputPath(options.outputPath);
    }

    return config;
}

/**
 * @brief Run tests for a specific module
 */
atom::test::TestStats runModuleTests(const std::string& moduleName,
                                     const TestRunnerOptions& options) {
    auto& registry = TestModuleRegistry::getInstance();
    auto* module = registry.findModule(moduleName);

    if (!module) {
        throw std::runtime_error("Module not found: " + moduleName);
    }

    if (!module->enabled) {
        throw std::runtime_error("Module is disabled: " + moduleName);
    }

    // For now, we'll use the existing Atom test framework
    // In a full implementation, this would execute the module's test executable
    // and collect results

    auto config = createRunnerConfig(options);
    atom::test::TestRunner runner(config);

    if (options.verbose) {
        std::cout << "Running tests for module: " << moduleName << std::endl;
    }

    return runner.runAll();
}

/**
 * @brief Run all enabled tests
 */
atom::test::TestStats runAllTests(const TestRunnerOptions& options) {
    auto& registry = TestModuleRegistry::getInstance();
    auto enabledModules = registry.getEnabledModules();

    if (enabledModules.empty()) {
        std::cout << "No enabled test modules found." << std::endl;
        return {};
    }

    auto config = createRunnerConfig(options);
    atom::test::TestRunner runner(config);

    if (options.verbose) {
        std::cout << "Running tests from " << enabledModules.size()
                  << " modules:" << std::endl;
        for (const auto& module : enabledModules) {
            std::cout << "  - " << module.name << std::endl;
        }
        std::cout << std::endl;
    }

    return runner.runAll();
}

// =============================================================================
// Main Function
// =============================================================================

int main(int argc, char* argv[]) {
    try {
        // Discover available test modules
        discoverTestModules();

        // Parse command line arguments
        TestRunnerOptions options = parseCommandLine(argc, argv);

        // Handle help and list options
        if (options.showHelp) {
            showHelp();
            return 0;
        }

        if (options.listTests) {
            listTests(options);
            return 0;
        }

        // Run tests based on options
        atom::test::TestStats stats;

        if (!options.moduleFilter.empty()) {
            stats = runModuleTests(options.moduleFilter, options);
        } else {
            stats = runAllTests(options);
        }

        // Print summary
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "Test Summary:\n";
        std::cout << "  Total tests: " << stats.totalTests << "\n";
        std::cout << "  Passed: " << stats.passedAsserts << "\n";
        std::cout << "  Failed: " << stats.failedAsserts << "\n";
        std::cout << "  Skipped: " << stats.skippedTests << "\n";
        std::cout << std::string(60, '=') << "\n";

        return stats.failedAsserts > 0 ? 1 : 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }
}
