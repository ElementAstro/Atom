#ifndef ATOM_UTILS_ARGUMENT_PARSER_HPP
#define ATOM_UTILS_ARGUMENT_PARSER_HPP

#include <algorithm>
#include <any>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "atom/error/exception.hpp"
#include "atom/macro.hpp"

namespace atom::utils {

/**
 * @class ArgumentParser
 * @brief A class for parsing command-line arguments with enhanced C++20
 * features.
 *
 * The ArgumentParser class provides functionality to define and parse
 * command-line arguments, including support for different argument types,
 * flags, subcommands, and mutually exclusive groups.
 */
class ArgumentParser {
public:
    /**
     * @enum ArgType
     * @brief Enumeration of possible argument types.
     */
    enum class ArgType {
        STRING,           /**< String argument type */
        INTEGER,          /**< Integer argument type */
        UNSIGNED_INTEGER, /**< Unsigned integer argument type */
        LONG,             /**< Long integer argument type */
        UNSIGNED_LONG,    /**< Unsigned long integer argument type */
        FLOAT,            /**< Float argument type */
        DOUBLE,           /**< Double argument type */
        BOOLEAN,          /**< Boolean argument type */
        FILEPATH,         /**< File path argument type */
        AUTO              /**< Automatically detect argument type */
    };

    /**
     * @enum NargsType
     * @brief Enumeration of possible nargs types.
     */
    enum class NargsType {
        NONE,         /**< No arguments */
        OPTIONAL,     /**< Optional argument */
        ZERO_OR_MORE, /**< Zero or more arguments */
        ONE_OR_MORE,  /**< One or more arguments */
        CONSTANT      /**< Constant number of arguments */
    };

    /**
     * @struct Nargs
     * @brief Structure to define the number of arguments.
     */
    struct Nargs {
        NargsType type; /**< Type of nargs */
        int count;      /**< Count of arguments, used if type is CONSTANT */

        /**
         * @brief Default constructor initializing to NONE type and count 1.
         */
        constexpr Nargs() noexcept : type(NargsType::NONE), count(1) {}

        /**
         * @brief Constructor with specified type and count.
         * @param t Type of nargs.
         * @param c Count of arguments, default is 1.
         */
        constexpr Nargs(NargsType t, int c = 1) : type(t), count(c) {
            if (c < 0) {
                THROW_INVALID_ARGUMENT("Nargs count cannot be negative");
            }
        }
    };

    /**
     * @brief Default constructor.
     */
    ArgumentParser() = default;

    /**
     * @brief Constructor with program name.
     * @param program_name Name of the program.
     */
    explicit ArgumentParser(std::string program_name);

    /**
     * @brief Set the description of the program.
     * @param description Description text.
     */
    void setDescription(std::string_view description);

    /**
     * @brief Set the epilog of the program.
     * @param epilog Epilog text.
     */
    void setEpilog(std::string_view epilog);

    /**
     * @brief Add an argument to the parser.
     * @param name Name of the argument.
     * @param type Type of the argument.
     * @param required Whether the argument is required.
     * @param default_value Default value of the argument.
     * @param help Help text for the argument.
     * @param aliases Aliases for the argument.
     * @param is_positional Whether the argument is positional.
     * @param nargs Number of arguments.
     * @throws std::invalid_argument If argument name is invalid.
     */
    void addArgument(std::string_view name, ArgType type = ArgType::AUTO,
                     bool required = false, const std::any& default_value = {},
                     std::string_view help = "",
                     const std::vector<std::string>& aliases = {},
                     bool is_positional = false, const Nargs& nargs = Nargs());

    /**
     * @brief Add a flag to the parser.
     * @param name Name of the flag.
     * @param help Help text for the flag.
     * @param aliases Aliases for the flag.
     * @throws std::invalid_argument If flag name is invalid.
     */
    void addFlag(std::string_view name, std::string_view help = "",
                 const std::vector<std::string>& aliases = {});

    /**
     * @brief Add a subcommand to the parser.
     * @param name Name of the subcommand.
     * @param help Help text for the subcommand.
     * @throws std::invalid_argument If subcommand name is invalid.
     */
    void addSubcommand(std::string_view name, std::string_view help = "");

    /**
     * @brief Add a mutually exclusive group of arguments.
     * @param group_args Vector of argument names that are mutually exclusive.
     * @throws std::invalid_argument If any argument in the group doesn't exist.
     */
    void addMutuallyExclusiveGroup(const std::vector<std::string>& group_args);

    /**
     * @brief Enable parsing arguments from a file.
     * @param prefix Prefix to identify file arguments.
     */
    void addArgumentFromFile(std::string_view prefix = "@");

    /**
     * @brief Set the delimiter for file parsing.
     * @param delimiter Delimiter character.
     */
    void setFileDelimiter(char delimiter);

    /**
     * @brief Parse the command-line arguments.
     * @param argc Argument count.
     * @param argv Argument vector.
     * @throws std::invalid_argument For various argument parsing errors.
     */
    void parse(int argc, std::span<const std::string> argv);

