#ifndef ATOM_UTILS_ARGUMENT_PARSER_HPP
#define ATOM_UTILS_ARGUMENT_PARSER_HPP

#include <algorithm>
#include <any>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>  // Required for std::setprecision
#include <iostream>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <sstream>
// #include <string> // Replaced by high_performance.hpp
// #include <string_view> // Replaced by high_performance.hpp String
#include <thread>
#include <type_traits>
// #include <unordered_map> // Replaced by high_performance.hpp
#include <utility>
// #include <vector> // Replaced by high_performance.hpp

#include "atom/containers/high_performance.hpp"  // Include high performance containers
#include "atom/error/exception.hpp"
#include "atom/macro.hpp"

namespace atom::utils {

// Use type aliases from high_performance.hpp
using atom::containers::String;
template <typename K, typename V>
using HashMap = atom::containers::HashMap<K, V>;
template <typename T>
using Vector = atom::containers::Vector<T>;

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
    explicit ArgumentParser(String program_name);

    /**
     * @brief Set the description of the program.
     * @param description Description text.
     */
    void setDescription(const String& description);

    /**
     * @brief Set the epilog of the program.
     * @param epilog Epilog text.
     */
    void setEpilog(const String& epilog);

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
    void addArgument(const String& name, ArgType type = ArgType::AUTO,
                     bool required = false, const std::any& default_value = {},
                     const String& help = "",
                     const Vector<String>& aliases = {},
                     bool is_positional = false, const Nargs& nargs = Nargs());

    /**
     * @brief Add a flag to the parser.
     * @param name Name of the flag.
     * @param help Help text for the flag.
     * @param aliases Aliases for the flag.
     * @throws std::invalid_argument If flag name is invalid.
     */
    void addFlag(const String& name, const String& help = "",
                 const Vector<String>& aliases = {});

    /**
     * @brief Add a subcommand to the parser.
     * @param name Name of the subcommand.
     * @param help Help text for the subcommand.
     * @throws std::invalid_argument If subcommand name is invalid.
     */
    void addSubcommand(const String& name, const String& help = "");

    /**
     * @brief Add a mutually exclusive group of arguments.
     * @param group_args Vector of argument names that are mutually exclusive.
     * @throws std::invalid_argument If any argument in the group doesn't exist.
     */
    void addMutuallyExclusiveGroup(const Vector<String>& group_args);

    /**
     * @brief Enable parsing arguments from a file.
     * @param prefix Prefix to identify file arguments.
     */
    void addArgumentFromFile(const String& prefix = "@");

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
    void parse(int argc, std::span<const String> argv);

    /**
     * @brief Get the value of an argument.
     * @tparam T Type of the argument value.
     * @param name Name of the argument.
     * @return Optional value of the argument.
     */
    template <typename T>
    auto get(const String& name) const -> std::optional<T>;

    /**
     * @brief Get the value of a flag.
     * @param name Name of the flag.
     * @return Boolean value of the flag.
     */
    auto getFlag(const String& name) const -> bool;

    /**
     * @brief Get the parser for a subcommand.
     * @param name Name of the subcommand.
     * @return Optional reference to the subcommand parser.
     */
    auto getSubcommandParser(const String& name)
        -> std::optional<std::reference_wrapper<ArgumentParser>>;

    /**
     * @brief Print the help message.
     */
    void printHelp() const;

    /**
     * @brief Add a description to the parser.
     * @param description Description text.
     */
    void addDescription(const String& description);

