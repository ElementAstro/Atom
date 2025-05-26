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
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "atom/error/exception.hpp"
#include "atom/type/json.hpp"
using json = nlohmann::json;

namespace atom::meta {

class ProxyTypeError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ProxyArgumentError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

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
    Arg() = default;
    explicit Arg(std::string name) : name_(std::move(name)) {}
    Arg(std::string name, std::any default_value)
        : name_(std::move(name)), default_value_(std::move(default_value)) {}

    template <ArgumentCompatible T>
    Arg(std::string name, T&& value)
        : name_(std::move(name)), default_value_(std::forward<T>(value)) {}

    Arg(Arg&& other) noexcept = default;
    Arg& operator=(Arg&& other) noexcept = default;
    Arg(const Arg&) = default;
    Arg& operator=(const Arg&) = default;

    /**
     * @brief Get the parameter name
     * @return Reference to parameter name
     */
    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    /**
     * @brief Get the parameter type
     * @return Type information of the parameter
     */
    [[nodiscard]] const std::type_info& getType() const noexcept {
        return default_value_ ? default_value_->type() : typeid(void);
    }

    /**
     * @brief Get the default value
     * @return Optional containing the default value if set
     */
    [[nodiscard]] const std::optional<std::any>& getDefaultValue()
        const noexcept {
        return default_value_;
    }

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
    [[nodiscard]] bool isType() const noexcept {
        return default_value_ && default_value_->type() == typeid(T);
    }

