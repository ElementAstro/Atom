#ifndef ATOM_TEST_CLI_HPP
#define ATOM_TEST_CLI_HPP

#include <charconv>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>
#include "atom/tests/test_runner.hpp"

namespace atom::test {

// ═══════════════════════════════════════════════════════════════════════════════════
// ░█▀▀░█▀█░█▄█░█▄█░█▀▀░█▀█░█▀▄░█░░░▀█▀░█▀█░█▀▀░░░█▀█░█▀▀░█▀▄░█▀▀░█▀▀░█▀▄
// ░█░░░█░█░█░█░█░█░█▀▀░█░█░█░█░█░░░░█░░█░█░█▀▀░░░█▀▀░█▀▀░█▀▄░▀▀█░█▀▀░█▀▄
// ░▀▀▀░▀▀▀░▀░▀░▀░▀░▀▀▀░▀░▀░▀▀░░▀▀▀░▀▀▀░▀░▀░▀▀▀░░░▀░░░▀▀▀░▀░▀░▀▀▀░▀▀▀░▀░▀
// ═══════════════════════════════════════════════════════════════════════════════════

/**
 * @brief 🎨 Modern Command-Line Interface with Enhanced Visual Appeal
 * @details A beautifully crafted command-line argument parser designed for
 *          optimal user experience with colorful output, intuitive formatting,
 *          and comprehensive help documentation.
 *
 * ✨ Features:
 * • 🎯 Type-safe argument parsing with modern C++ variants
 * • 🌈 Colorful and aesthetically pleasing help output
 * • 🔧 Flexible option registration with chaining support
 * • 📊 Smart formatting and alignment for readability
 * • 🛡️ Robust error handling with helpful messages
 * • 🚀 High-performance parsing with zero-cost abstractions
 */
class CommandLineParser {
public:
    /**
     * @brief 🎯 Type-safe argument value container
     * @details Supports boolean flags, integers, floating-point numbers, and
     * strings
     */
    using ArgValue = std::variant<bool, int, double, std::string>;

    // ═══════════════════════════════════════════════════════════════════════════
    // Color Constants for Beautiful Terminal Output
    // ═══════════════════════════════════════════════════════════════════════════

    struct Colors {
        static constexpr const char* RESET = "\033[0m";
        static constexpr const char* BOLD = "\033[1m";
        static constexpr const char* DIM = "\033[2m";
        static constexpr const char* ITALIC = "\033[3m";
        static constexpr const char* UNDERLINE = "\033[4m";

        // Text Colors
        static constexpr const char* BLACK = "\033[30m";
        static constexpr const char* RED = "\033[31m";
        static constexpr const char* GREEN = "\033[32m";
        static constexpr const char* YELLOW = "\033[33m";
        static constexpr const char* BLUE = "\033[34m";
        static constexpr const char* MAGENTA = "\033[35m";
        static constexpr const char* CYAN = "\033[36m";
        static constexpr const char* WHITE = "\033[37m";

        // Bright Colors
        static constexpr const char* BRIGHT_BLACK = "\033[90m";
        static constexpr const char* BRIGHT_RED = "\033[91m";
        static constexpr const char* BRIGHT_GREEN = "\033[92m";
        static constexpr const char* BRIGHT_YELLOW = "\033[93m";
        static constexpr const char* BRIGHT_BLUE = "\033[94m";
        static constexpr const char* BRIGHT_MAGENTA = "\033[95m";
        static constexpr const char* BRIGHT_CYAN = "\033[96m";
        static constexpr const char* BRIGHT_WHITE = "\033[97m";

        // Background Colors
        static constexpr const char* BG_BLACK = "\033[40m";
        static constexpr const char* BG_RED = "\033[41m";
        static constexpr const char* BG_GREEN = "\033[42m";
        static constexpr const char* BG_YELLOW = "\033[43m";
        static constexpr const char* BG_BLUE = "\033[44m";
        static constexpr const char* BG_MAGENTA = "\033[45m";
        static constexpr const char* BG_CYAN = "\033[46m";
        static constexpr const char* BG_WHITE = "\033[47m";
    };

