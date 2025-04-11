#include "dispatch.hpp"

#include <future>
#include <thread>

#include "atom/log/loguru.hpp"
#include "atom/utils/to_string.hpp"

void CommandDispatcher::checkPrecondition(const Command& cmd,
                                          const std::string& name) {
    LOG_SCOPE_FUNCTION(INFO);
    if (!cmd.precondition.has_value()) {
        LOG_F(INFO, "No precondition for command: {}", name);
        return;
    }
    try {
        if (!std::invoke(cmd.precondition.value())) {
            LOG_F(ERROR, "Precondition failed for command '{}'", name);
            THROW_DISPATCH_EXCEPTION("Precondition failed for command '{}'", name);
        }
        LOG_F(INFO, "Precondition for command '{}' passed.", name);
    } catch (const std::bad_function_call& e) {
        LOG_F(ERROR, "Bad precondition function invoke for command '{}': {}", name, e.what());
        THROW_DISPATCH_EXCEPTION("Bad precondition function invoke for command '{}': {}", 
                               name, e.what());
    } catch (const std::bad_optional_access& e) {
        LOG_F(ERROR, "Bad precondition function access for command '{}': {}", name, e.what());
        THROW_DISPATCH_EXCEPTION("Bad precondition function access for command '{}': {}", 
                               name, e.what());
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Precondition for command '{}' failed: {}", name, e.what());
        THROW_DISPATCH_EXCEPTION("Precondition failed for command '{}': {}", name, e.what());
    }
}

void CommandDispatcher::checkPostcondition(const Command& cmd,
                                           const std::string& name) {
    LOG_SCOPE_FUNCTION(INFO);
    if (!cmd.postcondition.has_value()) {
        LOG_F(INFO, "No postcondition for command: {}", name);
        return;
    }
    try {
        std::invoke(cmd.postcondition.value());
        LOG_F(INFO, "Postcondition for command '{}' passed.", name);
    } catch (const std::bad_function_call& e) {
        LOG_F(ERROR, "Bad postcondition function invoke for command '{}': {}", name, e.what());
        THROW_DISPATCH_EXCEPTION("Bad postcondition function invoke for command '{}': {}", 
                               name, e.what());
    } catch (const std::bad_optional_access& e) {
        LOG_F(ERROR, "Bad postcondition function access for command '{}': {}", name, e.what());
        THROW_DISPATCH_EXCEPTION("Bad postcondition function access for command '{}': {}", 
                               name, e.what());
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Postcondition for command '{}' failed: {}", name, e.what());
        THROW_DISPATCH_EXCEPTION("Postcondition failed for command '{}': {}", name, e.what());
    }
}

auto CommandDispatcher::executeCommand(
    const Command& cmd, const std::string& name,
    const std::vector<std::any>& args) -> std::any {
    LOG_SCOPE_FUNCTION(INFO);
    if (auto timeoutIt = timeoutMap_.find(name);
        timeoutIt != timeoutMap_.end()) {
        LOG_F(INFO, "Executing command '{}' with timeout {}ms.", name, 
              timeoutIt->second.count());
        return executeWithTimeout(cmd, name, args, timeoutIt->second);
    }
    LOG_F(INFO, "Executing command '{}' without timeout.", name);
    return executeWithoutTimeout(cmd, name, args);
}

auto CommandDispatcher::executeWithTimeout(
    const Command& cmd, const std::string& name,
    const std::vector<std::any>& args,
    const std::chrono::milliseconds& timeout) -> std::any {
    LOG_SCOPE_FUNCTION(INFO);
    
    // Create a promise and associated future
    std::promise<std::any> promise;
    auto future = promise.get_future();
    
    // Run the function in a separate thread
    std::thread worker([&cmd, &args, &promise]() {
        try {
            std::any result = executeFunctions(cmd, args);
            promise.set_value(result);
        } catch (...) {
            // Capture any exception and store it in the promise
            promise.set_exception(std::current_exception());
        }
    });
    
    // Detach the thread to avoid blocking
    worker.detach();
    
    // Wait for the result with timeout
    if (future.wait_for(timeout) == std::future_status::timeout) {
        LOG_F(ERROR, "Command '{}' timed out after {}ms.", name, timeout.count());
        THROW_DISPATCH_TIMEOUT("Command '{}' timed out after {}ms.", 
                             name, timeout.count());
    }
    
    // Get the result or propagate exception
    try {
        return future.get();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Command '{}' failed: {}", name, e.what());
        throw; // Re-throw the captured exception
    }
}