    /**
     * @brief Add an epilog to the parser.
     * @param epilog Epilog text.
     */
    void addEpilog(const String& epilog);

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
        String help;                   /**< Help text for the argument */
        Vector<String> aliases;        /**< Aliases for the argument */
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
        Argument(ArgType t, bool req, std::any def, String hlp,
                 const Vector<String>& als, bool mult = false, bool pos = false,
                 const Nargs& ng = Nargs())
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
        bool value;             /**< Value of the flag */
        String help;            /**< Help text for the flag */
        Vector<String> aliases; /**< Aliases for the flag */
    } ATOM_ALIGNAS(64);

    struct Subcommand;

    HashMap<String, Argument> arguments_; /**< Map of arguments */
    HashMap<String, Flag> flags_;         /**< Map of flags */
    HashMap<String, std::shared_ptr<Subcommand>>
        subcommands_;                    /**< Map of subcommands */
    HashMap<String, String> aliases_;    /**< Map of aliases */
    Vector<String> positionalArguments_; /**< Vector of positional arguments */
    String description_;                 /**< Description of the program */
    String epilog_;                      /**< Epilog of the program */
    String programName_;                 /**< Name of the program */

    Vector<Vector<String>>
        mutuallyExclusiveGroups_; /**< Vector of mutually exclusive groups */

    bool enableFileParsing_ = false; /**< Enable file parsing */
    String filePrefix_ = "@";        /**< Prefix for file arguments */
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
    static auto parseValue(ArgType type, const String& value) -> std::any;

    /**
     * @brief Convert an argument type to a string.
     * @param type The type of the argument.
     * @return The argument type as a string.
     */
    static constexpr auto argTypeToString(ArgType type) -> const
        char*;  // Return const char* for constexpr

    /**
     * @brief Convert a value of type std::any to a string.
     * @param value The value to convert.
     * @return The value as a string.
     */
    static auto anyToString(const std::any& value) -> String;

    /**
     * @brief Expand arguments from a file.
     * @param argv The argument vector to expand.
     * @throws std::runtime_error If file cannot be opened or read.
     */
    void expandArgumentsFromFile(Vector<String>& argv);

    /**
     * @brief Validate argument name.
     * @param name The name to validate.
     * @throws std::invalid_argument If name is invalid.
     */
    static void validateName(const String& name);

    /**
     * @brief Process positional arguments.
     * @param pos_args Vector of positional arguments.
     * @throws std::invalid_argument If required positional arguments are
     * missing.
     */
    void processPositionalArguments(const Vector<String>& pos_args);

    /**
     * @brief Process a file containing arguments.
     * @param filename Name of the file to process.
     * @return Vector of arguments from the file.
     * @throws std::runtime_error If file cannot be opened or read.
     */
    auto processArgumentFile(const String& filename) const -> Vector<String>;

    /**
     * @brief Parallel processing of multiple argument files.
     * @param filenames Vector of filenames to process.
     * @return Vector containing all arguments from all files.
     */
    auto parallelProcessFiles(const Vector<String>& filenames) const
        -> Vector<String>;
};

struct ArgumentParser::Subcommand {
    String help;
    ArgumentParser parser;
} ATOM_ALIGNAS(128);

inline ArgumentParser::ArgumentParser(String program_name)
    : programName_(std::move(program_name)) {}

inline void ArgumentParser::setDescription(const String& description) {
    description_ = description;
}

inline void ArgumentParser::setEpilog(const String& epilog) {
    epilog_ = epilog;
}

inline void ArgumentParser::validateName(const String& name) {
    if (name.empty()) {
        THROW_INVALID_ARGUMENT("Argument name cannot be empty");
    }
    // Assuming String has find method similar to std::string
    if (name.find(' ') != String::npos) {
        THROW_INVALID_ARGUMENT("Argument name cannot contain spaces");
    }
    // Assuming String has starts_with method or equivalent
    if (name.rfind('-', 0) == 0) {  // Check if starts with '-'
        THROW_INVALID_ARGUMENT("Argument name cannot start with '-'");
    }
}

