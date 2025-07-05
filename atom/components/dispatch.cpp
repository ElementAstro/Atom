#include "dispatch.hpp"

#include <future>
#include <thread>

#include "atom/utils/to_string.hpp"
#include "spdlog/spdlog.h"

void CommandDispatcher::checkPrecondition(const Command& cmd,
                                          const std::string& name) {
    // spdlog::trace("Entering function: {}", __func__); // Replaced
    // LOG_SCOPE_FUNCTION
    if (!cmd.precondition.has_value()) {
        spdlog::info("No precondition for command: {}", name);
        return;
    }
    try {
        if (!std::invoke(cmd.precondition.value())) {
            spdlog::error("Precondition failed for command '{}'", name);
            THROW_DISPATCH_EXCEPTION("Precondition failed for command '{}'",
                                     name);
        }
        spdlog::info("Precondition for command '{}' passed.", name);
    } catch (const std::bad_function_call& e) {
        spdlog::error("Bad precondition function invoke for command '{}': {}",
                      name, e.what());
        THROW_DISPATCH_EXCEPTION(
            "Bad precondition function invoke for command '{}': {}", name,
            e.what());
    } catch (const std::bad_optional_access& e) {
        spdlog::error("Bad precondition function access for command '{}': {}",
                      name, e.what());
        THROW_DISPATCH_EXCEPTION(
            "Bad precondition function access for command '{}': {}", name,
            e.what());
    } catch (const std::exception& e) {
        spdlog::error("Precondition for command '{}' failed: {}", name,
                      e.what());
        THROW_DISPATCH_EXCEPTION("Precondition failed for command '{}': {}",
                                 name, e.what());
    }
}

void CommandDispatcher::checkPostcondition(const Command& cmd,
                                           const std::string& name) {
    // spdlog::trace("Entering function: {}", __func__); // Replaced
    // LOG_SCOPE_FUNCTION
    if (!cmd.postcondition.has_value()) {
        spdlog::info("No postcondition for command: {}", name);
        return;
    }
    try {
        std::invoke(cmd.postcondition.value());
        spdlog::info("Postcondition for command '{}' passed.", name);
    } catch (const std::bad_function_call& e) {
        spdlog::error("Bad postcondition function invoke for command '{}': {}",
                      name, e.what());
        THROW_DISPATCH_EXCEPTION(
            "Bad postcondition function invoke for command '{}': {}", name,
            e.what());
    } catch (const std::bad_optional_access& e) {
        spdlog::error("Bad postcondition function access for command '{}': {}",
                      name, e.what());
        THROW_DISPATCH_EXCEPTION(
            "Bad postcondition function access for command '{}': {}", name,
            e.what());
    } catch (const std::exception& e) {
        spdlog::error("Postcondition for command '{}' failed: {}", name,
                      e.what());
        THROW_DISPATCH_EXCEPTION("Postcondition failed for command '{}': {}",
                                 name, e.what());
    }
}

auto CommandDispatcher::executeCommand(const Command& cmd,
                                       const std::string& name,
                                       const std::vector<std::any>& args)
    -> std::any {
    // spdlog::trace("Entering function: {}", __func__); // Replaced
    // LOG_SCOPE_FUNCTION

    // Thread-safe shared lock for read operation
    std::shared_lock lock(mutex_);

    // Check if the command has a timeout
    auto timeoutIt = timeoutMap_.find(name);
    bool hasTimeout = timeoutIt != timeoutMap_.end();

    // Make a local copy of the timeout value if it exists
    std::chrono::milliseconds timeout;
    if (hasTimeout) {
        timeout = timeoutIt->second;
    }

    // Release the lock
    lock.unlock();

    // Execute with or without timeout
    if (hasTimeout) {
        spdlog::info("Executing command '{}' with timeout {}ms.", name,
                     timeout.count());
        return executeWithTimeout(cmd, name, args, timeout);
    } else {
        spdlog::info("Executing command '{}' without timeout.", name);
        return executeWithoutTimeout(cmd, name, args);
    }
}