auto CommandDispatcher::executeWithoutTimeout(
    const Command& cmd, const std::string& name,
    const std::vector<std::any>& args) -> std::any {
    LOG_SCOPE_FUNCTION(INFO);
    // Check for nested arguments
    if (!args.empty() && args.size() == 1 && args[0].type() == typeid(std::vector<std::any>)) {
        LOG_F(INFO, "Executing command '{}' with nested arguments.", name);
        return executeFunctions(cmd, std::any_cast<std::vector<std::any>>(args[0]));
    }

    LOG_F(INFO, "Executing command '{}' with arguments.", name);
    return executeFunctions(cmd, args);
}

auto CommandDispatcher::executeFunctions(
    const Command& cmd, const std::vector<std::any>& args) -> std::any {
    LOG_SCOPE_FUNCTION(INFO);
    std::string funcHash = computeFunctionHash(args);
    
    // Check if hash matches
    if (cmd.hash == funcHash) {
        try {
            LOG_F(INFO, "Executing function for command with hash: {}", funcHash);
            return std::invoke(cmd.func, args);
        } catch (const std::bad_any_cast& e) {
            LOG_F(ERROR, "Failed to call function for command with hash {}: {}", 
                  funcHash, e.what());
            THROW_DISPATCH_EXCEPTION("Failed to call function for command with hash {}: {}", 
                                   funcHash, e.what());
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error executing function for command with hash {}: {}", 
                  funcHash, e.what());
            THROW_DISPATCH_EXCEPTION("Error executing function for command with hash {}: {}", 
                                   funcHash, e.what());
        }
    }

    LOG_F(ERROR, "No matching overload found for command with hash: {}", funcHash);
    THROW_INVALID_ARGUMENT("No matching overload found for command with hash: {}", funcHash);
}

auto CommandDispatcher::computeFunctionHash(const std::vector<std::any>& args)
    -> std::string {
    LOG_SCOPE_FUNCTION(INFO);
    // Use a more efficient approach for small arg counts
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
    LOG_F(INFO, "Computed function hash: {}", hash);
    return hash;
}

auto CommandDispatcher::has(const std::string& name) -> bool {
    LOG_SCOPE_FUNCTION(INFO);
    // Direct lookup first for performance
    if (commands_.find(name) != commands_.end()) {
        LOG_F(INFO, "Command '{}' found.", name);
        return true;
    }
    
    // Then check aliases
    for (const auto& [cmdName, cmdMap] : commands_) {
        for (const auto& [hash, cmd] : cmdMap) {
            if (cmd.aliases.find(name) != cmd.aliases.end()) {
                LOG_F(INFO, "Alias '{}' found for command '{}'.", name, cmdName);
                return true;
            }
        }
    }
    
    LOG_F(INFO, "Command '{}' not found.", name);
    return false;
}

void CommandDispatcher::addAlias(const std::string& name,
                                 const std::string& alias) {
    LOG_SCOPE_FUNCTION(INFO);
    auto it = commands_.find(name);
    if (it != commands_.end()) {
        // Add alias to each overload of the command
        for (auto& [hash, cmd] : it->second) {
            if (cmd.aliases.find(alias) != cmd.aliases.end()) {
                LOG_F(WARNING, "Alias '{}' already exists for command '{}'.", alias, name);
                return;
            }
            cmd.aliases.insert(alias);
        }
        
        // Add command map at the alias key for direct access
        commands_[alias] = it->second;
        
        // Copy group if exists
        if (auto groupIt = groupMap_.find(name); groupIt != groupMap_.end()) {
            groupMap_[alias] = groupIt->second;
        }
        
        LOG_F(INFO, "Alias '{}' added for command '{}'.", alias, name);
    } else {
        LOG_F(WARNING, "Command '{}' not found. Alias '{}' not added.", name, alias);
    }
}

void CommandDispatcher::addGroup(const std::string& name,
                                 const std::string& group) {
    LOG_SCOPE_FUNCTION(INFO);
    if (commands_.find(name) == commands_.end()) {
        LOG_F(WARNING, "Command '{}' not found. Group '{}' not added.", name, group);
        return;
    }
    
    groupMap_[name] = group;
    LOG_F(INFO, "Command '{}' added to group '{}'.", name, group);
}