inline void ArgumentParser::addArgument(
    const String& name, ArgType type, bool required,
    const std::any& default_value, const String& help,
    const Vector<String>& aliases, bool is_positional, const Nargs& nargs) {
    try {
        validateName(name);

        if (type == ArgType::AUTO && default_value.has_value()) {
            type = detectType(default_value);
        } else if (type == ArgType::AUTO) {
            type = ArgType::STRING;
        }

        // Assuming HashMap operator[] works like std::unordered_map
        arguments_[name] =
            Argument{type,          required, default_value,
                     help,          aliases,  nargs.type != NargsType::NONE,
                     is_positional, nargs};

        for (const auto& alias : aliases) {
            // Assuming HashMap has contains method
            if (aliases_.contains(alias)) {
                THROW_INVALID_ARGUMENT("Alias '" + alias + "' is already used");
            }
            aliases_[alias] = name;
        }
    } catch (const std::exception& e) {
        // Assuming String can be constructed from const char* and concatenated
        THROW_INVALID_ARGUMENT(String("Error adding argument: ") + e.what());
    }
}

inline void ArgumentParser::addFlag(const String& name, const String& help,
                                    const Vector<String>& aliases) {
    try {
        validateName(name);

        flags_[name] = Flag{false, help, aliases};

        for (const auto& alias : aliases) {
            if (aliases_.contains(alias)) {
                THROW_INVALID_ARGUMENT("Alias '" + alias + "' is already used");
            }
            aliases_[alias] = name;
        }
    } catch (const std::exception& e) {
        THROW_INVALID_ARGUMENT(String("Error adding flag: ") + e.what());
    }
}

inline void ArgumentParser::addSubcommand(const String& name,
                                          const String& help) {
    try {
        validateName(name);

        subcommands_[name] = std::make_shared<Subcommand>(
            Subcommand{help, ArgumentParser(name)});
    } catch (const std::exception& e) {
        THROW_INVALID_ARGUMENT(String("Error adding subcommand: ") + e.what());
    }
}

inline void ArgumentParser::addMutuallyExclusiveGroup(
    const Vector<String>& group_args) {
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
        // Assuming Vector has emplace_back
        mutuallyExclusiveGroups_.emplace_back(group_args);
    } catch (const std::exception& e) {
        THROW_INVALID_ARGUMENT(
            String("Error adding mutually exclusive group: ") + e.what());
    }
}

inline void ArgumentParser::addArgumentFromFile(const String& prefix) {
    enableFileParsing_ = true;
    filePrefix_ = prefix;
}

inline void ArgumentParser::setFileDelimiter(char delimiter) {
    fileDelimiter_ = delimiter;
}