    /**
     * @brief Type-safe value getter
     * @tparam T Expected return type
     * @return Optional containing the value if cast successful
     */
    template <typename T>
    [[nodiscard]] std::optional<T> getValueAs() const noexcept {
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

/**
 * @brief Serialize std::any to JSON
 * @param j Output JSON object
 * @param a std::any value to serialize
 * @throws ProxyTypeError if serialization fails
 */
inline void to_json(nlohmann::json& j, const std::any& a) {
    static const auto type_handlers = []() {
        std::unordered_map<
            std::type_index,
            std::function<void(nlohmann::json&, const std::any&)>>
            handlers;
        handlers[std::type_index(typeid(int))] = [](nlohmann::json& j,
                                                    const std::any& a) {
            j = std::any_cast<int>(a);
        };
        handlers[std::type_index(typeid(float))] = [](nlohmann::json& j,
                                                      const std::any& a) {
            j = std::any_cast<float>(a);
        };
        handlers[std::type_index(typeid(double))] = [](nlohmann::json& j,
                                                       const std::any& a) {
            j = std::any_cast<double>(a);
        };
        handlers[std::type_index(typeid(bool))] = [](nlohmann::json& j,
                                                     const std::any& a) {
            j = std::any_cast<bool>(a);
        };
        handlers[std::type_index(typeid(std::string))] = [](nlohmann::json& j,
                                                            const std::any& a) {
            j = std::any_cast<std::string>(a);
        };
        handlers[std::type_index(typeid(std::string_view))] =
            [](nlohmann::json& j, const std::any& a) {
                j = static_cast<std::string>(
                    std::any_cast<std::string_view>(a));
            };
        handlers[std::type_index(typeid(const char*))] = [](nlohmann::json& j,
                                                            const std::any& a) {
            j = std::any_cast<const char*>(a);
        };
        handlers[std::type_index(typeid(std::vector<std::string>))] =
            [](nlohmann::json& j, const std::any& a) {
                j = std::any_cast<std::vector<std::string>>(a);
            };
        handlers[std::type_index(typeid(std::vector<int>))] =
            [](nlohmann::json& j, const std::any& a) {
                j = std::any_cast<std::vector<int>>(a);
            };
        handlers[std::type_index(typeid(std::vector<double>))] =
            [](nlohmann::json& j, const std::any& a) {
                j = std::any_cast<std::vector<double>>(a);
            };
        return handlers;
    }();

    try {
        auto it = type_handlers.find(std::type_index(a.type()));
        if (it != type_handlers.end()) {
            it->second(j, a);
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
            if (j.empty()) {
                a = std::vector<std::string>{};
            } else if (j[0].is_string()) {
                a = j.get<std::vector<std::string>>();
            } else if (j[0].is_number_integer()) {
                a = j.get<std::vector<int>>();
            } else if (j[0].is_number_float()) {
                a = j.get<std::vector<double>>();
            } else {
                throw ProxyTypeError("Unsupported array element type in JSON");
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
    if (const auto& defaultValue = arg.getDefaultValue(); defaultValue) {
        try {
            to_json(j["default_value"], *defaultValue);
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

    if (const auto& defaultValueJson = j.at("default_value");
        !defaultValueJson.is_null()) {
        std::any value;
        from_json(defaultValueJson, value);
        arg = Arg(std::move(name), std::move(value));
    } else {
        arg = Arg(std::move(name));
    }
}

/**
 * @brief Serialize vector of Args to JSON
 * @param j Output JSON array
 * @param args Vector of Args to serialize
 */
inline void to_json(nlohmann::json& j, const std::vector<Arg>& args) {
    j = nlohmann::json::array();
    for (const auto& arg : args) {
        j.push_back(arg);
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

    for (const auto& jsonArg : j) {
        Arg temp;
        from_json(jsonArg, temp);
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
    FunctionParams() = default;
    explicit FunctionParams(const Arg& arg) : params_{arg} {}
    explicit FunctionParams(Arg&& arg) : params_{std::move(arg)} {}

    /**
     * @brief Constructs FunctionParams from any range of Args
     * @tparam Range Type of the range
     * @param range Range of Arg values to initialize parameters
     */
    template <std::ranges::input_range Range>
        requires std::same_as<std::ranges::range_value_t<Range>, Arg>
    explicit constexpr FunctionParams(const Range& range)
        : params_(std::ranges::begin(range), std::ranges::end(range)) {}

    constexpr FunctionParams(std::initializer_list<Arg> ilist)
        : params_(ilist) {}

    FunctionParams(FunctionParams&& other) noexcept = default;
    FunctionParams& operator=(FunctionParams&& other) noexcept = default;
    FunctionParams(const FunctionParams&) = default;
    FunctionParams& operator=(const FunctionParams&) = default;

    /**
     * @brief Access parameter at given index
     * @param t_i Index of parameter to access
     * @return const Arg& Parameter at given index
     * @throws std::out_of_range if index is out of range
     */
    [[nodiscard]] const Arg& operator[](std::size_t t_i) const {
        if (t_i >= params_.size()) {
            THROW_OUT_OF_RANGE("Index out of range: " + std::to_string(t_i) +
                               " >= " + std::to_string(params_.size()));
        }
        return params_[t_i];
    }

    [[nodiscard]] Arg& operator[](std::size_t t_i) {
        if (t_i >= params_.size()) {
            THROW_OUT_OF_RANGE("Index out of range: " + std::to_string(t_i) +
                               " >= " + std::to_string(params_.size()));
        }
        return params_[t_i];
    }

    [[nodiscard]] auto begin() const noexcept { return params_.begin(); }
    [[nodiscard]] auto begin() noexcept { return params_.begin(); }
    [[nodiscard]] auto end() const noexcept { return params_.end(); }
    [[nodiscard]] auto end() noexcept { return params_.end(); }

    /**
     * @brief Return the first parameter
     * @return Reference to first parameter
     * @throws std::out_of_range if container is empty
     */
    [[nodiscard]] const Arg& front() const {
        if (params_.empty()) {
            THROW_OUT_OF_RANGE("Cannot access front() of empty FunctionParams");
        }
        return params_.front();
    }

    [[nodiscard]] Arg& front() {
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
    [[nodiscard]] const Arg& back() const {
        if (params_.empty()) {
            THROW_OUT_OF_RANGE("Cannot access back() of empty FunctionParams");
        }
        return params_.back();
    }

    [[nodiscard]] Arg& back() {
        if (params_.empty()) {
            THROW_OUT_OF_RANGE("Cannot access back() of empty FunctionParams");
        }
        return params_.back();
    }

    [[nodiscard]] std::size_t size() const noexcept { return params_.size(); }
    [[nodiscard]] bool empty() const noexcept { return params_.empty(); }

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

    void push_back(Arg&& arg) { params_.push_back(std::move(arg)); }
    void push_back(const Arg& arg) { params_.push_back(arg); }
    void clear() noexcept { params_.clear(); }
    void resize(std::size_t new_size) { params_.resize(new_size); }

    /**
     * @brief Convert parameters to vector of Args
     * @return Const reference to internal vector
     */
    [[nodiscard]] const std::vector<Arg>& toVector() const noexcept {
        return params_;
    }
    [[nodiscard]] std::vector<Arg>& toVector() noexcept { return params_; }

    /**
     * @brief Convert parameters to vector of std::any
     * @return Vector containing parameter values as std::any
     */
    [[nodiscard]] std::vector<std::any> toAnyVector() const {
        std::vector<std::any> anyVec;
        anyVec.reserve(params_.size());

        for (const auto& arg : params_) {
            if (const auto& defaultValue = arg.getDefaultValue();
                defaultValue) {
                anyVec.push_back(*defaultValue);
            } else {
                anyVec.emplace_back();
            }
        }
        return anyVec;
    }

    /**
     * @brief Get parameter by name
     * @param name Name of parameter to get
     * @return std::optional<Arg> Parameter if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<Arg> getByName(const std::string& name) const {
        if (auto it = std::ranges::find_if(
                params_,
                [&name](const Arg& arg) { return arg.getName() == name; });
            it != params_.end()) {
            return *it;
        }
        return std::nullopt;
    }

    /**
     * @brief Get parameter reference by name
     * @param name Name of parameter to get
     * @return Arg* Pointer to parameter if found, nullptr otherwise
     */
    [[nodiscard]] Arg* getByNameRef(const std::string& name) noexcept {
        auto it = std::ranges::find_if(
            params_, [&name](const Arg& arg) { return arg.getName() == name; });
        return it != params_.end() ? &(*it) : nullptr;
    }

    /**
     * @brief Slice parameters from given start index to end index
     * @param start Start index of slice
     * @param end End index of slice
     * @return FunctionParams Parameters after slicing
     * @throws std::out_of_range if slice range is invalid
     */
    [[nodiscard]] FunctionParams slice(std::size_t start,
                                       std::size_t end) const {
        if (start > end || end > params_.size()) {
            THROW_OUT_OF_RANGE("Invalid slice range: [" +
                               std::to_string(start) + ", " +
                               std::to_string(end) + "] for size " +
                               std::to_string(params_.size()));
        }
        return FunctionParams(std::vector<Arg>(
            params_.begin() + static_cast<std::ptrdiff_t>(start),
            params_.begin() + static_cast<std::ptrdiff_t>(end)));
    }

    /**
     * @brief Filter parameters using predicate
     * @tparam Predicate Type of predicate
     * @param pred Predicate to filter parameters
     * @return FunctionParams Parameters after filtering
     */
    template <typename Predicate>
    [[nodiscard]] FunctionParams filter(Predicate pred) const {
        std::vector<Arg> filtered;
        filtered.reserve(params_.size());
        std::ranges::copy_if(params_, std::back_inserter(filtered), pred);
        return FunctionParams(std::move(filtered));
    }

    /**
     * @brief Set parameter at given index to new Arg
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

    void set(std::size_t index, Arg&& arg) {
        if (index >= params_.size()) {
            THROW_OUT_OF_RANGE("Index out of range: " + std::to_string(index) +
                               " >= " + std::to_string(params_.size()));
        }
        params_[index] = std::move(arg);
    }

    /**
     * @brief Get parameter value as specified type
     * @tparam T Expected return type
     * @param index Parameter index
     * @return std::optional<T> Value if successful, std::nullopt otherwise
     */
    template <typename T>
    [[nodiscard]] std::optional<T> getValueAs(
        std::size_t index) const noexcept {
        if (index >= params_.size()) {
            return std::nullopt;
        }
        return params_[index].template getValueAs<T>();
    }

    /**
     * @brief Get value or return default if not found
     * @tparam T Expected return type
     * @param index Parameter index
     * @param defaultVal Default value to return if parameter not found
     * @return Parameter value or default value
     */
    template <typename T>
    [[nodiscard]] T getValue(std::size_t index,
                             const T& defaultVal) const noexcept {
        auto result = getValueAs<T>(index);
        return result.value_or(defaultVal);
    }

    /**
     * @brief Get string_view for improved performance with string parameters
     * @param index Parameter index
     * @return std::optional<std::string_view> String view if parameter is
     * string-like
     */
    [[nodiscard]] std::optional<std::string_view> getStringView(
        std::size_t index) const noexcept {
        if (index >= params_.size()) {
            return std::nullopt;
        }

        const auto& arg = params_[index];
        const auto& defaultValue = arg.getDefaultValue();
        if (!defaultValue) {
            return std::nullopt;
        }

        const auto& value = *defaultValue;
        const auto& type = value.type();

        if (type == typeid(std::string)) {
            return std::string_view(std::any_cast<const std::string&>(value));
        } else if (type == typeid(const char*)) {
            return std::string_view(std::any_cast<const char*>(value));
        } else if (type == typeid(std::string_view)) {
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
            result.push_back(arg);
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
    std::vector<Arg> params_;
};

}  // namespace atom::meta

#endif