void CommandDispatcher::setTimeout(const std::string& name,
                                   std::chrono::milliseconds timeout) {
    LOG_SCOPE_FUNCTION(INFO);
    if (commands_.find(name) == commands_.end()) {
        LOG_F(WARNING, "Command '{}' not found. Timeout not set.", name);
        return;
    }
    
    timeoutMap_[name] = timeout;
    LOG_F(INFO, "Timeout set for command '{}': {} ms.", name, timeout.count());
}

void CommandDispatcher::removeCommand(const std::string& name) {
    LOG_SCOPE_FUNCTION(INFO);
    if (commands_.find(name) == commands_.end()) {
        LOG_F(WARNING, "Command '{}' not found. Cannot remove.", name);
        return;
    }
    
    // Remove all aliases first
    std::vector<std::string> aliases;
    for (const auto& [hash, cmd] : commands_[name]) {
        for (const auto& alias : cmd.aliases) {
            aliases.push_back(alias);
        }
    }
    
    // Remove the command and its aliases
    commands_.erase(name);
    for (const auto& alias : aliases) {
        commands_.erase(alias);
    }
    
    // Clean up associated maps
    groupMap_.erase(name);
    timeoutMap_.erase(name);
    
    LOG_F(INFO, "Command '{}' and its aliases removed.", name);
}

auto CommandDispatcher::getCommandsInGroup(const std::string& group) const
    -> std::vector<std::string> {
    LOG_SCOPE_FUNCTION(INFO);
    std::vector<std::string> result;
    std::unordered_set<std::string> uniqueCommands; // For deduplication
    
    for (const auto& [name, grp] : groupMap_) {
        if (grp == group && commands_.find(name) != commands_.end()) {
            // Only add main commands, not aliases
            bool isAlias = false;
            for (const auto& [cmdName, cmdMap] : commands_) {
                if (cmdName == name) continue; // Skip self-check
                
                for (const auto& [hash, cmd] : cmdMap) {
                    if (cmd.aliases.find(name) != cmd.aliases.end()) {
                        isAlias = true;
                        break;
                    }
                }
                if (isAlias) break;
            }
            
            if (!isAlias && uniqueCommands.insert(name).second) {
                result.push_back(name);
            }
        }
    }
    
    LOG_F(INFO, "Found {} commands in group '{}'", result.size(), group);
    return result;
}

auto CommandDispatcher::getCommandDescription(const std::string& name) const
    -> std::string {
    LOG_SCOPE_FUNCTION(INFO);
    auto it = commands_.find(name);
    if (it != commands_.end() && !it->second.empty()) {
        // Return description of the first overload
        const auto& [hash, cmd] = *it->second.begin();
        LOG_F(INFO, "Description for command '{}': {}", name, cmd.description);
        return cmd.description;
    }
    
    // Check if this is an alias
    for (const auto& [cmdName, cmdMap] : commands_) {
        for (const auto& [hash, cmd] : cmdMap) {
            if (cmd.aliases.find(name) != cmd.aliases.end()) {
                LOG_F(INFO, "Description for alias '{}': {}", name, cmd.description);
                return cmd.description;
            }
        }
    }
    
    LOG_F(INFO, "No description found for command '{}'.", name);
    return "";
}

auto CommandDispatcher::getCommandAliases(const std::string& name) const
#if ENABLE_FASTHASH
    -> emhash::HashSet<std::string>
#else
    -> std::unordered_set<std::string>
#endif
{
    LOG_SCOPE_FUNCTION(INFO);
    auto it = commands_.find(name);
    if (it != commands_.end() && !it->second.empty()) {
        // Return aliases of the first overload
        const auto& [hash, cmd] = *it->second.begin();
        LOG_F(INFO, "Found {} aliases for command '{}'", cmd.aliases.size(), name);
        return cmd.aliases;
    }
    
    // Check if this is itself an alias
    for (const auto& [cmdName, cmdMap] : commands_) {
        for (const auto& [hash, cmd] : cmdMap) {
            if (cmd.aliases.find(name) != cmd.aliases.end()) {
                // Return all aliases except the name itself
                auto result = cmd.aliases;
                result.erase(name);
                result.insert(cmdName); // Add the original command name
                LOG_F(INFO, "Found {} aliases for alias '{}'", result.size(), name);
                return result;
            }
        }
    }
    
    LOG_F(INFO, "No aliases found for command '{}'.", name);
#if ENABLE_FASTHASH
    return emhash::HashSet<std::string>{};
#else
    return {};
#endif
}

