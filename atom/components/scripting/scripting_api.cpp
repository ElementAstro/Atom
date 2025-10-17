/*
 * scripting_api.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "scripting_api.hpp"
#include "../core/registry.hpp"
#include "lua_engine.hpp"
#include "python_engine.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <thread>

namespace atom::components::scripting {

// ComponentScriptingAPI implementation
ComponentScriptingAPI& ComponentScriptingAPI::instance() {
    static ComponentScriptingAPI instance;
    return instance;
}

bool ComponentScriptingAPI::initialize(const ScriptEngineConfig& config) {
    if (initialized_) {
        return true;
    }

    defaultConfig_ = config;
    initialized_ = true;

    return true;
}

std::unique_ptr<IScriptEngine> ComponentScriptingAPI::createEngine(
    ScriptLanguage language, const ScriptEngineConfig& config) {
    if (!initialized_) {
        return nullptr;
    }

    globalStats_.totalEnginesCreated++;
    globalStats_.activeEngines++;

    std::unique_ptr<IScriptEngine> engine;

    switch (language) {
        case ScriptLanguage::Lua:
#if ATOM_ENABLE_LUA
            if (LuaEngineFactory::isAvailable()) {
                engine = LuaEngineFactory::create();
            }
#endif
            break;

        case ScriptLanguage::ChaiScript:
            // ChaiScript engine would be created here
            break;

        case ScriptLanguage::Auto:
            // Try Lua first, then Python, then ChaiScript
#if ATOM_ENABLE_LUA
            if (LuaEngineFactory::isAvailable()) {
                engine = LuaEngineFactory::create();
                break;
            }
#endif
#if ATOM_ENABLE_PYTHON
            if (PythonEngineFactory::isAvailable()) {
                engine = PythonEngineFactory::create();
                break;
            }
#endif
            break;
    }

    if (engine && !engine->initialize(config)) {
        engine.reset();
        globalStats_.activeEngines--;
    }

    return engine;
}

ScriptResult ComponentScriptingAPI::execute(const std::string& script,
                                            bool isFile,
                                            ScriptLanguage language) {
    ScriptResult result;

    if (!initialized_) {
        result.errorMessage = "Scripting API not initialized";
        return result;
    }

    if (language == ScriptLanguage::Auto) {
        language = detectLanguage(script, isFile);
    }

    auto engine = createEngine(language, defaultConfig_);
    if (!engine) {
        result.errorMessage = "Failed to create script engine";
        return result;
    }

    if (isFile) {
        result = engine->executeFile(script);
    } else {
        result = engine->executeScript(script);
    }

    return result;
}

void ComponentScriptingAPI::registerComponentAPI(IScriptEngine& engine) {
    // Register core component API functions
    engine.registerFunction("createComponent", createComponent);
    engine.registerFunction("getComponent", getComponent);
    engine.registerFunction("removeComponent", removeComponent);
    engine.registerFunction("listComponents", listComponents);
    engine.registerFunction("callCommand", callCommand);
    engine.registerFunction("getVariable", getVariable);
    engine.registerFunction("setVariable", setVariable);
    engine.registerFunction("addEventListener", addEventListener);
    engine.registerFunction("removeEventListener", removeEventListener);
    engine.registerFunction("emitEvent", emitEvent);

    // Register utility functions
    engine.registerFunction("log", log);
    engine.registerFunction("sleep", sleep);
    engine.registerFunction("getCurrentTime", getCurrentTime);
}

void ComponentScriptingAPI::registerComponent(
    const std::string& /*name*/, std::shared_ptr<Component> /*component*/,
    IScriptEngine& /*engine*/) {
    // Store component reference and expose to script
    // Implementation would depend on the specific script engine
}