    /**
     * @brief 🔧 Register a command-line option with beautiful formatting
     * @param name The long name of the option (e.g., "--help")
     * @param shortName The short name of the option (e.g., "-h")
     * @param description A descriptive explanation of the option
     * @param defaultValue The default value (supports bool, int, double,
     * string)
     * @param required Whether this option is mandatory
     * @return Reference to this parser for method chaining ⛓️
     */
    auto registerOption(std::string name, std::string shortName,
                        std::string description, ArgValue defaultValue = false,
                        bool required = false) -> CommandLineParser& {
        options_[name] = {.shortName = std::move(shortName),
                          .description = std::move(description),
                          .defaultValue = defaultValue,
                          .required = required,
                          .isSet = false,
                          .value = std::move(defaultValue)};

        if (!options_[name].shortName.empty()) {
            shortNameMap_[options_[name].shortName] = name;
        }
        return *this;
    }

    /**
     * @brief 🔍 Parse command-line arguments from argc/argv
     * @param argc Argument count from main()
     * @param argv Argument vector from main()
     * @return ✅ true if parsing succeeded, ❌ false otherwise
     */
    [[nodiscard]] auto parse(int argc, char* argv[]) -> bool {
        if (argc < 1) {
            printError("No arguments provided");
            return false;
        }
        programName_ = argv[0];
        std::vector<std::string_view> args;
        args.reserve(argc - 1);

        for (int i = 1; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }

        return parseArgs(args);
    }

    /**
     * @brief 🔍 Parse command-line arguments from string_view span
     * @param args Span containing arguments (including program name)
     * @return ✅ true if parsing succeeded, ❌ false otherwise
     */
    [[nodiscard]] auto parse(std::span<const std::string_view> args) -> bool {
        if (!args.empty()) {
            programName_ = std::string(args[0]);
            return parseArgs(args.subspan(1));
        }
        printError("No arguments provided");
        return false;
    }

    /**
     * @brief ✔️ Check if an option was provided
     * @param name The long name of the option
     * @return true if option was set, false otherwise
     */
    [[nodiscard]] auto contains(const std::string& name) const -> bool {
        auto it = options_.find(name);
        return it != options_.end() && it->second.isSet;
    }

    /**
     * @brief 🎯 Get typed value of an option with fallback
     * @tparam T Expected type (bool, int, double, std::string)
     * @param name Option name
     * @param defaultValue Fallback value if option not set or type mismatch
     * @return Option value or default
     */
    template <typename T>
    [[nodiscard]] auto getValue(const std::string& name,
                                T defaultValue = T{}) const -> T {
        auto it = options_.find(name);
        if (it == options_.end() || !it->second.isSet) {
            return defaultValue;
        }

        try {
            return std::get<T>(it->second.value);
        } catch (const std::bad_variant_access&) {
            printWarning("Type mismatch for option '" + name +
                         "'. Using default value");
            return defaultValue;
        }
    }

    /**
     * @brief 🎨 Print a stunning help message with beautiful formatting
     * @details Creates an aesthetically pleasing help output with colors,
     *          proper alignment, and intuitive organization
     */
    void printHelp() const {
        printBanner();

        // Usage section
        std::cout << Colors::BOLD << Colors::CYAN << "USAGE:" << Colors::RESET
                  << "\n";
        std::cout << "  " << Colors::BRIGHT_BLUE << programName_
                  << Colors::RESET << " " << Colors::DIM << "[options]"
                  << Colors::RESET << "\n\n";

        if (options_.empty()) {
            std::cout << Colors::YELLOW << "No options registered."
                      << Colors::RESET << "\n";
            return;
        }

        // Calculate optimal column width for alignment
        size_t maxOptionWidth = 0;
        for (const auto& [name, option] : options_) {
            size_t width = name.length();
            if (!option.shortName.empty()) {
                width += option.shortName.length() + 2;  // ", " separator
            }
            maxOptionWidth = std::max(maxOptionWidth, width);
        }
        maxOptionWidth =
            std::min(maxOptionWidth, size_t(30));  // Reasonable max width

        // Options header
        std::cout << Colors::BOLD << Colors::CYAN << "OPTIONS:" << Colors::RESET
                  << "\n";

        // Group options by category
        std::vector<std::pair<std::string, const Option*>> required_options;
        std::vector<std::pair<std::string, const Option*>> flag_options;
        std::vector<std::pair<std::string, const Option*>> value_options;

        for (const auto& [name, option] : options_) {
            if (option.required) {
                required_options.emplace_back(name, &option);
            } else if (std::holds_alternative<bool>(option.defaultValue)) {
                flag_options.emplace_back(name, &option);
            } else {
                value_options.emplace_back(name, &option);
            }
        }

        // Print required options first (if any)
        if (!required_options.empty()) {
            std::cout << "\n"
                      << Colors::BOLD << Colors::RED
                      << "  Required:" << Colors::RESET << "\n";
            for (const auto& [name, option] : required_options) {
                printOptionLine(name, *option, maxOptionWidth, true);
            }
        }

        // Print flag options
        if (!flag_options.empty()) {
            std::cout << "\n"
                      << Colors::BOLD << Colors::GREEN
                      << "  Flags:" << Colors::RESET << "\n";
            for (const auto& [name, option] : flag_options) {
                printOptionLine(name, *option, maxOptionWidth, false);
            }
        }

        // Print value options
        if (!value_options.empty()) {
            std::cout << "\n"
                      << Colors::BOLD << Colors::BLUE
                      << "  Options:" << Colors::RESET << "\n";
            for (const auto& [name, option] : value_options) {
                printOptionLine(name, *option, maxOptionWidth, false);
            }
        }

        printFooter();
    }

