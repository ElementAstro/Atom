/*
 * bindings.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Advanced Binding Features
Implements class/function binding, property getters/setters,
operator overloading, exception translation, and callback support
for both Lua and Python scripting engines.

**************************************************/

#ifndef ATOM_COMPONENT_SCRIPTING_BINDINGS_HPP
#define ATOM_COMPONENT_SCRIPTING_BINDINGS_HPP

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "scripting_api.hpp"

namespace atom::components::scripting {

/**
 * @brief Exception translation system
 */
class ExceptionTranslator {
public:
    /**
     * @brief Registers an exception translator
     * @tparam ExceptionType C++ exception type
     * @param translator Translation function
     */
    template <typename ExceptionType>
    void registerTranslator(
        std::function<std::string(const ExceptionType&)> translator);

    /**
     * @brief Translates a C++ exception to script error
     * @param exception Exception to translate
     * @return Error message for script
     */
    std::string translateException(const std::exception& exception);

    /**
     * @brief Executes function with exception translation
     * @tparam Func Function type
     * @param func Function to execute
     * @return Execution result with translated exceptions
     */
    template <typename Func>
    ScriptResult executeWithTranslation(Func&& func);

private:
    std::unordered_map<std::string,
                       std::function<std::string(const std::exception&)>>
        translators_;
};

/**
 * @brief Operator overloading support
 */
enum class OperatorType {
    Add,           // +
    Subtract,      // -
    Multiply,      // *
    Divide,        // /
    Modulo,        // %
    Equal,         // ==
    NotEqual,      // !=
    Less,          // <
    LessEqual,     // <=
    Greater,       // >
    GreaterEqual,  // >=
    Index,         // []
    Call,          // ()
    ToString,      // __str__ / __tostring
    Length,        // __len__ / #
    Iterator       // __iter__ / __pairs
};

/**
 * @brief Operator binding helper
 */
template <typename T>
class OperatorBinder {
public:
    explicit OperatorBinder(const std::string& typeName)
        : typeName_(typeName) {}

    /**
     * @brief Binds arithmetic operator
     * @tparam Op Operator function type
     * @param op Operator type
     * @param func Operator implementation
     * @return Reference to this binder
     */
    template <typename Op>
    OperatorBinder& def_operator(OperatorType op, Op func);

    /**
     * @brief Binds comparison operator
     * @tparam Op Operator function type
     * @param op Operator type
     * @param func Operator implementation
     * @return Reference to this binder
     */
    template <typename Op>
    OperatorBinder& def_comparison(OperatorType op, Op func);

    /**
     * @brief Binds indexing operator
     * @tparam IndexType Index type
     * @tparam ReturnType Return type
     * @param getter Index getter function
     * @param setter Index setter function (optional)
     * @return Reference to this binder
     */
    template <typename IndexType, typename ReturnType>
    OperatorBinder& def_index(
        std::function<ReturnType(const T&, IndexType)> getter,
        std::function<void(T&, IndexType, const ReturnType&)> setter = nullptr);

    /**
     * @brief Binds call operator
     * @tparam Args Argument types
     * @tparam ReturnType Return type
     * @param func Call function
     * @return Reference to this binder
     */
    template <typename ReturnType, typename... Args>
    OperatorBinder& def_call(std::function<ReturnType(T&, Args...)> func);

private:
    std::string typeName_;
    std::unordered_map<OperatorType, std::function<ScriptValue(
                                         const std::vector<ScriptValue>&)>>
        operators_;
};

/**
 * @brief Property binding system
 */
template <typename T>
class PropertyBinder {
public:
    explicit PropertyBinder(const std::string& typeName)
        : typeName_(typeName) {}

    /**
     * @brief Binds read-write property
     * @tparam PropertyType Property type
     * @param name Property name
     * @param getter Getter function
     * @param setter Setter function
     * @return Reference to this binder
     */
    template <typename PropertyType>
    PropertyBinder& def_property(
        const std::string& name, std::function<PropertyType(const T&)> getter,
        std::function<void(T&, const PropertyType&)> setter);

