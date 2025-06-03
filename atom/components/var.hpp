/*
 * var.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-10

Description: Variable Manager

**************************************************/

#ifndef ATOM_COMPONENT_VAR_HPP
#define ATOM_COMPONENT_VAR_HPP

#include <any>
#include <span>

#if ENABLE_FASTHASH
#include "emhash/hash_table8.hpp"
#endif

#include <spdlog/spdlog.h>
#include "atom/error/exception.hpp"
#include "atom/macro.hpp"
#include "atom/meta/concept.hpp"
#include "atom/type/trackable.hpp"

/**
 * @brief Exception for variable type errors
 */
class VariableTypeError : public atom::error::Exception {
public:
    using atom::error::Exception::Exception;
};

#define THROW_TYPE_ERROR(...)                                               \
    throw VariableTypeError(ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, \
                            __VA_ARGS__)

/**
 * @brief Manages variables with tracking, validation, and serialization
 * capabilities
 */
class VariableManager {
public:
    /**
     * @brief Add a variable with initial value
     * @tparam T Variable type (must be copy constructible)
     * @param name Variable name
     * @param initialValue Initial value
     * @param description Variable description
     * @param alias Variable alias
     * @param group Variable group
     */
    template <typename T>
        requires std::is_copy_constructible_v<T>
    void addVariable(const std::string& name, T initialValue,
                     const std::string& description = "",
                     const std::string& alias = "",
                     const std::string& group = "");

    /**
     * @brief Add a variable bound to a member pointer
     * @tparam T Variable type (must be copy constructible)
     * @tparam C Class type
     * @param name Variable name
     * @param memberPointer Pointer to class member
     * @param instance Class instance
     * @param description Variable description
     * @param alias Variable alias
     * @param group Variable group
     */
    template <typename T, typename C>
        requires std::is_copy_constructible_v<T>
    void addVariable(const std::string& name, T C::* memberPointer, C& instance,
                     const std::string& description = "",
                     const std::string& alias = "",
                     const std::string& group = "");

    /**
     * @brief Set range constraints for arithmetic variables
     * @tparam T Arithmetic type
     * @param name Variable name
     * @param min Minimum value
     * @param max Maximum value
     */
    template <Arithmetic T>
    void setRange(const std::string& name, T min, T max);

    /**
     * @brief Set string options for string variables
     * @param name Variable name
     * @param options Available string options
     */
    void setStringOptions(const std::string& name,
                          std::span<const std::string> options);

    /**
     * @brief Get trackable variable by name
     * @tparam T Variable type
     * @param name Variable name or alias
     * @return Shared pointer to trackable variable
     */
    template <typename T>
    [[nodiscard]] auto getVariable(const std::string& name)
        -> std::shared_ptr<Trackable<T>>;

    /**
     * @brief Set variable value (C-string overload)
     * @param name Variable name
     * @param newValue New value
     */
    void setValue(const std::string& name, const char* newValue);

    /**
     * @brief Set variable value
     * @tparam T Variable type
     * @param name Variable name
     * @param newValue New value
     */
    template <typename T>
    void setValue(const std::string& name, T newValue);

    /**
     * @brief Check if variable exists
     * @param name Variable name or alias
     * @return True if variable exists
     */
    [[nodiscard]] bool has(const std::string& name) const;

    /**
     * @brief Get variable description
     * @param name Variable name or alias
     * @return Variable description
     */
    [[nodiscard]] std::string getDescription(const std::string& name) const;

    /**
     * @brief Get variable alias or primary name
     * @param name Variable name or alias
     * @return Alias if name is primary, primary name if name is alias
     */
    [[nodiscard]] std::string getAlias(const std::string& name) const;

    /**
     * @brief Get variable group
     * @param name Variable name or alias
     * @return Variable group
     */
    [[nodiscard]] std::string getGroup(const std::string& name) const;

    /**
     * @brief Remove variable by name or alias
     * @param name Variable name or alias
     */
    void removeVariable(const std::string& name);

