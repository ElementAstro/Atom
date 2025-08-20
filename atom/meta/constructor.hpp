/*!
 * \file constructors.hpp
 * \brief Enhanced C++ Function Constructors with C++20/23 features
 * \author Max Qian <lightapt.com>
 * \date 2024-03-12
 * \copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef ATOM_META_CONSTRUCTOR_HPP
#define ATOM_META_CONSTRUCTOR_HPP

#include <atomic>
#include <concepts>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>
#include "atom/type/expected.hpp"

#if __has_include(<expected>) && __cplusplus > 202002L
#include <expected>
#define HAS_EXPECTED 1
#else
#define HAS_EXPECTED 0
#endif

#include "atom/error/exception.hpp"
#include "func_traits.hpp"

namespace atom::meta {

// Forward declarations
template <typename T>
struct ConstructorResult;

// ==================== Concept Definitions ====================

/**
 * @brief Concept for types that are move constructible
 */
template <typename T>
concept MoveConstructible = std::is_move_constructible_v<T>;

/**
 * @brief Concept for member function pointers
 */
template <typename T, typename Class>
concept MemberFunctionPtr = std::is_member_function_pointer_v<T Class::*>;

/**
 * @brief Concept for member variable pointers
 */
template <typename T, typename Class>
concept MemberVariablePtr = std::is_member_object_pointer_v<T Class::*>;

// ==================== Exception-Safe Result Type ====================

/**
 * @brief Generic constructor result wrapper that can hold a value or an error
 * @tparam T The constructed type
 */
template <typename T>
struct ConstructorResult {
    std::optional<T> value;
    std::optional<std::string> error;

    /**
     * @brief Check if the construction was successful
     */
    [[nodiscard]] bool isValid() const noexcept { return value.has_value(); }

    /**
     * @brief Get the underlying value or throw an exception if not valid
     * @throws atom::Exception if no value is present
     */
    T& getValue() {
        if (!value) {
            THROW_INVALID_ARGUMENT(error.value_or("Construction failed"));
        }
        return *value;
    }

    /**
     * @brief Get the underlying value or throw an exception if not valid
     * @throws atom::Exception if no value is present
     */
    const T& getValue() const {
        if (!value) {
            THROW_INVALID_ARGUMENT(error.value_or("Construction failed"));
        }
        return *value;
    }

    /**
     * @brief Create a success result
     */
    static ConstructorResult<T> success(T&& val) {
        ConstructorResult<T> result;
        result.value = std::move(val);
        return result;
    }

    /**
     * @brief Create an error result
     */
    static ConstructorResult<T> getError(std::string message) {
        ConstructorResult<T> result;
        result.error = std::move(message);
        return result;
    }
};

#if HAS_EXPECTED
// C++23 version with std::expected
template <typename T>
using SafeConstructorResult = type::expected<T, std::string>;
#else
template <typename T>
using SafeConstructorResult = ConstructorResult<T>;
#endif

// ==================== Enhanced Function Binding Utilities ====================

/**
 * @brief Binds a member function to an object with improved type safety
 * @tparam MemberFunc Type of the member function
 * @tparam ClassType Type of the class
 * @param member_func Pointer to the member function
 * @return A lambda that binds the member function to an object with perfect
 * forwarding
 */
template <typename MemberFunc, typename ClassType>
    requires std::is_member_function_pointer_v<MemberFunc ClassType::*>
auto bindMemberFunction(MemberFunc ClassType::* member_func) {
    return [member_func](ClassType& obj, auto&&... params) -> decltype(auto) {
        // Use std::invoke for more uniform function calling
        return std::invoke(member_func, obj,
                           std::forward<decltype(params)>(params)...);
    };
}

/**
 * @brief Binds a const member function to an object
 * @tparam MemberFunc Type of the member function
 * @tparam ClassType Type of the class
 * @param member_func Pointer to the member function
 * @return A lambda that binds the const member function to an object
 */
template <typename MemberFunc, typename ClassType>
    requires std::is_member_function_pointer_v<MemberFunc ClassType::*>
auto bindConstMemberFunction(MemberFunc ClassType::* member_func) {
    return [member_func](const ClassType& obj,
                         auto&&... params) -> decltype(auto) {
        // Always use as const
        return std::invoke(member_func, obj,
                           std::forward<decltype(params)>(params)...);
    };
}

/**
 * @brief Binds a static function with improved type checking
 * @tparam Func Type of the function
 * @param func The static function
 * @return A wrapper that ensures proper forwarding
 */
template <typename Func>
    requires std::is_function_v<std::remove_pointer_t<std::decay_t<Func>>> ||
             std::is_invocable_v<Func>