    /**
     * @brief Get the value of an argument.
     * @tparam T Type of the argument value.
     * @param name Name of the argument.
     * @return Optional value of the argument.
     */
    template <typename T>
    auto get(std::string_view name) const -> std::optional<T>;

    /**
     * @brief Get the value of a flag.
     * @param name Name of the flag.
     * @return Boolean value of the flag.
     */
    auto getFlag(std::string_view name) const -> bool;

    /**
     * @brief Get the parser for a subcommand.
     * @param name Name of the subcommand.
     * @return Optional reference to the subcommand parser.
     */
    auto getSubcommandParser(std::string_view name)
        -> std::optional<std::reference_wrapper<ArgumentParser>>;

    /**
     * @brief Print the help message.
     */
    void printHelp() const;

    /**
     * @brief Add a description to the parser.
     * @param description Description text.
     */
    void addDescription(std::string_view description);

    /**
     * @brief Add an epilog to the parser.
     * @param epilog Epilog text.
     */
    void addEpilog(std::string_view epilog);

private:
    /**
     * @struct Argument
     * @brief Structure to define an argument.
     */
    struct Argument {
        ArgType type;                  /**< Type of the argument */
        bool required;                 /**< Whether the argument is required */
        std::any defaultValue;         /**< Default value of the argument */
        std::optional<std::any> value; /**< Value of the argument */
        std::string help;              /**< Help text for the argument */
        std::vector<std::string> aliases; /**< Aliases for the argument */
        bool isMultivalue;  /**< Whether the argument is multivalue */
        bool is_positional; /**< Whether the argument is positional */
        Nargs nargs;        /**< Number of arguments */

        /**
         * @brief Default constructor.
         */
        Argument() = default;

        /**
         * @brief Constructor with specified parameters.
         * @param t Type of the argument.
         * @param req Whether the argument is required.
         * @param def Default value of the argument.
         * @param hlp Help text for the argument.
         * @param als Aliases for the argument.
         * @param mult Whether the argument is multivalue.
         * @param pos Whether the argument is positional.
         * @param ng Number of arguments.
         */
        Argument(ArgType t, bool req, std::any def, std::string hlp,
                 const std::vector<std::string>& als, bool mult = false,
                 bool pos = false, const Nargs& ng = Nargs())
            : type(t),
              required(req),
              defaultValue(std::move(def)),
              help(std::move(hlp)),
              aliases(als),
              isMultivalue(mult),
              is_positional(pos),
              nargs(ng) {}
    } ATOM_ALIGNAS(128);

    /**
     * @struct Flag
     * @brief Structure to define a flag.
     */
    struct Flag {
        bool value;                       /**< Value of the flag */
        std::string help;                 /**< Help text for the flag */
        std::vector<std::string> aliases; /**< Aliases for the flag */
    } ATOM_ALIGNAS(64);

    struct Subcommand;

    std::unordered_map<std::string, Argument>
        arguments_;                               /**< Map of arguments */
    std::unordered_map<std::string, Flag> flags_; /**< Map of flags */
    std::unordered_map<std::string, std::shared_ptr<Subcommand>>
        subcommands_; /**< Map of subcommands */
    std::unordered_map<std::string, std::string>
        aliases_; /**< Map of aliases */
    std::vector<std::string>
        positionalArguments_; /**< Vector of positional arguments */
    std::string description_; /**< Description of the program */
    std::string epilog_;      /**< Epilog of the program */
    std::string programName_; /**< Name of the program */

    std::vector<std::vector<std::string>>
        mutuallyExclusiveGroups_; /**< Vector of mutually exclusive groups */

    bool enableFileParsing_ = false; /**< Enable file parsing */
    std::string filePrefix_ = "@";   /**< Prefix for file arguments */
    char fileDelimiter_ = ' ';       /**< Delimiter for file parsing */

    /**
     * @brief Detect the type of an argument from its value.
     * @param value The value of the argument.
     * @return The detected argument type.
     */
    static auto detectType(const std::any& value) -> ArgType;

    /**
     * @brief Parse the value of an argument.
     * @param type The type of the argument.
     * @param value The value of the argument as a string.
     * @return The parsed value as std::any.
     * @throws std::invalid_argument If parsing fails.
     */
    static auto parseValue(ArgType type, std::string_view value) -> std::any;

    /**
     * @brief Convert an argument type to a string.
     * @param type The type of the argument.
     * @return The argument type as a string.
     */
    static constexpr auto argTypeToString(ArgType type) -> std::string_view;

    /**
     * @brief Convert a value of type std::any to a string.
     * @param value The value to convert.
     * @return The value as a string.
     */
    static auto anyToString(const std::any& value) -> std::string;

    /**
     * @brief Expand arguments from a file.
     * @param argv The argument vector to expand.
     * @throws std::runtime_error If file cannot be opened or read.
     */
    void expandArgumentsFromFile(std::vector<std::string>& argv);

