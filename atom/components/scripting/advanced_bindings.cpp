/*
 * advanced_bindings.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "advanced_bindings.hpp"

#include <algorithm>
#include <stdexcept>

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

template <typename ExceptionType>
void ExceptionTranslator::registerTranslator(
    std::function<std::string(const ExceptionType&)> translator) {
    std::string typeName = typeid(ExceptionType).name();

    translators_[typeName] =
        [translator](const std::exception& e) -> std::string {
        try {
            const ExceptionType& typed_exception =
                dynamic_cast<const ExceptionType&>(e);
            return translator(typed_exception);
        } catch (const std::bad_cast&) {
            return std::string("Exception translation failed: ") + e.what();
        }
    };
}

template <typename Func>
ScriptResult ExceptionTranslator::executeWithTranslation(Func&& func) {
    ScriptResult result;

    try {
        result = func();
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = translateException(e);
    } catch (...) {
        result.success = false;
        result.errorMessage = "Unknown C++ exception occurred";
    }

    return result;
}

// CallbackManager implementation
template <typename Func>
void CallbackManager::registerCallback(const std::string& name, Func func) {
    callbacks_[name] = createWrapper(func);
}

template <typename Func>
ScriptFunction CallbackManager::createWrapper(Func func) {
    return [func](const std::vector<ScriptValue>& args) -> ScriptValue {
        // This is a simplified implementation
        // A full implementation would need template metaprogramming to handle
        // arbitrary function signatures and argument conversion

        if constexpr (std::is_void_v<std::invoke_result_t<Func>>) {
            // Void return type
            if constexpr (std::is_invocable_v<Func>) {
                func();
                return ScriptValue();
            }
        } else {
            // Non-void return type
            if constexpr (std::is_invocable_v<Func>) {
                auto result = func();
                // Convert result to ScriptValue
                return ScriptValue();  // Simplified
            }
        }

        return ScriptValue();
    };
}

ScriptValue CallbackManager::invokeCallback(
    const std::string& name, const std::vector<ScriptValue>& args) {
    auto it = callbacks_.find(name);
    if (it == callbacks_.end()) {
        return ScriptValue();  // Callback not found
    }

    return it->second(args);
}

template <typename Signature>
std::function<Signature> CallbackManager::createCppCallback(
    ScriptFunction scriptFunc) {
    return createCppCallbackImpl(
        scriptFunc, static_cast<std::function<Signature>*>(nullptr));
}

template <typename R, typename... Args>
std::function<R(Args...)> CallbackManager::createCppCallbackImpl(
    ScriptFunction scriptFunc, std::function<R(Args...)>*) {
    return [scriptFunc](Args... args) -> R {
        // Convert C++ arguments to ScriptValues
        std::vector<ScriptValue> scriptArgs;
        ((scriptArgs.push_back(ScriptValue(args))),
         ...);  // C++17 fold expression

        // Call script function
        ScriptValue result = scriptFunc(scriptArgs);

        // Convert result back to C++ type
        if constexpr (std::is_void_v<R>) {
            return;
        } else {
            if (result.holds<R>()) {
                return result.get<R>();
            } else {
                // Type conversion failed, return default value
                return R{};
            }
        }
    };
}

// OperatorBinder implementation
template <typename T>
template <typename Op>
OperatorBinder<T>& OperatorBinder<T>::def_operator(OperatorType op, Op func) {
    operators_[op] =
        [func](const std::vector<ScriptValue>& args) -> ScriptValue {
        // Simplified implementation - would need proper argument conversion
        if (args.size() >= 2) {
            // Binary operator
            if constexpr (std::is_invocable_v<Op, T, T>) {
                // Extract operands from args and call func
                // This is a simplified version
                return ScriptValue();
            }
        } else if (args.size() == 1) {
            // Unary operator
            if constexpr (std::is_invocable_v<Op, T>) {
                // Extract operand from args and call func
                return ScriptValue();
            }
        }
        return ScriptValue();
    };

    return *this;
}

template <typename T>
template <typename Op>
OperatorBinder<T>& OperatorBinder<T>::def_comparison(OperatorType op, Op func) {
    return def_operator(op, func);
}

template <typename T>
template <typename IndexType, typename ReturnType>
OperatorBinder<T>& OperatorBinder<T>::def_index(
    std::function<ReturnType(const T&, IndexType)> getter,
    std::function<void(T&, IndexType, const ReturnType&)> setter) {
    operators_[OperatorType::Index] =
        [getter, setter](const std::vector<ScriptValue>& args) -> ScriptValue {
        if (args.size() >= 2) {
            // Get operation: obj[index]
            // Extract T object and IndexType index from args
            // Call getter and return result
            // This is a simplified implementation
        } else if (args.size() >= 3 && setter) {
            // Set operation: obj[index] = value
            // Extract T object, IndexType index, and ReturnType value from args
            // Call setter
        }
        return ScriptValue();
    };

    return *this;
}

template <typename T>
template <typename ReturnType, typename... Args>
OperatorBinder<T>& OperatorBinder<T>::def_call(
    std::function<ReturnType(T&, Args...)> func) {
    operators_[OperatorType::Call] =
        [func](const std::vector<ScriptValue>& args) -> ScriptValue {
        // Extract T object and Args... from script args
        // Call func and return result
        // This is a simplified implementation
        return ScriptValue();
    };

    return *this;
}

// PropertyBinder implementation
template <typename T>
template <typename PropertyType>
PropertyBinder<T>& PropertyBinder<T>::def_property(
    const std::string& name, std::function<PropertyType(const T&)> getter,
    std::function<void(T&, const PropertyType&)> setter) {
    PropertyInfo info;
    info.getter =
        [getter](const std::vector<ScriptValue>& args) -> ScriptValue {
        // Extract T object from args, call getter, return result
        // This is a simplified implementation
        return ScriptValue();
    };

    info.setter =
        [setter](const std::vector<ScriptValue>& args) -> ScriptValue {
        // Extract T object and PropertyType value from args, call setter
        // This is a simplified implementation
        return ScriptValue();
    };

    info.isReadOnly = false;
    properties_[name] = info;

    return *this;
}

template <typename T>
template <typename PropertyType>
PropertyBinder<T>& PropertyBinder<T>::def_property_readonly(
    const std::string& name, std::function<PropertyType(const T&)> getter) {
    PropertyInfo info;
    info.getter =
        [getter](const std::vector<ScriptValue>& args) -> ScriptValue {
        // Extract T object from args, call getter, return result
        return ScriptValue();
    };

    info.isReadOnly = true;
    properties_[name] = info;

    return *this;
}

template <typename T>
template <typename PropertyType>
PropertyBinder<T>& PropertyBinder<T>::def_static_property(
    const std::string& name, std::function<PropertyType()> getter,
    std::function<void(const PropertyType&)> setter) {
    PropertyInfo info;
    info.getter =
        [getter](const std::vector<ScriptValue>& args) -> ScriptValue {
        // Call static getter, return result
        auto result = getter();
        return ScriptValue();  // Convert result to ScriptValue
    };

    if (setter) {
        info.setter =
            [setter](const std::vector<ScriptValue>& args) -> ScriptValue {
            // Extract PropertyType value from args, call static setter
            if (!args.empty()) {
                // Convert args[0] to PropertyType and call setter
                // This is a simplified implementation
            }
            return ScriptValue();
        };
    }

    info.isStatic = true;
    info.isReadOnly = (setter == nullptr);
    properties_[name] = info;

    return *this;
}

// AdvancedClassBinder implementation
template <typename T, typename ScriptEngine>
template <typename... Args>
AdvancedClassBinder<T, ScriptEngine>&
AdvancedClassBinder<T, ScriptEngine>::def_constructor() {
    // Register constructor with the script engine
    std::string constructorName = className_ + ".__init__";

    ScriptFunction constructor =
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
        // Create new instance of T with Args...
        // This would need proper argument conversion and object creation
        return ScriptValue();
    };

    engine_.registerFunction(constructorName, constructor);
    return *this;
}

template <typename T, typename ScriptEngine>
template <typename Func>
AdvancedClassBinder<T, ScriptEngine>&
AdvancedClassBinder<T, ScriptEngine>::def_method(const std::string& name,
                                                 Func func,
                                                 const std::string& doc) {
    std::string methodName = className_ + "." + name;
    ScriptFunction wrapper = createMethodWrapper(func);

    // Wrap with exception translation
    ScriptFunction translatedWrapper =
        [this, wrapper](const std::vector<ScriptValue>& args) -> ScriptValue {
        try {
            return wrapper(args);
        } catch (const std::exception& e) {
            // Create error result with translated exception
            ScriptValue error;
            // Set error information
            return error;
        }
    };

    engine_.registerFunction(methodName, translatedWrapper);
    return *this;
}

template <typename T, typename ScriptEngine>
template <typename Func>
AdvancedClassBinder<T, ScriptEngine>&
AdvancedClassBinder<T, ScriptEngine>::def_static_method(
    const std::string& name, Func func, const std::string& doc) {
    std::string methodName = className_ + "." + name;
    ScriptFunction wrapper = createStaticMethodWrapper(func);

    engine_.registerFunction(methodName, wrapper);
    return *this;
}

template <typename T, typename ScriptEngine>
template <typename E>
AdvancedClassBinder<T, ScriptEngine>&
AdvancedClassBinder<T, ScriptEngine>::def_enum(
    const std::string& enumName,
    const std::vector<std::pair<E, std::string>>& values) {
    for (const auto& [value, name] : values) {
        std::string fullName = className_ + "." + enumName + "." + name;
        engine_.setGlobal(fullName, ScriptValue(static_cast<int64_t>(value)));
    }

    return *this;
}

template <typename T, typename ScriptEngine>
template <typename Func>
AdvancedClassBinder<T, ScriptEngine>&
AdvancedClassBinder<T, ScriptEngine>::def_str(Func func) {
    operatorBinder_.def_operator(OperatorType::ToString, func);
    return *this;
}

template <typename T, typename ScriptEngine>
template <typename Func>
AdvancedClassBinder<T, ScriptEngine>&
AdvancedClassBinder<T, ScriptEngine>::def_repr(Func func) {
    // Similar to def_str but for representation
    return def_str(func);
}

template <typename T, typename ScriptEngine>
void AdvancedClassBinder<T, ScriptEngine>::finalize() {
    // Execute all finalizers
    for (auto& finalizer : finalizers_) {
        finalizer();
    }
}

template <typename T, typename ScriptEngine>
template <typename Func>
ScriptFunction AdvancedClassBinder<T, ScriptEngine>::createMethodWrapper(
    Func func) {
    return [func](const std::vector<ScriptValue>& args) -> ScriptValue {
        // Extract T object from first argument
        // Extract method arguments from remaining arguments
        // Call func with proper arguments
        // Convert result to ScriptValue
        // This is a simplified implementation
        return ScriptValue();
    };
}

template <typename T, typename ScriptEngine>
template <typename Func>
ScriptFunction AdvancedClassBinder<T, ScriptEngine>::createStaticMethodWrapper(
    Func func) {
    return [func](const std::vector<ScriptValue>& args) -> ScriptValue {
        // Extract arguments and call static function
        // Convert result to ScriptValue
        // This is a simplified implementation
        return ScriptValue();
    };
}

// InheritanceBinder static member initialization
template <typename Derived, typename Base>
std::unordered_map<std::string, std::vector<std::string>>
    InheritanceBinder<Derived, Base>::inheritanceMap_;

template <typename Derived, typename Base>
void InheritanceBinder<Derived, Base>::registerInheritance(
    const std::string& derivedName, const std::string& baseName) {
    inheritanceMap_[derivedName].push_back(baseName);
}

template <typename Derived, typename Base>
bool InheritanceBinder<Derived, Base>::isDerivedFrom(
    const std::string& derivedName, const std::string& baseName) {
    auto it = inheritanceMap_.find(derivedName);
    if (it == inheritanceMap_.end())
        return false;

    return std::find(it->second.begin(), it->second.end(), baseName) !=
           it->second.end();
}

template <typename Derived, typename Base>
std::shared_ptr<Derived> InheritanceBinder<Derived, Base>::safeCast(
    std::shared_ptr<Base> basePtr) {
    return std::dynamic_pointer_cast<Derived>(basePtr);
}

// AdvancedModule implementation
template <typename ScriptEngine>
template <typename Func>
AdvancedModule<ScriptEngine>& AdvancedModule<ScriptEngine>::def(
    const std::string& name, Func func, const std::string& doc) {
    std::string fullName = moduleName_ + "." + name;
    ScriptFunction wrapper = callbackManager_.createWrapper(func);

    engine_.registerFunction(fullName, wrapper);
    return *this;
}

template <typename ScriptEngine>
template <typename T>
AdvancedClassBinder<T, ScriptEngine> AdvancedModule<ScriptEngine>::class_(
    const std::string& name, const std::string& doc) {
    std::string fullName = moduleName_ + "." + name;
    return AdvancedClassBinder<T, ScriptEngine>(engine_, fullName);
}

template <typename ScriptEngine>
template <typename T>
AdvancedModule<ScriptEngine>& AdvancedModule<ScriptEngine>::attr(
    const std::string& name, const T& value) {
    std::string fullName = moduleName_ + "." + name;
    engine_.setGlobal(fullName, ScriptValue(value));
    return *this;
}

}  // namespace atom::components::scripting