auto CommandDispatcher::executeWithTimeout(
    const Command& cmd, const std::string& name,
    const std::vector<std::any>& args, const std::chrono::milliseconds& timeout)
    -> std::any {
    // spdlog::trace("Entering function: {}", __func__); // Replaced
    // LOG_SCOPE_FUNCTION

    // Create local copies of the args to ensure thread safety
    const std::vector<std::any> localArgs(args);

    // Create a promise and associated future
    std::promise<std::any> promise;
    auto future = promise.get_future();

    // Create atomic flag to track if the thread is finished
    std::atomic<bool> isFinished{false};

    // Run the function in a separate thread with safe captures
    std::thread worker([cmd, localArgs, &promise, &isFinished]() {
        try {
            std::any result = executeFunctions(cmd, localArgs);
            promise.set_value(std::move(result));
        } catch (...) {
            // Capture any exception and store it in the promise
            promise.set_exception(std::current_exception());
        }
        isFinished.store(true, std::memory_order_release);
    });

    // Detach the thread to avoid blocking
    worker.detach();

    // Wait for the result with timeout
    const auto waitResult = future.wait_for(timeout);

    if (waitResult == std::future_status::timeout) {
        spdlog::error("Command '{}' timed out after {}ms.", name,
                      timeout.count());

        // Note: we can't safely terminate the thread in C++,
        // so it will continue running in the background.
        // In a real system, you might need additional mechanisms to
        // handle cleanup of long-running operations.

        THROW_DISPATCH_TIMEOUT("Command '{}' timed out after {}ms.", name,
                               timeout.count());
    }

    // Get the result or propagate exception
    try {
        return future.get();
    } catch (const std::exception& e) {
        spdlog::error("Command '{}' failed: {}", name, e.what());
        throw;  // Re-throw the captured exception
    }
}

auto CommandDispatcher::executeWithoutTimeout(const Command& cmd,
                                              const std::string& name,
                                              const std::vector<std::any>& args)
    -> std::any {
    // spdlog::trace("Entering function: {}", __func__); // Replaced
    // LOG_SCOPE_FUNCTION Check for nested arguments
    if (!args.empty() && args.size() == 1 &&
        args[0].type() == typeid(std::vector<std::any>)) {
        spdlog::info("Executing command '{}' with nested arguments.", name);
        return executeFunctions(cmd,
                                std::any_cast<std::vector<std::any>>(args[0]));
    }

    spdlog::info("Executing command '{}' with arguments.", name);
    return executeFunctions(cmd, args);
}

auto CommandDispatcher::executeFunctions(const Command& cmd,
                                         const std::vector<std::any>& args)
    -> std::any {
    // spdlog::trace("Entering function: {}", __func__); // Replaced
    // LOG_SCOPE_FUNCTION
    std::string funcHash = computeFunctionHash(args);

    // Check if hash matches
    if (cmd.hash == funcHash) {
        try {
            spdlog::info("Executing function for command with hash: {}",
                         funcHash);
            return std::invoke(cmd.func, args);
        } catch (const std::bad_any_cast& e) {
            spdlog::error(
                "Failed to call function for command with hash {}: {}",
                funcHash, e.what());
            THROW_DISPATCH_EXCEPTION(
                "Failed to call function for command with hash {}: {}",
                funcHash, e.what());
        } catch (const std::exception& e) {
            spdlog::error(
                "Error executing function for command with hash {}: {}",
                funcHash, e.what());
            THROW_DISPATCH_EXCEPTION(
                "Error executing function for command with hash {}: {}",
                funcHash, e.what());
        }
    }

    spdlog::error("No matching overload found for command with hash: {}",
                  funcHash);
    THROW_INVALID_ARGUMENT(
        "No matching overload found for command with hash: {}", funcHash);
}