    /**
     * @brief Validate argument name.
     * @param name The name to validate.
     * @throws std::invalid_argument If name is invalid.
     */
    static void validateName(std::string_view name);

    /**
     * @brief Process positional arguments.
     * @param pos_args Vector of positional arguments.
     * @throws std::invalid_argument If required positional arguments are
     * missing.
     */
    void processPositionalArguments(const std::vector<std::string>& pos_args);

    /**
     * @brief Process a file containing arguments.
     * @param filename Name of the file to process.
     * @return Vector of arguments from the file.
     * @throws std::runtime_error If file cannot be opened or read.
     */
    std::vector<std::string> processArgumentFile(
        const std::string& filename) const;

    /**
     * @brief Parallel processing of multiple argument files.
     * @param filenames Vector of filenames to process.
     * @return Vector containing all arguments from all files.
     */
    std::vector<std::string> parallelProcessFiles(
        const std::vector<std::string>& filenames) const;
};

struct ArgumentParser::Subcommand {
    std::string help;
    ArgumentParser parser;
} ATOM_ALIGNAS(128);

inline ArgumentParser::ArgumentParser(std::string program_name)
    : programName_(std::move(program_name)) {}

inline void ArgumentParser::setDescription(std::string_view description) {
    description_ = std::string(description);
}

inline void ArgumentParser::setEpilog(std::string_view epilog) {
    epilog_ = std::string(epilog);
}

inline void ArgumentParser::validateName(std::string_view name) {
    if (name.empty()) {
        THROW_INVALID_ARGUMENT("Argument name cannot be empty");
    }
    if (name.find(' ') != std::string_view::npos) {
        THROW_INVALID_ARGUMENT("Argument name cannot contain spaces");
    }
    if (name.starts_with('-')) {
        THROW_INVALID_ARGUMENT("Argument name cannot start with '-'");
    }
}

inline void ArgumentParser::addArgument(std::string_view name, ArgType type,
                                        bool required,
                                        const std::any& default_value,
                                        std::string_view help,
                                        const std::vector<std::string>& aliases,
                                        bool is_positional,
                                        const Nargs& nargs) {
    try {
        validateName(name);

        if (type == ArgType::AUTO && default_value.has_value()) {
            type = detectType(default_value);
        } else if (type == ArgType::AUTO) {
            type = ArgType::STRING;
        }

        std::string name_str(name);
        arguments_[name_str] =
            Argument{type,          required,
                     default_value, std::string(help),
                     aliases,       nargs.type != NargsType::NONE,
                     is_positional, nargs};

        for (const auto& alias : aliases) {
            if (aliases_.contains(alias)) {
                THROW_INVALID_ARGUMENT("Alias '" + alias + "' is already used");
            }
            aliases_[alias] = name_str;
        }
    } catch (const std::exception& e) {
        THROW_INVALID_ARGUMENT(std::string("Error adding argument: ") +
                               e.what());
    }
}

inline void ArgumentParser::addFlag(std::string_view name,
                                    std::string_view help,
                                    const std::vector<std::string>& aliases) {
    try {
        validateName(name);

        std::string name_str(name);
        flags_[name_str] = Flag{false, std::string(help), aliases};

        for (const auto& alias : aliases) {
            if (aliases_.contains(alias)) {
                THROW_INVALID_ARGUMENT("Alias '" + alias + "' is already used");
            }
            aliases_[alias] = name_str;
        }
    } catch (const std::exception& e) {
        THROW_INVALID_ARGUMENT(std::string("Error adding flag: ") + e.what());
    }
}

inline void ArgumentParser::addSubcommand(std::string_view name,
                                          std::string_view help) {
    try {
        validateName(name);

        std::string name_str(name);
        subcommands_[name_str] = std::make_shared<Subcommand>(
            Subcommand{std::string(help), ArgumentParser(name_str)});
    } catch (const std::exception& e) {
        THROW_INVALID_ARGUMENT(std::string("Error adding subcommand: ") +
                               e.what());
    }
}

inline void ArgumentParser::addMutuallyExclusiveGroup(
    const std::vector<std::string>& group_args) {
    try {
        if (group_args.size() < 2) {
            THROW_INVALID_ARGUMENT(
                "Mutually exclusive group must contain at least 2 arguments");
        }

        for (const auto& arg : group_args) {
            if (!arguments_.contains(arg) && !flags_.contains(arg)) {
                THROW_INVALID_ARGUMENT("Argument or flag '" + arg +
                                       "' does not exist");
            }
        }

        mutuallyExclusiveGroups_.emplace_back(group_args);
    } catch (const std::exception& e) {
        THROW_INVALID_ARGUMENT(
            std::string("Error adding mutually exclusive group: ") + e.what());
    }
}

inline void ArgumentParser::addArgumentFromFile(std::string_view prefix) {
    enableFileParsing_ = true;
    filePrefix_ = std::string(prefix);
}

inline void ArgumentParser::setFileDelimiter(char delimiter) {
    fileDelimiter_ = delimiter;
}

inline void ArgumentParser::parse(int argc, std::span<const std::string> argv) {
    if (argc < 1 || argv.empty()) {
        THROW_INVALID_ARGUMENT("Empty command line arguments");
    }

    try {
        // Convert span to vector for modification
        std::vector<std::string> args_vector(argv.begin(), argv.end());

        // Expand arguments from files
        if (enableFileParsing_) {
            expandArgumentsFromFile(args_vector);
        }

        std::string currentSubcommand;
        std::vector<std::string> subcommandArgs;
        std::vector<std::string> positional_args;

        // Track which mutually exclusive groups have been used
        std::vector<bool> groupUsed(mutuallyExclusiveGroups_.size(), false);

        // First argument is program name
        size_t i = 1;
        while (i < args_vector.size()) {
            std::string_view arg = args_vector[i];

            // Check for subcommand
            if (subcommands_.contains(std::string(arg))) {
                currentSubcommand = std::string(arg);
                subcommandArgs.push_back(args_vector[0]);  // Program name
                ++i;
                break;  // Rest of arguments go to subcommand
            }

            // Handle help flag
            if (arg == "--help" || arg == "-h") {
                printHelp();
                std::exit(0);
            }

            // Handle optional arguments and flags
            if (arg.starts_with("--") ||
                (arg.starts_with("-") && arg.size() > 1)) {
                std::string argName;

                if (arg.starts_with("--")) {
                    argName = std::string(arg.substr(2));
                } else {
                    argName = std::string(arg.substr(1));
                }

                // Resolve aliases
                if (aliases_.contains(argName)) {
                    argName = aliases_[argName];
                }

                // Check if it's a flag
                if (flags_.contains(argName)) {
                    flags_[argName].value = true;
                    ++i;
                    continue;
                }

                // Check if it's an argument
                if (arguments_.contains(argName)) {
                    Argument& argument = arguments_[argName];
                    std::vector<std::string> values;

                    // Handle nargs
                    int expected = 1;
                    bool isConstant = false;
                    if (argument.nargs.type == NargsType::ONE_OR_MORE) {
                        expected = -1;  // Indicate multiple
                    } else if (argument.nargs.type == NargsType::ZERO_OR_MORE) {
                        expected = -1;
                    } else if (argument.nargs.type == NargsType::OPTIONAL) {
                        expected = 1;
                    } else if (argument.nargs.type == NargsType::CONSTANT) {
                        expected = argument.nargs.count;
                        isConstant = true;
                    }

                    // Collect values based on nargs
                    for (int j = 0; j < expected || expected == -1; ++j) {
                        if (i + 1 < args_vector.size() &&
                            !args_vector[i + 1].starts_with("-")) {
                            values.emplace_back(args_vector[++i]);
                        } else {
                            break;
                        }
                    }

                    if (isConstant && static_cast<int>(values.size()) !=
                                          argument.nargs.count) {
                        THROW_INVALID_ARGUMENT(
                            "Argument " + argName + " expects " +
                            std::to_string(argument.nargs.count) +
                            " value(s).");
                    }

                    if (values.empty() &&
                        argument.nargs.type == NargsType::OPTIONAL) {
                        // Optional argument without a value
                        if (argument.defaultValue.has_value()) {
                            argument.value = argument.defaultValue;
                        }
                    } else if (!values.empty()) {
                        if (expected == -1) {  // Multiple values
                            // Store as vector<string>
                            argument.value = std::any(values);
                        } else {  // Single value
                            argument.value =
                                parseValue(argument.type, values[0]);
                        }
                    }

                    ++i;
                    continue;
                }

                THROW_INVALID_ARGUMENT("Unknown argument: " + std::string(arg));
            } else {
                // Handle positional arguments
                positional_args.push_back(std::string(arg));
                ++i;
            }
        }

        // Collect remaining arguments for subcommand
        while (i < args_vector.size()) {
            subcommandArgs.push_back(args_vector[i++]);
        }

        // Process positional arguments
        if (!positional_args.empty()) {
            processPositionalArguments(positional_args);
        }

        // Parse subcommand if present
        if (!currentSubcommand.empty() && !subcommandArgs.empty()) {
            if (auto subcommand = subcommands_[currentSubcommand]) {
                subcommand->parser.parse(
                    static_cast<int>(subcommandArgs.size()),
                    std::span<const std::string>(subcommandArgs));
            }
        }

        // Validate mutually exclusive groups
        for (size_t g = 0; g < mutuallyExclusiveGroups_.size(); ++g) {
            int count = 0;
            for (const auto& arg : mutuallyExclusiveGroups_[g]) {
                if (flags_.contains(arg) && flags_[arg].value) {
                    count++;
                }
                if (arguments_.contains(arg) &&
                    arguments_[arg].value.has_value()) {
                    count++;
                }
            }
            if (count > 1) {
                THROW_INVALID_ARGUMENT(
                    "Arguments in mutually exclusive group " +
                    std::to_string(g + 1) + " cannot be used together.");
            }
        }

        // Check required arguments
        for (const auto& [name, argument] : arguments_) {
            if (argument.required && !argument.value.has_value() &&
                !argument.defaultValue.has_value()) {
                THROW_INVALID_ARGUMENT("Required argument missing: " + name);
            }
        }
    } catch (const std::exception& e) {
        THROW_INVALID_ARGUMENT(std::string("Error parsing arguments: ") +
                               e.what());
    }
}

inline void ArgumentParser::processPositionalArguments(
    const std::vector<std::string>& pos_args) {
    // Collect positional arguments
    std::vector<std::string> positional_names;
    for (const auto& [name, arg] : arguments_) {
        if (arg.is_positional) {
            positional_names.push_back(name);
        }
    }

    if (positional_names.empty() && !pos_args.empty()) {
        THROW_INVALID_ARGUMENT(
            "No positional arguments defined, but got: " + pos_args[0] +
            (pos_args.size() > 1 ? " and others" : ""));
    }

    // Match positional args to their definitions
    size_t pos_index = 0;
    for (const auto& name : positional_names) {
        if (pos_index >= pos_args.size()) {
            // Check if this positional arg is required
            const auto& arg = arguments_[name];
            if (arg.required && !arg.defaultValue.has_value()) {
                THROW_INVALID_ARGUMENT(
                    "Missing required positional argument: " + name);
            }
            continue;
        }

        auto& arg = arguments_[name];

        // Handle based on nargs type
        if (arg.nargs.type == NargsType::ONE_OR_MORE ||
            arg.nargs.type == NargsType::ZERO_OR_MORE) {
            // Collect all remaining positional arguments for this parameter
            std::vector<std::string> values;
            while (pos_index < pos_args.size()) {
                values.push_back(pos_args[pos_index++]);
            }

            if (arg.nargs.type == NargsType::ONE_OR_MORE && values.empty()) {
                THROW_INVALID_ARGUMENT("Positional argument " + name +
                                       " requires at least one value");
            }

            if (!values.empty()) {
                arg.value = std::any(values);
            }
        } else if (arg.nargs.type == NargsType::CONSTANT) {
            // Collect exact number of arguments
            std::vector<std::string> values;
            for (int i = 0; i < arg.nargs.count && pos_index < pos_args.size();
                 ++i) {
                values.push_back(pos_args[pos_index++]);
            }

            if (static_cast<int>(values.size()) != arg.nargs.count) {
                THROW_INVALID_ARGUMENT(
                    "Positional argument " + name + " requires exactly " +
                    std::to_string(arg.nargs.count) + " values");
            }

            arg.value = std::any(values);
        } else {
            // Single value or optional
            std::string value = pos_args[pos_index++];
            arg.value = parseValue(arg.type, value);
        }
    }

    // Check if there are unused positional arguments
    if (pos_index < pos_args.size()) {
        THROW_INVALID_ARGUMENT(
            "Too many positional arguments provided: " + pos_args[pos_index] +
            (pos_args.size() - pos_index > 1 ? " and others" : ""));
    }
}

template <typename T>
auto ArgumentParser::get(std::string_view name) const -> std::optional<T> {
    std::string name_str(name);
    if (!arguments_.contains(name_str)) {
        return std::nullopt;
    }

    const auto& arg = arguments_.at(name_str);
    if (arg.value.has_value()) {
        try {
            return std::any_cast<T>(arg.value.value());
        } catch (const std::bad_any_cast&) {
            // Try conversion for common types
            if constexpr (std::is_same_v<T, std::string> &&
                          std::is_integral_v<
                              typename std::decay_t<decltype(std::any_cast<int>(
                                  arg.value.value()))>>) {
                return std::to_string(std::any_cast<int>(arg.value.value()));
            }
            return std::nullopt;
        }
    }
    if (arg.defaultValue.has_value()) {
        try {
            return std::any_cast<T>(arg.defaultValue);
        } catch (const std::bad_any_cast&) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

inline auto ArgumentParser::getFlag(std::string_view name) const -> bool {
    std::string name_str(name);
    if (flags_.contains(name_str)) {
        return flags_.at(name_str).value;
    }
    return false;
}

inline auto ArgumentParser::getSubcommandParser(std::string_view name)
    -> std::optional<std::reference_wrapper<ArgumentParser>> {
    std::string name_str(name);
    if (auto it = subcommands_.find(name_str); it != subcommands_.end()) {
        return it->second->parser;
    }
    return std::nullopt;
}

inline void ArgumentParser::printHelp() const {
    std::cout << "Usage:\n  " << programName_ << " [options] ";
    if (!subcommands_.empty()) {
        std::cout << "<subcommand> [subcommand options]";
    }
    std::cout << "\n\n";

    if (!description_.empty()) {
        std::cout << description_ << "\n\n";
    }

    // Options section
    if (!arguments_.empty() || !flags_.empty()) {
        std::cout << "Options:\n";

        // Sort arguments for consistent output
        std::vector<std::pair<std::string, const Argument*>> sorted_args;
        for (const auto& [name, arg] : arguments_) {
            if (!arg.is_positional) {
                sorted_args.emplace_back(name, &arg);
            }
        }
        std::ranges::sort(sorted_args, [](const auto& a, const auto& b) {
            return a.first < b.first;
        });

        // Print arguments
        for (const auto& [name, arg_ptr] : sorted_args) {
            const auto& argument = *arg_ptr;
            std::cout << "  --" << name;
            for (const auto& alias : argument.aliases) {
                std::cout << ", -" << alias;
            }
            std::cout << " : " << argument.help;
            if (argument.defaultValue.has_value()) {
                std::cout << " (default: " << anyToString(argument.defaultValue)
                          << ")";
            }
            if (argument.nargs.type != NargsType::NONE) {
                std::cout << " [nargs: ";
                switch (argument.nargs.type) {
                    case NargsType::OPTIONAL:
                        std::cout << "?";
                        break;
                    case NargsType::ZERO_OR_MORE:
                        std::cout << "*";
                        break;
                    case NargsType::ONE_OR_MORE:
                        std::cout << "+";
                        break;
                    case NargsType::CONSTANT:
                        std::cout << std::to_string(argument.nargs.count);
                        break;
                    default:
                        std::cout << "1";
                }
                std::cout << "]";
            }
            std::cout << "\n";
        }
        for (const auto& [name, flag] : flags_) {
            std::cout << "  --" << name;
            for (const auto& alias : flag.aliases) {
                std::cout << ", -" << alias;
            }
            std::cout << " : " << flag.help << "\n";
        }
    }

    // Positional arguments
    std::vector<std::string> positional;
    for (const auto& [name, argument] : arguments_) {
        if (argument.is_positional) {
            positional.push_back(name);
        }
    }
    if (!positional.empty()) {
        std::cout << "\nPositional Arguments:\n";
        for (const auto& name : positional) {
            const auto& argument = arguments_.at(name);
            std::cout << "  " << name;
            std::cout << " : " << argument.help;
            if (argument.defaultValue.has_value()) {
                std::cout << " (default: " << anyToString(argument.defaultValue)
                          << ")";
            }
            if (argument.nargs.type != NargsType::NONE) {
                std::cout << " [nargs: ";
                switch (argument.nargs.type) {
                    case NargsType::OPTIONAL:
                        std::cout << "?";
                        break;
                    case NargsType::ZERO_OR_MORE:
                        std::cout << "*";
                        break;
                    case NargsType::ONE_OR_MORE:
                        std::cout << "+";
                        break;
                    case NargsType::CONSTANT:
                        std::cout << std::to_string(argument.nargs.count);
                        break;
                    default:
                        std::cout << "1";
                }
                std::cout << "]";
            }
            std::cout << "\n";
        }
    }

    if (!mutuallyExclusiveGroups_.empty()) {
        std::cout << "\nMutually Exclusive Groups:\n";
        for (size_t g = 0; g < mutuallyExclusiveGroups_.size(); ++g) {
            std::cout << "  Group " << g + 1 << ": ";
            for (size_t i = 0; i < mutuallyExclusiveGroups_[g].size(); ++i) {
                std::cout << "--" << mutuallyExclusiveGroups_[g][i];
                if (i != mutuallyExclusiveGroups_[g].size() - 1) {
                    std::cout << ", ";
                }
            }
            std::cout << "\n";
        }
    }

    if (!subcommands_.empty()) {
        std::cout << "\nSubcommands:\n";
        for (const auto& [name, subcommand] : subcommands_) {
            std::cout << "  " << name << " : " << subcommand->help << "\n";
        }
    }

    if (!epilog_.empty()) {
        std::cout << "\n" << epilog_ << "\n";
    }
}

inline auto ArgumentParser::detectType(const std::any& value) -> ArgType {
    if (value.type() == typeid(int)) {
        return ArgType::INTEGER;
    }
    if (value.type() == typeid(unsigned int)) {
        return ArgType::UNSIGNED_INTEGER;
    }
    if (value.type() == typeid(long)) {
        return ArgType::LONG;
    }
    if (value.type() == typeid(unsigned long)) {
        return ArgType::UNSIGNED_LONG;
    }
    if (value.type() == typeid(float)) {
        return ArgType::FLOAT;
    }
    if (value.type() == typeid(double)) {
        return ArgType::DOUBLE;
    }
    if (value.type() == typeid(bool)) {
        return ArgType::BOOLEAN;
    }
    if (value.type() == typeid(std::string)) {
        return ArgType::STRING;
    }
    if (value.type() == typeid(std::filesystem::path)) {
        return ArgType::FILEPATH;
    }
    return ArgType::STRING;
}

inline auto ArgumentParser::parseValue(ArgType type,
                                       std::string_view value) -> std::any {
    try {
        switch (type) {
            case ArgType::STRING:
                return std::string(value);

            case ArgType::INTEGER: {
                // Fast integer parsing with bounds checking
                const char* str = value.data();
                char* end;
                errno = 0;
                long val = std::strtol(str, &end, 10);

                if (errno == ERANGE || val < INT_MIN || val > INT_MAX) {
                    throw std::out_of_range("Integer value out of range");
                }
                if (end == str || *end != '\0') {
                    THROW_INVALID_ARGUMENT("Invalid integer format");
                }
                return static_cast<int>(val);
            }

            case ArgType::UNSIGNED_INTEGER: {
                // Fast unsigned integer parsing with bounds checking
                const char* str = value.data();
                char* end;
                errno = 0;
                unsigned long val = std::strtoul(str, &end, 10);

                if (errno == ERANGE || val > UINT_MAX) {
                    throw std::out_of_range(
                        "Unsigned integer value out of range");
                }
                if (end == str || *end != '\0' || value.starts_with("-")) {
                    THROW_INVALID_ARGUMENT("Invalid unsigned integer format");
                }
                return static_cast<unsigned int>(val);
            }

            case ArgType::LONG: {
                const char* str = value.data();
                char* end;
                errno = 0;
                long val = std::strtol(str, &end, 10);

                if (errno == ERANGE) {
                    throw std::out_of_range("Long value out of range");
                }
                if (end == str || *end != '\0') {
                    THROW_INVALID_ARGUMENT("Invalid long format");
                }
                return val;
            }

            case ArgType::UNSIGNED_LONG: {
                const char* str = value.data();
                char* end;
                errno = 0;
                unsigned long val = std::strtoul(str, &end, 10);

                if (errno == ERANGE) {
                    throw std::out_of_range("Unsigned long value out of range");
                }
                if (end == str || *end != '\0' || value.starts_with("-")) {
                    THROW_INVALID_ARGUMENT("Invalid unsigned long format");
                }
                return val;
            }

            case ArgType::FLOAT: {
                const char* str = value.data();
                char* end;
                errno = 0;
                float val = std::strtof(str, &end);

                if (errno == ERANGE) {
                    throw std::out_of_range("Float value out of range");
                }
                if (end == str || *end != '\0') {
                    THROW_INVALID_ARGUMENT("Invalid float format");
                }
                return val;
            }

            case ArgType::DOUBLE: {
                const char* str = value.data();
                char* end;
                errno = 0;
                double val = std::strtod(str, &end);

                if (errno == ERANGE) {
                    throw std::out_of_range("Double value out of range");
                }
                if (end == str || *end != '\0') {
                    THROW_INVALID_ARGUMENT("Invalid double format");
                }
                return val;
            }

            case ArgType::BOOLEAN:
                // Improved boolean parsing with more options
                if (value == "true" || value == "1" || value == "yes" ||
                    value == "y" || value == "on") {
                    return true;
                }
                if (value == "false" || value == "0" || value == "no" ||
                    value == "n" || value == "off") {
                    return false;
                }
                THROW_INVALID_ARGUMENT("Invalid boolean value: " +
                                       std::string(value));

            case ArgType::FILEPATH: {
                std::filesystem::path path((std::string(value)));
                // Validate path format
                if (value.find('\0') != std::string_view::npos) {
                    THROW_INVALID_ARGUMENT("Path contains null characters");
                }
                return path;
            }

            default:
                return std::string(value);
        }
    } catch (const std::exception& e) {
        THROW_INVALID_ARGUMENT("Failed to parse value '" + std::string(value) +
                               "': " + e.what());
    }
}

inline constexpr auto ArgumentParser::argTypeToString(ArgType type)
    -> std::string_view {
    switch (type) {
        case ArgType::STRING:
            return "string";
        case ArgType::INTEGER:
            return "integer";
        case ArgType::UNSIGNED_INTEGER:
            return "unsigned integer";
        case ArgType::LONG:
            return "long";
        case ArgType::UNSIGNED_LONG:
            return "unsigned long";
        case ArgType::FLOAT:
            return "float";
        case ArgType::DOUBLE:
            return "double";
        case ArgType::BOOLEAN:
            return "boolean";
        case ArgType::FILEPATH:
            return "filepath";
        case ArgType::AUTO:
            return "auto";
        default:
            return "unknown";
    }
}

inline auto ArgumentParser::anyToString(const std::any& value) -> std::string {
    if (!value.has_value()) {
        return "null";
    }

    try {
        if (value.type() == typeid(std::string)) {
            return std::any_cast<std::string>(value);
        }
        if (value.type() == typeid(int)) {
            return std::to_string(std::any_cast<int>(value));
        }
        if (value.type() == typeid(unsigned int)) {
            return std::to_string(std::any_cast<unsigned int>(value));
        }
        if (value.type() == typeid(long)) {
            return std::to_string(std::any_cast<long>(value));
        }
        if (value.type() == typeid(unsigned long)) {
            return std::to_string(std::any_cast<unsigned long>(value));
        }
        if (value.type() == typeid(float)) {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(6)
               << std::any_cast<float>(value);
            return ss.str();
        }
        if (value.type() == typeid(double)) {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(10)
               << std::any_cast<double>(value);
            return ss.str();
        }
        if (value.type() == typeid(bool)) {
            return std::any_cast<bool>(value) ? "true" : "false";
        }
        if (value.type() == typeid(std::filesystem::path)) {
            return std::any_cast<std::filesystem::path>(value).string();
        }
        // Array types
        if (value.type() == typeid(std::vector<std::string>)) {
            const auto& vec =
                std::any_cast<const std::vector<std::string>&>(value);
            std::ostringstream ss;
            ss << "[";
            for (size_t i = 0; i < vec.size(); ++i) {
                ss << "\"" << vec[i] << "\"";
                if (i < vec.size() - 1) {
                    ss << ", ";
                }
            }
            ss << "]";
            return ss.str();
        }

        return "unknown type: " + std::string(value.type().name());
    } catch (const std::exception& e) {
        return "error: " + std::string(e.what());
    }
}

// 自定义文件解析实现
inline void ArgumentParser::expandArgumentsFromFile(
    std::vector<std::string>& argv) {
    try {
        std::vector<std::string> expandedArgs;
        std::vector<std::string> filenames;

        // First pass: collect normal args and files to process
        for (const auto& arg : argv) {
            if (arg.starts_with(filePrefix_)) {
                std::string filename = arg.substr(filePrefix_.length());
                filenames.push_back(filename);
            } else {
                expandedArgs.emplace_back(arg);
            }
        }

        // Process files in parallel if we have multiple files
        if (!filenames.empty()) {
            std::vector<std::string> file_args;

            if (filenames.size() > 1 &&
                std::thread::hardware_concurrency() > 1) {
                // Use parallel processing for multiple files
                file_args = parallelProcessFiles(filenames);
            } else {
                // Process files sequentially
                for (const auto& filename : filenames) {
                    auto args = processArgumentFile(filename);
                    file_args.insert(file_args.end(), args.begin(), args.end());
                }
            }

            // Merge file arguments with normal arguments
            expandedArgs.insert(expandedArgs.end(), file_args.begin(),
                                file_args.end());
        }

        argv = std::move(expandedArgs);
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("Error expanding arguments from file: ") + e.what());
    }
}

inline std::vector<std::string> ArgumentParser::processArgumentFile(
    const std::string& filename) const {
    std::vector<std::string> args;

    std::ifstream infile(filename);
    if (!infile) {
        throw std::runtime_error("Unable to open argument file: " + filename);
    }

    std::string line;
    while (std::getline(infile, line)) {
        // Skip empty lines and comments
        if (line.empty() || line.starts_with("#")) {
            continue;
        }

        std::istringstream iss(line);
        std::string token;

        while (std::getline(iss, token, fileDelimiter_)) {
            if (!token.empty()) {
                // Trim whitespace
                token.erase(0, token.find_first_not_of(" \t"));
                token.erase(token.find_last_not_of(" \t") + 1);

                if (!token.empty()) {
                    args.emplace_back(token);
                }
            }
        }
    }

    return args;
}

inline std::vector<std::string> ArgumentParser::parallelProcessFiles(
    const std::vector<std::string>& filenames) const {
    const unsigned int num_threads =
        std::min(static_cast<unsigned int>(filenames.size()),
                 std::thread::hardware_concurrency());

    std::vector<std::vector<std::string>> results(filenames.size());
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    auto worker = [this, &filenames, &results](size_t start, size_t end) {
        for (size_t i = start; i < end; ++i) {
            try {
                results[i] = processArgumentFile(filenames[i]);
            } catch (const std::exception& e) {
                // Log error but continue with other files
                std::cerr << "Error processing file " << filenames[i] << ": "
                          << e.what() << std::endl;
            }
        }
    };

    // Calculate workload per thread
    size_t files_per_thread = filenames.size() / num_threads;
    size_t remainder = filenames.size() % num_threads;

    // Launch threads
    size_t start = 0;
    for (unsigned int i = 0; i < num_threads; ++i) {
        size_t end = start + files_per_thread + (i < remainder ? 1 : 0);
        threads.emplace_back(worker, start, end);
        start = end;
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    // Combine results
    std::vector<std::string> combined_args;
    for (const auto& result : results) {
        combined_args.insert(combined_args.end(), result.begin(), result.end());
    }

    return combined_args;
}

inline void ArgumentParser::addDescription(std::string_view description) {
    description_ = std::string(description);
}

inline void ArgumentParser::addEpilog(std::string_view epilog) {
    epilog_ = std::string(epilog);
}

}  // namespace atom::utils

#endif  // ATOM_UTILS_ARGUMENT_PARSER_HPP