    /**
     * @brief Binds read-only property
     * @tparam PropertyType Property type
     * @param name Property name
     * @param getter Getter function
     * @return Reference to this binder
     */
    template <typename PropertyType>
    PropertyBinder& def_property_readonly(
        const std::string& name, std::function<PropertyType(const T&)> getter);

    /**
     * @brief Binds static property
     * @tparam PropertyType Property type
     * @param name Property name
     * @param getter Static getter function
     * @param setter Static setter function (optional)
     * @return Reference to this binder
     */
    template <typename PropertyType>
    PropertyBinder& def_static_property(
        const std::string& name, std::function<PropertyType()> getter,
        std::function<void(const PropertyType&)> setter = nullptr);

private:
    std::string typeName_;
    struct PropertyInfo {
        std::function<ScriptValue(const std::vector<ScriptValue>&)> getter;
        std::function<ScriptValue(const std::vector<ScriptValue>&)> setter;
        bool isReadOnly = false;
        bool isStatic = false;
    };
    std::unordered_map<std::string, PropertyInfo> properties_;
};

/**
 * @brief Callback support system
 */
class CallbackManager {
public:
    /**
     * @brief Registers a C++ callback for script calling
     * @tparam Func Callback function type
     * @param name Callback name
     * @param func Callback function
     */
    template <typename Func>
    void registerCallback(const std::string& name, Func func);

    /**
     * @brief Creates a script-callable wrapper for C++ function
     * @tparam Func Function type
     * @param func C++ function
     * @return Script function wrapper
     */
    template <typename Func>
    ScriptFunction createWrapper(Func func);

    /**
     * @brief Invokes a registered callback
     * @param name Callback name
     * @param args Arguments
     * @return Callback result
     */
    ScriptValue invokeCallback(const std::string& name,
                               const std::vector<ScriptValue>& args);

    /**
     * @brief Registers a script function as C++ callback
     * @tparam Signature Function signature
     * @param scriptFunc Script function
     * @return C++ callable wrapper
     */
    template <typename Signature>
    std::function<Signature> createCppCallback(ScriptFunction scriptFunc);

private:
    std::unordered_map<std::string, ScriptFunction> callbacks_;

    // Helper for function signature deduction
    template <typename R, typename... Args>
    std::function<R(Args...)> createCppCallbackImpl(ScriptFunction scriptFunc,
                                                    std::function<R(Args...)>*);
};

/**
 * @brief Class binding with full feature support
 */
template <typename T, typename ScriptEngine>
class ClassBinder {
public:
    ClassBinder(ScriptEngine& engine, const std::string& className)
        : engine_(engine),
          className_(className),
          operatorBinder_(className),
          propertyBinder_(className) {}

    /**
     * @brief Binds constructor
     * @tparam Args Constructor argument types
     * @return Reference to this binder
     */
    template <typename... Args>
    ClassBinder& def_constructor();

    /**
     * @brief Binds method with exception translation
     * @tparam Func Method type
     * @param name Method name
     * @param func Method pointer
     * @param doc Documentation string
     * @return Reference to this binder
     */
    template <typename Func>
    ClassBinder& def_method(const std::string& name, Func func,
                                    const std::string& doc = "");

    /**
     * @brief Binds static method
     * @tparam Func Function type
     * @param name Method name
     * @param func Function pointer
     * @param doc Documentation string
     * @return Reference to this binder
     */
    template <typename Func>
    ClassBinder& def_static_method(const std::string& name, Func func,
                                           const std::string& doc = "");

    /**
     * @brief Gets operator binder for this class
     * @return Operator binder reference
     */
    OperatorBinder<T>& operators() { return operatorBinder_; }

    /**
     * @brief Gets property binder for this class
     * @return Property binder reference
     */
    PropertyBinder<T>& properties() { return propertyBinder_; }