auto bindStaticFunction(Func&& func) {
    return [func = std::forward<Func>(func)](auto&&... args) -> decltype(auto) {
        return std::invoke(func, std::forward<decltype(args)>(args)...);
    };
}

/**
 * @brief Binds a member variable to an object with improved const correctness
 * @tparam MemberType Type of the member variable
 * @tparam ClassType Type of the class
 * @param member_var Pointer to the member variable
 * @return A lambda that provides access to the member variable
 */
template <typename MemberType, typename ClassType>
    requires std::is_member_object_pointer_v<MemberType ClassType::*>
auto bindMemberVariable(MemberType ClassType::* member_var) {
    return [member_var](ClassType& instance) -> MemberType& {
        return instance.*member_var;
    };
}

/**
 * @brief Binds a const member variable to an object
 * @tparam MemberType Type of the member variable
 * @tparam ClassType Type of the class
 * @param member_var Pointer to the member variable
 * @return A lambda that provides const access to the member variable
 */
template <typename MemberType, typename ClassType>
    requires std::is_member_object_pointer_v<MemberType ClassType::*>
auto bindConstMemberVariable(MemberType ClassType::* member_var) {
    return [member_var](const ClassType& instance) -> const MemberType& {
        return instance.*member_var;
    };
}

// ==================== Constructor Utilities ====================

/**
 * @brief Builds an exception-safe shared constructor for a class
 * @tparam Class Type of the class
 * @tparam Params Types of the constructor parameters
 * @return A lambda that safely constructs a shared pointer to the class
 */
template <typename Class, typename... Params>
auto buildSafeSharedConstructor(Class (* /*unused*/)(Params...)) {
    return
        [](auto&&... params) -> SafeConstructorResult<std::shared_ptr<Class>> {
            try {
                return SafeConstructorResult<std::shared_ptr<Class>>::success(
                    std::make_shared<Class>(
                        std::forward<decltype(params)>(params)...));
            } catch (const std::exception& e) {
                return SafeConstructorResult<std::shared_ptr<Class>>::error(
                    std::string("Failed to construct shared object: ") +
                    e.what());
            } catch (...) {
                return SafeConstructorResult<std::shared_ptr<Class>>::error(
                    "Unknown error during shared construction");
            }
        };
}

/**
 * @brief Builds a shared constructor for a class with validation
 * @tparam Class Type of the class
 * @tparam Params Types of the constructor parameters
 * @param validator Function to validate parameters before construction
 * @return A lambda that constructs a shared pointer to the class after
 * validation
 */
template <typename Class, typename... Params, typename Validator>
    requires std::invocable<Validator, Params...>
auto buildValidatedSharedConstructor(Validator&& validator) {
    return [validator = std::forward<Validator>(validator)](auto&&... params)
               -> SafeConstructorResult<std::shared_ptr<Class>> {
        try {
            // Validate parameters first
            if (!std::invoke(validator,
                             std::forward<decltype(params)>(params)...)) {
                return SafeConstructorResult<std::shared_ptr<Class>>::error(
                    "Parameter validation failed");
            }

            return SafeConstructorResult<std::shared_ptr<Class>>::success(
                std::make_shared<Class>(
                    std::forward<decltype(params)>(params)...));
        } catch (const std::exception& e) {
            return SafeConstructorResult<std::shared_ptr<Class>>::error(
                std::string("Failed to construct shared object: ") + e.what());
        }
    };
}

/**
 * @brief Builds a shared constructor for a class
 * @tparam Class Type of the class
 * @tparam Params Types of the constructor parameters
 * @return A lambda that constructs a shared pointer to the class
 */
template <typename Class, typename... Params>
auto buildSharedConstructor(Class (* /*unused*/)(Params...)) {
    return [](auto&&... params) {
        return std::make_shared<Class>(
            std::forward<decltype(params)>(params)...);
    };
}

/**
 * @brief Builds a copy constructor for a class
 * @tparam Class Type of the class
 * @tparam Params Types of the constructor parameters
 * @return A lambda that constructs an instance of the class
 */
template <typename Class, typename... Params>
auto buildCopyConstructor(Class (* /*unused*/)(Params...)) {
    return [](auto&&... params) {
        return Class(std::forward<decltype(params)>(params)...);
    };
}

/**
 * @brief Builds a plain constructor for a class
 * @tparam Class Type of the class
 * @tparam Params Types of the constructor parameters
 * @return A lambda that constructs an instance of the class
 */
template <typename Class, typename... Params>
auto buildPlainConstructor(Class (* /*unused*/)(Params...)) {
    return [](auto&&... params) {
        return Class(std::forward<decltype(params)>(params)...);
    };
}

