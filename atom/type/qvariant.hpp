#ifndef ATOM_TYPE_QVARIANT_HPP
#define ATOM_TYPE_QVARIANT_HPP

#include <exception>
#include <format>
#include <iostream>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <variant>

namespace atom::type {

/**
 * @brief Custom exception class for variant operations
 */
class VariantException : public std::runtime_error {
public:
    explicit VariantException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief A wrapper class for std::variant with additional utility functions.
 *
 * @tparam Types The types that the variant can hold.
 */
template <typename... Types>
class VariantWrapper {
public:
    using VariantType = std::variant<std::monostate, Types...>;

    /**
     * @brief Default constructor.
     */
    constexpr VariantWrapper() noexcept = default;

    /**
     * @brief Constructs from another VariantWrapper with different types.
     */
    template <typename... OtherTypes>
    explicit VariantWrapper(
        const VariantWrapper<OtherTypes...>&
            other) noexcept(std::
                                is_nothrow_copy_constructible_v<std::variant<
                                    std::monostate, OtherTypes...>>);

    /**
     * @brief Constructs a VariantWrapper with an initial value.
     *
     * @tparam T The type of the initial value.
     * @param value The initial value to store in the variant.
     */
    template <typename T>
    explicit VariantWrapper(T&& value) noexcept(
        std::is_nothrow_constructible_v<VariantType, T>);

    /**
     * @brief Copy constructor.
     */
    VariantWrapper(const VariantWrapper& other) noexcept(
        std::is_nothrow_copy_constructible_v<VariantType>);

    /**
     * @brief Move constructor.
     */
    VariantWrapper(VariantWrapper&& other) noexcept;

    /**
     * @brief Copy assignment operator.
     */
    auto operator=(const VariantWrapper& other) noexcept(
        std::is_nothrow_copy_assignable_v<VariantType>) -> VariantWrapper&;

    /**
     * @brief Move assignment operator.
     */
    auto operator=(VariantWrapper&& other) noexcept -> VariantWrapper&;

    /**
     * @brief Assignment operator for a value.
     */
    template <typename T>
    auto operator=(T&& value) noexcept(
        std::is_nothrow_assignable_v<VariantType, T>) -> VariantWrapper&;

    /**
     * @brief Gets the name of the type currently held by the variant.
     */
    [[nodiscard]] auto typeName() const -> std::string;

    /**
     * @brief Gets the value of the specified type from the variant.
     *
     * @throws VariantException if the variant does not hold the specified type.
     */
    template <typename T>
    [[nodiscard]] auto get() const -> T;

    /**
     * @brief Checks if the variant holds the specified type.
     */
    template <typename T>
    [[nodiscard]] constexpr auto is() const noexcept -> bool;

    /**
     * @brief Prints the current value of the variant to the standard output.
     */
    void print() const;

    /**
     * @brief Equality operator.
     */
    [[nodiscard]] constexpr auto operator==(
        const VariantWrapper& other) const noexcept -> bool;

    /**
     * @brief Inequality operator.
     */
    [[nodiscard]] constexpr auto operator!=(
        const VariantWrapper& other) const noexcept -> bool;

    /**
     * @brief Visits the variant with a visitor.
     */
    template <typename Visitor>
    [[nodiscard]] auto visit(Visitor&& visitor) const -> decltype(auto);

    /**
     * @brief Gets the index of the currently held type in the variant.
     */
    [[nodiscard]] constexpr auto index() const noexcept -> std::size_t;

    /**
     * @brief Tries to get the value of the specified type from the variant.
     */
    template <typename T>
    [[nodiscard]] auto tryGet() const noexcept -> std::optional<T>;

    /**
     * @brief Tries to convert the current value to an int.
     */
    [[nodiscard]] auto toInt() const noexcept -> std::optional<int>;

    /**
     * @brief Tries to convert the current value to a double.
     */
    [[nodiscard]] auto toDouble() const noexcept -> std::optional<double>;

    /**
     * @brief Tries to convert the current value to a bool.
     */
    [[nodiscard]] auto toBool() const noexcept -> std::optional<bool>;

    /**
     * @brief Converts the current value to a string.
     */
    [[nodiscard]] auto toString() const -> std::string;

