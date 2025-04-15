#ifndef ATOM_TEST_CLI_HPP
#define ATOM_TEST_CLI_HPP

#include <charconv>
#include <chrono>  // Required for std::chrono
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>  // Required for std::thread::hardware_concurrency
#include <unordered_map>
#include <variant>
#include <vector>
#include "atom/tests/test_runner.hpp"

namespace atom::test {

/**
 * @brief A command-line argument parser.
 *
 * Provides modern C++ based command-line argument parsing, supporting various
 * argument types including flags, string options, and numerical options.
 */
class CommandLineParser {
public:
    /**
     * @brief Type alias for the possible values an argument can hold.
     * Supports boolean flags, integer, double, and string values.
     */
    using ArgValue = std::variant<bool, int, double, std::string>;

    /**
     * @brief Registers a command-line option.
     *
     * @param name The long name of the option (e.g., "--help").
     * @param shortName The short name of the option (e.g., "-h"). Can be empty.
     * @param description A description of the option for help messages.
     * @param defaultValue The default value of the option. Defaults to false
     * (boolean flag).
     * @param required Whether the option must be provided by the user. Defaults
     * to false.
     * @return A reference to this CommandLineParser instance for method
     * chaining.
     */
    auto registerOption(std::string name, std::string shortName,
                        std::string description, ArgValue defaultValue = false,
                        bool required = false) -> CommandLineParser& {
        options_[name] = {
            std::move(shortName),
            std::move(description),
            defaultValue,  // Use the provided default value
            required,
            false,                   // Initially not set
            std::move(defaultValue)  // Initialize 'value' with the default
        };
        // Also map the short name if provided
        if (!options_[name].shortName.empty()) {
            shortNameMap_[options_[name].shortName] = name;
        }
        return *this;
    }

    /**
     * @brief Parses command-line arguments provided as argc and argv.
     *
     * @param argc The argument count, typically from main().
     * @param argv The argument vector, typically from main().
     * @return true if parsing was successful, false otherwise.
     */
    [[nodiscard]] auto parse(int argc, char* argv[]) -> bool {
        if (argc < 1) {
            return false;  // Should at least have the program name
        }
        programName_ = argv[0];
        std::vector<std::string_view> args;
        args.reserve(argc - 1);  // Pre-allocate memory

        for (int i = 1; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }

        return parseArgs(args);
    }

    /**
     * @brief Parses command-line arguments provided as a span of string views.
     *
     * @param args A span containing the command-line arguments (including
     * program name).
     * @return true if parsing was successful, false otherwise.
     */
    [[nodiscard]] auto parse(std::span<const std::string_view> args) -> bool {
        if (!args.empty()) {
            programName_ = std::string(args[0]);
            return parseArgs(
                args.subspan(1));  // Parse arguments excluding program name
        }
        std::cerr << "Error: No arguments provided.\n";
        return false;
    }

    /**
     * @brief Checks if a specific option was provided in the parsed arguments.
     *
     * @param name The long name of the option (e.g., "--help").
     * @return true if the option was present and set, false otherwise.
     */
    [[nodiscard]] auto contains(const std::string& name) const -> bool {
        auto it = options_.find(name);
        return it != options_.end() && it->second.isSet;
    }

    /**
     * @brief Retrieves the value of a specific option.
     *
     * If the option was not provided or its value cannot be converted to type
     * T, the specified defaultValue is returned.
     *
     * @tparam T The expected type of the option's value (bool, int, double,
     * std::string).
     * @param name The long name of the option (e.g., "--threads").
     * @param defaultValue The value to return if the option is not set or type
     * mismatch occurs.
     * @return The value of the option if set and type matches, otherwise
     * defaultValue.
     */
    template <typename T>
    [[nodiscard]] auto getValue(const std::string& name,
                                T defaultValue = T{}) const -> T {
        auto it = options_.find(name);
        if (it == options_.end() || !it->second.isSet) {
            // If not found by long name, try finding the corresponding long
            // name via short name map This handles cases where getValue is
            // called with a long name but the user provided the short one.
            // However, the primary lookup should be the long name as stored in
            // options_. If the option wasn't set at all (isSet is false),
            // return default.
            return defaultValue;
        }

        try {
            // Attempt to get the value as the requested type T
            return std::get<T>(it->second.value);
        } catch (const std::bad_variant_access&) {
            // If the stored type doesn't match T, return the default value
            std::cerr << "Warning: Type mismatch for option '" << name
                      << "'. Returning default value.\n";
            return defaultValue;
        }
    }