/**
 * @brief Builds a constructor for a class with specified arguments
 * @tparam Class Type of the class
 * @tparam Args Types of the constructor arguments
 * @return A lambda that constructs a shared pointer to the class
 */
template <typename Class, typename... Args>
auto buildConstructor() {
    return [](Args... args) -> std::shared_ptr<Class> {
        return std::make_shared<Class>(std::forward<Args>(args)...);
    };
}

/**
 * @brief Builds a default constructor for a class
 * @tparam Class Type of the class
 * @return A lambda that constructs an instance of the class
 */
template <typename Class>
    requires DefaultConstructible<Class>
auto buildDefaultConstructor() {
    return []() { return Class(); };
}

/**
 * @brief Constructs an instance of a class based on its traits
 * @tparam T Type of the function
 * @return A lambda that constructs an instance of the class
 */
template <typename T>
auto constructor() {
    T* func = nullptr;
    using ClassType = typename FunctionTraits<T>::class_type;

    if constexpr (!std::is_copy_constructible_v<ClassType>) {
        return buildSharedConstructor(func);
    } else {
        return buildCopyConstructor(func);
    }
}

/**
 * @brief Constructs an instance of a class with specified arguments
 * @tparam Class Type of the class
 * @tparam Args Types of the constructor arguments
 * @return A lambda that constructs a shared pointer to the class
 */
template <typename Class, typename... Args>
auto constructor() {
    return buildConstructor<Class, Args...>();
}

/**
 * @brief Constructs an instance of a class using the default constructor
 * @tparam Class Type of the class
 * @return A lambda that constructs an instance of the class
 * @throws Exception if the class is not default constructible
 */
template <typename Class>
auto defaultConstructor() {
    if constexpr (std::is_default_constructible_v<Class>) {
        return buildDefaultConstructor<Class>();
    } else {
        THROW_NOT_FOUND("Class is not default constructible");
    }
}

/**
 * @brief Constructs an instance of a class using a move constructor
 * @tparam Class Type of the class
 * @return A lambda that constructs an instance of the class using a move
 * constructor
 */
template <typename Class>
    requires MoveConstructible<Class>
auto buildMoveConstructor() {
    return [](Class&& instance) { return Class(std::move(instance)); };
}

/**
 * @brief Constructs an instance of a class using an initializer list
 * @tparam Class Type of the class
 * @tparam T Type of the elements in the initializer list
 * @return A lambda that constructs an instance of the class using an
 * initializer list
 */
template <typename Class, typename T>
    requires requires(std::initializer_list<T> init) { Class(init); }
auto buildInitializerListConstructor() {
    return [](std::initializer_list<T> init_list) { return Class(init_list); };
}

/**
 * @brief Constructs an instance of a class asynchronously with configurable
 * launch policy
 * @tparam Class Type of the class
 * @tparam Args Types of the constructor arguments
 * @param policy The std::launch policy to use (default: async)
 * @return A future that constructs an instance of the class
 */
template <typename Class, typename... Args>
auto asyncConstructor(std::launch policy = std::launch::async) {
    return [policy](Args... args) -> std::future<std::shared_ptr<Class>> {
        return std::async(
            policy,
            [](Args... innerArgs) {
                return std::make_shared<Class>(
                    std::forward<Args>(innerArgs)...);
            },
            std::forward<Args>(args)...);
    };
}

/**
 * @brief Thread-safe singleton constructor with customizable construction and
 * destruction policies
 * @tparam Class Type of the class
 * @tparam ConstructPolicy Policy for construction (lazy or eager)
 * @return A lambda that returns the singleton instance
 */
template <typename Class, bool ThreadSafe = true>
    requires DefaultConstructible<Class>
auto singletonConstructor() {
    if constexpr (ThreadSafe) {
        // Thread-safe implementation using double-checked locking
        return []() -> std::shared_ptr<Class> {
            static std::mutex instanceMutex;
            static std::atomic<std::shared_ptr<Class>> instance{nullptr};

            auto currentInstance = instance.load(std::memory_order_acquire);
            if (!currentInstance) {
                std::lock_guard<std::mutex> lock(instanceMutex);
                currentInstance = instance.load(std::memory_order_relaxed);
                if (!currentInstance) {
                    currentInstance = std::make_shared<Class>();
                    instance.store(currentInstance, std::memory_order_release);
                }
            }
            return currentInstance;
        };
    } else {
        // Non-thread-safe but more efficient implementation
        return []() -> std::shared_ptr<Class> {
            static std::shared_ptr<Class> instance = std::make_shared<Class>();
            return instance;
        };
    }
}