ScriptLanguage ComponentScriptingAPI::detectLanguage(const std::string& script,
                                                     bool isFile) {
    if (isFile) {
        // Detect by file extension
        std::filesystem::path path(script);
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       ::tolower);

        if (extension == ".lua") {
            return ScriptLanguage::Lua;
        } else if (extension == ".chai" || extension == ".chaiscript") {
            return ScriptLanguage::ChaiScript;
        }
    } else {
        // Detect by content patterns (simplified)
        if (script.find("function") != std::string::npos ||
            script.find("local") != std::string::npos ||
            script.find("end") != std::string::npos) {
            return ScriptLanguage::Lua;
        } else if (script.find("def") != std::string::npos ||
                   script.find("var") != std::string::npos ||
                   script.find("auto") != std::string::npos) {
            return ScriptLanguage::ChaiScript;
        }
    }

    // Default to Lua if detection fails
    return ScriptLanguage::Lua;
}

void ComponentScriptingAPI::resetGlobalStatistics() {
    globalStats_ = GlobalStatistics{};
}

void ComponentScriptingAPI::shutdown() {
    engines_.clear();
    globalStats_.activeEngines = 0;
    initialized_ = false;
}

// Core API function implementations
ScriptValue ComponentScriptingAPI::createComponent(
    const std::vector<ScriptValue>& args) {
    if (args.empty() || !args[0].holds<std::string>()) {
        return ScriptValue(false);
    }

    std::string name = args[0].get<std::string>();

    // TODO: Fix Registry access issue
    // try {
    //     auto& registry = ::Registry::instance();
    //     auto component = registry.createComponent<::Component>(name);
    //
    //     if (component) {
    //         return ScriptValue(true);
    //     }
    // } catch (const std::exception&) {
    //     // Error handling
    // }

    return ScriptValue(false);
}

ScriptValue ComponentScriptingAPI::getComponent(
    const std::vector<ScriptValue>& args) {
    if (args.empty() || !args[0].holds<std::string>()) {
        return ScriptValue();
    }

    std::string name = args[0].get<std::string>();

    try {
        auto& registry = ::Registry::instance();
        auto component = registry.getComponent(name);

        if (component) {
            // Return component information as a map
            std::unordered_map<std::string, ScriptValue> componentInfo;
            componentInfo["name"] =
                ScriptValue(std::string(component->getName()));
            componentInfo["state"] =
                ScriptValue(static_cast<int64_t>(component->getState()));

            ScriptValue result;
            result.value = componentInfo;
            return result;
        }
    } catch (const std::exception&) {
        // Error handling
    }

    return ScriptValue();
}

ScriptValue ComponentScriptingAPI::removeComponent(
    const std::vector<ScriptValue>& args) {
    if (args.empty() || !args[0].holds<std::string>()) {
        return ScriptValue(false);
    }

    std::string name = args[0].get<std::string>();

    try {
        auto& registry = ::Registry::instance();
        return ScriptValue(registry.removeComponent(name));
    } catch (const std::exception&) {
        // Error handling
    }

    return ScriptValue(false);
}

ScriptValue ComponentScriptingAPI::listComponents(
    const std::vector<ScriptValue>& /*args*/) {
    try {
        auto& registry = ::Registry::instance();
        auto components = registry.getAllComponents();

        std::vector<ScriptValue> componentList;
        for (const auto& component : components) {
            std::unordered_map<std::string, ScriptValue> componentInfo;
            componentInfo["name"] =
                ScriptValue(std::string(component->getName()));
            componentInfo["state"] =
                ScriptValue(static_cast<int64_t>(component->getState()));
            componentList.push_back(ScriptValue(componentInfo));
        }

        return ScriptValue(componentList);
    } catch (const std::exception&) {
        // Error handling
    }

    return ScriptValue(std::vector<ScriptValue>{});
}