    /**
     * @brief Get all primary variable names
     * @return Vector of primary variable names
     */
    [[nodiscard]] std::vector<std::string> getAllVariables() const;

    /**
     * @brief Execute function for each variable
     * @tparam Func Function type
     * @param func Function to execute
     */
    template <typename Func>
    void forEachVariable(Func&& func) const;

    /**
     * @brief Get variables by group
     * @param group Group name
     * @return Vector of primary variable names in group
     */
    [[nodiscard]] std::vector<std::string> getVariablesByGroup(
        const std::string& group) const;

    /**
     * @brief Export variables to JSON file
     * @param filePath Output file path
     */
    void exportVariablesToJson(const std::string& filePath) const;

    /**
     * @brief Import variables from JSON file
     * @param filePath Input file path
     */
    void importVariablesFromJson(const std::string& filePath);

private:
    struct VariableInfo {
        std::any variable;
        std::string description;
        std::string alias;
        std::string group;
    } ATOM_ALIGNAS(128);

#if USE_BOOST_CONTAINERS
    atom::components::containers::flat_map<std::string, VariableInfo>
        variables_;
    atom::components::containers::flat_map<std::string, std::any> ranges_;
    atom::components::containers::flat_map<std::string,
                                           std::vector<std::string>>
        stringOptions_;
    atom::components::containers::flat_map<
        std::string, atom::components::containers::flat_set<std::string>>
        groups_;
#elif ENABLE_FASTHASH
    emhash8::HashMap<std::string, VariableInfo> variables_;
    emhash8::HashMap<std::string, std::any> ranges_;
    emhash8::HashMap<std::string, std::vector<std::string>> stringOptions_;
    emhash8::HashMap<std::string, std::unordered_set<std::string>> groups_;
#else
    std::unordered_map<std::string, VariableInfo> variables_;
    std::unordered_map<std::string, std::any> ranges_;
    std::unordered_map<std::string, std::vector<std::string>> stringOptions_;
    std::unordered_map<std::string, std::unordered_set<std::string>> groups_;
#endif
};

template <typename T>
    requires std::is_copy_constructible_v<T>
void VariableManager::addVariable(const std::string& name, T initialValue,
                                  const std::string& description,
                                  const std::string& alias,
                                  const std::string& group) {
    spdlog::info("Adding variable: {}", name);

    if (variables_.contains(name)) {
        spdlog::warn("Variable already exists: {}", name);
        THROW_OBJ_ALREADY_EXIST(name);
    }

    auto trackable = std::make_shared<Trackable<T>>(std::move(initialValue));
    variables_[name] = {trackable, description, alias, group};

    if (!group.empty()) {
        groups_[group].insert(name);
    }

    if (!alias.empty()) {
        spdlog::info("Adding alias '{}' for variable '{}'", alias, name);
        if (variables_.contains(alias)) {
            spdlog::warn(
                "Variable with name '{}' already exists, not adding alias",
                alias);
        } else {
            variables_[alias] = variables_[name];
            if (!group.empty()) {
                groups_[group].insert(alias);
            }
        }
    }
}

template <typename T, typename C>
    requires std::is_copy_constructible_v<T>
void VariableManager::addVariable(const std::string& name, T C::* memberPointer,
                                  C& instance, const std::string& description,
                                  const std::string& alias,
                                  const std::string& group) {
    spdlog::info("Adding member variable: {}", name);

    if (variables_.contains(name)) {
        spdlog::warn("Variable already exists: {}", name);
        THROW_OBJ_ALREADY_EXIST(name);
    }

    auto trackable = std::make_shared<Trackable<T>>(instance.*memberPointer);
    trackable->onChange([memberPointer, &instance](const T& value) {
        instance.*memberPointer = value;
    });

    variables_[name] = {trackable, description, alias, group};

    if (!group.empty()) {
        groups_[group].insert(name);
    }

    if (!alias.empty()) {
        spdlog::info("Adding alias '{}' for variable '{}'", alias, name);
        if (variables_.contains(alias)) {
            spdlog::warn(
                "Variable with name '{}' already exists, not adding alias",
                alias);
        } else {
            variables_[alias] = variables_[name];
            if (!group.empty()) {
                groups_[group].insert(alias);
            }
        }
    }
}