/**
 * @brief Constructs an instance of a class using a custom constructor with
 * error handling
 * @tparam Class Type of the class
 * @tparam CustomConstructor Type of the custom constructor
 * @param custom_constructor The custom constructor function
 * @return A lambda that safely constructs an instance of the class
 */
template <typename Class, typename CustomConstructor>
auto safeCustomConstructor(CustomConstructor&& custom_constructor) {
    return [constructor = std::forward<CustomConstructor>(custom_constructor)](
               auto&&... args) -> SafeConstructorResult<Class> {
        try {
            return SafeConstructorResult<Class>::success(
                constructor(std::forward<decltype(args)>(args)...));
        } catch (const std::exception& e) {
            return SafeConstructorResult<Class>::error(
                std::string("Custom construction failed: ") + e.what());
        } catch (...) {
            return SafeConstructorResult<Class>::error(
                "Unknown error in custom constructor");
        }
    };
}

/**
 * @brief Constructs an instance of a class using a custom constructor
 * @tparam Class Type of the class
 * @tparam CustomConstructor Type of the custom constructor
 * @param custom_constructor The custom constructor function
 * @return A lambda that constructs an instance of the class
 */
template <typename Class, typename CustomConstructor>
auto customConstructor(CustomConstructor&& custom_constructor) {
    return [constructor = std::forward<CustomConstructor>(custom_constructor)](
               auto&&... args) -> decltype(auto) {
        return constructor(std::forward<decltype(args)>(args)...);
    };
}

/**
 * @brief Lazy constructor that defers creation until first access
 * @tparam Class Type of the class
 * @tparam Args Types of the constructor arguments
 * @return A function that lazily constructs and returns the class instance
 */
template <typename Class, typename... Args>
auto lazyConstructor() {
    return [](Args... args) {
        // Use optional to allow deferred construction
        static thread_local std::optional<Class> instance;

        if (!instance) {
            instance.emplace(std::forward<Args>(args)...);
        }

        return *instance;
    };
}

/**
 * @brief Factory function that selects the appropriate constructor based on
 * arguments
 * @tparam Class Type of the class
 * @return A function that constructs the class using the best matching
 * constructor
 */
template <typename Class>
auto factoryConstructor() {
    return [](auto&&... args) -> std::shared_ptr<Class> {
        // using ArgsTuple = std::tuple<std::decay_t<decltype(args)>...>;

        if constexpr (std::is_constructible_v<Class, decltype(args)...>) {
            // Direct construction if arguments match
            return std::make_shared<Class>(
                std::forward<decltype(args)>(args)...);
        } else if constexpr (sizeof...(args) == 0 &&
                             std::is_default_constructible_v<Class>) {
            // Default construction if no arguments provided
            return std::make_shared<Class>();
        } else {
            // Fail at compile time with a clear error message
            static_assert(
                std::is_constructible_v<Class, decltype(args)...>,
                "No suitable constructor available for the provided arguments");
            return nullptr;  // Never reached, just to satisfy the compiler
        }
    };
}

/**
 * @brief Creates a builder pattern for constructing objects step by step
 * @tparam Class Type of the class to build
 * @return A builder object that allows setting properties before construction
 */
template <typename Class>
class ObjectBuilder {
private:
    std::function<std::shared_ptr<Class>()> m_buildFunc;

public:
    ObjectBuilder() : m_buildFunc([]() { return std::make_shared<Class>(); }) {}

    template <typename Prop, typename Value>
    ObjectBuilder& with(Prop Class::* prop, Value&& value) {
        auto prevFunc = m_buildFunc;
        m_buildFunc = [prevFunc, prop, value = std::forward<Value>(value)]() {
            auto obj = prevFunc();
            obj->*prop = value;
            return obj;
        };
        return *this;
    }

    template <typename Func, typename... Args>
    ObjectBuilder& call(Func Class::* method, Args&&... args) {
        auto prevFunc = m_buildFunc;
        m_buildFunc = [prevFunc, method,
                       args = std::make_tuple(std::forward<Args>(args)...)]() {
            auto obj = prevFunc();
            std::apply(
                [&obj, method](auto&&... callArgs) {
                    std::invoke(method, *obj,
                                std::forward<decltype(callArgs)>(callArgs)...);
                },
                args);
            return obj;
        };
        return *this;
    }

    std::shared_ptr<Class> build() { return m_buildFunc(); }
};

/**
 * @brief Creates a builder for step-by-step object construction
 * @tparam Class Type of the class to build
 * @return A builder object for the specified class
 */
template <typename Class>
auto makeBuilder() {
    return ObjectBuilder<Class>();
}

}  // namespace atom::meta

#endif  // ATOM_META_CONSTRUCTOR_HPP
