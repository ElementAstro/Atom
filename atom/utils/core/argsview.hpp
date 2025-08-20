#ifndef ATOM_UTILS_ARGUMENT_PARSER_HPP
#define ATOM_UTILS_ARGUMENT_PARSER_HPP

#include <algorithm>
#include <any>
#include <cerrno>
#include <climits>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <sstream>
#include <thread>
#include <type_traits>
#include <utility>

#include "atom/containers/high_performance.hpp"
#include "atom/error/exception.hpp"
#include "atom/macro.hpp"

namespace atom::utils {

using atom::containers::String;
template <typename K, typename V>
using HashMap = atom::containers::HashMap<K, V>;
template <typename T>
using Vector = atom::containers::Vector<T>;

/**
 * @class ArgumentParser
 * @brief A class for parsing command-line arguments with enhanced C++20
 * features.
 */
class ArgumentParser {
public:
    /**
     * @enum ArgType
     * @brief Enumeration of possible argument types.
     */
    enum class ArgType {
        STRING,
        INTEGER,
        UNSIGNED_INTEGER,
        LONG,
        UNSIGNED_LONG,
        FLOAT,
        DOUBLE,
        BOOLEAN,
        FILEPATH,
        AUTO
    };

    /**
     * @enum NargsType
     * @brief Enumeration of possible nargs types.
     */
    enum class NargsType {
        NONE,
        OPTIONAL,
        ZERO_OR_MORE,
        ONE_OR_MORE,
        CONSTANT
    };

    /**
     * @struct Nargs
     * @brief Structure to define the number of arguments.
     */
    struct Nargs {
        NargsType type;
        int count;

        constexpr Nargs() noexcept : type(NargsType::NONE), count(1) {}

        constexpr Nargs(NargsType t, int c = 1) : type(t), count(c) {
            if (c < 0) {
                THROW_INVALID_ARGUMENT("Nargs count cannot be negative");
            }
        }
    };

    ArgumentParser() = default;
    explicit ArgumentParser(String program_name);

    void setDescription(const String& description);
    void setEpilog(const String& epilog);

    void addArgument(const String& name, ArgType type = ArgType::AUTO,
                     bool required = false, const std::any& default_value = {},
                     const String& help = "",
                     const Vector<String>& aliases = {},
                     bool is_positional = false, const Nargs& nargs = Nargs());

    void addFlag(const String& name, const String& help = "",
                 const Vector<String>& aliases = {});

    void addSubcommand(const String& name, const String& help = "");

    void addMutuallyExclusiveGroup(const Vector<String>& group_args);

    void addArgumentFromFile(const String& prefix = "@");

    void setFileDelimiter(char delimiter);

    void parse(int argc, std::span<const String> argv);

    template <typename T>
    auto get(const String& name) const -> std::optional<T>;

    auto getFlag(const String& name) const -> bool;

    auto getSubcommandParser(const String& name)
        -> std::optional<std::reference_wrapper<ArgumentParser>>;

    void printHelp() const;

    void addDescription(const String& description);

    void addEpilog(const String& epilog);

private:
    struct Argument {
        ArgType type;
        bool required;
        std::any defaultValue;
        std::optional<std::any> value;
        String help;
        Vector<String> aliases;
        bool isMultivalue;
        bool is_positional;
        Nargs nargs;

        Argument() = default;

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

    struct Flag {
        bool value;
        String help;
        Vector<String> aliases;
    } ATOM_ALIGNAS(64);

    struct Subcommand;

    HashMap<String, Argument> arguments_;
    HashMap<String, Flag> flags_;
    HashMap<String, std::shared_ptr<Subcommand>> subcommands_;
    HashMap<String, String> aliases_;
    Vector<String> positionalArguments_;
    String description_;
    String epilog_;
    String programName_;
    Vector<Vector<String>> mutuallyExclusiveGroups_;

    bool enableFileParsing_ = false;
    String filePrefix_ = "@";
    char fileDelimiter_ = ' ';

