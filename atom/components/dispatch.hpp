#ifndef ATOM_COMMAND_DISPATCH_HPP
#define ATOM_COMMAND_DISPATCH_HPP

#include <any>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#if ENABLE_FASTHASH
#include "emhash/hash_set8.hpp"
#include "emhash/hash_table8.hpp"
#else
#include <unordered_map>
#include <unordered_set>
#endif

#include "atom/error/exception.hpp"
#include "atom/function/proxy.hpp"
#include "atom/function/type_caster.hpp"

#include "macro.hpp"

// -------------------------------------------------------------------
// Command Exception
// -------------------------------------------------------------------

class DispatchException : public atom::error::Exception {
public:
    using atom::error::Exception::Exception;
};

#define THROW_DISPATCH_EXCEPTION(...)                                       \
    throw DispatchException(ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, \
                            __VA_ARGS__);

class DispatchTimeout : public atom::error::Exception {
public:
    using atom::error::Exception::Exception;
};

#define THROW_DISPATCH_TIMEOUT(...)                                       \
    throw DispatchTimeout(ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, \
                          __VA_ARGS__);

// -------------------------------------------------------------------
// Command Dispatcher
// -------------------------------------------------------------------

/**
 * @brief Manages and dispatches commands.
 */
class CommandDispatcher {
public:
    /**
     * @brief Constructs a CommandDispatcher with a TypeCaster.
     * @param typeCaster A weak pointer to a TypeCaster.
     */
    explicit CommandDispatcher(std::weak_ptr<atom::meta::TypeCaster> typeCaster)
        : typeCaster_(std::move(typeCaster)) {}

    /**
     * @brief Defines a command.
     * @tparam Ret The return type of the command function.
     * @tparam Args The argument types of the command function.
     * @param name The name of the command.
     * @param group The group of the command.
     * @param description The description of the command.
     * @param func The command function.
     * @param precondition An optional precondition function.
     * @param postcondition An optional postcondition function.
     * @param arg_info Information about the arguments.
     */
    template <typename Ret, typename... Args>
    void def(const std::string& name, const std::string& group,
             const std::string& description, std::function<Ret(Args...)> func,
             std::optional<std::function<bool()>> precondition = std::nullopt,
             std::optional<std::function<void()>> postcondition = std::nullopt,
             std::vector<atom::meta::Arg> arg_info = {}, bool isTimed = false);

    /**
     * @brief Checks if a command exists.
     * @param name The name of the command.
     * @return True if the command exists, false otherwise.
     */
    [[nodiscard]] auto has(const std::string& name) const -> bool;

    /**
     * @brief Adds an alias for a command.
     * @param name The name of the command.
     * @param alias The alias for the command.
     */
    void addAlias(const std::string& name, const std::string& alias);

    /**
     * @brief Adds a command to a group.
     * @param name The name of the command.
     * @param group The group of the command.
     */
    void addGroup(const std::string& name, const std::string& group);

    /**
     * @brief Sets a timeout for a command.
     * @param name The name of the command.
     * @param timeout The timeout duration.
     */
    void setTimeout(const std::string& name, std::chrono::milliseconds timeout);

    /**
     * @brief Dispatches a command with arguments.
     * @tparam Args The argument types.
     * @param name The name of the command.
     * @param args The arguments for the command.
     * @return The result of the command execution.
     */
    template <typename... Args>
    auto dispatch(const std::string& name, Args&&... args) -> std::any;

    /**
     * @brief Dispatches a command with a vector of arguments.
     * @param name The name of the command.
     * @param args The arguments for the command.
     * @return The result of the command execution.
     */
    auto dispatch(const std::string& name,
                  const std::vector<std::any>& args) -> std::any;

    /**
     * @brief Dispatches a command with function parameters.
     * @param name The name of the command.
     * @param params The function parameters.
     * @return The result of the command execution.
     */
    auto dispatch(const std::string& name,
                  const atom::meta::FunctionParams& params) -> std::any;

    /**
     * @brief Removes a command.
     * @param name The name of the command.
     */
    void removeCommand(const std::string& name);

    /**
     * @brief Gets the commands in a group.
     * @param group The group name.
     * @return A vector of command names in the group.
     */
    [[nodiscard]] auto getCommandsInGroup(const std::string& group) const
        -> std::vector<std::string>;

