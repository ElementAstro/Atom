/*!
 * \file proxy_params.hpp
 * \brief Proxy Function Parameters
 * \author Max Qian <lightapt.com>
 * \date 2024-03-01
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_META_PROXY_PARAMS_HPP
#define ATOM_META_PROXY_PARAMS_HPP

#include <algorithm>
#include <any>
#include <concepts>
#include <iterator>
#include <optional>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <vector>

#include "atom/error/exception.hpp"
#include "atom/type/json.hpp"
using json = nlohmann::json;

namespace atom::meta {

// Custom exception types
class ProxyTypeError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ProxyArgumentError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Parameter compatibility concept
template <typename T>
concept ArgumentCompatible =
    std::copyable<T> && !std::is_pointer_v<T> && !std::is_void_v<T>;

/**
 * @brief Class representing a function parameter
 *
 * This class encapsulates a named parameter with an optional default value.
 * It provides type-safe access to the parameter value.
 */
class Arg {
public:
    Arg();
    explicit Arg(std::string name);
    Arg(std::string name, std::any default_value);

    // Template constructor
    template <ArgumentCompatible T>
    Arg(std::string name, T&& value)
        : name_(std::move(name)), default_value_(std::forward<T>(value)) {}

    // Move constructor
    Arg(Arg&& other) noexcept
        : name_(std::move(other.name_)),
          default_value_(std::move(other.default_value_)) {}

    Arg& operator=(Arg&& other) noexcept {
        if (this != &other) {
            name_ = std::move(other.name_);
            default_value_ = std::move(other.default_value_);
        }
        return *this;
    }

    // Copy constructor and assignment operator
    Arg(const Arg&) = default;
    Arg& operator=(const Arg&) = default;

    /**
     * @brief Get the parameter name
     * @return Reference to parameter name
     */
    [[nodiscard]] auto getName() const -> const std::string&;

    /**
     * @brief Get the parameter type
     * @return Type information of the parameter
     */
    [[nodiscard]] auto getType() const -> const std::type_info&;

    /**
     * @brief Get the default value
     * @return Optional containing the default value if set
     */
    [[nodiscard]] auto getDefaultValue() const
        -> const std::optional<std::any>&;

    /**
     * @brief Type-safe value setter
     * @tparam T Type of the value
     * @param value Value to set
     */
    template <ArgumentCompatible T>
    void setValue(T&& value) {
        default_value_ = std::forward<T>(value);
    }

    /**
     * @brief Check if parameter is of specific type
     * @tparam T Type to check against
     * @return True if parameter is of type T
     */
    template <typename T>
    [[nodiscard]] bool isType() const {
        return default_value_ && default_value_->type() == typeid(T);
    }

    /**
     * @brief Type-safe value getter
     * @tparam T Expected return type
     * @return Optional containing the value if cast successful
     */
    template <typename T>
    [[nodiscard]] std::optional<T> getValueAs() const {
        if (!default_value_) {
            return std::nullopt;
        }

        try {
            return std::any_cast<T>(*default_value_);
        } catch (const std::bad_any_cast&) {
            return std::nullopt;
        }
    }

private:
    std::string name_;
    std::optional<std::any> default_value_;
};

inline Arg::Arg() = default;
inline Arg::Arg(std::string name) : name_(std::move(name)) {}

inline Arg::Arg(std::string name, std::any default_value)
    : name_(std::move(name)), default_value_(std::move(default_value)) {}

inline auto Arg::getName() const -> const std::string& { return name_; }

inline auto Arg::getType() const -> const std::type_info& {
    return default_value_ ? default_value_->type() : typeid(void);
}

inline auto Arg::getDefaultValue() const -> const std::optional<std::any>& {
    return default_value_;
}

/**
 * @brief Serialize std::any to JSON
 * @param j Output JSON object
 * @param a std::any value to serialize
 * @throws ProxyTypeError if serialization fails
 */