    /**
     * @brief ⚙️ Apply parsed options to TestRunnerConfig
     * @param config Configuration object to update
     */
    void applyToConfig(TestRunnerConfig& config) const {
        if (contains("--parallel")) {
            config.enableParallel = true;
            config.numThreads = getValue<int>("--threads", config.numThreads);
        } else if (contains("--threads")) {
            config.enableParallel = true;
            config.numThreads = getValue<int>("--threads", config.numThreads);
        }

        if (contains("--retry")) {
            config.maxRetries = getValue<int>("--retry", config.maxRetries);
        }

        if (contains("--fail-fast")) {
            config.failFast = true;
        }

        if (contains("--output-format")) {
            config.outputFormat = getValue<std::string>(
                "--output-format", config.outputFormat.value_or(""));
        }

        if (contains("--output-path")) {
            config.outputPath =
                getValue<std::string>("--output-path", config.outputPath);
        }

        if (contains("--filter")) {
            config.testFilter = getValue<std::string>(
                "--filter", config.testFilter.value_or(""));
        }

        if (contains("--verbose")) {
            config.enableVerboseOutput = true;
        }

        if (contains("--timeout")) {
            int timeout_ms = getValue<int>("--timeout", 0);
            config.globalTimeout = (timeout_ms > 0)
                                       ? std::chrono::milliseconds(timeout_ms)
                                       : std::chrono::milliseconds(0);
        }

        if (contains("--shuffle")) {
            config.shuffleTests = true;
            if (contains("--seed")) {
                config.randomSeed =
                    getValue<int>("--seed", config.randomSeed.value_or(0));
            }
        } else if (contains("--seed")) {
            config.randomSeed =
                getValue<int>("--seed", config.randomSeed.value_or(0));
        }
    }

private:
    /**
     * @brief Structure to hold information about a registered command-line
     * option
     */
    struct Option {
        std::string shortName;
        std::string description;
        ArgValue defaultValue;
        bool required;
        bool isSet;
        ArgValue value;
    };

    std::unordered_map<std::string, Option> options_;
    std::unordered_map<std::string, std::string> shortNameMap_;
    std::string programName_;

    /**
     * @brief 🎨 Print a beautiful banner for the application
     * @details Creates an eye-catching header with program information
     */
    void printBanner() const {
        std::cout << Colors::BOLD << Colors::BRIGHT_CYAN << "\n";
        std::cout << "╔════════════════════════════════════════════════════════"
                     "══════════════════════╗\n";
        std::cout << "║                           " << Colors::BRIGHT_WHITE
                  << "🧪 ATOM TEST RUNNER" << Colors::BRIGHT_CYAN
                  << "                           ║\n";
        std::cout << "║                    " << Colors::BRIGHT_YELLOW
                  << "High-Performance C++ Testing Framework"
                  << Colors::BRIGHT_CYAN << "                    ║\n";
        std::cout << "╚════════════════════════════════════════════════════════"
                     "══════════════════════╝"
                  << Colors::RESET << "\n\n";
    }