    /**
     * @brief Gets the description of a command.
     * @param name The name of the command.
     * @return The description of the command.
     */
    [[nodiscard]] auto getCommandDescription(const std::string& name) const
        -> std::string;

#if ENABLE_FASTHASH
    /**
     * @brief Gets the aliases of a command.
     * @param name The name of the command.
     * @return A set of aliases for the command.
     */
    emhash::HashSet<std::string> getCommandAliases(
        const std::string& name) const;
#else
    /**
     * @brief Gets the aliases of a command.
     * @param name The name of the command.
     * @return A set of aliases for the command.
     */
    [[nodiscard]] auto getCommandAliases(const std::string& name) const
        -> std::unordered_set<std::string>;
#endif

    /**
     * @brief Gets all commands.
     * @return A vector of all command names.
     */
    [[nodiscard]] auto getAllCommands() const -> std::vector<std::string>;

    [[nodiscard]] auto getCommandArgAndReturnType(const std::string& name)
        -> std::pair<std::vector<atom::meta::Arg>, std::string>;

private:
    struct Command;

    /**
     * @brief Helper function to dispatch a command.
     * @tparam ArgsType The type of the arguments.
     * @param name The name of the command.
     * @param args The arguments for the command.
     * @return The result of the command execution.
     */
    template <typename ArgsType>
    auto dispatchHelper(const std::string& name,
                        const ArgsType& args) -> std::any;

    /**
     * @brief Converts a tuple to a vector of arguments.
     * @tparam Args The types of the arguments.
     * @param tuple The tuple of arguments.
     * @return A vector of arguments.
     */
    template <typename... Args>
    auto convertToArgsVector(std::tuple<Args...>&& tuple)
        -> std::vector<std::any>;

    /**
     * @brief Finds a command by name.
     * @param name The name of the command.
     * @return An iterator to the command.
     */
    auto findCommand(const std::string& name);

    /**
     * @brief Completes the arguments for a command.
     * @tparam ArgsType The type of the arguments.
     * @param cmd The command.
     * @param args The arguments for the command.
     * @return A vector of completed arguments.
     */
    template <typename ArgsType>
    auto completeArgs(const Command& cmd,
                      const ArgsType& args) -> std::vector<std::any>;

    /**
     * @brief Checks the precondition of a command.
     * @param cmd The command.
     * @param name The name of the command.
     */
    static void checkPrecondition(const Command& cmd, const std::string& name);

    /**
     * @brief Checks the postcondition of a command.
     * @param cmd The command.
     * @param name The name of the command.
     */
    static void checkPostcondition(const Command& cmd, const std::string& name);

    /**
     * @brief Executes a command.
     * @param cmd The command.
     * @param name The name of the command.
     * @param args The arguments for the command.
     * @return The result of the command execution.
     */
    auto executeCommand(const Command& cmd, const std::string& name,
                        const std::vector<std::any>& args) -> std::any;

    /**
     * @brief Executes a command with a timeout.
     * @param cmd The command.
     * @param name The name of the command.
     * @param args The arguments for the command.
     * @param timeout The timeout duration.
     * @return The result of the command execution.
     */
    static auto executeWithTimeout(const Command& cmd, const std::string& name,
                            const std::vector<std::any>& args,
                            const std::chrono::duration<double>& timeout)
        -> std::any;

    /**
     * @brief Executes a command without a timeout.
     * @param cmd The command.
     * @param name The name of the command.
     * @param args The arguments for the command.
     * @return The result of the command execution.
     */
    static auto executeWithoutTimeout(const Command& cmd, const std::string& name,
                               const std::vector<std::any>& args) -> std::any;

    /**
     * @brief Executes the functions of a command.
     * @param cmd The command.
     * @param args The arguments for the command.
     * @return The result of the command execution.
     */
    static auto executeFunctions(const Command& cmd,
                          const std::vector<std::any>& args) -> std::any;

    /**
     * @brief Computes the hash of the function arguments.
     * @param args The arguments for the command.
     * @return The hash of the function arguments.
     */
    static auto computeFunctionHash(const std::vector<std::any>& args) -> std::string;

    struct Command {
        std::function<std::any(const std::vector<std::any>&)> func;
        std::string returnType;
        std::vector<atom::meta::Arg> argTypes;
        std::string hash;
        std::string description;
#if ENABLE_FASTHASH
        emhash::HashSet<std::string> aliases;
#else
        std::unordered_set<std::string> aliases;
#endif
        std::optional<std::function<bool()>> precondition;
        std::optional<std::function<void()>> postcondition;
    } ATOM_ALIGNAS(128);

#if ENABLE_FASTHASH
    emhash8::HashMap<std::string, Command> commands;
    emhash8::HashMap<std::string, std::string> groupMap;
    emhash8::HashMap<std::string, std::chrono::milliseconds> timeoutMap;
#else
    std::unordered_map<std::string, Command> commands_;
    std::unordered_map<std::string, std::string> groupMap_;
    std::unordered_map<std::string, std::chrono::milliseconds> timeoutMap_;
#endif

