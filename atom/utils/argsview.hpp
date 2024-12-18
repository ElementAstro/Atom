#ifndef ATOM_UTILS_ARGUMENT_PARSER_HPP
#define ATOM_UTILS_ARGUMENT_PARSER_HPP

#include <any>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "atom/error/exception.hpp"
#include "atom/macro.hpp"

namespace atom::utils {

/**
 * @class ArgumentParser
 * @brief A class for parsing command-line arguments.
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
        Nargs() : type(NargsType::NONE), count(1) {}

        /**
         * @brief Constructor with specified type and count.
         * @param t Type of nargs.
         * @param c Count of arguments, default is 1.
         */
        Nargs(NargsType t, int c = 1) : type(t), count(c) {}
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
    void setDescription(const std::string& description);

    /**
     * @brief Set the epilog of the program.
     * @param epilog Epilog text.
     */
    void setEpilog(const std::string& epilog);

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
     */
    void addArgument(const std::string& name, ArgType type = ArgType::AUTO,
                     bool required = false, const std::any& default_value = {},
                     const std::string& help = "",
                     const std::vector<std::string>& aliases = {},
                     bool is_positional = false, const Nargs& nargs = Nargs());

    /**
     * @brief Add a flag to the parser.
     * @param name Name of the flag.
     * @param help Help text for the flag.
     * @param aliases Aliases for the flag.
     */
    void addFlag(const std::string& name, const std::string& help = "",
                 const std::vector<std::string>& aliases = {});

    /**
     * @brief Add a subcommand to the parser.
     * @param name Name of the subcommand.
     * @param help Help text for the subcommand.
     */
    void addSubcommand(const std::string& name, const std::string& help = "");

    /**
     * @brief Add a mutually exclusive group of arguments.
     * @param group_args Vector of argument names that are mutually exclusive.
     */
    void addMutuallyExclusiveGroup(const std::vector<std::string>& group_args);

    /**
     * @brief Enable parsing arguments from a file.
     * @param prefix Prefix to identify file arguments.
     */
    void addArgumentFromFile(const std::string& prefix = "@");

    /**
     * @brief Set the delimiter for file parsing.
     * @param delimiter Delimiter character.
     */
    void setFileDelimiter(char delimiter);

    /**
     * @brief Parse the command-line arguments.
     * @param argc Argument count.
     * @param argv Argument vector.
     */
    void parse(int argc, std::vector<std::string> argv);

    /**
     * @brief Get the value of an argument.
     * @tparam T Type of the argument value.
     * @param name Name of the argument.
     * @return Optional value of the argument.
     */
    template <typename T>
    auto get(const std::string& name) const -> std::optional<T>;

    /**
     * @brief Get the value of a flag.
     * @param name Name of the flag.
     * @return Boolean value of the flag.
     */
    auto getFlag(const std::string& name) const -> bool;

    /**
     * @brief Get the parser for a subcommand.
     * @param name Name of the subcommand.
     * @return Optional reference to the subcommand parser.
     */
    auto getSubcommandParser(const std::string& name) const
        -> std::optional<std::reference_wrapper<const ArgumentParser>>;

    /**
     * @brief Print the help message.
     */
    void printHelp() const;

    /**
     * @brief Add a description to the parser.
     * @param description Description text.
     */
    void addDescription(const std::string& description);

    /**
     * @brief Add an epilog to the parser.
     * @param epilog Epilog text.
     */
    void addEpilog(const std::string& epilog);

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
    std::unordered_map<std::string, Subcommand>
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
     */
    static auto parseValue(ArgType type, const std::string& value) -> std::any;

    /**
     * @brief Convert an argument type to a string.
     * @param type The type of the argument.
     * @return The argument type as a string.
     */
    static auto argTypeToString(ArgType type) -> std::string;

    /**
     * @brief Convert a value of type std::any to a string.
     * @param value The value to convert.
     * @return The value as a string.
     */
    static auto anyToString(const std::any& value) -> std::string;

    /**
     * @brief Expand arguments from a file.
     * @param argv The argument vector to expand.
     */
    void expandArgumentsFromFile(std::vector<std::string>& argv);
};

struct ArgumentParser::Subcommand {
    std::string help;
    ArgumentParser parser;
} ATOM_ALIGNAS(128);

inline ArgumentParser::ArgumentParser(std::string program_name)
    : programName_(std::move(program_name)) {}

inline void ArgumentParser::setDescription(const std::string& description) {
    description_ = description;
}

