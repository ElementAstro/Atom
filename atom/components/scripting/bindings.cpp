/*
 * bindings.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "bindings.hpp"

namespace atom::components::scripting {

// ExceptionTranslator implementation
std::string ExceptionTranslator::translateException(
    const std::exception& exception) {
    std::string typeName = typeid(exception).name();

    auto it = translators_.find(typeName);
    if (it != translators_.end()) {
        return it->second(exception);
    }

    // Default translation
    return std::string("C++ Exception: ") + exception.what();
}

// CallbackManager implementation
ScriptValue CallbackManager::invokeCallback(
    const std::string& name, const std::vector<ScriptValue>& args) {
    auto it = callbacks_.find(name);
    if (it == callbacks_.end()) {
        return ScriptValue();  // Callback not found
    }

    return it->second(args);
}

}  // namespace atom::components::scripting