inline void to_json(nlohmann::json& j, const std::any& a) {
    try {
        if (a.type() == typeid(int)) {
            j = std::any_cast<int>(a);
        } else if (a.type() == typeid(float)) {
            j = std::any_cast<float>(a);
        } else if (a.type() == typeid(double)) {
            j = std::any_cast<double>(a);
        } else if (a.type() == typeid(bool)) {
            j = std::any_cast<bool>(a);
        } else if (a.type() == typeid(std::string)) {
            j = std::any_cast<std::string>(a);
        } else if (a.type() == typeid(std::string_view)) {
            j = static_cast<std::string>(std::any_cast<std::string_view>(a));
        } else if (a.type() == typeid(const char*)) {
            j = std::any_cast<const char*>(a);
        } else if (a.type() == typeid(std::vector<std::string>)) {
            j = std::any_cast<std::vector<std::string>>(a);
        } else if (a.type() == typeid(std::vector<int>)) {
            j = std::any_cast<std::vector<int>>(a);
        } else if (a.type() == typeid(std::vector<double>)) {
            j = std::any_cast<std::vector<double>>(a);
        } else {
            throw ProxyTypeError("Unsupported type for JSON serialization: " +
                                 std::string(a.type().name()));
        }
    } catch (const std::bad_any_cast& e) {
        throw ProxyTypeError(std::string("Failed to serialize value: ") +
                             e.what());
    }
}

/**
 * @brief Deserialize JSON to std::any
 * @param j Input JSON object
 * @param a Output std::any value
 * @throws ProxyTypeError if deserialization fails
 */
inline void from_json(const nlohmann::json& j, std::any& a) {
    try {
        if (j.is_number_integer()) {
            a = j.get<int>();
        } else if (j.is_number_float()) {
            a = j.get<double>();
        } else if (j.is_string()) {
            a = j.get<std::string>();
        } else if (j.is_boolean()) {
            a = j.get<bool>();
        } else if (j.is_null()) {
            a = std::any{};
        } else if (j.is_array()) {
            if (!j.empty()) {
                if (j[0].is_string()) {
                    a = j.get<std::vector<std::string>>();
                } else if (j[0].is_number_integer()) {
                    a = j.get<std::vector<int>>();
                } else if (j[0].is_number_float()) {
                    a = j.get<std::vector<double>>();
                } else {
                    throw ProxyTypeError(
                        "Unsupported array element type in JSON");
                }
            } else {
                a = std::vector<std::string>{};  // Default to empty string
                                                 // array
            }
        } else {
            throw ProxyTypeError("Unsupported JSON type: " +
                                 std::string(j.type_name()));
        }
    } catch (const nlohmann::json::exception& e) {
        throw ProxyTypeError(std::string("JSON parsing error: ") + e.what());
    }
}

/**
 * @brief Serialize Arg to JSON
 * @param j Output JSON object
 * @param arg Arg to serialize
 */
inline void to_json(nlohmann::json& j, const Arg& arg) {
    j = nlohmann::json{{"name", arg.getName()}};
    if (arg.getDefaultValue()) {
        try {
            to_json(j["default_value"], *arg.getDefaultValue());
            j["type"] = arg.getType().name();
        } catch (const ProxyTypeError& e) {
            j["default_value"] = nullptr;
            j["error"] = e.what();
        }
    } else {
        j["default_value"] = nullptr;
    }
}

/**
 * @brief Deserialize JSON to Arg
 * @param j Input JSON object
 * @param arg Output Arg
 */
inline void from_json(const nlohmann::json& j, Arg& arg) {
    std::string name = j.at("name").get<std::string>();

    std::optional<std::any> defaultValue;
    if (!j.at("default_value").is_null()) {
        std::any value;
        from_json(j.at("default_value"), value);
        defaultValue = value;
    }

    arg = Arg(std::move(name), defaultValue ? *defaultValue : std::any());
}

/**
 * @brief Serialize vector of Args to JSON
 * @param j Output JSON array
 * @param args Vector of Args to serialize
 */
inline void to_json(nlohmann::json& j, const std::vector<Arg>& args) {
    j = nlohmann::json::array();
    for (const auto& a : args) {
        j.push_back(a);
    }
}

/**
 * @brief Deserialize JSON to vector of Args
 * @param j Input JSON array
 * @param args Output vector of Args
 */
inline void from_json(const nlohmann::json& j, std::vector<Arg>& args) {
    args.clear();
    args.reserve(j.size());

    for (const auto& a : j) {
        Arg temp;
        from_json(a, temp);
        args.push_back(std::move(temp));
    }
}

