#include "var.hpp"

#include <algorithm>
#include <fstream>

#include "atom/error/exception.hpp"
#include "atom/type/json.hpp"
#include "spdlog/spdlog.h"

void VariableManager::setValue(const std::string& name, const char* newValue) {
    spdlog::debug("Setting value for variable: {}", name);
    setValue(name, std::string(newValue));
}

auto VariableManager::has(const std::string& name) const -> bool {
    spdlog::debug("Checking if variable exists: {}", name);

    if (variables_.contains(name)) {
        return true;
    }

    return std::any_of(
        variables_.begin(), variables_.end(),
        [&name](const auto& pair) { return pair.second.alias == name; });
}

auto VariableManager::getDescription(const std::string& name) const
    -> std::string {
    spdlog::debug("Getting description for variable: {}", name);

    if (auto it = variables_.find(name); it != variables_.end()) {
        return it->second.description;
    }

    for (const auto& [key, value] : variables_) {
        if (value.alias == name) {
            return value.description;
        }
    }

    spdlog::warn("Variable or alias not found: {}", name);
    return "";
}

auto VariableManager::getAlias(const std::string& name) const -> std::string {
    spdlog::debug("Getting alias for variable: {}", name);

    if (auto it = variables_.find(name); it != variables_.end()) {
        return it->second.alias;
    }

    for (const auto& [key, value] : variables_) {
        if (value.alias == name) {
            return key;
        }
    }

    spdlog::warn("Variable or alias not found: {}", name);
    return "";
}

auto VariableManager::getGroup(const std::string& name) const -> std::string {
    spdlog::debug("Getting group for variable: {}", name);

    if (auto it = variables_.find(name); it != variables_.end()) {
        return it->second.group;
    }

    for (const auto& [key, value] : variables_) {
        if (value.alias == name) {
            return value.group;
        }
    }

    spdlog::warn("Variable or alias not found: {}", name);
    return "";
}

void VariableManager::removeVariable(const std::string& name) {
    spdlog::info("Removing variable: {}", name);

    auto it = variables_.find(name);
    if (it != variables_.end()) {
        const auto& info = it->second;

        if (!info.group.empty()) {
            auto groupIt = groups_.find(info.group);
            if (groupIt != groups_.end()) {
                groupIt->second.erase(name);
                if (!info.alias.empty()) {
                    groupIt->second.erase(info.alias);
                }
                if (groupIt->second.empty()) {
                    groups_.erase(groupIt);
                }
            }
        }

        ranges_.erase(name);
        stringOptions_.erase(name);

        if (!info.alias.empty()) {
            variables_.erase(info.alias);
            ranges_.erase(info.alias);
            stringOptions_.erase(info.alias);
        }

        variables_.erase(it);
    } else {
        std::string primaryNameToRemove;
        for (const auto& [primaryName, info] : variables_) {
            if (info.alias == name) {
                primaryNameToRemove = primaryName;
                break;
            }
        }

        if (!primaryNameToRemove.empty()) {
            removeVariable(primaryNameToRemove);
        } else {
            spdlog::warn("Variable or alias not found: {}", name);
        }
    }
}

auto VariableManager::getAllVariables() const -> std::vector<std::string> {
    spdlog::debug("Getting all primary variables");

    std::vector<std::string> variableNames;
    variableNames.reserve(variables_.size());

    for (const auto& [name, info] : variables_) {
        bool isAlias = std::any_of(
            variables_.begin(), variables_.end(), [&name](const auto& pair) {
                return pair.first != name && pair.second.alias == name;
            });
        if (!isAlias) {
            variableNames.push_back(name);
        }
    }
    return variableNames;
}

auto VariableManager::getVariablesByGroup(const std::string& group) const
    -> std::vector<std::string> {
    spdlog::debug("Getting variables for group: {}", group);

    std::vector<std::string> result;

    auto it = groups_.find(group);
    if (it != groups_.end()) {
        result.reserve(it->second.size());

        for (const auto& name : it->second) {
            auto varIt = variables_.find(name);
            if (varIt != variables_.end()) {
                bool isAlias = std::any_of(variables_.begin(), variables_.end(),
                                           [&name](const auto& pair) {
                                               return pair.first != name &&
                                                      pair.second.alias == name;
                                           });

                if (!isAlias) {
                    result.push_back(name);
                }
            }
        }
    } else {
        spdlog::debug("Group not found: {}", group);
    }

    return result;
}