    std::weak_ptr<atom::meta::TypeCaster> typeCaster_;
};

ATOM_INLINE auto CommandDispatcher::findCommand(const std::string& name) {
    auto it = commands_.find(name);
    if (it == commands_.end()) {
        for (const auto& [cmdName, cmd] : commands_) {
            if (std::ranges::find(cmd.aliases.begin(), cmd.aliases.end(), name) !=
                cmd.aliases.end()) {
#if ENABLE_DEBUG
                std::cout << "Command '" << name
                          << "' not found, did you mean '" << cmdName << "'?\n";
#endif
                return commands_.find(cmdName);
            }
        }
    }
    return it;
}

template <typename Ret, typename... Args>
void CommandDispatcher::def(const std::string& name, const std::string& group,
                            const std::string& description,
                            std::function<Ret(Args...)> func,
                            std::optional<std::function<bool()>> precondition,
                            std::optional<std::function<void()>> postcondition,
                            std::vector<atom::meta::Arg> arg_info,
                            bool isTimed) {
    std::function<std::any(const std::vector<std::any>&)> wrappedFunc;
    atom::meta::FunctionInfo info;
    if (isTimed) {
        auto _func = atom::meta::TimerProxyFunction(std::move(func));
        info = _func.getFunctionInfo();
        wrappedFunc =
            [_func](const std::vector<std::any>& args) mutable -> std::any {
            std::chrono::milliseconds defaultTimeout(1000);
            return _func(args, defaultTimeout);
        };
    } else {
        auto _func = atom::meta::ProxyFunction(std::move(func));
        wrappedFunc =
            [_func](const std::vector<std::any>& args) mutable -> std::any {
            return _func(args);
        };
    }
    Command cmd{{std::move(wrappedFunc)},
                {info.getReturnType()},
                arg_info,
                {info.getHash()},
                description,
                {},
                std::move(precondition),
                std::move(postcondition)};
    commands_[name] = std::move(cmd);
    groupMap_[name] = group;
}

template <typename... Args>
auto CommandDispatcher::dispatch(const std::string& name,
                                 Args&&... args) -> std::any {
    auto argsTuple = std::make_tuple(std::forward<Args>(args)...);
    auto argsVec = convertToArgsVector(std::move(argsTuple));
    return dispatchHelper(name, argsVec);
}

template <typename... Args>
auto CommandDispatcher::convertToArgsVector(std::tuple<Args...>&& tuple)
    -> std::vector<std::any> {
    std::vector<std::any> argsVec;
    argsVec.reserve(sizeof...(Args));
    std::apply(
        [&argsVec](auto&&... args) {
            ((argsVec.emplace_back(std::forward<decltype(args)>(args))), ...);
        },
        std::move(tuple));
    return argsVec;
}

template <typename ArgsType>
auto CommandDispatcher::dispatchHelper(const std::string& name,
                                       const ArgsType& args) -> std::any {
    auto it = findCommand(name);
    if (it == commands_.end()) {
        THROW_INVALID_ARGUMENT("Unknown command: " + name);
    }

    const auto& cmd = it->second;
    std::vector<std::any> fullArgs;
    fullArgs = completeArgs(cmd, args);

    if constexpr (std::is_same_v<ArgsType, std::vector<std::any>>) {
        auto it1 = args.begin();
        auto it2 = cmd.argTypes.begin();
        for (; it1 != args.end() && it2 != cmd.argTypes.end(); ++it1, ++it2) {
        }
    }

    checkPrecondition(cmd, name);

    auto result = executeCommand(cmd, name, fullArgs);

    checkPostcondition(cmd, name);

    return result;
}

template <typename ArgsType>
auto CommandDispatcher::completeArgs(const Command& cmd, const ArgsType& args)
    -> std::vector<std::any> {
    std::vector<std::any> fullArgs(args.begin(), args.end());
    for (size_t i = args.size(); i < cmd.argTypes.size(); ++i) {
        if (cmd.argTypes[i].getDefaultValue()) {
            fullArgs.push_back(cmd.argTypes[i].getDefaultValue().value());
        } else {
            THROW_INVALID_ARGUMENT("Missing argument: " +
                                   cmd.argTypes[i].getName());
        }
    }
    return fullArgs;
}

#endif