/**
 * @brief Class encapsulating function parameters
 *
 * This class provides a container for function parameters with various
 * utilities for accessing, modifying, and manipulating parameters.
 */
class FunctionParams {
public:
    /**
     * @brief Default constructor for FunctionParams
     */
    FunctionParams() = default;

    /**
     * @brief Constructs FunctionParams with a single Arg value
     *
     * @param arg Initial Arg to store in the parameters
     */
    explicit FunctionParams(const Arg& arg) : params_{arg} {}
    explicit FunctionParams(Arg&& arg) : params_{std::move(arg)} {}

    /**
     * @brief Constructs FunctionParams from any range of Args
     *
     * @tparam Range Type of the range
     * @param range Range of Arg values to initialize parameters
     */
    template <std::ranges::input_range Range>
        requires std::same_as<std::ranges::range_value_t<Range>, Arg>
    explicit constexpr FunctionParams(const Range& range)
        : params_(std::ranges::begin(range), std::ranges::end(range)) {}

    /**
     * @brief Constructs FunctionParams from initializer list of Args
     *
     * @param ilist Initializer list of Arg values
     */
    constexpr FunctionParams(std::initializer_list<Arg> ilist)
        : params_(ilist) {}

    // Move constructor and assignment operator
    FunctionParams(FunctionParams&& other) noexcept
        : params_(std::move(other.params_)) {}

    FunctionParams& operator=(FunctionParams&& other) noexcept {
        if (this != &other) {
            params_ = std::move(other.params_);
        }
        return *this;
    }

    // Copy constructor and assignment operator
    FunctionParams(const FunctionParams&) = default;
    FunctionParams& operator=(const FunctionParams&) = default;

    /**
     * @brief Access parameter at given index
     *
     * @param t_i Index of parameter to access
     * @return const Arg& Parameter at given index
     * @throws std::out_of_range if index is out of range
     */
    [[nodiscard]] auto operator[](std::size_t t_i) const -> const Arg& {
        if (t_i >= params_.size()) {
            THROW_OUT_OF_RANGE("Index out of range: " + std::to_string(t_i) +
                               " >= " + std::to_string(params_.size()));
        }
        return params_.at(t_i);
    }

    /**
     * @brief Access parameter at given index (non-const version)
     *
     * @param t_i Index of parameter to access
     * @return Arg& Parameter at given index
     * @throws std::out_of_range if index is out of range
     */
    [[nodiscard]] auto operator[](std::size_t t_i) -> Arg& {
        if (t_i >= params_.size()) {
            THROW_OUT_OF_RANGE("Index out of range: " + std::to_string(t_i) +
                               " >= " + std::to_string(params_.size()));
        }
        return params_.at(t_i);
    }

    /**
     * @brief Return beginning iterator for parameters
     * @return Iterator to first parameter
     */
    [[nodiscard]] auto begin() const noexcept { return params_.begin(); }
    [[nodiscard]] auto begin() noexcept { return params_.begin(); }

    /**
     * @brief Return end iterator for parameters
     * @return Iterator past the last parameter
     */
    [[nodiscard]] auto end() const noexcept { return params_.end(); }
    [[nodiscard]] auto end() noexcept { return params_.end(); }

    /**
     * @brief Return the first parameter
     * @return Reference to first parameter
     * @throws std::out_of_range if container is empty
     */
    [[nodiscard]] auto front() const -> const Arg& {
        if (params_.empty()) {
            THROW_OUT_OF_RANGE("Cannot access front() of empty FunctionParams");
        }
        return params_.front();
    }

    /**
     * @brief Return the first parameter (non-const version)
     * @return Reference to first parameter
     * @throws std::out_of_range if container is empty
     */
    [[nodiscard]] auto front() -> Arg& {
        if (params_.empty()) {
            THROW_OUT_OF_RANGE("Cannot access front() of empty FunctionParams");
        }
        return params_.front();
    }

    /**
     * @brief Return the last parameter
     * @return Reference to last parameter
     * @throws std::out_of_range if container is empty
     */
    [[nodiscard]] auto back() const -> const Arg& {
        if (params_.empty()) {
            THROW_OUT_OF_RANGE("Cannot access back() of empty FunctionParams");
        }
        return params_.back();
    }

