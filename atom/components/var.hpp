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

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
#include "atom/meta/concept.hpp"
#include "atom/type/trackable.hpp"

#include "atom/macro.hpp"

class VariableTypeError : public atom::error::Exception {
public:
    using atom::error::Exception::Exception;
};

#define THROW_TYPE_ERROR(...)                                               \
    throw VariableTypeError(ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, \
                            __VA_ARGS__)

class VariableManager {
public:
    template <typename T>
        requires std::is_copy_constructible_v<T>
    void addVariable(const std::string& name, T initialValue,
                     const std::string& description = "",
                     const std::string& alias = "",
                     const std::string& group = "");

    template <typename T, typename C>
        requires std::is_copy_constructible_v<T>
    void addVariable(const std::string& name, T C::* memberPointer, C& instance,
                     const std::string& description = "",
                     const std::string& alias = "",
                     const std::string& group = "");

    template <Arithmetic T>
    void setRange(const std::string& name, T min, T max);

    void setStringOptions(const std::string& name,
                          std::span<const std::string> options);

    template <typename T>
    [[nodiscard]] auto getVariable(const std::string& name)
        -> std::shared_ptr<Trackable<T>>;

    void setValue(const std::string& name, const char* newValue);

    template <typename T>
    void setValue(const std::string& name, T newValue);

    [[nodiscard]] bool has(const std::string& name) const;

    [[nodiscard]] std::string getDescription(const std::string& name) const;

    [[nodiscard]] std::string getAlias(const std::string& name) const;

    [[nodiscard]] std::string getGroup(const std::string& name) const;

    // New functionalities
    void removeVariable(const std::string& name);
    [[nodiscard]] std::vector<std::string> getAllVariables() const;

    // 批量操作方法
    template <typename Func>
    void forEachVariable(Func&& func) const;

    // 按组获取变量名称
    [[nodiscard]] std::vector<std::string> getVariablesByGroup(
        const std::string& group) const;

    // 导出/导入变量配置
    void exportVariablesToJson(const std::string& filePath) const;
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
    LOG_F(INFO, "Adding variable: {}", name);
    if (variables_.contains(name)) {
        LOG_F(WARNING, "Variable already exists: {}", name);
        THROW_OBJ_ALREADY_EXIST(name);
    }

    auto trackable = std::make_shared<Trackable<T>>(initialValue);
    variables_[name] = {trackable, description, alias, group};

    // 添加到组映射
    if (!group.empty()) {
        groups_[group].insert(name);
    }

    if (!alias.empty()) {
        LOG_F(INFO, "Adding alias '{}' for variable '{}'", alias, name);
        if (variables_.contains(alias)) {
            LOG_F(WARNING,
                  "Variable with name '{}' already exists, not adding alias",
                  alias);
        } else {
            variables_[alias] = variables_[name];
            // 也添加别名到组映射
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
    LOG_F(INFO, "Adding member variable: {}", name);
    if (variables_.contains(name)) {
        LOG_F(WARNING, "Variable already exists: {}", name);
        THROW_OBJ_ALREADY_EXIST(name);
    }

    auto trackable = std::make_shared<Trackable<T>>(instance.*memberPointer);
    trackable->onChange([memberPointer, &instance](const T& value) {
        instance.*memberPointer = value;
    });

    variables_[name] = {trackable, description, alias, group};

    // 添加到组映射
    if (!group.empty()) {
        groups_[group].insert(name);
    }

    if (!alias.empty()) {
        LOG_F(INFO, "Adding alias '{}' for variable '{}'", alias, name);
        if (variables_.contains(alias)) {
            LOG_F(WARNING,
                  "Variable with name '{}' already exists, not adding alias",
                  alias);
        } else {
            variables_[alias] = variables_[name];
            // 也添加别名到组映射
            if (!group.empty()) {
                groups_[group].insert(alias);
            }
        }
    }
}

template <Arithmetic T>
void VariableManager::setRange(const std::string& name, T min, T max) {
    LOG_F(INFO, "Setting range for variable: {} [{}, {}]", name, min, max);
    if (!variables_.contains(name)) {
        LOG_F(WARNING, "Variable not found: {}", name);
        THROW_OBJ_NOT_EXIST(name);
    }

    struct Range {
        T min;
        T max;
    };

    ranges_[name] = Range{min, max};

    auto trackableVar =
        std::any_cast<std::shared_ptr<Trackable<T>>>(variables_[name].variable);

    trackableVar->subscribe(
        [min, max]([[maybe_unused]] const T& oldValue, const T& newValue) {
            if (newValue < min || newValue > max) {
                THROW_INVALID_ARGUMENT("Value {} out of range [{}, {}]",
                                       newValue, min, max);
            }
        });
}

template <typename T>
auto VariableManager::getVariable(const std::string& name)
    -> std::shared_ptr<Trackable<T>> {
    LOG_F(INFO, "Getting variable: {}", name);
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        try {
            return std::any_cast<std::shared_ptr<Trackable<T>>>(
                it->second.variable);
        } catch (const std::bad_any_cast& e) {
            LOG_F(ERROR, "Type mismatch for variable '{}': {}", name, e.what());
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
                LOG_F(ERROR, "Type mismatch for variable alias '{}': {}", name,
                      e.what());
                THROW_TYPE_ERROR("Type mismatch for variable alias '{}': {}",
                                 name, e.what());
            }
        }
    }

    LOG_F(ERROR, "Variable not found: {}", name);
    THROW_OBJ_NOT_EXIST(name);
}

template <typename T>
void VariableManager::setValue(const std::string& name, T newValue) {
    LOG_F(INFO, "Setting value for variable: {}", name);
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
                    LOG_F(ERROR,
                          "Value {} out of range [{}, {}] for variable '{}'",
                          newValue, rangePtr->min, rangePtr->max, name);
                    THROW_INVALID_ARGUMENT(
                        "Value {} out of range [{}, {}] for variable '{}'",
                        newValue, rangePtr->min, rangePtr->max, name);
                }
            }
        } catch (const std::bad_any_cast&) {
            LOG_F(WARNING, "Failed to cast range for variable '{}'", name);
        }
    }

    if constexpr (std::is_same_v<T, std::string>) {
        auto optionsIt = stringOptions_.find(name);
        if (optionsIt != stringOptions_.end()) {
            const auto& options = optionsIt->second;
            if (std::find(options.begin(), options.end(), newValue) ==
                options.end()) {
                LOG_F(ERROR, "Invalid option '{}' for variable '{}'", newValue,
                      name);
                THROW_INVALID_ARGUMENT("Invalid option '{}' for variable '{}'",
                                       newValue, name);
            }
        }
    }

    *var = newValue;
}

template <typename Func>
void VariableManager::forEachVariable(Func&& func) const {
    for (const auto& [name, info] : variables_) {
        func(name, info);
    }
}

#endif  // ATOM_COMPONENT_VAR_HPP