auto CommandDispatcher::computeFunctionHash(const std::vector<std::any>& args)
    -> std::string {
    // spdlog::trace("Entering function: {}", __func__); // Replaced
    // LOG_SCOPE_FUNCTION Use a more efficient approach for small arg counts
    if (args.empty()) {
        return "void";
    }

    std::vector<std::string> argTypes;
    argTypes.reserve(args.size());

    for (const auto& arg : args) {
        argTypes.emplace_back(
            atom::meta::DemangleHelper::demangle(arg.type().name()));
    }

    auto hash = atom::utils::toString(atom::algorithm::computeHash(argTypes));
    spdlog::info("Computed function hash: {}", hash);
    return hash;
}

bool CommandDispatcher::has(std::string_view name) const noexcept {
    // spdlog::trace("Entering function: {}", __func__); // Replaced
    // LOG_SCOPE_FUNCTION

    try {
        // Thread-safe shared lock for read operations
        std::shared_lock lock(mutex_);

        const std::string nameStr(name);

        // Direct lookup first for performance
        if (commands_.find(nameStr) != commands_.end()) {
            spdlog::info("Command '{}' found.", nameStr);
            return true;
        }

        // Then check aliases
        for (const auto& [cmdName, cmdMap] : commands_) {
            for (const auto& [hash, cmd] : cmdMap) {
                if (cmd.aliases.find(nameStr) != cmd.aliases.end()) {
                    spdlog::info("Alias '{}' found for command '{}'.", nameStr,
                                 cmdName);
                    return true;
                }
            }
        }

        spdlog::info("Command '{}' not found.", nameStr);
    } catch (const std::exception& e) {
        // Ensure noexcept guarantee
        spdlog::error("Exception in has(): {}", e.what());
    }

    return false;
}

bool CommandDispatcher::addAlias(std::string_view name,
                                 std::string_view alias) {
    // spdlog::trace("Entering function: {}", __func__); // Replaced
    // LOG_SCOPE_FUNCTION

    const std::string nameStr(name);
    const std::string aliasStr(alias);

    // Thread-safe exclusive lock for write operation
    std::unique_lock lock(mutex_);

    auto it = commands_.find(nameStr);
    if (it != commands_.end()) {
        // Add alias to each overload of the command
        for (auto& [hash, cmd] : it->second) {
            if (cmd.aliases.find(aliasStr) != cmd.aliases.end()) {
                spdlog::warn("Alias '{}' already exists for command '{}'.",
                             aliasStr, nameStr);
                return false;
            }
            cmd.aliases.insert(aliasStr);
        }

        // Add command map at the alias key for direct access
        commands_[aliasStr] = it->second;

        // Copy group if exists
        if (auto groupIt = groupMap_.find(nameStr);
            groupIt != groupMap_.end()) {
            groupMap_[aliasStr] = groupIt->second;
        }

        spdlog::info("Alias '{}' added for command '{}'.", aliasStr, nameStr);
        return true;
    } else {
        spdlog::warn("Command '{}' not found. Alias '{}' not added.", nameStr,
                     aliasStr);
        return false;
    }
}

bool CommandDispatcher::addGroup(std::string_view name,
                                 std::string_view group) {
    // spdlog::trace("Entering function: {}", __func__); // Replaced
    // LOG_SCOPE_FUNCTION

    const std::string nameStr(name);
    const std::string groupStr(group);

    // Thread-safe exclusive lock for write operation
    std::unique_lock lock(mutex_);

    if (commands_.find(nameStr) == commands_.end()) {
        spdlog::warn("Command '{}' not found. Group '{}' not added.", nameStr,
                     groupStr);
        return false;
    }

    groupMap_[nameStr] = groupStr;
    spdlog::info("Command '{}' added to group '{}'.", nameStr, groupStr);
    return true;
}