inline void ArgumentParser::setEpilog(const std::string& epilog) {
    epilog_ = epilog;
}

inline void ArgumentParser::addArgument(const std::string& name, ArgType type,
                                        bool required,
                                        const std::any& default_value,
                                        const std::string& help,
                                        const std::vector<std::string>& aliases,
                                        bool is_positional,
                                        const Nargs& nargs) {
    if (type == ArgType::AUTO && default_value.has_value()) {
        type = detectType(default_value);
    } else if (type == ArgType::AUTO) {
        type = ArgType::STRING;
    }

    arguments_[name] =
        Argument{type,          required, default_value,
                 help,          aliases,  nargs.type != NargsType::NONE,
                 is_positional, nargs};

    for (const auto& alias : aliases) {
        aliases_[alias] = name;
    }
}

inline void ArgumentParser::addFlag(const std::string& name,
                                    const std::string& help,
                                    const std::vector<std::string>& aliases) {
    flags_[name] = Flag{false, help, aliases};
    for (const auto& alias : aliases) {
        aliases_[alias] = name;
    }
}

inline void ArgumentParser::addSubcommand(const std::string& name,
                                          const std::string& help) {
    subcommands_[name] = Subcommand{help, ArgumentParser(name)};
}

inline void ArgumentParser::addMutuallyExclusiveGroup(
    const std::vector<std::string>& group_args) {
    mutuallyExclusiveGroups_.emplace_back(group_args);
}

inline void ArgumentParser::addArgumentFromFile(const std::string& prefix) {
    enableFileParsing_ = true;
    filePrefix_ = prefix;
}

inline void ArgumentParser::setFileDelimiter(char delimiter) {
    fileDelimiter_ = delimiter;
}

inline void ArgumentParser::parse(int argc, std::vector<std::string> argv) {
    if (argc < 1)
        return;

    // 扩展来自文件的参数
    if (enableFileParsing_) {
        expandArgumentsFromFile(argv);
    }

    std::string currentSubcommand;
    std::vector<std::string> subcommandArgs;

    // Track which mutually exclusive groups have been used
    std::vector<bool> groupUsed(mutuallyExclusiveGroups_.size(), false);

    for (size_t i = 0; i < argv.size(); ++i) {
        std::string arg = argv[i];

        // Check for subcommand
        if (subcommands_.find(arg) != subcommands_.end()) {
            currentSubcommand = arg;
            subcommandArgs.push_back(argv[0]);  // Program name
            continue;
        }

        // If inside a subcommand, pass arguments to subcommand parser
        if (!currentSubcommand.empty()) {
            subcommandArgs.push_back(argv[i]);
            continue;
        }

        // Handle help flag
        if (arg == "--help" || arg == "-h") {
            printHelp();
            std::exit(0);
        }

        // Handle optional arguments and flags
        if (arg.starts_with("--") || arg.starts_with("-")) {
            std::string argName;
            // bool isFlag = false;

            if (arg.starts_with("--")) {
                argName = arg.substr(2);
            } else {
                argName = arg.substr(1);
            }

            // Resolve aliases
            if (aliases_.find(argName) != aliases_.end()) {
                argName = aliases_[argName];
            }

            // Check if it's a flag
            if (flags_.find(argName) != flags_.end()) {
                flags_[argName].value = true;
                continue;
            }

            // Check if it's an argument
            if (arguments_.find(argName) != arguments_.end()) {
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
                    if (i + 1 < argv.size() && !argv[i + 1].starts_with("-")) {
                        values.emplace_back(argv[++i]);
                    } else {
                        break;
                    }
                }

                if (isConstant &&
                    static_cast<int>(values.size()) != argument.nargs.count) {
                    THROW_INVALID_ARGUMENT(
                        "Argument " + argName + " expects " +
                        std::to_string(argument.nargs.count) + " value(s).");
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
                        argument.value = parseValue(argument.type, values[0]);
                    }
                }

                continue;
            }

            THROW_INVALID_ARGUMENT("Unknown argument: " + arg);
        }

        // Handle positional arguments
        positionalArguments_.push_back(arg);
    }

    if (!currentSubcommand.empty() && !subcommandArgs.empty()) {
        subcommands_[currentSubcommand].parser.parse(
            static_cast<int>(subcommandArgs.size()), subcommandArgs);
    }

    // Validate mutually exclusive groups
    for (size_t g = 0; g < mutuallyExclusiveGroups_.size(); ++g) {
        int count = 0;
        for (const auto& arg : mutuallyExclusiveGroups_[g]) {
            if (flags_.find(arg) != flags_.end() && flags_[arg].value) {
                count++;
            }
            if (arguments_.find(arg) != arguments_.end() &&
                arguments_[arg].value.has_value()) {
                count++;
            }
        }
        if (count > 1) {
            THROW_INVALID_ARGUMENT("Arguments in mutually exclusive group " +
                                   std::to_string(g + 1) +
                                   " cannot be used together.");
        }
    }

    // Check required arguments
    for (const auto& [name, argument] : arguments_) {
        if (argument.required && !argument.value.has_value() &&
            !argument.defaultValue.has_value()) {
            THROW_INVALID_ARGUMENT("Argument required: " + name);
        }
    }
}