    static auto detectType(const std::any& value) -> ArgType;
    static auto parseValue(ArgType type, const String& value) -> std::any;
    static constexpr auto argTypeToString(ArgType type) -> const char*;
    static auto anyToString(const std::any& value) -> String;

    void expandArgumentsFromFile(Vector<String>& argv);
    static void validateName(const String& name);
    void processPositionalArguments(const Vector<String>& pos_args);
    auto processArgumentFile(const String& filename) const -> Vector<String>;
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
    if (name.find(' ') != String::npos) {
        THROW_INVALID_ARGUMENT("Argument name cannot contain spaces");
    }
    if (name.starts_with('-')) {
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

        arguments_[name] =
            Argument{type,          required, default_value,
                     help,          aliases,  nargs.type != NargsType::NONE,
                     is_positional, nargs};

        for (const auto& alias : aliases) {
            if (aliases_.contains(alias)) {
                THROW_INVALID_ARGUMENT("Alias '" + alias + "' is already used");
            }
            aliases_[alias] = name;
        }
    } catch (const std::exception& e) {
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
        Vector<String> args_vector(argv.begin(), argv.end());

        if (enableFileParsing_) {
            expandArgumentsFromFile(args_vector);
        }

        String currentSubcommand;
        Vector<String> subcommandArgs;
        Vector<String> positional_args;

        std::vector<bool> groupUsed(mutuallyExclusiveGroups_.size(), false);

        size_t i = 1;
        while (i < args_vector.size()) {
            const String& arg = args_vector[i];

            if (subcommands_.contains(arg)) {
                currentSubcommand = arg;
                subcommandArgs.push_back(args_vector[0]);
                ++i;
                break;
            }

            if (arg == "--help" || arg == "-h") {
                printHelp();
                std::exit(0);
            }

            if (arg.starts_with("--") ||
                (arg.starts_with("-") && arg.size() > 1)) {
                String argName;

                if (arg.starts_with("--")) {
                    argName = arg.substr(2);
                } else {
                    argName = arg.substr(1);
                }

                if (aliases_.contains(argName)) {
                    argName = aliases_.at(argName);
                }

                if (flags_.contains(argName)) {
                    flags_[argName].value = true;
                    ++i;
                    continue;
                }

                if (arguments_.contains(argName)) {
                    Argument& argument = arguments_[argName];
                    Vector<String> values;

                    int expected = 1;
                    bool isConstant = false;
                    if (argument.nargs.type == NargsType::ONE_OR_MORE) {
                        expected = -1;
                    } else if (argument.nargs.type == NargsType::ZERO_OR_MORE) {
                        expected = -1;
                    } else if (argument.nargs.type == NargsType::OPTIONAL) {
                        expected = 1;
                    } else if (argument.nargs.type == NargsType::CONSTANT) {
                        expected = argument.nargs.count;
                        isConstant = true;
                    }

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
                            String(std::to_string(argument.nargs.count)) +
                            " value(s).");
                    }

                    if (values.empty() &&
                        argument.nargs.type == NargsType::OPTIONAL) {
                        if (argument.defaultValue.has_value()) {
                            argument.value = argument.defaultValue;
                        }
                    } else if (!values.empty()) {
                        if (expected == -1) {
                            argument.value = std::any(values);
                        } else {
                            argument.value =
                                parseValue(argument.type, values[0]);
                        }
                    }

                    ++i;
                    continue;
                }

                THROW_INVALID_ARGUMENT("Unknown argument: " + arg);
            } else {
                positional_args.push_back(arg);
                ++i;
            }
        }

        while (i < args_vector.size()) {
            subcommandArgs.push_back(args_vector[i++]);
        }

        if (!positional_args.empty()) {
            processPositionalArguments(positional_args);
        }