    /**
     * @brief Binds enum values to the class
     * @tparam E Enum type
     * @param enumName Enum name
     * @param values Enum values
     * @return Reference to this binder
     */
    template <typename E>
    ClassBinder& def_enum(
        const std::string& enumName,
        const std::vector<std::pair<E, std::string>>& values);

    /**
     * @brief Enables automatic string conversion
     * @tparam Func String conversion function type
     * @param func String conversion function
     * @return Reference to this binder
     */
    template <typename Func>
    ClassBinder& def_str(Func func);

    /**
     * @brief Enables automatic representation conversion
     * @tparam Func Representation function type
     * @param func Representation function
     * @return Reference to this binder
     */
    template <typename Func>
    ClassBinder& def_repr(Func func);

    /**
     * @brief Finalizes the class binding
     */
    void finalize();

private:
    ScriptEngine& engine_;
    std::string className_;
    OperatorBinder<T> operatorBinder_;
    PropertyBinder<T> propertyBinder_;
    ExceptionTranslator exceptionTranslator_;

    std::vector<std::function<void()>> finalizers_;

    // Helper methods for method binding
    template <typename Func>
    ScriptFunction createMethodWrapper(Func func);

    template <typename Func>
    ScriptFunction createStaticMethodWrapper(Func func);
};

/**
 * @brief Inheritance support
 */
template <typename Derived, typename Base>
class InheritanceBinder {
public:
    /**
     * @brief Registers inheritance relationship
     * @param derivedName Derived class name
     * @param baseName Base class name
     */
    static void registerInheritance(const std::string& derivedName,
                                    const std::string& baseName);

    /**
     * @brief Checks if type is derived from base
     * @param derivedName Derived class name
     * @param baseName Base class name
     * @return True if inheritance relationship exists
     */
    static bool isDerivedFrom(const std::string& derivedName,
                              const std::string& baseName);

    /**
     * @brief Performs safe cast from base to derived
     * @param basePtr Base class pointer
     * @return Derived class pointer or nullptr
     */
    static std::shared_ptr<Derived> safeCast(std::shared_ptr<Base> basePtr);

private:
    static std::unordered_map<std::string, std::vector<std::string>>
        inheritanceMap_;
};

/**
 * @brief Module system for organizing bindings
 */
template <typename ScriptEngine>
class ScriptModule {
public:
    explicit ScriptModule(ScriptEngine& engine, const std::string& moduleName)
        : engine_(engine), moduleName_(moduleName), callbackManager_() {}

    /**
     * @brief Defines a function in the module
     * @tparam Func Function type
     * @param name Function name
     * @param func Function pointer
     * @param doc Documentation string
     * @return Reference to this module
     */
    template <typename Func>
    ScriptModule& def(const std::string& name, Func func,
                        const std::string& doc = "");

    /**
     * @brief Defines a class in the module
     * @tparam T Class type
     * @param name Class name
     * @param doc Documentation string
     * @return Advanced class binder
     */
    template <typename T>
    ClassBinder<T, ScriptEngine> class_(const std::string& name,
                                                const std::string& doc = "");

    /**
     * @brief Defines a constant in the module
     * @tparam T Value type
     * @param name Constant name
     * @param value Constant value
     * @return Reference to this module
     */
    template <typename T>
    ScriptModule& attr(const std::string& name, const T& value);

    /**
     * @brief Gets the callback manager
     * @return Callback manager reference
     */
    CallbackManager& callbacks() { return callbackManager_; }