bool CommandDispatcher::setTimeout(std::string_view name,
                                   std::chrono::milliseconds timeout) {
    // spdlog::trace("Entering function: {}", __func__); // Replaced
    // LOG_SCOPE_FUNCTION

    const std::string nameStr(name);

    // Thread-safe exclusive lock for write operation
    std::unique_lock lock(mutex_);

    if (commands_.find(nameStr) == commands_.end()) {
        spdlog::warn("Command '{}' not found. Timeout not set.", nameStr);
        return false;
    }

    timeoutMap_[nameStr] = timeout;
    spdlog::info("Timeout set for command '{}': {} ms.", nameStr,
                 timeout.count());
    return true;
}

bool CommandDispatcher::removeCommand(std::string_view name) {
    // spdlog::trace("Entering function: {}", __func__); // Replaced
    // LOG_SCOPE_FUNCTION

    const std::string nameStr(name);

    // Thread-safe exclusive lock for write operation
    std::unique_lock lock(mutex_);

    if (commands_.find(nameStr) == commands_.end()) {
        spdlog::warn("Command '{}' not found. Cannot remove.", nameStr);
        return false;
    }

    // Remove all aliases first
    std::vector<std::string> aliases;
    for (const auto& [hash, cmd] : commands_[nameStr]) {
        for (const auto& alias : cmd.aliases) {
            aliases.push_back(alias);
        }
    }

    // Remove the command and its aliases
    commands_.erase(nameStr);
    for (const auto& alias : aliases) {
        commands_.erase(alias);
    }

    // Clean up associated maps
    groupMap_.erase(nameStr);
    timeoutMap_.erase(nameStr);

    spdlog::info("Command '{}' and its aliases removed.", nameStr);
    return true;
}

std::vector<std::string> CommandDispatcher::getCommandsInGroup(
    std::string_view group) const {
    // spdlog::trace("Entering function: {}", __func__); // Replaced
    // LOG_SCOPE_FUNCTION

    std::vector<std::string> result;
    std::unordered_set<std::string> uniqueCommands;  // For deduplication

    // Thread-safe shared lock for read operation
    std::shared_lock lock(mutex_);

    const std::string groupStr(group);

    for (const auto& [name, grp] : groupMap_) {
        if (grp == groupStr && commands_.find(name) != commands_.end()) {
            // Only add main commands, not aliases
            bool isAlias = false;
            for (const auto& [cmdName, cmdMap] : commands_) {
                if (cmdName == name)
                    continue;  // Skip self-check

                for (const auto& [hash, cmd] : cmdMap) {
                    if (cmd.aliases.find(name) != cmd.aliases.end()) {
                        isAlias = true;
                        break;
                    }
                }
                if (isAlias)
                    break;
            }

            if (!isAlias && uniqueCommands.insert(name).second) {
                result.push_back(name);
            }
        }
    }

    spdlog::info("Found {} commands in group '{}'", result.size(), groupStr);
    return result;
}

std::string CommandDispatcher::getCommandDescription(
    std::string_view name) const {
    // spdlog::trace("Entering function: {}", __func__); // Replaced
    // LOG_SCOPE_FUNCTION

    // Thread-safe shared lock for read operation
    std::shared_lock lock(mutex_);

    const std::string nameStr(name);

    auto it = commands_.find(nameStr);
    if (it != commands_.end() && !it->second.empty()) {
        // Return description of the first overload
        const auto& [hash, cmd] = *it->second.begin();
        spdlog::info("Description for command '{}': {}", nameStr,
                     cmd.description);
        return cmd.description;
    }

    // Check if this is an alias
    for (const auto& [cmdName, cmdMap] : commands_) {
        for (const auto& [hash, cmd] : cmdMap) {
            if (cmd.aliases.find(nameStr) != cmd.aliases.end()) {
                spdlog::info("Description for alias '{}': {}", nameStr,
                             cmd.description);
                return cmd.description;
            }
        }
    }

    spdlog::info("No description found for command '{}'.", nameStr);
    return "";
}