ScriptValue ComponentScriptingAPI::callCommand(
    const std::vector<ScriptValue>& args) {
    if (args.size() < 2 || !args[0].holds<std::string>() ||
        !args[1].holds<std::string>()) {
        return ScriptValue(false);
    }

    std::string componentName = args[0].get<std::string>();
    std::string commandName = args[1].get<std::string>();

    try {
        auto& registry = ::Registry::instance();
        auto component = registry.getComponent(componentName);

        if (component) {
            // Extract arguments (simplified)
            std::vector<std::any> commandArgs;
            for (size_t i = 2; i < args.size(); ++i) {
                // Convert ScriptValue to std::any (simplified)
                if (args[i].holds<std::string>()) {
                    commandArgs.push_back(args[i].get<std::string>());
                } else if (args[i].holds<int64_t>()) {
                    commandArgs.push_back(args[i].get<int64_t>());
                } else if (args[i].holds<double>()) {
                    commandArgs.push_back(args[i].get<double>());
                } else if (args[i].holds<bool>()) {
                    commandArgs.push_back(args[i].get<bool>());
                }
            }

            // TODO: Implement component->call method
            // For now, return false as the method doesn't exist
            return ScriptValue(false);
        }
    } catch (const std::exception&) {
        // Error handling
    }

    return ScriptValue(false);
}

ScriptValue ComponentScriptingAPI::getVariable(
    const std::vector<ScriptValue>& args) {
    if (args.size() < 2 || !args[0].holds<std::string>() ||
        !args[1].holds<std::string>()) {
        return ScriptValue();
    }

    std::string componentName = args[0].get<std::string>();
    std::string variableName = args[1].get<std::string>();

    try {
        auto& registry = ::Registry::instance();
        auto component = registry.getComponent(componentName);

        if (component) {
            // TODO: Implement component->getVar method
            // For now, return empty string as the method doesn't exist
            return ScriptValue(std::string(""));
        }
    } catch (const std::exception&) {
        // Error handling
    }

    return ScriptValue();
}

ScriptValue ComponentScriptingAPI::setVariable(
    const std::vector<ScriptValue>& args) {
    if (args.size() < 3 || !args[0].holds<std::string>() ||
        !args[1].holds<std::string>()) {
        return ScriptValue(false);
    }

    std::string componentName = args[0].get<std::string>();
    std::string variableName = args[1].get<std::string>();

    try {
        auto& registry = ::Registry::instance();
        auto component = registry.getComponent(componentName);

        if (component) {
            // TODO: Implement component->setVar method
            // For now, return true as if the operation succeeded
            return ScriptValue(true);
        }
    } catch (const std::exception&) {
        // Error handling
    }

    return ScriptValue(false);
}

ScriptValue ComponentScriptingAPI::addEventListener(
    [[maybe_unused]] const std::vector<ScriptValue>& args) {
    // Simplified implementation - would need proper event system integration
    return ScriptValue(true);
}

ScriptValue ComponentScriptingAPI::removeEventListener(
    [[maybe_unused]] const std::vector<ScriptValue>& args) {
    // Simplified implementation - would need proper event system integration
    return ScriptValue(true);
}

ScriptValue ComponentScriptingAPI::emitEvent(
    [[maybe_unused]] const std::vector<ScriptValue>& args) {
    // Simplified implementation - would need proper event system integration
    return ScriptValue(true);
}

// Utility function implementations
ScriptValue ComponentScriptingAPI::log(const std::vector<ScriptValue>& args) {
    std::ostringstream oss;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0)
            oss << " ";

        if (args[i].holds<std::string>()) {
            oss << args[i].get<std::string>();
        } else if (args[i].holds<int64_t>()) {
            oss << args[i].get<int64_t>();
        } else if (args[i].holds<double>()) {
            oss << args[i].get<double>();
        } else if (args[i].holds<bool>()) {
            oss << (args[i].get<bool>() ? "true" : "false");
        } else {
            oss << "[object]";
        }
    }

    // In a real implementation, this would use the logging system
    // For now, just print to stdout
    std::cout << "[SCRIPT] " << oss.str() << std::endl;

    return ScriptValue();
}

ScriptValue ComponentScriptingAPI::sleep(const std::vector<ScriptValue>& args) {
    if (args.empty() || !args[0].holds<int64_t>()) {
        return ScriptValue(false);
    }

    int64_t milliseconds = args[0].get<int64_t>();
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));

    return ScriptValue(true);
}

ScriptValue ComponentScriptingAPI::getCurrentTime(
    [[maybe_unused]] const std::vector<ScriptValue>& args) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now.time_since_epoch())
                         .count();

    return ScriptValue(static_cast<int64_t>(timestamp));
}