void VariableManager::exportVariablesToJson(const std::string& filePath) const {
    spdlog::info("Exporting variables to JSON file: {}", filePath);

    nlohmann::json jsonData;

    for (const auto& [name, info] : variables_) {
        bool isAlias = std::any_of(
            variables_.begin(), variables_.end(), [&name](const auto& pair) {
                return pair.first != name && pair.second.alias == name;
            });
        if (isAlias) {
            continue;
        }

        nlohmann::json varData;
        varData["description"] = info.description;
        varData["alias"] = info.alias;
        varData["group"] = info.group;

        try {
            if (info.variable.type() ==
                typeid(std::shared_ptr<Trackable<int>>)) {
                auto ptr = std::any_cast<std::shared_ptr<Trackable<int>>>(
                    info.variable);
                varData["type"] = "int";
                varData["value"] = ptr->get();
            } else if (info.variable.type() ==
                       typeid(std::shared_ptr<Trackable<double>>)) {
                auto ptr = std::any_cast<std::shared_ptr<Trackable<double>>>(
                    info.variable);
                varData["type"] = "double";
                varData["value"] = ptr->get();
            } else if (info.variable.type() ==
                       typeid(std::shared_ptr<Trackable<float>>)) {
                auto ptr = std::any_cast<std::shared_ptr<Trackable<float>>>(
                    info.variable);
                varData["type"] = "float";
                varData["value"] = ptr->get();
            } else if (info.variable.type() ==
                       typeid(std::shared_ptr<Trackable<bool>>)) {
                auto ptr = std::any_cast<std::shared_ptr<Trackable<bool>>>(
                    info.variable);
                varData["type"] = "bool";
                varData["value"] = ptr->get();
            } else if (info.variable.type() ==
                       typeid(std::shared_ptr<Trackable<std::string>>)) {
                auto ptr =
                    std::any_cast<std::shared_ptr<Trackable<std::string>>>(
                        info.variable);
                varData["type"] = "string";
                varData["value"] = ptr->get();
            } else {
                varData["type"] = "unknown";
                varData["value"] = nullptr;
                spdlog::warn(
                    "Unknown type for variable '{}', value not exported", name);
            }
        } catch (const std::bad_any_cast& e) {
            spdlog::warn("Failed to cast variable '{}': {}", name, e.what());
            varData["type"] = "unknown";
            varData["value"] = nullptr;
        }

        auto rangeIt = ranges_.find(name);
        if (rangeIt != ranges_.end()) {
            try {
                std::string currentType = varData["type"];
                if (currentType == "int") {
                    struct Range {
                        int min;
                        int max;
                    };
                    if (auto* rangePtr =
                            std::any_cast<Range>(&rangeIt->second)) {
                        varData["min"] = rangePtr->min;
                        varData["max"] = rangePtr->max;
                    }
                } else if (currentType == "double") {
                    struct Range {
                        double min;
                        double max;
                    };
                    if (auto* rangePtr =
                            std::any_cast<Range>(&rangeIt->second)) {
                        varData["min"] = rangePtr->min;
                        varData["max"] = rangePtr->max;
                    }
                } else if (currentType == "float") {
                    struct Range {
                        float min;
                        float max;
                    };
                    if (auto* rangePtr =
                            std::any_cast<Range>(&rangeIt->second)) {
                        varData["min"] = rangePtr->min;
                        varData["max"] = rangePtr->max;
                    }
                }
            } catch (const std::bad_any_cast&) {
                spdlog::warn(
                    "Failed to cast range for variable '{}' with type '{}'",
                    name, varData["type"].get<std::string>());
            }
        }

        auto optionsIt = stringOptions_.find(name);
        if (optionsIt != stringOptions_.end()) {
            if (varData["type"] == "string") {
                varData["options"] = optionsIt->second;
            } else {
                spdlog::warn(
                    "Found string options for non-string variable '{}' (type: "
                    "{}), options not exported.",
                    name, varData["type"].get<std::string>());
            }
        }

        jsonData[name] = varData;
    }

    try {
        std::ofstream file(filePath);
        if (!file) {
            spdlog::error("Failed to open file for writing: {}", filePath);
            return;
        }
        file << jsonData.dump(4);
        file.close();
        spdlog::info("Successfully exported variables to {}", filePath);
    } catch (const std::exception& e) {
        spdlog::error("Failed to export variables to JSON: {}", e.what());
    }
}