CommandDispatcher::StringSet CommandDispatcher::getCommandAliases(
    std::string_view name) const {
    // spdlog::trace("Entering function: {}", __func__); // Replaced
    // LOG_SCOPE_FUNCTION

    // Thread-safe shared lock for read operation
    std::shared_lock lock(mutex_);

    const std::string nameStr(name);

    auto it = commands_.find(nameStr);
    if (it != commands_.end() && !it->second.empty()) {
        // Return aliases of the first overload
        const auto& [hash, cmd] = *it->second.begin();
        spdlog::info("Found {} aliases for command '{}'", cmd.aliases.size(),
                     nameStr);
        return cmd.aliases;
    }

    // Check if this is itself an alias
    for (const auto& [cmdName, cmdMap] : commands_) {
        for (const auto& [hash, cmd] : cmdMap) {
            if (cmd.aliases.find(nameStr) != cmd.aliases.end()) {
                // Return all aliases except the name itself
                auto result = cmd.aliases;
                result.erase(nameStr);
                result.insert(cmdName);  // Add the original command name
                spdlog::info("Found {} aliases for alias '{}'", result.size(),
                             nameStr);
                return result;
            }
        }
    }

    spdlog::info("No aliases found for command '{}'.", nameStr);
#if ENABLE_FASTHASH
    return emhash::HashSet<std::string>{};
#else
    return {};
#endif
}

std::any CommandDispatcher::dispatch(std::string_view name,
                                     const std::vector<std::any>& args) {
    // spdlog::trace("Entering function: {}", __func__); // Replaced
    // LOG_SCOPE_FUNCTION

    if (isShuttingDown_.load(std::memory_order_acquire)) {
        THROW_DISPATCH_EXCEPTION("CommandDispatcher is shutting down");
    }

    spdlog::info("Dispatching command '{}'.", name);
    return dispatchHelper(std::string(name), args);
}

std::any CommandDispatcher::dispatch(std::string_view name,
                                     std::span<const std::any> args) {
    // spdlog::trace("Entering function: {}", __func__); // Replaced
    // LOG_SCOPE_FUNCTION

    if (isShuttingDown_.load(std::memory_order_acquire)) {
        THROW_DISPATCH_EXCEPTION("CommandDispatcher is shutting down");
    }

    spdlog::info("Dispatching command '{}' with span arguments.", name);
    std::vector<std::any> argsVec(args.begin(), args.end());
    return dispatchHelper(std::string(name), argsVec);
}

std::any CommandDispatcher::dispatch(std::string_view name,
                                     const atom::meta::FunctionParams& params) {
    // spdlog::trace("Entering function: {}", __func__); // Replaced
    // LOG_SCOPE_FUNCTION

    if (isShuttingDown_.load(std::memory_order_acquire)) {
        THROW_DISPATCH_EXCEPTION("CommandDispatcher is shutting down");
    }

    spdlog::info("Dispatching command '{}' with FunctionParams.", name);
    return dispatchHelper(std::string(name), params.toAnyVector());
}

std::vector<std::string> CommandDispatcher::getAllCommands() const {
    // spdlog::trace("Entering function: {}", __func__); // Replaced
    // LOG_SCOPE_FUNCTION

    // Thread-safe shared lock for read operation
    std::shared_lock lock(mutex_);

    std::vector<std::string> result;
    std::unordered_set<std::string> uniqueCommands;

    // Reserve space for efficiency
    result.reserve(commands_.size());
    uniqueCommands.reserve(commands_.size());

    // First add all main commands
    for (const auto& [name, _] : commands_) {
        if (uniqueCommands.insert(name).second) {
            result.push_back(name);
        }
    }

    spdlog::info("Found {} unique commands", result.size());
    return result;
}

namespace atom::utils {
auto toString(const std::vector<atom::meta::Arg>& arg) -> std::string {
    std::string result;
    bool first = true;
    for (const auto& a : arg) {
        if (!first) {
            result += ", ";
        }
        result += a.getName();
        first = false;
    }
    return result;
}
}  // namespace atom::utils

