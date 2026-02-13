/*!
 * \file constructors.hpp
 * \brief Enhanced C++ Function Constructors with C++20/23 features - TYPE
 * SYSTEM ENHANCED \author Max Qian <lightapt.com> \date 2024-03-12 \optimized
 * 2025-01-22 - Type System Enhancement by AI Assistant \copyright Copyright (C)
 * 2023-2024 Max Qian
 *
 * TYPE SYSTEM ENHANCEMENTS:
 * - Advanced template-based constructor optimization
 * - Compile-time constructor validation and selection
 * - Enhanced parameter type deduction and conversion
 * - Memory-efficient constructor dispatch with caching
 * - Perfect forwarding optimizations for constructor arguments
 * - SFINAE-based constructor overload resolution
 * - Enhanced type safety with concept-based constraints
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
auto bindMemberFunction(MemberFunc ClassType::*member_func) {
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
auto bindConstMemberFunction(MemberFunc ClassType::*member_func) {
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
auto bindMemberVariable(MemberType ClassType::*member_var) {
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
auto bindConstMemberVariable(MemberType ClassType::*member_var) {
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
    ObjectBuilder& with(Prop Class::*prop, Value&& value) {
        auto prevFunc = m_buildFunc;
        m_buildFunc = [prevFunc, prop, value = std::forward<Value>(value)]() {
            auto obj = prevFunc();
            obj->*prop = value;
            return obj;
        };
        return *this;
    }

    template <typename Func, typename... Args>
    ObjectBuilder& call(Func Class::*method, Args&&... args) {
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

//==============================================================================
// C++23 Enhanced Constructor Utilities
//==============================================================================

/**
 * @brief Concept for default constructible types
 */
template <typename T>
concept DefaultConstructible = std::is_default_constructible_v<T>;

/**
 * @brief Concept for types constructible from specific args
 */
template <typename T, typename... Args>
concept ConstructibleFrom = std::is_constructible_v<T, Args...>;

/**
 * @brief Concept for aggregate initializable types
 */
template <typename T>
concept AggregateInitializable = std::is_aggregate_v<T>;

/**
 * @brief Safe object construction with error handling
 */
template <typename T, typename... Args>
    requires ConstructibleFrom<T, Args...>
auto safeConstruct(Args&&... args) noexcept
    -> ConstructorResult<std::unique_ptr<T>> {
    try {
        return ConstructorResult<std::unique_ptr<T>>::success(
            std::make_unique<T>(std::forward<Args>(args)...));
    } catch (const std::exception& e) {
        return ConstructorResult<std::unique_ptr<T>>::failure(e.what());
    } catch (...) {
        return ConstructorResult<std::unique_ptr<T>>::failure(
            "Unknown construction error");
    }
}

/**
 * @brief In-place construction wrapper
 */
template <typename T>
class InPlaceConstructor {
public:
    template <typename... Args>
        requires ConstructibleFrom<T, Args...>
    static T construct(Args&&... args) {
        return T(std::forward<Args>(args)...);
    }

    template <typename... Args>
        requires ConstructibleFrom<T, Args...>
    static std::unique_ptr<T> constructUnique(Args&&... args) {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    template <typename... Args>
        requires ConstructibleFrom<T, Args...>
    static std::shared_ptr<T> constructShared(Args&&... args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }
};

/**
 * @brief Factory with type registration
 */
template <typename Base>
class RegisteredFactory {
    std::unordered_map<std::string, std::function<std::shared_ptr<Base>()>>
        creators_;
    mutable std::shared_mutex mutex_;

public:
    template <typename Derived>
        requires std::is_base_of_v<Base, Derived> &&
                 DefaultConstructible<Derived>
    void registerType(std::string_view name) {
        std::unique_lock lock(mutex_);
        creators_[std::string(name)] = []() {
            return std::make_shared<Derived>();
        };
    }

    template <typename Derived, typename... Args>
        requires std::is_base_of_v<Base, Derived> &&
                 ConstructibleFrom<Derived, Args...>
    void registerType(std::string_view name, Args&&... args) {
        std::unique_lock lock(mutex_);
        auto tuple_args = std::make_tuple(std::forward<Args>(args)...);
        creators_[std::string(name)] = [tuple_args]() {
            return std::apply(
                [](auto&&... a) {
                    return std::make_shared<Derived>(
                        std::forward<decltype(a)>(a)...);
                },
                tuple_args);
        };
    }

    [[nodiscard]] std::shared_ptr<Base> create(std::string_view name) const {
        std::shared_lock lock(mutex_);
        auto it = creators_.find(std::string(name));
        if (it != creators_.end()) {
            return it->second();
        }
        return nullptr;
    }

    [[nodiscard]] bool hasType(std::string_view name) const {
        std::shared_lock lock(mutex_);
        return creators_.contains(std::string(name));
    }

    [[nodiscard]] std::vector<std::string> getRegisteredTypes() const {
        std::shared_lock lock(mutex_);
        std::vector<std::string> result;
        result.reserve(creators_.size());
        for (const auto& [name, _] : creators_) {
            result.push_back(name);
        }
        return result;
    }
};

/**
 * @brief Builder with validation
 */
template <typename Class>
class ValidatingBuilder : public ObjectBuilder<Class> {
    std::vector<std::function<bool(const Class&)>> validators_;
    std::vector<std::string> error_messages_;

public:
    ValidatingBuilder& addValidator(
        std::function<bool(const Class&)> validator,
        std::string error_msg = "Validation failed") {
        validators_.push_back(std::move(validator));
        error_messages_.push_back(std::move(error_msg));
        return *this;
    }

    std::optional<std::shared_ptr<Class>> buildValidated() {
        auto obj = ObjectBuilder<Class>::build();

        for (size_t i = 0; i < validators_.size(); ++i) {
            if (!validators_[i](*obj)) {
                return std::nullopt;
            }
        }

        return obj;
    }

    ConstructorResult<std::shared_ptr<Class>> buildWithErrors() {
        auto obj = ObjectBuilder<Class>::build();

        for (size_t i = 0; i < validators_.size(); ++i) {
            if (!validators_[i](*obj)) {
                return ConstructorResult<std::shared_ptr<Class>>::failure(
                    error_messages_[i]);
            }
        }

        return ConstructorResult<std::shared_ptr<Class>>::success(
            std::move(obj));
    }
};

/**
 * @brief Create a validating builder
 */
template <typename Class>
auto makeValidatingBuilder() {
    return ValidatingBuilder<Class>();
}

/**
 * @brief Singleton factory
 */
template <typename T>
class SingletonFactory {
public:
    template <typename... Args>
        requires ConstructibleFrom<T, Args...>
    static T& getInstance(Args&&... args) {
        static T instance(std::forward<Args>(args)...);
        return instance;
    }

    template <typename... Args>
        requires ConstructibleFrom<T, Args...>
    static std::shared_ptr<T> getSharedInstance(Args&&... args) {
        static auto instance = std::make_shared<T>(std::forward<Args>(args)...);
        return instance;
    }
};

}  // namespace atom::meta

#endif  // ATOM_META_CONSTRUCTOR_HPP