    /**
     * @brief Return the last parameter (non-const version)
     * @return Reference to last parameter
     * @throws std::out_of_range if container is empty
     */
    [[nodiscard]] auto back() -> Arg& {
        if (params_.empty()) {
            THROW_OUT_OF_RANGE("Cannot access back() of empty FunctionParams");
        }
        return params_.back();
    }

    /**
     * @brief Return count of parameters
     * @return Number of parameters
     */
    [[nodiscard]] auto size() const noexcept -> std::size_t {
        return params_.size();
    }

    /**
     * @brief Check if there are no parameters
     * @return True if no parameters exist
     */
    [[nodiscard]] auto empty() const noexcept -> bool {
        return params_.empty();
    }

    /**
     * @brief Reserve memory for parameters
     * @param capacity Number of parameters to reserve space for
     */
    void reserve(std::size_t capacity) { params_.reserve(capacity); }

    /**
     * @brief Construct parameter in-place
     * @tparam Args Types of arguments to forward to constructor
     * @param args Arguments to forward to constructor
     */
    template <typename... Args>
    void emplace_back(Args&&... args) {
        params_.emplace_back(std::forward<Args>(args)...);
    }

    /**
     * @brief Add parameter to the end (move version)
     * @param arg Parameter to add
     */
    void push_back(Arg&& arg) { params_.push_back(std::move(arg)); }

    /**
     * @brief Add parameter to the end (copy version)
     * @param arg Parameter to add
     */
    void push_back(const Arg& arg) { params_.push_back(arg); }

    /**
     * @brief Clear all parameters
     */
    void clear() noexcept { params_.clear(); }

    /**
     * @brief Resize parameter container
     * @param new_size New size of container
     */
    void resize(std::size_t new_size) { params_.resize(new_size); }

    /**
     * @brief Convert parameters to vector of Args
     * @return Const reference to internal vector
     */
    [[nodiscard]] auto toVector() const -> const std::vector<Arg>& {
        return params_;
    }

    /**
     * @brief Convert parameters to vector of Args (non-const version)
     * @return Reference to internal vector
     */
    [[nodiscard]] auto toVector() -> std::vector<Arg>& { return params_; }

    /**
     * @brief Convert parameters to vector of std::any
     * @return Vector containing parameter values as std::any
     */
    [[nodiscard]] auto toAnyVector() const -> std::vector<std::any> {
        std::vector<std::any> anyVec;
        anyVec.reserve(params_.size());

        for (const auto& arg : params_) {
            if (arg.getDefaultValue()) {
                anyVec.push_back(*arg.getDefaultValue());
            } else {
                anyVec.push_back({});
            }
        }
        return anyVec;
    }

    /**
     * @brief Get parameter by name
     *
     * @param name Name of parameter to get
     * @return std::optional<Arg> Parameter if found, std::nullopt otherwise
     */
    [[nodiscard]] auto getByName(const std::string& name) const
        -> std::optional<Arg> {
        if (auto findTt = std::ranges::find_if(
                params_, [&](const Arg& arg) { return arg.getName() == name; });
            findTt != params_.end()) {
            return *findTt;
        }
        return std::nullopt;
    }

    /**
     * @brief Get parameter reference by name
     *
     * @param name Name of parameter to get
     * @return Arg* Pointer to parameter if found, nullptr otherwise
     */
    [[nodiscard]] auto getByNameRef(const std::string& name) -> Arg* {
        auto it = std::ranges::find_if(
            params_, [&](const Arg& arg) { return arg.getName() == name; });
        return it != params_.end() ? &(*it) : nullptr;
    }

    /**
     * @brief Slice parameters from given start index to end index
     *
     * @param start Start index of slice
     * @param end End index of slice
     * @return FunctionParams Parameters after slicing
     * @throws std::out_of_range if slice range is invalid
     */
    [[nodiscard]] auto slice(std::size_t start,
                             std::size_t end) const -> FunctionParams {
        if (start > end || end > params_.size()) {
            THROW_OUT_OF_RANGE("Invalid slice range: [" +
                               std::to_string(start) + ", " +
                               std::to_string(end) + "] for size " +
                               std::to_string(params_.size()));
        }
        using DifferenceType = std::make_signed_t<std::size_t>;
        return FunctionParams(std::vector<Arg>(
            params_.begin() + static_cast<DifferenceType>(start),
            params_.begin() + static_cast<DifferenceType>(end)));
    }