    /**
     * @brief Prints a help message describing the registered options.
     *
     * The output includes usage information, option names (long and short),
     * descriptions, default values, and whether an option is required.
     */
    void printHelp() const {
        std::cout << "Usage: " << programName_ << " [options]\n\n";
        std::cout << "Options:\n";

        // Calculate the maximum length of the option string (e.g., "--long,
        // -s") for alignment
        size_t maxLength = 0;
        for (const auto& [name, option] : options_) {
            size_t length = name.length();
            if (!option.shortName.empty()) {
                length += option.shortName.length() + 2;  // ", "
            }
            maxLength = std::max(maxLength, length);
        }

        // Print details for each registered option
        for (const auto& [name, option] : options_) {
            std::string optionText = name;
            if (!option.shortName.empty()) {
                optionText += ", " + option.shortName;
            }

            // Print the option name(s) with padding for alignment
            std::cout << "  " << optionText;
            std::cout << std::string(maxLength + 4 - optionText.length(),
                                     ' ');  // Padding

            // Print the description
            std::cout << option.description;

            // Print default value if applicable
            std::visit(
                [&](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, bool>) {
                        // Don't print default for flags unless it's true? Or
                        // maybe never print default for bool? Current logic
                        // prints "(Default: enabled)" if default is true. Let's
                        // keep it simple. if (arg) { std::cout << " (Default:
                        // enabled)"; }
                    } else if constexpr (std::is_same_v<T, int> ||
                                         std::is_same_v<T, double>) {
                        std::cout << " (Default: " << arg << ")";
                    } else if constexpr (std::is_same_v<T, std::string>) {
                        if (!arg.empty()) {
                            std::cout << " (Default: \"" << arg << "\")";
                        }
                    }
                },
                option.defaultValue);  // Use defaultValue here, not value

            // Indicate if the option is required
            if (option.required) {
                std::cout << " (Required)";
            }

            std::cout << "\n";
        }
    }

    /**
     * @brief Applies the parsed command-line options to a TestRunnerConfig
     * object.
     *
     * Updates the configuration based on the presence and values of relevant
     * options.
     *
     * @param config The TestRunnerConfig object to update.
     */
    void applyToConfig(TestRunnerConfig& config) const {
        if (contains("--parallel")) {
            config.enableParallel = true;
            // Use the value provided for --threads, or default if --threads
            // wasn't specified
            config.numThreads = getValue<int>(
                "--threads", config.numThreads);  // Keep existing default if
                                                  // --threads not given
        } else if (contains("--threads")) {
            // If only --threads is specified, still enable parallel execution
            config.enableParallel = true;
            config.numThreads = getValue<int>("--threads", config.numThreads);
        }

        if (contains("--retry")) {
            config.maxRetries = getValue<int>("--retry", config.maxRetries);
        }

        if (contains("--fail-fast")) {
            config.failFast = true;  // This is a flag, presence means true
        }

        if (contains("--output-format")) {
            config.outputFormat = getValue<std::string>(
                "--output-format", config.outputFormat.value());
        }

        if (contains("--output-path")) {
            config.outputPath =
                getValue<std::string>("--output-path", config.outputPath);
        }

        if (contains("--filter")) {
            config.testFilter =
                getValue<std::string>("--filter", config.testFilter.value());
        }

        if (contains("--verbose")) {
            config.enableVerboseOutput = true;  // Flag
        }

        if (contains("--timeout")) {
            int timeout_ms = getValue<int>(
                "--timeout", 0);  // Default to 0 if not specified or invalid
            if (timeout_ms > 0) {
                config.globalTimeout = std::chrono::milliseconds(timeout_ms);
            } else {
                config.globalTimeout =
                    std::chrono::milliseconds(0);  // No timeout
            }
        }

        if (contains("--shuffle")) {
            config.shuffleTests = true;  // Flag
            // Seed is only relevant if shuffle is enabled
            if (contains("--seed")) {
                config.randomSeed =
                    getValue<int>("--seed", config.randomSeed.value());
            }
        } else if (contains("--seed")) {
            // Warn if seed is provided without shuffle? Or just set it anyway?
            // Let's set it anyway, might be used independently later.
            config.randomSeed =
                getValue<int>("--seed", config.randomSeed.value());
        }

        // Note: --list is handled separately, usually before running tests.
        // This function only configures how tests are run.
    }

private:
    /**
     * @brief Structure to hold information about a registered command-line
     * option.
     */
    struct Option {
        std::string shortName;    ///< Short alias for the option (e.g., "-h").
        std::string description;  ///< Description for help messages.
        ArgValue
            defaultValue;  ///< Default value if the option is not provided.
        bool required;     ///< Is this option mandatory?
        bool isSet;        ///< Was this option provided in the command line?
        ArgValue value;    ///< Actual value parsed from the command line
                           ///< (initially holds default).
    };