template <Arithmetic T>
void VariableManager::setRange(const std::string& name, T min, T max) {
    spdlog::info("Setting range for variable: {} [{}, {}]", name, min, max);

    if (!variables_.contains(name)) {
        spdlog::warn("Variable not found: {}", name);
        THROW_OBJ_NOT_EXIST(name);
    }

    struct Range {
        T min;
        T max;
    };

    ranges_[name] = Range{min, max};

    auto trackableVar =
        std::any_cast<std::shared_ptr<Trackable<T>>>(variables_[name].variable);

    trackableVar->subscribe([min, max, name]([[maybe_unused]] const T& oldValue,
                                             const T& newValue) {
        if (newValue < min || newValue > max) {
            THROW_INVALID_ARGUMENT(
                "Value {} out of range [{}, {}] for variable '{}'", newValue,
                min, max, name);
        }
    });
}

template <typename T>
auto VariableManager::getVariable(const std::string& name)
    -> std::shared_ptr<Trackable<T>> {
    spdlog::debug("Getting variable: {}", name);

    if (auto it = variables_.find(name); it != variables_.end()) {
        try {
            return std::any_cast<std::shared_ptr<Trackable<T>>>(
                it->second.variable);
        } catch (const std::bad_any_cast& e) {
            spdlog::error("Type mismatch for variable '{}': {}", name,
                          e.what());
            THROW_TYPE_ERROR("Type mismatch for variable '{}': {}", name,
                             e.what());
        }
    }

    for (const auto& [key, value] : variables_) {
        if (value.alias == name) {
            try {
                return std::any_cast<std::shared_ptr<Trackable<T>>>(
                    value.variable);
            } catch (const std::bad_any_cast& e) {
                spdlog::error("Type mismatch for variable alias '{}': {}", name,
                              e.what());
                THROW_TYPE_ERROR("Type mismatch for variable alias '{}': {}",
                                 name, e.what());
            }
        }
    }

    spdlog::error("Variable not found: {}", name);
    THROW_OBJ_NOT_EXIST(name);
}

template <typename T>
void VariableManager::setValue(const std::string& name, T newValue) {
    spdlog::debug("Setting value for variable: {}", name);

    auto var = getVariable<T>(name);

    auto rangeIt = ranges_.find(name);
    if (rangeIt != ranges_.end()) {
        try {
            struct Range {
                T min;
                T max;
            };

            if (auto* rangePtr = std::any_cast<Range>(&rangeIt->second)) {
                if (newValue < rangePtr->min || newValue > rangePtr->max) {
                    spdlog::error(
                        "Value {} out of range [{}, {}] for variable '{}'",
                        newValue, rangePtr->min, rangePtr->max, name);
                    THROW_INVALID_ARGUMENT(
                        "Value {} out of range [{}, {}] for variable '{}'",
                        newValue, rangePtr->min, rangePtr->max, name);
                }
            }
        } catch (const std::bad_any_cast&) {
            spdlog::warn("Failed to cast range for variable '{}'", name);
        }
    }

    if constexpr (std::is_same_v<T, std::string>) {
        auto optionsIt = stringOptions_.find(name);
        if (optionsIt != stringOptions_.end()) {
            const auto& options = optionsIt->second;
            if (std::find(options.begin(), options.end(), newValue) ==
                options.end()) {
                spdlog::error("Invalid option '{}' for variable '{}'", newValue,
                              name);
                THROW_INVALID_ARGUMENT("Invalid option '{}' for variable '{}'",
                                       newValue, name);
            }
        }
    }

    *var = std::move(newValue);
}

template <typename Func>
void VariableManager::forEachVariable(Func&& func) const {
    for (const auto& [name, info] : variables_) {
        func(name, info);
    }
}

#endif  // ATOM_COMPONENT_VAR_HPP