    /**
     * @brief Resets the variant to hold std::monostate.
     */
    constexpr void reset() noexcept;

    /**
     * @brief Checks if the variant holds a value other than std::monostate.
     */
    [[nodiscard]] constexpr auto hasValue() const noexcept -> bool;

    /**
     * @brief Execute a function safely with thread protection if this is
     * shared.
     */
    template <typename Func>
    auto withThreadSafety(Func&& func) const -> decltype(auto);

    /**
     * @brief Stream insertion operator for VariantWrapper.
     */
    friend auto operator<<(std::ostream& outputStream,
                           const VariantWrapper& variantWrapper)
        -> std::ostream&;

    /**
     * @brief Default destructor.
     */
    ~VariantWrapper() = default;

private:
    VariantType variant_{std::in_place_index<0>};
    mutable std::shared_mutex mutex_;  // For thread-safe operations

    // Helper template for checking if a type is suitable for our variant
    template <typename T>
    static constexpr bool is_valid_type_v = (std::is_same_v<T, Types> || ...);
};

// Implementation

template <typename... Types>
template <typename... OtherTypes>
VariantWrapper<Types...>::
    VariantWrapper(const VariantWrapper<OtherTypes...>& other) noexcept(
        std::is_nothrow_copy_constructible_v<
            std::variant<std::monostate, OtherTypes...>>)
    : variant_(other.withThreadSafety([&](const auto& v) { return v; })) {}

template <typename... Types>
template <typename T>
VariantWrapper<Types...>::VariantWrapper(T&& value) noexcept(
    std::is_nothrow_constructible_v<VariantType, T>) {
    static_assert(is_valid_type_v<std::decay_t<T>> ||
                      std::is_same_v<std::decay_t<T>, std::monostate>,
                  "Type not supported by this VariantWrapper");
    variant_ = std::forward<T>(value);
}

template <typename... Types>
VariantWrapper<Types...>::VariantWrapper(const VariantWrapper& other) noexcept(
    std::is_nothrow_copy_constructible_v<VariantType>) {
    auto shared_lock = std::shared_lock(other.mutex_);
    variant_ = other.variant_;
}

template <typename... Types>
VariantWrapper<Types...>::VariantWrapper(VariantWrapper&& other) noexcept {
    auto lock = std::unique_lock(other.mutex_);
    variant_ = std::move(other.variant_);
}

template <typename... Types>
auto VariantWrapper<Types...>::operator=(const VariantWrapper& other) noexcept(
    std::is_nothrow_copy_assignable_v<VariantType>) -> VariantWrapper& {
    if (this != &other) {
        auto this_lock = std::unique_lock(mutex_);
        auto other_lock = std::shared_lock(other.mutex_);
        variant_ = other.variant_;
    }
    return *this;
}

template <typename... Types>
auto VariantWrapper<Types...>::operator=(VariantWrapper&& other) noexcept
    -> VariantWrapper& {
    if (this != &other) {
        auto this_lock = std::unique_lock(mutex_);
        auto other_lock = std::unique_lock(other.mutex_);
        variant_ = std::move(other.variant_);
    }
    return *this;
}

template <typename... Types>
template <typename T>
auto VariantWrapper<Types...>::operator=(T&& value) noexcept(
    std::is_nothrow_assignable_v<VariantType, T>) -> VariantWrapper& {
    static_assert(is_valid_type_v<std::decay_t<T>> ||
                      std::is_same_v<std::decay_t<T>, std::monostate>,
                  "Type not supported by this VariantWrapper");
    auto lock = std::unique_lock(mutex_);
    variant_ = std::forward<T>(value);
    return *this;
}

template <typename... Types>
auto VariantWrapper<Types...>::typeName() const -> std::string {
    return withThreadSafety([this]() -> std::string {
        try {
            return std::visit(
                [](auto&& arg) -> std::string { return typeid(arg).name(); },
                variant_);
        } catch (const std::exception& e) {
            return std::string("Error getting type name: ") + e.what();
        }
    });
}

template <typename... Types>
template <typename T>
auto VariantWrapper<Types...>::get() const -> T {
    return withThreadSafety([this]() -> T {
        try {
            if (!std::holds_alternative<T>(variant_)) {
                throw VariantException(
                    std::format("Variant does not hold requested type {}",
                                typeid(T).name()));
            }
            return std::get<T>(variant_);
        } catch (const std::bad_variant_access& e) {
            throw VariantException(
                std::format("Bad variant access: {}", e.what()));
        } catch (const VariantException&) {
            throw;  // Rethrow our custom exception
        } catch (const std::exception& e) {
            throw VariantException(
                std::format("Error getting value: {}", e.what()));
        }
    });
}

template <typename... Types>
template <typename T>
constexpr auto VariantWrapper<Types...>::is() const noexcept -> bool {
    return withThreadSafety(
        [this]() { return std::holds_alternative<T>(variant_); });
}

template <typename... Types>
void VariantWrapper<Types...>::print() const {
    withThreadSafety([this]() {
        try {
            std::visit(
                [](const auto& value) {
                    if constexpr (std::is_same_v<std::decay_t<decltype(value)>,
                                                 std::monostate>) {
                        std::cout << "Current value: std::monostate"
                                  << std::endl;
                    } else {
                        std::cout << "Current value: " << value << std::endl;
                    }
                },
                variant_);
        } catch (const std::exception& e) {
            std::cerr << "Error printing variant: " << e.what() << std::endl;
        }
    });
}

template <typename... Types>
constexpr auto VariantWrapper<Types...>::operator==(
    const VariantWrapper& other) const noexcept -> bool {
    if (this == &other)
        return true;

    return withThreadSafety([this, &other]() {
        auto other_lock = std::shared_lock(other.mutex_);
        return variant_ == other.variant_;
    });
}

template <typename... Types>
constexpr auto VariantWrapper<Types...>::operator!=(
    const VariantWrapper& other) const noexcept -> bool {
    return !(*this == other);
}

template <typename... Types>
template <typename Visitor>
auto VariantWrapper<Types...>::visit(Visitor&& visitor) const
    -> decltype(auto) {
    return withThreadSafety([this, &visitor]() -> decltype(auto) {
        try {
            return std::visit(std::forward<Visitor>(visitor), variant_);
        } catch (const std::exception& e) {
            throw VariantException(
                std::format("Error during variant visit: {}", e.what()));
        }
    });
}

template <typename... Types>
constexpr auto VariantWrapper<Types...>::index() const noexcept -> std::size_t {
    return withThreadSafety([this]() { return variant_.index(); });
}

template <typename... Types>
template <typename T>
auto VariantWrapper<Types...>::tryGet() const noexcept -> std::optional<T> {
    return withThreadSafety([this]() -> std::optional<T> {
        try {
            if (is<T>()) {
                return std::get<T>(variant_);
            }
            return std::nullopt;
        } catch (...) {
            return std::nullopt;
        }
    });
}

template <typename... Types>
auto VariantWrapper<Types...>::toInt() const noexcept -> std::optional<int> {
    return withThreadSafety([this]() -> std::optional<int> {
        try {
            return std::visit(
                [](auto&& arg) -> std::optional<int> {
                    using T = std::decay_t<decltype(arg)>;

                    if constexpr (std::is_same_v<T, std::monostate>) {
                        return std::nullopt;
                    } else if constexpr (std::is_arithmetic_v<T>) {
                        if constexpr (std::is_floating_point_v<T>) {
                            // Check for overflow/underflow for floating-point
                            // conversions
                            constexpr auto int_max =
                                static_cast<T>(std::numeric_limits<int>::max());
                            constexpr auto int_min =
                                static_cast<T>(std::numeric_limits<int>::min());

                            if (arg > int_max || arg < int_min) {
                                return std::nullopt;
                            }
                        }
                        return static_cast<int>(arg);
                    } else if constexpr (std::is_convertible_v<T,
                                                               std::string> ||
                                         std::is_same_v<T, std::string>) {
                        try {
                            std::size_t pos = 0;
                            const int result = std::stoi(arg, &pos);
                            // Ensure entire string was consumed
                            if (pos == arg.length()) {
                                return result;
                            }
                            return std::nullopt;
                        } catch (...) {
                            return std::nullopt;
                        }
                    } else {
                        return std::nullopt;
                    }
                },
                variant_);
        } catch (...) {
            return std::nullopt;
        }
    });
}

template <typename... Types>
auto VariantWrapper<Types...>::toDouble() const noexcept
    -> std::optional<double> {
    return withThreadSafety([this]() -> std::optional<double> {
        try {
            return std::visit(
                [](auto&& arg) -> std::optional<double> {
                    using T = std::decay_t<decltype(arg)>;

                    if constexpr (std::is_same_v<T, std::monostate>) {
                        return std::nullopt;
                    } else if constexpr (std::is_arithmetic_v<T>) {
                        return static_cast<double>(arg);
                    } else if constexpr (std::is_convertible_v<T,
                                                               std::string> ||
                                         std::is_same_v<T, std::string>) {
                        try {
                            std::size_t pos = 0;
                            const double result = std::stod(arg, &pos);
                            // Ensure entire string was consumed
                            if (pos == arg.length()) {
                                return result;
                            }
                            return std::nullopt;
                        } catch (...) {
                            return std::nullopt;
                        }
                    } else {
                        return std::nullopt;
                    }
                },
                variant_);
        } catch (...) {
            return std::nullopt;
        }
    });
}

template <typename... Types>
auto VariantWrapper<Types...>::toBool() const noexcept -> std::optional<bool> {
    return withThreadSafety([this]() -> std::optional<bool> {
        try {
            return std::visit(
                [](auto&& arg) -> std::optional<bool> {
                    using T = std::decay_t<decltype(arg)>;

                    if constexpr (std::is_same_v<T, std::monostate>) {
                        return std::nullopt;
                    } else if constexpr (std::is_convertible_v<T, bool>) {
                        return static_cast<bool>(arg);
                    } else if constexpr (std::is_convertible_v<T,
                                                               std::string> ||
                                         std::is_same_v<T, std::string>) {
                        std::string str = arg;
                        // Convert to lowercase for case-insensitive comparison
                        std::transform(
                            str.begin(), str.end(), str.begin(),
                            [](unsigned char c) { return std::tolower(c); });

                        if (str == "true" || str == "1" || str == "yes" ||
                            str == "y") {
                            return true;
                        }
                        if (str == "false" || str == "0" || str == "no" ||
                            str == "n") {
                            return false;
                        }
                        return std::nullopt;
                    } else {
                        return std::nullopt;
                    }
                },
                variant_);
        } catch (...) {
            return std::nullopt;
        }
    });
}

template <typename... Types>
auto VariantWrapper<Types...>::toString() const -> std::string {
    return withThreadSafety([this]() -> std::string {
        try {
            return std::visit(
                [](auto&& arg) -> std::string {
                    using T = std::decay_t<decltype(arg)>;

                    if constexpr (std::is_same_v<T, std::monostate>) {
                        return "std::monostate";
                    } else if constexpr (std::is_same_v<T, std::string>) {
                        return arg;
                    } else if constexpr (requires { std::to_string(arg); }) {
                        return std::to_string(arg);
                    } else {
                        std::ostringstream oss;
                        oss << arg;
                        return oss.str();
                    }
                },
                variant_);
        } catch (const std::exception& e) {
            return std::string("Error converting to string: ") + e.what();
        }
    });
}

template <typename... Types>
constexpr void VariantWrapper<Types...>::reset() noexcept {
    auto lock = std::unique_lock(mutex_);
    variant_.template emplace<std::monostate>();
}

template <typename... Types>
constexpr auto VariantWrapper<Types...>::hasValue() const noexcept -> bool {
    return withThreadSafety([this]() {
        return variant_.index() != 0;  // std::monostate is the first type
    });
}

template <typename... Types>
template <typename Func>
auto VariantWrapper<Types...>::withThreadSafety(Func&& func) const
    -> decltype(auto) {
    auto lock = std::shared_lock(mutex_);
    return std::forward<Func>(func)();
}

template <typename... Types>
auto operator<<(std::ostream& outputStream,
                const VariantWrapper<Types...>& variantWrapper)
    -> std::ostream& {
    outputStream << variantWrapper.toString();
    return outputStream;
}

}  // namespace atom::type

#endif