template <typename T>
auto ArgumentParser::get(const std::string& name) const -> std::optional<T> {
    if (arguments_.find(name) != arguments_.end()) {
        const auto& arg = arguments_.at(name);
        if (arg.value.has_value()) {
            try {
                return std::any_cast<T>(arg.value.value());
            } catch (const std::bad_any_cast&) {
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
    }
    return std::nullopt;
}

inline auto ArgumentParser::getFlag(const std::string& name) const -> bool {
    if (flags_.find(name) != flags_.end()) {
        return flags_.at(name).value;
    }
    return false;
}

inline auto ArgumentParser::getSubcommandParser(const std::string& name) const
    -> std::optional<std::reference_wrapper<const ArgumentParser>> {
    if (subcommands_.find(name) != subcommands_.end()) {
        return subcommands_.at(name).parser;
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

    std::cout << "Options:\n";
    for (const auto& [name, argument] : arguments_) {
        if (argument.is_positional)
            continue;
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
            std::cout << "  " << name << " : " << subcommand.help << "\n";
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
                                       const std::string& value) -> std::any {
    try {
        switch (type) {
            case ArgType::STRING:
                return value;
            case ArgType::INTEGER:
                return std::stoi(value);
            case ArgType::UNSIGNED_INTEGER:
                return static_cast<unsigned int>(std::stoul(value));
            case ArgType::LONG:
                return std::stol(value);
            case ArgType::UNSIGNED_LONG:
                return std::stoul(value);
            case ArgType::FLOAT:
                return std::stof(value);
            case ArgType::DOUBLE:
                return std::stod(value);
            case ArgType::BOOLEAN:
                return (value == "true" || value == "1");
            case ArgType::FILEPATH:
                return std::filesystem::path(value);
            default:
                return value;
        }
    } catch (...) {
        THROW_INVALID_ARGUMENT("Unable to parse argument value: " + value);
    }
}

inline auto ArgumentParser::argTypeToString(ArgType type) -> std::string {
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
        return std::to_string(std::any_cast<float>(value));
    }
    if (value.type() == typeid(double)) {
        return std::to_string(std::any_cast<double>(value));
    }
    if (value.type() == typeid(bool)) {
        return std::any_cast<bool>(value) ? "true" : "false";
    }
    if (value.type() == typeid(std::filesystem::path)) {
        return std::any_cast<std::filesystem::path>(value).string();
    }
    return "unknown type";
}

// 自定义文件解析实现
inline void ArgumentParser::expandArgumentsFromFile(
    std::vector<std::string>& argv) {
    std::vector<std::string> expandedArgs;
    for (const auto& arg : argv) {
        if (arg.starts_with(filePrefix_)) {
            std::string filename = arg.substr(filePrefix_.length());
            std::ifstream infile(filename);
            if (!infile.is_open()) {
                THROW_INVALID_ARGUMENT("Unable to open argument file: " +
                                       filename);
            }
            std::string line;
            while (std::getline(infile, line)) {
                std::istringstream iss(line);
                std::string token;
                while (std::getline(iss, token, fileDelimiter_)) {
                    if (!token.empty()) {
                        expandedArgs.emplace_back(token);
                    }
                }
            }
        } else {
            expandedArgs.emplace_back(arg);
        }
    }
    argv = expandedArgs;
}

inline void ArgumentParser::addDescription(const std::string& description) {
    description_ = description;
}

inline void ArgumentParser::addEpilog(const std::string& epilog) {
    epilog_ = epilog;
}

}  // namespace atom::utils

#endif  // ATOM_UTILS_ARGUMENT_PARSER_HPP