    /**
     * @brief 📝 Print a beautifully formatted option line
     * @param name The long name of the option
     * @param option The option configuration
     * @param maxWidth Maximum width for alignment
     * @param isRequired Whether this is a required option
     */
    void printOptionLine(const std::string& name, const Option& option,
                         size_t maxWidth, bool isRequired) const {
        std::ostringstream optionStr;

        // Build option string (e.g., "--help, -h")
        if (!option.shortName.empty()) {
            optionStr << option.shortName << ", " << name;
        } else {
            optionStr << name;
        }

        std::string optText = optionStr.str();

        // Color based on type and requirements
        std::string color = Colors::BRIGHT_WHITE;
        if (isRequired) {
            color = Colors::BRIGHT_RED;
        } else if (std::holds_alternative<bool>(option.defaultValue)) {
            color = Colors::BRIGHT_GREEN;  // Flags
        } else {
            color = Colors::BRIGHT_BLUE;  // Value options
        }

        // Print option with proper padding
        std::cout << "    " << color << std::left
                  << std::setw(static_cast<int>(maxWidth + 2)) << optText
                  << Colors::RESET;

        // Print description
        std::cout << Colors::DIM << option.description;

        // Show default value if not a flag and not required
        if (!isRequired && !std::holds_alternative<bool>(option.defaultValue)) {
            std::cout << " " << Colors::BRIGHT_BLACK << "(default: ";

            std::visit(
                [](const auto& value) {
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<T, std::string>) {
                        if (value.empty()) {
                            std::cout << "\"\"";
                        } else {
                            std::cout << "\"" << value << "\"";
                        }
                    } else {
                        std::cout << value;
                    }
                },
                option.defaultValue);

            std::cout << ")";
        }

        std::cout << Colors::RESET << "\n";
    }

    /**
     * @brief 📋 Print a helpful footer with usage tips
     * @details Provides additional guidance and examples for users
     */
    void printFooter() const {
        std::cout << "\n"
                  << Colors::BOLD << Colors::CYAN
                  << "EXAMPLES:" << Colors::RESET << "\n";
        std::cout << "  " << Colors::DIM
                  << "# Run all tests with verbose output" << Colors::RESET
                  << "\n";
        std::cout << "  " << Colors::BRIGHT_BLUE << programName_
                  << Colors::RESET << " " << Colors::GREEN << "--verbose"
                  << Colors::RESET << "\n\n";

        std::cout << "  " << Colors::DIM
                  << "# Run tests in parallel with 8 threads" << Colors::RESET
                  << "\n";
        std::cout << "  " << Colors::BRIGHT_BLUE << programName_
                  << Colors::RESET << " " << Colors::GREEN
                  << "--parallel --threads 8" << Colors::RESET << "\n\n";

        std::cout << "  " << Colors::DIM << "# Filter and run specific tests"
                  << Colors::RESET << "\n";
        std::cout << "  " << Colors::BRIGHT_BLUE << programName_
                  << Colors::RESET << " " << Colors::GREEN
                  << "--filter \"performance.*\"" << Colors::RESET << "\n\n";

        std::cout << "  " << Colors::DIM
                  << "# Enable fail-fast mode with retries" << Colors::RESET
                  << "\n";
        std::cout << "  " << Colors::BRIGHT_BLUE << programName_
                  << Colors::RESET << " " << Colors::GREEN
                  << "--fail-fast --retry 3" << Colors::RESET << "\n\n";

        std::cout << Colors::BOLD << Colors::YELLOW
                  << "💡 TIP:" << Colors::RESET << " Use " << Colors::GREEN
                  << "--help" << Colors::RESET
                  << " anytime to see this information!\n\n";
    }

    /**
     * @brief ❌ Print a formatted error message
     * @param message The error message to display
     */
    void printError(const std::string& message) const {
        std::cout << Colors::BOLD << Colors::BRIGHT_RED
                  << "✗ ERROR: " << Colors::RESET << Colors::RED << message
                  << Colors::RESET << "\n";
    }

    /**
     * @brief ⚠️ Print a formatted warning message
     * @param message The warning message to display
     */
    void printWarning(const std::string& message) const {
        std::cout << Colors::BOLD << Colors::BRIGHT_YELLOW
                  << "⚠ WARNING: " << Colors::RESET << Colors::YELLOW << message
                  << Colors::RESET << "\n";
    }