// ScriptHotReloader implementation
void ScriptHotReloader::watchFile(const std::string& filename,
                                  IScriptEngine& engine,
                                  std::function<void(bool)> callback) {
    std::lock_guard lock(watchedFilesMutex_);

    WatchedFile watchedFile;
    watchedFile.filename = filename;
    watchedFile.engine = &engine;
    watchedFile.callback = callback;

    try {
        watchedFile.lastModified = std::filesystem::last_write_time(filename);
    } catch (const std::filesystem::filesystem_error&) {
        // File doesn't exist yet, use current time
        watchedFile.lastModified =
            std::filesystem::file_time_type::clock::now();
    }

    // Remove existing watch for this file
    watchedFiles_.erase(
        std::remove_if(watchedFiles_.begin(), watchedFiles_.end(),
                       [&filename](const WatchedFile& wf) {
                           return wf.filename == filename;
                       }),
        watchedFiles_.end());

    watchedFiles_.push_back(watchedFile);

    // Start watcher thread if not running
    if (!running_.load() && !watchedFiles_.empty()) {
        start();
    }
}

void ScriptHotReloader::unwatchFile(const std::string& filename) {
    std::lock_guard lock(watchedFilesMutex_);

    watchedFiles_.erase(
        std::remove_if(watchedFiles_.begin(), watchedFiles_.end(),
                       [&filename](const WatchedFile& wf) {
                           return wf.filename == filename;
                       }),
        watchedFiles_.end());

    // Stop watcher if no files are being watched
    if (watchedFiles_.empty() && running_.load()) {
        stop();
    }
}

bool ScriptHotReloader::reloadFile(const std::string& filename) {
    std::lock_guard lock(watchedFilesMutex_);

    auto it = std::find_if(
        watchedFiles_.begin(), watchedFiles_.end(),
        [&filename](const WatchedFile& wf) { return wf.filename == filename; });

    if (it == watchedFiles_.end()) {
        return false;  // File not being watched
    }

    try {
        // Execute the file
        ScriptResult result = it->engine->executeFile(filename);

        // Update last modified time
        it->lastModified = std::filesystem::last_write_time(filename);

        // Call callback if provided
        if (it->callback) {
            it->callback(result.success);
        }

        return result.success;
    } catch (const std::exception&) {
        if (it->callback) {
            it->callback(false);
        }
        return false;
    }
}

std::vector<std::string> ScriptHotReloader::getWatchedFiles() const {
    std::lock_guard lock(watchedFilesMutex_);

    std::vector<std::string> files;
    files.reserve(watchedFiles_.size());

    for (const auto& watchedFile : watchedFiles_) {
        files.push_back(watchedFile.filename);
    }

    return files;
}

void ScriptHotReloader::start() {
    if (running_.exchange(true)) {
        return;  // Already running
    }

    watcherThread_ = std::thread(&ScriptHotReloader::watcherLoop, this);
}

void ScriptHotReloader::stop() {
    if (!running_.exchange(false)) {
        return;  // Already stopped
    }

    if (watcherThread_.joinable()) {
        watcherThread_.join();
    }
}

void ScriptHotReloader::watcherLoop() {
    while (running_.load()) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(500));  // Check every 500ms

        std::lock_guard lock(watchedFilesMutex_);

        for (auto& watchedFile : watchedFiles_) {
            try {
                auto currentModified =
                    std::filesystem::last_write_time(watchedFile.filename);

                if (currentModified > watchedFile.lastModified) {
                    // File has been modified, reload it
                    ScriptResult result =
                        watchedFile.engine->executeFile(watchedFile.filename);
                    watchedFile.lastModified = currentModified;

                    // Call callback if provided
                    if (watchedFile.callback) {
                        watchedFile.callback(result.success);
                    }
                }
            } catch (const std::filesystem::filesystem_error&) {
                // File might have been deleted or is temporarily inaccessible
                // Continue monitoring
            } catch (const std::exception&) {
                // Script execution error
                if (watchedFile.callback) {
                    watchedFile.callback(false);
                }
            }
        }
    }
}

}  // namespace atom::components::scripting