void VariableManager::importVariablesFromJson(const std::string& filePath) {
    spdlog::info("Importing variables from JSON file: {}", filePath);

    std::ifstream file(filePath);
    if (!file) {
        spdlog::error("Failed to open file for reading: {}", filePath);
        return;
    }

    nlohmann::json jsonData;
    try {
        file >> jsonData;
        file.close();
    } catch (const nlohmann::json::parse_error& e) {
        spdlog::error("Failed to parse JSON file '{}': {}", filePath, e.what());
        file.close();
        return;
    } catch (const std::exception& e) {
        spdlog::error("Failed read from file '{}': {}", filePath, e.what());
        file.close();
        return;
    }

    try {
        for (auto it = jsonData.begin(); it != jsonData.end(); ++it) {
            const std::string& name = it.key();
            const nlohmann::json& varData = it.value();

            if (!varData.is_object() || !varData.contains("type") ||
                !varData.contains("value")) {
                spdlog::warn(
                    "Skipping invalid entry for variable '{}' in JSON.", name);
                continue;
            }

            std::string type = varData.value("type", "unknown");
            std::string description = varData.value("description", "");
            std::string alias = varData.value("alias", "");
            std::string group = varData.value("group", "");

            bool nameExists = has(name);
            bool aliasExists = !alias.empty() && has(alias);

            if (nameExists) {
                spdlog::info("Variable '{}' already exists, updating value.",
                             name);
                try {
                    if (type == "int") {
                        setValue<int>(name, varData["value"].get<int>());
                    } else if (type == "double") {
                        setValue<double>(name, varData["value"].get<double>());
                    } else if (type == "float") {
                        setValue<float>(name, varData["value"].get<float>());
                    } else if (type == "bool") {
                        setValue<bool>(name, varData["value"].get<bool>());
                    } else if (type == "string") {
                        setValue<std::string>(
                            name, varData["value"].get<std::string>());
                        if (varData.contains("options")) {
                            try {
                                std::vector<std::string> options =
                                    varData["options"]
                                        .get<std::vector<std::string>>();
                                setStringOptions(name, options);
                            } catch (const std::exception& e) {
                                spdlog::warn(
                                    "Failed to set string options for existing "
                                    "variable '{}': {}",
                                    name, e.what());
                            }
                        }
                    } else {
                        spdlog::warn(
                            "Unknown type '{}' for existing variable '{}', "
                            "cannot update value.",
                            type, name);
                    }
                } catch (const std::exception& e) {
                    spdlog::error(
                        "Failed to update value for variable '{}': {}", name,
                        e.what());
                }
            } else if (aliasExists) {
                spdlog::warn(
                    "Skipping import for variable '{}': its alias '{}' already "
                    "exists as a variable or alias.",
                    name, alias);
            } else {
                spdlog::info("Adding new variable '{}' from JSON.", name);
                try {
                    if (type == "int") {
                        int value = varData["value"].get<int>();
                        addVariable(name, value, description, alias, group);
                        if (varData.contains("min") &&
                            varData.contains("max")) {
                            setRange(name, varData["min"].get<int>(),
                                     varData["max"].get<int>());
                        }
                    } else if (type == "double") {
                        double value = varData["value"].get<double>();
                        addVariable(name, value, description, alias, group);
                        if (varData.contains("min") &&
                            varData.contains("max")) {
                            setRange(name, varData["min"].get<double>(),
                                     varData["max"].get<double>());
                        }
                    } else if (type == "float") {
                        float value = varData["value"].get<float>();
                        addVariable(name, value, description, alias, group);
                        if (varData.contains("min") &&
                            varData.contains("max")) {
                            setRange(name, varData["min"].get<float>(),
                                     varData["max"].get<float>());
                        }
                    } else if (type == "bool") {
                        bool value = varData["value"].get<bool>();
                        addVariable(name, value, description, alias, group);
                    } else if (type == "string") {
                        std::string value = varData["value"].get<std::string>();
                        addVariable(name, value, description, alias, group);
                        if (varData.contains("options")) {
                            std::vector<std::string> options =
                                varData["options"]
                                    .get<std::vector<std::string>>();
                            setStringOptions(name, options);
                        }
                    } else {
                        spdlog::warn(
                            "Unknown type '{}' for new variable '{}', skipping "
                            "import.",
                            type, name);
                    }
                } catch (const std::exception& e) {
                    spdlog::error("Failed to add variable '{}' from JSON: {}",
                                  name, e.what());
                    if (has(name)) {
                        try {
                            removeVariable(name);
                            spdlog::info(
                                "Cleaned up partially added variable '{}'",
                                name);
                        } catch (const std::exception& remove_e) {
                            spdlog::error(
                                "Failed to cleanup partially added variable "
                                "'{}': {}",
                                name, remove_e.what());
                        }
                    }
                }
            }
        }
        spdlog::info("Finished importing variables from JSON file: {}",
                     filePath);
    } catch (const std::exception& e) {
        spdlog::error("An error occurred during JSON import process: {}",
                      e.what());
    }
}