inline void ArgumentParser::parse(int argc, std::span<const String> argv) {
    if (argc < 1 || argv.empty()) {
        THROW_INVALID_ARGUMENT("Empty command line arguments");
    }

    try {
        // Convert span to vector for modification
        Vector<String> args_vector(argv.begin(), argv.end());

        // Expand arguments from files
        if (enableFileParsing_) {
            expandArgumentsFromFile(args_vector);
        }

        String currentSubcommand;
        Vector<String> subcommandArgs;
        Vector<String> positional_args;

        // Track which mutually exclusive groups have been used
        // Assuming std::vector<bool> is acceptable or replace with Vector<bool>
        // if needed
        std::vector<bool> groupUsed(mutuallyExclusiveGroups_.size(), false);

        // First argument is program name
        size_t i = 1;
        while (i < args_vector.size()) {
            const String& arg = args_vector[i];

            // Check for subcommand
            if (subcommands_.contains(arg)) {
                currentSubcommand = arg;
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
            // Assuming String has starts_with or equivalent (using rfind)
            if (arg.rfind("--", 0) == 0 ||
                (arg.rfind("-", 0) == 0 && arg.size() > 1)) {
                String argName;

                // Assuming String has substr method
                if (arg.rfind("--", 0) == 0) {
                    argName = arg.substr(2);
                } else {
                    argName = arg.substr(1);
                }

                // Resolve aliases
                if (aliases_.contains(argName)) {
                    // Assuming HashMap has at method
                    argName = aliases_.at(argName);
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
                    Vector<String> values;

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
                            args_vector[i + 1].rfind("-", 0) !=
                                0) {  // Check if next arg starts with '-'
                            values.emplace_back(args_vector[++i]);
                        } else {
                            break;
                        }
                    }

                    if (isConstant && static_cast<int>(values.size()) !=
                                          argument.nargs.count) {
                        THROW_INVALID_ARGUMENT(
                            "Argument " + argName + " expects " +
                            String(std::to_string(
                                argument.nargs
                                    .count)) +  // Convert int to String
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
                            // Store as Vector<String>
                            argument.value = std::any(values);
                        } else {  // Single value
                            argument.value =
                                parseValue(argument.type, values[0]);
                        }
                    }

                    ++i;
                    continue;
                }

                THROW_INVALID_ARGUMENT("Unknown argument: " + arg);
            } else {
                // Handle positional arguments
                positional_args.push_back(arg);
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
                    std::span<const String>(subcommandArgs));
            }
        }

        // Validate mutually exclusive groups
        for (size_t g = 0; g < mutuallyExclusiveGroups_.size(); ++g) {
            int count = 0;
            for (const auto& arg : mutuallyExclusiveGroups_[g]) {
                if (flags_.contains(arg) && flags_.at(arg).value) {
                    count++;
                }
                if (arguments_.contains(arg) &&
                    arguments_.at(arg).value.has_value()) {
                    count++;
                }
            }
            if (count > 1) {
                THROW_INVALID_ARGUMENT(
                    "Arguments in mutually exclusive group " +
                    String(std::to_string(g + 1)) +  // Convert int to String
                    " cannot be used together.");
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
        THROW_INVALID_ARGUMENT(String("Error parsing arguments: ") + e.what());
    }
}