    /**
     * @brief Map storing registered options, keyed by their long name.
     */
    std::unordered_map<std::string, Option> options_;
    /**
     * @brief Map storing short names and their corresponding long names for
     * quick lookup.
     */
    std::unordered_map<std::string, std::string> shortNameMap_;
    /**
     * @brief The name of the program (argv[0]).
     */
    std::string programName_;

    /**
     * @brief Internal helper function to parse arguments after initial setup.
     *
     * Iterates through the provided argument span, identifies options and their
     * values, updates the internal state, and performs validation.
     *
     * @param args A span containing the command-line arguments (excluding
     * program name).
     * @return true if parsing was successful, false otherwise (e.g., unknown
     * option, missing value).
     */
    [[nodiscard]] auto parseArgs(std::span<const std::string_view> args)
        -> bool {
        // Initialize all option values to their defaults and mark as not set
        for (auto& [name, option] : options_) {
            option.value = option.defaultValue;
            option.isSet = false;
        }

        for (size_t i = 0; i < args.size(); ++i) {
            std::string_view arg = args[i];
            std::string currentArgName;  // To store the identified option name
                                         // (long or short)
            std::string longArgName;     // To store the canonical long name

            Option* optionPtr = nullptr;

            if (arg.starts_with("--")) {  // Long option
                currentArgName = std::string(arg);
                longArgName = currentArgName;
                auto it = options_.find(longArgName);
                if (it != options_.end()) {
                    optionPtr = &it->second;
                }
            } else if (arg.starts_with("-") &&
                       arg.length() >
                           1) {  // Short option or combined short options
                currentArgName = std::string(arg);
                // Handle combined flags like -vf
                if (arg.length() > 2) {
                    bool all_flags = true;
                    for (size_t j = 1; j < arg.length(); ++j) {
                        std::string short_flag(1, arg[j]);
                        auto shortIt = shortNameMap_.find(
                            "-" +
                            short_flag);  // shortNameMap_ keys include "-"
                        if (shortIt == shortNameMap_.end()) {
                            std::cerr
                                << "Error: Unknown short option component '-"
                                << short_flag << "' in '" << arg << "'\n";
                            return false;
                        }
                        auto longIt = options_.find(shortIt->second);
                        if (longIt == options_.end() ||
                            !std::holds_alternative<bool>(
                                longIt->second.defaultValue)) {
                            // If combined flags contain a non-boolean option,
                            // it's an error unless it's the *last* character
                            // and expects a value.
                            if (j == arg.length() - 1 &&
                                !std::holds_alternative<bool>(
                                    longIt->second.defaultValue)) {
                                // Last char is non-bool, treat as single short
                                // option needing value
                                all_flags = false;
                                currentArgName =
                                    "-" +
                                    short_flag;  // Process this single option
                                longArgName = shortIt->second;
                                optionPtr = &longIt->second;
                                break;  // Exit inner loop, process
                                        // currentArgName
                            } else {
                                std::cerr << "Error: Combined short option '"
                                          << arg
                                          << "' contains non-flag or requires "
                                             "value before the end.\n";
                                return false;
                            }
                        }
                        // If it's a valid flag, mark it as set
                        longIt->second.value = true;
                        longIt->second.isSet = true;
                    }
                    if (all_flags) {
                        continue;  // Successfully processed combined flags,
                                   // move to next arg
                    }
                    // If we broke out, optionPtr is set for the last non-flag
                    // option
                } else {
                    // Single short option (e.g., "-h")
                    auto shortIt = shortNameMap_.find(currentArgName);
                    if (shortIt != shortNameMap_.end()) {
                        longArgName = shortIt->second;
                        auto longIt = options_.find(longArgName);
                        if (longIt != options_.end()) {
                            optionPtr = &longIt->second;
                        }
                    }
                }
            } else {
                // Argument does not start with '-' or '--', treat as positional
                // argument (currently unsupported)
                std::cerr << "Error: Unexpected positional argument: " << arg
                          << "\n";
                printHelp();  // Show usage on error
                return false;
            }

            // Check if the option was found
            if (!optionPtr) {
                std::cerr << "Error: Unknown option: " << arg << "\n";
                printHelp();
                return false;
            }

            // Mark the option as set
            optionPtr->isSet = true;

            // Handle option value
            if (std::holds_alternative<bool>(optionPtr->defaultValue)) {
                // Boolean flag: presence sets it to true
                optionPtr->value = true;
            } else {
                // Option requires a value
                if (i + 1 >= args.size() || args[i + 1].starts_with("-")) {
                    // Check if next arg exists and is not another option
                    std::cerr << "Error: Option " << arg
                              << " requires a value.\n";
                    printHelp();
                    return false;
                }

                // Consume the next argument as the value
                i++;
                std::string_view valueArg = args[i];

                // Try to parse the value based on the option's expected type
                // (from defaultValue)
                try {
                    if (std::holds_alternative<int>(optionPtr->defaultValue)) {
                        int parsedValue;
                        auto result = std::from_chars(
                            valueArg.data(), valueArg.data() + valueArg.size(),
                            parsedValue);
                        if (result.ec != std::errc() ||
                            result.ptr != valueArg.data() + valueArg.size()) {
                            throw std::invalid_argument(
                                "Invalid integer format");
                        }
                        optionPtr->value = parsedValue;
                    } else if (std::holds_alternative<double>(
                                   optionPtr->defaultValue)) {
// std::from_chars for double is available in C++17 but might have limited
// compiler support Using stod as a fallback, though it requires string
// allocation.
#if __cpp_lib_to_chars >= 201611L
                        double parsedValue;
                        auto result = std::from_chars(
                            valueArg.data(), valueArg.data() + valueArg.size(),
                            parsedValue);
                        if (result.ec != std::errc() ||
                            result.ptr != valueArg.data() + valueArg.size()) {
                            throw std::invalid_argument(
                                "Invalid double format");
                        }
                        optionPtr->value = parsedValue;
#else
                        // Fallback using stod (less safe, involves allocation)
                        size_t charsProcessed = 0;
                        double parsedValue =
                            std::stod(std::string(valueArg), &charsProcessed);
                        if (charsProcessed != valueArg.size()) {
                            throw std::invalid_argument(
                                "Invalid double format or trailing characters");
                        }
                        optionPtr->value = parsedValue;
#endif
                    } else if (std::holds_alternative<std::string>(
                                   optionPtr->defaultValue)) {
                        optionPtr->value = std::string(valueArg);
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error: Invalid value '" << valueArg
                              << "' for option " << arg << ". " << e.what()
                              << "\n";
                    printHelp();
                    return false;
                }
            }
        }

        // Final check: ensure all required options are set
        for (const auto& [name, option] : options_) {
            if (option.required && !option.isSet) {
                std::cerr << "Error: Missing required option: " << name << "\n";
                printHelp();
                return false;
            }
        }

        return true;
    }
};

/**
 * @brief Creates a CommandLineParser instance pre-configured with standard test
 * runner options.
 *
 * Registers common options like --help, --parallel, --filter, etc., used by
 * test frameworks.
 *
 * @return A CommandLineParser object with default test options registered.
 */
[[nodiscard]] inline auto createDefaultParser() -> CommandLineParser {
    CommandLineParser parser;

    // Determine default threads, handle potential 0 return value
    unsigned int defaultThreads = std::thread::hardware_concurrency();
    if (defaultThreads == 0) {
        defaultThreads = 1;  // Fallback to 1 thread if detection fails
    }

    parser
        .registerOption("--help", "-h", "Display this help message and exit",
                        false)
        .registerOption("--parallel", "-p", "Enable parallel test execution",
                        false)
        .registerOption("--threads", "-j",
                        "Number of threads for parallel execution",
                        static_cast<int>(defaultThreads))
        .registerOption("--retry", "-r",
                        "Number of times to retry failed tests", 0)
        .registerOption("--fail-fast", "-f",
                        "Stop execution on the first test failure", false)
        .registerOption("--output-format", "-o",
                        "Output format (e.g., json, xml, console)",
                        std::string("console"))
        .registerOption("--output-path", "-d",
                        "Path to write output file (if format requires it)",
                        std::string())
        .registerOption("--filter", "",
                        "Filter tests using a regular expression",
                        std::string())  // No short name common standard
        .registerOption("--verbose", "-v", "Enable verbose output", false)
        .registerOption("--timeout", "-t",
                        "Global timeout for the entire test suite in "
                        "milliseconds (0 for no timeout)",
                        0)
        .registerOption("--shuffle", "-s",
                        "Shuffle the order of test execution", false)
        .registerOption(
            "--seed", "",
            "Random seed for shuffling (used only if --shuffle is present)",
            0)  // No short name common standard
        .registerOption("--list", "-l", "List all tests without running them",
                        false);

    return parser;
}

}  // namespace atom::test

#endif  // ATOM_TEST_CLI_HPP