    /**
     * @brief Filter parameters using predicate
     *
     * @tparam Predicate Type of predicate
     * @param pred Predicate to filter parameters
     * @return FunctionParams Parameters after filtering
     */
    template <typename Predicate>
    [[nodiscard]] auto filter(Predicate pred) const -> FunctionParams {
        std::vector<Arg> filtered;
        filtered.reserve(params_.size());
        std::ranges::copy_if(params_, std::back_inserter(filtered), pred);
        return FunctionParams(filtered);
    }

    /**
     * @brief Set parameter at given index to new Arg
     *
     * @param index Index of parameter to set
     * @param arg New Arg to set
     * @throws std::out_of_range if index is out of range
     */
    void set(std::size_t index, const Arg& arg) {
        if (index >= params_.size()) {
            THROW_OUT_OF_RANGE("Index out of range: " + std::to_string(index) +
                               " >= " + std::to_string(params_.size()));
        }
        params_[index] = arg;
    }

    /**
     * @brief Set parameter at given index to new Arg (move version)
     *
     * @param index Index of parameter to set
     * @param arg New Arg to set
     * @throws std::out_of_range if index is out of range
     */
    void set(std::size_t index, Arg&& arg) {
        if (index >= params_.size()) {
            THROW_OUT_OF_RANGE("Index out of range: " + std::to_string(index) +
                               " >= " + std::to_string(params_.size()));
        }
        params_[index] = std::move(arg);
    }

    /**
     * @brief Get parameter value as specified type
     *
     * @tparam T Expected return type
     * @param index Parameter index
     * @return std::optional<T> Value if successful, std::nullopt otherwise
     */
    template <typename T>
    [[nodiscard]] auto getValueAs(size_t index) const -> std::optional<T> {
        if (index >= params_.size()) {
            return std::nullopt;
        }

        const auto& value = params_[index].getDefaultValue();
        if (!value.has_value()) {
            return std::nullopt;
        }

        try {
            return std::any_cast<T>(*value);
        } catch (const std::bad_any_cast&) {
            return std::nullopt;
        }
    }

    /**
     * @brief Get value or return default if not found
     *
     * @tparam T Expected return type
     * @param index Parameter index
     * @param defaultVal Default value to return if parameter not found
     * @return Parameter value or default value
     */
    template <typename T>
    [[nodiscard]] auto getValue(size_t index, const T& defaultVal) const -> T {
        auto result = getValueAs<T>(index);
        return result.value_or(defaultVal);
    }

    /**
     * @brief Get string_view for improved performance with string parameters
     *
     * @param index Parameter index
     * @return std::optional<std::string_view> String view if parameter is
     * string-like
     */
    [[nodiscard]] auto getStringView(size_t index) const
        -> std::optional<std::string_view> {
        if (index >= params_.size()) {
            return std::nullopt;
        }

        const auto& arg = params_[index];
        if (!arg.getDefaultValue()) {
            return std::nullopt;
        }

        const auto& value = *arg.getDefaultValue();

        if (value.type() == typeid(std::string)) {
            return std::string_view(std::any_cast<const std::string&>(value));
        } else if (value.type() == typeid(const char*)) {
            return std::string_view(std::any_cast<const char*>(value));
        } else if (value.type() == typeid(std::string_view)) {
            return std::any_cast<std::string_view>(value);
        }

        return std::nullopt;
    }

    /**
     * @brief Serialize to JSON
     * @return JSON representation of parameters
     */
    [[nodiscard]] nlohmann::json toJson() const {
        nlohmann::json result = nlohmann::json::array();
        for (const auto& arg : params_) {
            nlohmann::json argJson;
            to_json(argJson, arg);
            result.push_back(argJson);
        }
        return result;
    }

    /**
     * @brief Deserialize from JSON
     * @param j JSON to deserialize
     * @return FunctionParams instance created from JSON
     */
    static FunctionParams fromJson(const nlohmann::json& j) {
        FunctionParams params;
        params.reserve(j.size());
        from_json(j, params.params_);
        return params;
    }

private:
    std::vector<Arg> params_;  ///< Vector of Arg objects
};

}  // namespace atom::meta

#endif