inline void ArgumentParser::processPositionalArguments(
    const Vector<String>& pos_args) {
    // Collect positional arguments
    Vector<String> positional_names;
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
            const auto& arg = arguments_.at(name);
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
            Vector<String> values;
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
            Vector<String> values;
            for (int i = 0; i < arg.nargs.count && pos_index < pos_args.size();
                 ++i) {
                values.push_back(pos_args[pos_index++]);
            }

            if (static_cast<int>(values.size()) != arg.nargs.count) {
                THROW_INVALID_ARGUMENT("Positional argument " + name +
                                       " requires exactly " +
                                       String(std::to_string(arg.nargs.count)) +
                                       " values");  // Convert int to String
            }

            arg.value = std::any(values);
        } else {
            // Single value or optional
            String value = pos_args[pos_index++];
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
auto ArgumentParser::get(const String& name) const -> std::optional<T> {
    if (!arguments_.contains(name)) {
        return std::nullopt;
    }

    const auto& arg = arguments_.at(name);
    if (arg.value.has_value()) {
        try {
            // Handle Vector<String> case for multivalue arguments
            if constexpr (std::is_same_v<T, Vector<String>>) {
                if (arg.value.value().type() == typeid(Vector<String>)) {
                    return std::any_cast<Vector<String>>(arg.value.value());
                }
                // If stored as single string, wrap in vector
                if (arg.value.value().type() == typeid(String)) {
                    Vector<String> vec;
                    vec.push_back(std::any_cast<String>(arg.value.value()));
                    return vec;
                }
            }
            // Handle single value case
            return std::any_cast<T>(arg.value.value());
        } catch (const std::bad_any_cast&) {
            // Try conversion for common types (e.g., int to String)
            if constexpr (std::is_same_v<T, String>) {
                if (arg.value.value().type() == typeid(int)) {
                    return String(
                        std::to_string(std::any_cast<int>(arg.value.value())));
                }
                // Add other potential conversions if needed
            }
            return std::nullopt;
        }
    }
    if (arg.defaultValue.has_value()) {
        try {
            // Handle Vector<String> case for default multivalue arguments
            if constexpr (std::is_same_v<T, Vector<String>>) {
                if (arg.defaultValue.type() == typeid(Vector<String>)) {
                    return std::any_cast<Vector<String>>(arg.defaultValue);
                }
                // If default stored as single string, wrap in vector
                if (arg.defaultValue.type() == typeid(String)) {
                    Vector<String> vec;
                    vec.push_back(std::any_cast<String>(arg.defaultValue));
                    return vec;
                }
            }
            return std::any_cast<T>(arg.defaultValue);
        } catch (const std::bad_any_cast&) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

inline auto ArgumentParser::getFlag(const String& name) const -> bool {
    if (flags_.contains(name)) {
        return flags_.at(name).value;
    }
    return false;
}

inline auto ArgumentParser::getSubcommandParser(const String& name)
    -> std::optional<std::reference_wrapper<ArgumentParser>> {
    if (auto it = subcommands_.find(name); it != subcommands_.end()) {
        return it->second->parser;
    }
    return std::nullopt;
}

inline void ArgumentParser::printHelp() const {
    // Assuming String can be streamed to std::cout
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
        // Assuming Vector can store pairs and std::ranges::sort works
        Vector<std::pair<String, const Argument*>> sorted_args;
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
    Vector<String> positional;
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
    const auto& typeInfo = value.type();
    if (typeInfo == typeid(int))
        return ArgType::INTEGER;
    if (typeInfo == typeid(unsigned int))
        return ArgType::UNSIGNED_INTEGER;
    if (typeInfo == typeid(long))
        return ArgType::LONG;
    if (typeInfo == typeid(unsigned long))
        return ArgType::UNSIGNED_LONG;
    if (typeInfo == typeid(float))
        return ArgType::FLOAT;
    if (typeInfo == typeid(double))
        return ArgType::DOUBLE;
    if (typeInfo == typeid(bool))
        return ArgType::BOOLEAN;
    if (typeInfo == typeid(String))
        return ArgType::STRING;  // Check for atom::containers::String
    if (typeInfo == typeid(std::filesystem::path))
        return ArgType::FILEPATH;
    // Add check for Vector<String> if needed for default values
    if (typeInfo == typeid(Vector<String>))
        return ArgType::STRING;  // Treat as string for nargs

    return ArgType::STRING;  // Default fallback
}

inline auto ArgumentParser::parseValue(ArgType type, const String& value)
    -> std::any {
    try {
        // Assuming String has c_str() and data() methods
        const char* str = value.c_str();  // Use c_str() for C-style functions
        size_t len = value.length();

        switch (type) {
            case ArgType::STRING:
                return value;  // Return the original String

            case ArgType::INTEGER: {
                char* end;
                errno = 0;
                long val = std::strtol(str, &end, 10);

                if (errno == ERANGE || val < INT_MIN || val > INT_MAX) {
                    throw std::out_of_range("Integer value out of range");
                }
                if (end != str + len) {  // Check if entire string was parsed
                    THROW_INVALID_ARGUMENT("Invalid integer format");
                }
                return static_cast<int>(val);
            }

            case ArgType::UNSIGNED_INTEGER: {
                char* end;
                errno = 0;
                // Check for negative sign manually
                if (value.find('-') != String::npos) {
                    THROW_INVALID_ARGUMENT(
                        "Invalid unsigned integer format (contains '-')");
                }
                unsigned long val = std::strtoul(str, &end, 10);

                if (errno == ERANGE || val > UINT_MAX) {
                    throw std::out_of_range(
                        "Unsigned integer value out of range");
                }
                if (end != str + len) {  // Check if entire string was parsed
                    THROW_INVALID_ARGUMENT("Invalid unsigned integer format");
                }
                return static_cast<unsigned int>(val);
            }

            case ArgType::LONG: {
                char* end;
                errno = 0;
                long val = std::strtol(str, &end, 10);

                if (errno == ERANGE) {
                    throw std::out_of_range("Long value out of range");
                }
                if (end != str + len) {  // Check if entire string was parsed
                    THROW_INVALID_ARGUMENT("Invalid long format");
                }
                return val;
            }

            case ArgType::UNSIGNED_LONG: {
                char* end;
                errno = 0;
                // Check for negative sign manually
                if (value.find('-') != String::npos) {
                    THROW_INVALID_ARGUMENT(
                        "Invalid unsigned long format (contains '-')");
                }
                unsigned long val = std::strtoul(str, &end, 10);

                if (errno == ERANGE) {
                    throw std::out_of_range("Unsigned long value out of range");
                }
                if (end != str + len) {  // Check if entire string was parsed
                    THROW_INVALID_ARGUMENT("Invalid unsigned long format");
                }
                return val;
            }

            case ArgType::FLOAT: {
                char* end;
                errno = 0;
                float val = std::strtof(str, &end);

                if (errno == ERANGE) {
                    throw std::out_of_range("Float value out of range");
                }
                if (end != str + len) {  // Check if entire string was parsed
                    THROW_INVALID_ARGUMENT("Invalid float format");
                }
                return val;
            }

            case ArgType::DOUBLE: {
                char* end;
                errno = 0;
                double val = std::strtod(str, &end);

                if (errno == ERANGE) {
                    throw std::out_of_range("Double value out of range");
                }
                if (end != str + len) {  // Check if entire string was parsed
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
                THROW_INVALID_ARGUMENT("Invalid boolean value: " + value);

            case ArgType::FILEPATH: {
                // Assuming filesystem::path can be constructed from String or
                // its c_str()
                std::filesystem::path path(value.c_str());
                // Validate path format (basic check for null chars)
                if (value.find('\0') != String::npos) {
                    THROW_INVALID_ARGUMENT("Path contains null characters");
                }
                return path;
            }

            default:  // Includes ArgType::AUTO which should have been resolved
                return value;
        }
    } catch (const std::exception& e) {
        THROW_INVALID_ARGUMENT("Failed to parse value '" + value +
                               "': " + e.what());
    }
}

// Return const char* for constexpr compatibility
inline constexpr auto ArgumentParser::argTypeToString(ArgType type) -> const
    char* {
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

inline auto ArgumentParser::anyToString(const std::any& value) -> String {
    if (!value.has_value()) {
        return "null";
    }

    try {
        const auto& typeInfo = value.type();

        if (typeInfo == typeid(String))
            return std::any_cast<String>(value);
        if (typeInfo == typeid(int))
            return String(std::to_string(std::any_cast<int>(value)));
        if (typeInfo == typeid(unsigned int))
            return String(std::to_string(std::any_cast<unsigned int>(value)));
        if (typeInfo == typeid(long))
            return String(std::to_string(std::any_cast<long>(value)));
        if (typeInfo == typeid(unsigned long))
            return String(std::to_string(std::any_cast<unsigned long>(value)));
        if (typeInfo == typeid(float)) {
            // Use std::format if available and String is compatible, otherwise
            // ostringstream
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(6)
               << std::any_cast<float>(value);
            return String(
                ss.str().c_str());  // Convert std::string from ss to String
        }
        if (typeInfo == typeid(double)) {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(10)
               << std::any_cast<double>(value);
            return String(
                ss.str().c_str());  // Convert std::string from ss to String
        }
        if (typeInfo == typeid(bool))
            return std::any_cast<bool>(value) ? "true" : "false";
        if (typeInfo == typeid(std::filesystem::path)) {
            // Assuming path.string() returns std::string, convert to String
            return String(
                std::any_cast<std::filesystem::path>(value).string().c_str());
        }
        // Array types
        if (typeInfo == typeid(Vector<String>)) {
            const auto& vec = std::any_cast<const Vector<String>&>(value);
            String result = "[";
            for (size_t i = 0; i < vec.size(); ++i) {
                result += "\"" + vec[i] + "\"";
                if (i < vec.size() - 1) {
                    result += ", ";
                }
            }
            result += "]";
            return result;
        }

        return String("unknown type: ") + value.type().name();
    } catch (const std::exception& e) {
        return String("error: ") + e.what();
    }
}

// 自定义文件解析实现
inline void ArgumentParser::expandArgumentsFromFile(Vector<String>& argv) {
    try {
        Vector<String> expandedArgs;
        Vector<String> filenames;

        // First pass: collect normal args and files to process
        for (const auto& arg : argv) {
            // Assuming String has starts_with or equivalent (using rfind)
            if (arg.rfind(filePrefix_, 0) == 0) {
                // Assuming String has substr
                String filename = arg.substr(filePrefix_.length());
                filenames.push_back(filename);
            } else {
                expandedArgs.emplace_back(arg);
            }
        }

        // Process files in parallel if we have multiple files
        if (!filenames.empty()) {
            Vector<String> file_args;

            if (filenames.size() > 1 &&
                std::thread::hardware_concurrency() > 1) {
                // Use parallel processing for multiple files
                file_args = parallelProcessFiles(filenames);
            } else {
                // Process files sequentially
                for (const auto& filename : filenames) {
                    auto args = processArgumentFile(filename);
                    // Assuming Vector has insert method
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
            String("Error expanding arguments from file: ") + e.what());
    }
}

inline auto ArgumentParser::processArgumentFile(const String& filename) const
    -> Vector<String> {
    Vector<String> args;

    // Assuming String has c_str() for ifstream constructor
    std::ifstream infile(filename.c_str());
    if (!infile) {
        throw std::runtime_error("Unable to open argument file: " + filename);
    }

    std::string line;  // Use std::string for getline
    while (std::getline(infile, line)) {
        // Skip empty lines and comments (assuming String has starts_with or
        // equivalent)
        if (line.empty() || line.rfind("#", 0) == 0) {
            continue;
        }

        // Use std::string for istringstream processing
        std::istringstream iss(line);
        std::string token_std;  // Use std::string for token

        while (std::getline(iss, token_std, fileDelimiter_)) {
            if (!token_std.empty()) {
                // Trim whitespace using std::string methods
                token_std.erase(0, token_std.find_first_not_of(" \t"));
                token_std.erase(token_std.find_last_not_of(" \t") + 1);

                if (!token_std.empty()) {
                    // Convert std::string token back to String
                    args.emplace_back(token_std.c_str());
                }
            }
        }
    }

    return args;
}

inline auto ArgumentParser::parallelProcessFiles(
    const Vector<String>& filenames) const -> Vector<String> {
    const unsigned int num_threads =
        std::min(static_cast<unsigned int>(filenames.size()),
                 std::thread::hardware_concurrency());

    // Assuming Vector can store Vector<String>
    Vector<Vector<String>> results(filenames.size());
    // Assuming std::vector<std::thread> is okay
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    auto worker = [this, &filenames, &results](size_t start, size_t end) {
        for (size_t i = start; i < end; ++i) {
            try {
                results[i] = processArgumentFile(filenames[i]);
            } catch (const std::exception& e) {
                // Log error but continue with other files
                // Assuming String can be streamed to std::cerr
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
    Vector<String> combined_args;
    for (const auto& result : results) {
        combined_args.insert(combined_args.end(), result.begin(), result.end());
    }

    return combined_args;
}

inline void ArgumentParser::addDescription(const String& description) {
    description_ = description;
}

inline void ArgumentParser::addEpilog(const String& epilog) {
    epilog_ = epilog;
}

}  // namespace atom::utils

#endif  // ATOM_UTILS_ARGUMENT_PARSER_HPP