        if (!currentSubcommand.empty() && !subcommandArgs.empty()) {
            if (auto subcommand = subcommands_[currentSubcommand]) {
                subcommand->parser.parse(
                    static_cast<int>(subcommandArgs.size()),
                    std::span<const String>(subcommandArgs));
            }
        }

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
                    String(std::to_string(g + 1)) +
                    " cannot be used together.");
            }
        }

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

    size_t pos_index = 0;
    for (const auto& name : positional_names) {
        if (pos_index >= pos_args.size()) {
            const auto& arg = arguments_.at(name);
            if (arg.required && !arg.defaultValue.has_value()) {
                THROW_INVALID_ARGUMENT(
                    "Missing required positional argument: " + name);
            }
            continue;
        }

        auto& arg = arguments_[name];

        if (arg.nargs.type == NargsType::ONE_OR_MORE ||
            arg.nargs.type == NargsType::ZERO_OR_MORE) {
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
            Vector<String> values;
            for (int i = 0; i < arg.nargs.count && pos_index < pos_args.size();
                 ++i) {
                values.push_back(pos_args[pos_index++]);
            }

            if (static_cast<int>(values.size()) != arg.nargs.count) {
                THROW_INVALID_ARGUMENT(
                    "Positional argument " + name + " requires exactly " +
                    String(std::to_string(arg.nargs.count)) + " values");
            }

            arg.value = std::any(values);
        } else {
            String value = pos_args[pos_index++];
            arg.value = parseValue(arg.type, value);
        }
    }

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
            if constexpr (std::is_same_v<T, Vector<String>>) {
                if (arg.value.value().type() == typeid(Vector<String>)) {
                    return std::any_cast<Vector<String>>(arg.value.value());
                }
                if (arg.value.value().type() == typeid(String)) {
                    Vector<String> vec;
                    vec.push_back(std::any_cast<String>(arg.value.value()));
                    return vec;
                }
            }
            return std::any_cast<T>(arg.value.value());
        } catch (const std::bad_any_cast&) {
            if constexpr (std::is_same_v<T, String>) {
                if (arg.value.value().type() == typeid(int)) {
                    return String(
                        std::to_string(std::any_cast<int>(arg.value.value())));
                }
            }
            return std::nullopt;
        }
    }
    if (arg.defaultValue.has_value()) {
        try {
            if constexpr (std::is_same_v<T, Vector<String>>) {
                if (arg.defaultValue.type() == typeid(Vector<String>)) {
                    return std::any_cast<Vector<String>>(arg.defaultValue);
                }
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
    std::cout << "Usage:\n  " << programName_ << " [options] ";
    if (!subcommands_.empty()) {
        std::cout << "<subcommand> [subcommand options]";
    }
    std::cout << "\n\n";

    if (!description_.empty()) {
        std::cout << description_ << "\n\n";
    }

    if (!arguments_.empty() || !flags_.empty()) {
        std::cout << "Options:\n";

        Vector<std::pair<String, const Argument*>> sorted_args;
        for (const auto& [name, arg] : arguments_) {
            if (!arg.is_positional) {
                sorted_args.emplace_back(name, &arg);
            }
        }
        std::ranges::sort(sorted_args, [](const auto& a, const auto& b) {
            return a.first < b.first;
        });

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
        return ArgType::STRING;
    if (typeInfo == typeid(std::filesystem::path))
        return ArgType::FILEPATH;
    if (typeInfo == typeid(Vector<String>))
        return ArgType::STRING;

    return ArgType::STRING;
}

inline auto ArgumentParser::parseValue(ArgType type, const String& value)
    -> std::any {
    try {
        const char* str = value.c_str();
        size_t len = value.length();

        switch (type) {
            case ArgType::STRING:
                return value;

            case ArgType::INTEGER: {
                char* end;
                errno = 0;
                long val = std::strtol(str, &end, 10);

                if (errno == ERANGE || val < INT_MIN || val > INT_MAX) {
                    throw std::out_of_range("Integer value out of range");
                }
                if (end != str + len) {
                    THROW_INVALID_ARGUMENT("Invalid integer format");
                }
                return static_cast<int>(val);
            }

            case ArgType::UNSIGNED_INTEGER: {
                char* end;
                errno = 0;
                if (value.find('-') != String::npos) {
                    THROW_INVALID_ARGUMENT(
                        "Invalid unsigned integer format (contains '-')");
                }
                unsigned long val = std::strtoul(str, &end, 10);

                if (errno == ERANGE || val > UINT_MAX) {
                    throw std::out_of_range(
                        "Unsigned integer value out of range");
                }
                if (end != str + len) {
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
                if (end != str + len) {
                    THROW_INVALID_ARGUMENT("Invalid long format");
                }
                return val;
            }

            case ArgType::UNSIGNED_LONG: {
                char* end;
                errno = 0;
                if (value.find('-') != String::npos) {
                    THROW_INVALID_ARGUMENT(
                        "Invalid unsigned long format (contains '-')");
                }
                unsigned long val = std::strtoul(str, &end, 10);

                if (errno == ERANGE) {
                    throw std::out_of_range("Unsigned long value out of range");
                }
                if (end != str + len) {
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
                if (end != str + len) {
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
                if (end != str + len) {
                    THROW_INVALID_ARGUMENT("Invalid double format");
                }
                return val;
            }

            case ArgType::BOOLEAN:
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
                std::filesystem::path path(value.c_str());
                if (value.find('\0') != String::npos) {
                    THROW_INVALID_ARGUMENT("Path contains null characters");
                }
                return path;
            }

            default:
                return value;
        }
    } catch (const std::exception& e) {
        THROW_INVALID_ARGUMENT("Failed to parse value '" + value +
                               "': " + e.what());
    }
}

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
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(6)
               << std::any_cast<float>(value);
            return String(ss.str().c_str());
        }
        if (typeInfo == typeid(double)) {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(10)
               << std::any_cast<double>(value);
            return String(ss.str().c_str());
        }
        if (typeInfo == typeid(bool))
            return std::any_cast<bool>(value) ? "true" : "false";
        if (typeInfo == typeid(std::filesystem::path)) {
            return String(
                std::any_cast<std::filesystem::path>(value).string().c_str());
        }
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

inline void ArgumentParser::expandArgumentsFromFile(Vector<String>& argv) {
    try {
        Vector<String> expandedArgs;
        Vector<String> filenames;

        for (const auto& arg : argv) {
            if (arg.starts_with(filePrefix_)) {
                String filename = arg.substr(filePrefix_.length());
                filenames.push_back(filename);
            } else {
                expandedArgs.emplace_back(arg);
            }
        }

        if (!filenames.empty()) {
            Vector<String> file_args;

            if (filenames.size() > 1 &&
                std::thread::hardware_concurrency() > 1) {
                file_args = parallelProcessFiles(filenames);
            } else {
                for (const auto& filename : filenames) {
                    auto args = processArgumentFile(filename);
                    file_args.insert(file_args.end(), args.begin(), args.end());
                }
            }

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

    std::ifstream infile(filename.c_str());
    if (!infile) {
        throw std::runtime_error("Unable to open argument file: " + filename);
    }

    std::string line;
    while (std::getline(infile, line)) {
        if (line.empty() || line.starts_with("#")) {
            continue;
        }

        std::istringstream iss(line);
        std::string token_std;

        while (std::getline(iss, token_std, fileDelimiter_)) {
            if (!token_std.empty()) {
                token_std.erase(0, token_std.find_first_not_of(" \t"));
                token_std.erase(token_std.find_last_not_of(" \t") + 1);

                if (!token_std.empty()) {
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

    Vector<Vector<String>> results(filenames.size());
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    auto worker = [this, &filenames, &results](size_t start, size_t end) {
        for (size_t i = start; i < end; ++i) {
            try {
                results[i] = processArgumentFile(filenames[i]);
            } catch (const std::exception& e) {
                std::cerr << "Error processing file " << filenames[i] << ": "
                          << e.what() << std::endl;
            }
        }
    };

    size_t files_per_thread = filenames.size() / num_threads;
    size_t remainder = filenames.size() % num_threads;

    size_t start = 0;
    for (unsigned int i = 0; i < num_threads; ++i) {
        size_t end = start + files_per_thread + (i < remainder ? 1 : 0);
        threads.emplace_back(worker, start, end);
        start = end;
    }

    for (auto& t : threads) {
        t.join();
    }

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