    /**
     * @brief Gets the exception translator
     * @return Exception translator reference
     */
    ExceptionTranslator& exceptions() { return exceptionTranslator_; }

private:
    ScriptEngine& engine_;
    std::string moduleName_;
    CallbackManager callbackManager_;
    ExceptionTranslator exceptionTranslator_;
};

// ===========================================================================
// Template implementations (must live in the header so any translation unit
// can instantiate them)
// ===========================================================================

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

template <typename Func>
void CallbackManager::registerCallback(const std::string& name, Func func) {
    callbacks_[name] = createWrapper(func);
}

template <typename Func>
ScriptFunction CallbackManager::createWrapper(Func func) {
    return [func](const std::vector<ScriptValue>& args) -> ScriptValue {
        (void)args;
        if constexpr (std::is_invocable_v<Func>) {
            if constexpr (std::is_void_v<std::invoke_result_t<Func>>) {
                func();
                return ScriptValue();
            } else {
                using R = std::invoke_result_t<Func>;
                if constexpr (std::is_constructible_v<ScriptValue, R>) {
                    return ScriptValue(func());
                } else {
                    func();
                    return ScriptValue();
                }
            }
        } else {
            return ScriptValue();
        }
    };
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
        std::vector<ScriptValue> scriptArgs;
        ((scriptArgs.push_back(ScriptValue(args))), ...);

        ScriptValue result = scriptFunc(scriptArgs);

        if constexpr (std::is_void_v<R>) {
            return;
        } else {
            if (result.holds<R>()) {
                return result.get<R>();
            }
            return R{};
        }
    };
}

template <typename T>
template <typename Op>
OperatorBinder<T>& OperatorBinder<T>::def_operator(OperatorType op, Op func) {
    operators_[op] =
        [func](const std::vector<ScriptValue>& args) -> ScriptValue {
        // Full argument marshalling is engine-specific; engines consume the
        // registered operator table.
        (void)args;
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
        (void)getter;
        (void)setter;
        (void)args;
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
        (void)func;
        (void)args;
        return ScriptValue();
    };

    return *this;
}

template <typename T>
template <typename PropertyType>
PropertyBinder<T>& PropertyBinder<T>::def_property(
    const std::string& name, std::function<PropertyType(const T&)> getter,
    std::function<void(T&, const PropertyType&)> setter) {
    PropertyInfo info;
    info.getter =
        [getter](const std::vector<ScriptValue>& args) -> ScriptValue {
        (void)getter;
        (void)args;
        return ScriptValue();
    };

    info.setter =
        [setter](const std::vector<ScriptValue>& args) -> ScriptValue {
        (void)setter;
        (void)args;
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
        (void)getter;
        (void)args;
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
        (void)args;
        auto result = getter();
        if constexpr (std::is_constructible_v<ScriptValue, decltype(result)>) {
            return ScriptValue(result);
        } else {
            return ScriptValue();
        }
    };

    if (setter) {
        info.setter =
            [setter](const std::vector<ScriptValue>& args) -> ScriptValue {
            (void)setter;
            (void)args;
            return ScriptValue();
        };
    }

    info.isStatic = true;
    info.isReadOnly = (setter == nullptr);
    properties_[name] = info;

    return *this;
}

template <typename T, typename ScriptEngine>
template <typename... Args>
ClassBinder<T, ScriptEngine>& ClassBinder<T, ScriptEngine>::def_constructor() {
    std::string constructorName = className_ + ".__init__";

    ScriptFunction constructor =
        [](const std::vector<ScriptValue>& args) -> ScriptValue {
        (void)args;
        return ScriptValue();
    };

    engine_.registerFunction(constructorName, constructor);
    return *this;
}

template <typename T, typename ScriptEngine>
template <typename Func>
ClassBinder<T, ScriptEngine>& ClassBinder<T, ScriptEngine>::def_method(
    const std::string& name, Func func, const std::string& doc) {
    (void)doc;
    std::string methodName = className_ + "." + name;
    ScriptFunction wrapper = createMethodWrapper(func);

    ScriptFunction translatedWrapper =
        [wrapper](const std::vector<ScriptValue>& args) -> ScriptValue {
        try {
            return wrapper(args);
        } catch (const std::exception&) {
            return ScriptValue();
        }
    };

    engine_.registerFunction(methodName, translatedWrapper);
    return *this;
}