std::vector<CommandDispatcher::CommandArgRet>
CommandDispatcher::getCommandArgAndReturnType(std::string_view name) const {
    // spdlog::trace("Entering function: {}", __func__); // Replaced
    // LOG_SCOPE_FUNCTION

    // Thread-safe shared lock for read operation
    std::shared_lock lock(mutex_);

    const std::string nameStr(name);

    auto commandIterator = commands_.find(nameStr);
    if (commandIterator != commands_.end()) {
        std::vector<CommandArgRet> result;
        result.reserve(commandIterator->second.size());

        for (const auto& [hash, cmd] : commandIterator->second) {
            spdlog::info(
                "Argument and return types for command '{}': args = [{}], "
                "return = {}",
                nameStr, atom::utils::toString(cmd.argTypes), cmd.returnType);
            result.emplace_back(CommandArgRet{cmd.argTypes, cmd.returnType});
        }
        return result;
    }

    // Check aliases
    for (const auto& [cmdName, cmdMap] : commands_) {
        for (const auto& [hash, cmd] : cmdMap) {
            if (cmd.aliases.find(nameStr) != cmd.aliases.end()) {
                std::vector<CommandArgRet> result;
                result.reserve(1);
                spdlog::info(
                    "Argument and return types for alias '{}' (command '{}'): "
                    "args = [{}], "
                    "return = {}",
                    nameStr, cmdName, atom::utils::toString(cmd.argTypes),
                    cmd.returnType);
                result.emplace_back(
                    CommandArgRet{cmd.argTypes, cmd.returnType});
                return result;
            }
        }
    }

    spdlog::info("No argument and return types found for command '{}'.",
                 nameStr);
    return {};
}

auto CommandDispatcher::dispatchHelper(const std::string& name,
                                       const std::vector<std::any>& args)
    -> std::any {
    // spdlog::trace("Entering function: {}", __func__); // Replaced
    // LOG_SCOPE_FUNCTION

    // Thread-safe shared lock for read operations
    std::shared_lock lock(mutex_);

    // Compute function hash once
    const std::string funcHash = computeFunctionHash(args);
    spdlog::info("Dispatching command '{}' with hash '{}'", name, funcHash);

    auto commandIterator = commands_.find(name);
    if (commandIterator == commands_.end()) {
        // Command not found, check aliases
        for (const auto& [cmdName, cmdMap] : commands_) {
            for (const auto& [hash, cmd] : cmdMap) {
                if (cmd.aliases.find(name) != cmd.aliases.end()) {
                    spdlog::info("Found command alias '{}' -> '{}'", name,
                                 cmdName);
                    commandIterator = commands_.find(cmdName);
                    break;
                }
            }
            if (commandIterator != commands_.end())
                break;
        }

        if (commandIterator == commands_.end()) {
            spdlog::error("Command '{}' not found.", name);
            THROW_INVALID_ARGUMENT("Command '{}' not found.", name);
        }
    }

    // Find and make a local copy of the matching command
    Command matchingCmd;
    bool found = false;

    // Try to find a matching overload with the correct hash
    for (const auto& [hash, cmd] : commandIterator->second) {
        if (cmd.hash == funcHash) {
            matchingCmd = cmd;
            found = true;
            break;
        }
    }

    // Release the lock before executing
    lock.unlock();

    if (found) {
        // Check precondition
        checkPrecondition(matchingCmd, name);

        // Execute command
        std::any result = executeCommand(matchingCmd, name, args);

        // Check postcondition
        checkPostcondition(matchingCmd, name);

        return result;
    }

    spdlog::error(
        "No matching overload for command '{}' with the given arguments.",
        name);
    THROW_INVALID_ARGUMENT(
        "No matching overload for command '{}' with the given arguments.",
        name);
}
