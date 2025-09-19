/*
 * advanced_bindings.hpp
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

#ifndef ATOM_COMPONENT_ADVANCED_BINDINGS_HPP
#define ATOM_COMPONENT_ADVANCED_BINDINGS_HPP

#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "scripting_api.hpp"
#include "../data/type_conversion.hpp"

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
 * @brief Advanced class binding with full feature support
 */
template <typename T, typename ScriptEngine>
class AdvancedClassBinder {
public:
    AdvancedClassBinder(ScriptEngine& engine, const std::string& className)
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
    AdvancedClassBinder& def_constructor();

    /**
     * @brief Binds method with exception translation
     * @tparam Func Method type
     * @param name Method name
     * @param func Method pointer
     * @param doc Documentation string
     * @return Reference to this binder
     */
    template <typename Func>
    AdvancedClassBinder& def_method(const std::string& name, Func func,
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
    AdvancedClassBinder& def_static_method(const std::string& name, Func func,
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
    AdvancedClassBinder& def_enum(
        const std::string& enumName,
        const std::vector<std::pair<E, std::string>>& values);

    /**
     * @brief Enables automatic string conversion
     * @tparam Func String conversion function type
     * @param func String conversion function
     * @return Reference to this binder
     */
    template <typename Func>
    AdvancedClassBinder& def_str(Func func);

    /**
     * @brief Enables automatic representation conversion
     * @tparam Func Representation function type
     * @param func Representation function
     * @return Reference to this binder
     */
    template <typename Func>
    AdvancedClassBinder& def_repr(Func func);

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
class AdvancedModule {
public:
    explicit AdvancedModule(ScriptEngine& engine, const std::string& moduleName)
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
    AdvancedModule& def(const std::string& name, Func func,
                        const std::string& doc = "");

    /**
     * @brief Defines a class in the module
     * @tparam T Class type
     * @param name Class name
     * @param doc Documentation string
     * @return Advanced class binder
     */
    template <typename T>
    AdvancedClassBinder<T, ScriptEngine> class_(const std::string& name,
                                                const std::string& doc = "");

    /**
     * @brief Defines a constant in the module
     * @tparam T Value type
     * @param name Constant name
     * @param value Constant value
     * @return Reference to this module
     */
    template <typename T>
    AdvancedModule& attr(const std::string& name, const T& value);

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

/**
 * @brief Binding macros for convenience
 */
#define ATOM_BIND_CLASS(engine, className)                              \
    atom::components::scripting::AdvancedClassBinder<className,         \
                                                     decltype(engine)>( \
        engine, #className)

#define ATOM_BIND_MODULE(engine, moduleName)                              \
    atom::components::scripting::AdvancedModule<decltype(engine)>(engine, \
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

#endif  // ATOM_COMPONENT_ADVANCED_BINDINGS_HPP