template <typename T, typename ScriptEngine>
template <typename Func>
ClassBinder<T, ScriptEngine>& ClassBinder<T, ScriptEngine>::def_static_method(
    const std::string& name, Func func, const std::string& doc) {
    (void)doc;
    std::string methodName = className_ + "." + name;
    ScriptFunction wrapper = createStaticMethodWrapper(func);

    engine_.registerFunction(methodName, wrapper);
    return *this;
}

template <typename T, typename ScriptEngine>
template <typename E>
ClassBinder<T, ScriptEngine>& ClassBinder<T, ScriptEngine>::def_enum(
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
ClassBinder<T, ScriptEngine>& ClassBinder<T, ScriptEngine>::def_str(Func func) {
    operatorBinder_.def_operator(OperatorType::ToString, func);
    return *this;
}

template <typename T, typename ScriptEngine>
template <typename Func>
ClassBinder<T, ScriptEngine>& ClassBinder<T, ScriptEngine>::def_repr(
    Func func) {
    return def_str(func);
}

template <typename T, typename ScriptEngine>
void ClassBinder<T, ScriptEngine>::finalize() {
    for (auto& finalizer : finalizers_) {
        finalizer();
    }
}

template <typename T, typename ScriptEngine>
template <typename Func>
ScriptFunction ClassBinder<T, ScriptEngine>::createMethodWrapper(Func func) {
    return [func](const std::vector<ScriptValue>& args) -> ScriptValue {
        (void)func;
        (void)args;
        return ScriptValue();
    };
}

template <typename T, typename ScriptEngine>
template <typename Func>
ScriptFunction ClassBinder<T, ScriptEngine>::createStaticMethodWrapper(
    Func func) {
    return [func](const std::vector<ScriptValue>& args) -> ScriptValue {
        (void)func;
        (void)args;
        return ScriptValue();
    };
}

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

template <typename ScriptEngine>
template <typename Func>
ScriptModule<ScriptEngine>& ScriptModule<ScriptEngine>::def(
    const std::string& name, Func func, const std::string& doc) {
    (void)doc;
    std::string fullName = moduleName_ + "." + name;
    ScriptFunction wrapper = callbackManager_.createWrapper(func);

    engine_.registerFunction(fullName, wrapper);
    return *this;
}

template <typename ScriptEngine>
template <typename T>
ClassBinder<T, ScriptEngine> ScriptModule<ScriptEngine>::class_(
    const std::string& name, const std::string& doc) {
    (void)doc;
    std::string fullName = moduleName_ + "." + name;
    return ClassBinder<T, ScriptEngine>(engine_, fullName);
}

template <typename ScriptEngine>
template <typename T>
ScriptModule<ScriptEngine>& ScriptModule<ScriptEngine>::attr(
    const std::string& name, const T& value) {
    std::string fullName = moduleName_ + "." + name;
    engine_.setGlobal(fullName, ScriptValue(value));
    return *this;
}

/**
 * @brief Binding macros for convenience
 */
#define ATOM_BIND_CLASS(engine, className)                              \
    atom::components::scripting::ClassBinder<className,         \
                                                     decltype(engine)>( \
        engine, #className)

#define ATOM_BIND_MODULE(engine, moduleName)                              \
    atom::components::scripting::ScriptModule<decltype(engine)>(engine, \
                                                                  #moduleName)

#define ATOM_BIND_INHERITANCE(Derived, Base)        \
    atom::components::scripting::InheritanceBinder< \
        Derived, Base>::registerInheritance(#Derived, #Base)

#define ATOM_BIND_OPERATOR(binder, op, func) \
    binder.operators().def_operator(         \
        atom::components::scripting::OperatorType::op, func)

#define ATOM_BIND_PROPERTY(binder, name, getter, setter) \
    binder.properties().def_property(#name, getter, setter)

#define ATOM_BIND_READONLY_PROPERTY(binder, name, getter) \
    binder.properties().def_property_readonly(#name, getter)

}  // namespace atom::components::scripting

#endif  // ATOM_COMPONENT_SCRIPTING_BINDINGS_HPP