auto CommandDispatcher::dispatch(
    const std::string& name, const std::vector<std::any>& args) -> std::any {
    LOG_SCOPE_FUNCTION(INFO);
    LOG_F(INFO, "Dispatching command '{}'.", name);
    return dispatchHelper(name, args);
}

auto CommandDispatcher::dispatch(const std::string& name,
                                 const atom::meta::FunctionParams& params)
    -> std::any {
    LOG_SCOPE_FUNCTION(INFO);
    LOG_F(INFO, "Dispatching command '{}' with FunctionParams.", name);
    return dispatchHelper(name, params.toAnyVector());
}

auto CommandDispatcher::getAllCommands() const -> std::vector<std::string> {
    LOG_SCOPE_FUNCTION(INFO);
    std::vector<std::string> result;
    std::unordered_set<std::string> uniqueCommands;
    
    // First add all main commands
    for (const auto& [name, _] : commands_) {
        if (uniqueCommands.insert(name).second) {
            result.push_back(name);
        }
    }
    
    LOG_F(INFO, "Found {} unique commands", result.size());
    return result;
}

namespace atom::utils {
auto toString(const std::vector<atom::meta::Arg>& arg) -> std::string {
    std::string result;
    for (const auto& a : arg) {
        result += a.getName() + " ";
    }
    return result;
}
}  // namespace atom::utils

auto CommandDispatcher::getCommandArgAndReturnType(const std::string& name)
    -> std::vector<CommandArgRet> {
    LOG_SCOPE_FUNCTION(INFO);
    auto commandIterator = commands_.find(name);
    if (commandIterator != commands_.end()) {
        std::vector<CommandArgRet> result;
        result.reserve(commandIterator->second.size());
        
        for (const auto& [hash, cmd] : commandIterator->second) {
            LOG_F(INFO,
                  "Argument and return types for command '{}': args = [{}], "
                  "return = {}",
                  name, atom::utils::toString(cmd.argTypes), cmd.returnType);
            result.emplace_back(CommandArgRet{cmd.argTypes, cmd.returnType});
        }
        return result;
    }
    
    // Check aliases
    for (const auto& [cmdName, cmdMap] : commands_) {
        for (const auto& [hash, cmd] : cmdMap) {
            if (cmd.aliases.find(name) != cmd.aliases.end()) {
                std::vector<CommandArgRet> result;
                result.reserve(1);
                result.emplace_back(CommandArgRet{cmd.argTypes, cmd.returnType});
                return result;
            }
        }
    }
    
    LOG_F(INFO, "No argument and return types found for command '{}'.", name);
    return {};
}

auto CommandDispatcher::dispatchHelper(
    const std::string& name, const std::vector<std::any>& args) -> std::any {
    LOG_SCOPE_FUNCTION(INFO);
    auto commandIterator = commands_.find(name);
    if (commandIterator == commands_.end()) {
        // Check if it's an alias
        for (const auto& [cmdName, cmdMap] : commands_) {
            for (const auto& [hash, cmd] : cmdMap) {
                if (cmd.aliases.find(name) != cmd.aliases.end()) {
                    LOG_F(INFO, "Found command alias '{}' -> '{}'", name, cmdName);
                    commandIterator = commands_.find(cmdName);
                    break;
                }
            }
            if (commandIterator != commands_.end()) break;
        }
        
        if (commandIterator == commands_.end()) {
            LOG_F(ERROR, "Command '{}' not found.", name);
            THROW_INVALID_ARGUMENT("Command '{}' not found.", name);
        }
    }
    
    const std::string funcHash = computeFunctionHash(args);
    LOG_F(INFO, "Dispatching command '{}' with hash '{}'", name, funcHash);
    
    // Try to find a matching overload
    for (const auto& [hash, cmd] : commandIterator->second) {
        if (cmd.hash == funcHash) {
            // Check precondition
            checkPrecondition(cmd, name);
            
            // Execute command
            std::any result = executeCommand(cmd, name, args);
            
            // Check postcondition
            checkPostcondition(cmd, name);
            
            return result;
        }
    }
    
    LOG_F(ERROR, "No matching overload for command '{}' with the given arguments.", name);
    THROW_INVALID_ARGUMENT("No matching overload for command '{}' with the given arguments.", 
                         name);
}