    /**
     * @brief Internal helper function to parse arguments after initial setup
     * @param args A span containing the command-line arguments (excluding
     * program name)
     * @return true if parsing was successful, false otherwise
     */
    [[nodiscard]] auto parseArgs(std::span<const std::string_view> args)
        -> bool {
        for (auto& [name, option] : options_) {
            option.value = option.defaultValue;
            option.isSet = false;
        }

        for (size_t i = 0; i < args.size(); ++i) {
            std::string_view arg = args[i];
            std::string currentArgName;
            std::string longArgName;
            Option* optionPtr = nullptr;

            if (arg.starts_with("--")) {
                currentArgName = std::string(arg);
                longArgName = currentArgName;
                auto it = options_.find(longArgName);
                if (it != options_.end()) {
                    optionPtr = &it->second;
                }
            } else if (arg.starts_with("-") && arg.length() > 1) {
                currentArgName = std::string(arg);

                if (arg.length() > 2) {
                    bool all_flags = true;
                    for (size_t j = 1; j < arg.length(); ++j) {
                        std::string short_flag = "-" + std::string(1, arg[j]);
                        auto shortIt = shortNameMap_.find(short_flag);
                        if (shortIt == shortNameMap_.end()) {
                            printError("Unknown short option component '" +
                                       short_flag + "' in '" +
                                       std::string(arg) + "'");
                            return false;
                        }
                        auto longIt = options_.find(shortIt->second);
                        if (longIt == options_.end() ||
                            !std::holds_alternative<bool>(
                                longIt->second.defaultValue)) {
                            if (j == arg.length() - 1 &&
                                !std::holds_alternative<bool>(
                                    longIt->second.defaultValue)) {
                                all_flags = false;
                                currentArgName = short_flag;
                                longArgName = shortIt->second;
                                optionPtr = &longIt->second;
                                break;
                            } else {
                                printError("Combined short option '" +
                                           std::string(arg) +
                                           "' contains non-flag or requires "
                                           "value before the end");
                                return false;
                            }
                        }
                        longIt->second.value = true;
                        longIt->second.isSet = true;
                    }
                    if (all_flags) {
                        continue;
                    }
                } else {
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
                printError("Unexpected positional argument: " +
                           std::string(arg));
                printHelp();
                return false;
            }

            if (!optionPtr) {
                printError("Unknown option: " + std::string(arg));
                printHelp();
                return false;
            }

            optionPtr->isSet = true;

            if (std::holds_alternative<bool>(optionPtr->defaultValue)) {
                optionPtr->value = true;
            } else {
                if (i + 1 >= args.size() || args[i + 1].starts_with("-")) {
                    printError("Option " + std::string(arg) +
                               " requires a value");
                    printHelp();
                    return false;
                }

                i++;
                std::string_view valueArg = args[i];

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
                    printError("Invalid value '" + std::string(valueArg) +
                               "' for option " + std::string(arg) + ". " +
                               e.what());
                    printHelp();
                    return false;
                }
            }
        }

        for (const auto& [name, option] : options_) {
            if (option.required && !option.isSet) {
                printError("Missing required option: " + name);
                printHelp();
                return false;
            }
        }

        return true;
    }
};

/**
 * @brief Creates a CommandLineParser instance pre-configured with standard test
 * runner options
 * @details Registers common options like --help, --parallel, --filter, etc.,
 * used by test frameworks
 * @return A CommandLineParser object with default test options registered
 */
[[nodiscard]] inline auto createDefaultParser() -> CommandLineParser {
    CommandLineParser parser;

    unsigned int defaultThreads = std::thread::hardware_concurrency();
    if (defaultThreads == 0) {
        defaultThreads = 1;
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
                        std::string())
        .registerOption("--verbose", "-v", "Enable verbose output", false)
        .registerOption("--timeout", "-t",
                        "Global timeout for the entire test suite in "
                        "milliseconds (0 for no timeout)",
                        0)
        .registerOption("--shuffle", "-s",
                        "Shuffle the order of test execution", false)
        .registerOption(
            "--seed", "",
            "Random seed for shuffling (used only if --shuffle is present)", 0)
        .registerOption("--list", "-l", "List all tests without running them",
                        false);

    return parser;
}

}  // namespace atom::test

#endif  // ATOM_TEST_CLI_HPP