void VariableManager::setStringOptions(const std::string& name,
                                       std::span<const std::string> options) {
    spdlog::info("Setting string options for variable: {}", name);

    std::string primaryName = name;
    std::shared_ptr<Trackable<std::string>> trackableVar = nullptr;
    auto it = variables_.find(name);

    if (it != variables_.end()) {
        try {
            trackableVar =
                std::any_cast<std::shared_ptr<Trackable<std::string>>>(
                    it->second.variable);
        } catch (const std::bad_any_cast&) {
            spdlog::error("Variable '{}' is not of type string.", name);
            THROW_TYPE_ERROR("Variable '{}' is not of type string.", name);
        }
    } else {
        bool foundAlias = false;
        for (const auto& [p_name, info] : variables_) {
            if (info.alias == name) {
                try {
                    trackableVar =
                        std::any_cast<std::shared_ptr<Trackable<std::string>>>(
                            info.variable);
                    primaryName = p_name;
                    foundAlias = true;
                    break;
                } catch (const std::bad_any_cast&) {
                    spdlog::error(
                        "Variable alias '{}' points to a non-string variable "
                        "'{}'.",
                        name, p_name);
                    THROW_TYPE_ERROR(
                        "Variable alias '{}' points to a non-string variable "
                        "'{}'.",
                        name, p_name);
                }
            }
        }
        if (!foundAlias) {
            spdlog::warn("Variable or alias not found: {}", name);
            THROW_OBJ_NOT_EXIST(name);
        }
    }

    stringOptions_[primaryName].assign(options.begin(), options.end());

    const std::string& currentValue = trackableVar->get();
    const auto& opts = stringOptions_[primaryName];
    if (std::find(opts.begin(), opts.end(), currentValue) == opts.end()) {
        spdlog::error(
            "Current value '{}' is not valid with the new options for variable "
            "'{}'.",
            currentValue, primaryName);
        stringOptions_.erase(primaryName);
        THROW_INVALID_ARGUMENT(
            "Current value '{}' is not valid with the new options for variable "
            "'{}'",
            currentValue, primaryName);
    }

    trackableVar->subscribe(
        [this, primaryName]([[maybe_unused]] const std::string& oldValue,
                            const std::string& newValue) {
            const auto& currentOpts = stringOptions_[primaryName];
            if (std::find(currentOpts.begin(), currentOpts.end(), newValue) ==
                currentOpts.end()) {
                THROW_INVALID_ARGUMENT("Invalid option '{}' for variable '{}'",
                                       newValue, primaryName);
            }
        });

    spdlog::info(
        "Successfully set string options for variable '{}' (primary name: "
        "'{}')",
        name, primaryName);
}
