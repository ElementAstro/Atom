#include "var.hpp"

#include <algorithm>
#include <fstream>

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

void VariableManager::setValue(const std::string& name, const char* newValue) {
    LOG_F(INFO, "Setting value for variable: {}", name);
    setValue(name, std::string(newValue));
}

auto VariableManager::has(const std::string& name) const -> bool {
    LOG_F(INFO, "Checking if variable exists: {}", name);
    // Check primary name first
    if (variables_.contains(name)) {
        return true;
    }
    // Check if the name provided is an alias for another variable
    for (const auto& [key, value] : variables_) {
        if (value.alias == name) {
            return true;  // Found an alias matching the name
        }
    }
    return false;  // Not found as primary name or alias
}

auto VariableManager::getDescription(const std::string& name) const
    -> std::string {
    LOG_F(INFO, "Getting description for variable: {}", name);
    if (auto it = variables_.find(name); it != variables_.end()) {
        return it->second.description;
    }
    // Check if the name is an alias
    for (const auto& [key, value] : variables_) {
        if (value.alias == name) {
            return value
                .description;  // Return description of the original variable
        }
    }
    LOG_F(WARNING, "Variable or alias not found: {}", name);
    return "";  // Return empty string if not found
}

auto VariableManager::getAlias(const std::string& name) const -> std::string {
    LOG_F(INFO, "Getting alias for variable: {}", name);
    if (auto it = variables_.find(name); it != variables_.end()) {
        // If 'name' is a primary variable name, return its alias
        return it->second.alias;
    }
    // If 'name' might be an alias itself, find the primary variable name
    for (const auto& [key, value] : variables_) {
        if (value.alias == name) {
            // 'name' is an alias, return the primary variable name 'key'
            return key;
        }
    }
    LOG_F(WARNING, "Variable or alias not found: {}", name);
    return "";  // Return empty string if not found
}

auto VariableManager::getGroup(const std::string& name) const -> std::string {
    LOG_F(INFO, "Getting group for variable: {}", name);
    if (auto it = variables_.find(name); it != variables_.end()) {
        return it->second.group;
    }
    // Check if the name is an alias
    for (const auto& [key, value] : variables_) {
        if (value.alias == name) {
            return value.group;  // Return group of the original variable
        }
    }
    LOG_F(WARNING, "Variable or alias not found: {}", name);
    return "";  // Return empty string if not found
}

void VariableManager::removeVariable(const std::string& name) {
    LOG_F(INFO, "Removing variable: {}", name);

    // Find the variable by primary name
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        const auto& info = it->second;

        // Remove from group mapping if it belongs to one
        if (!info.group.empty()) {
            auto groupIt = groups_.find(info.group);
            if (groupIt != groups_.end()) {
                groupIt->second.erase(name);  // Erase the primary name
                // If the variable had an alias, remove the alias from the group
                // too
                if (!info.alias.empty()) {
                    groupIt->second.erase(info.alias);
                }
                // If the group becomes empty, remove the group entry
                if (groupIt->second.empty()) {
                    groups_.erase(groupIt);
                }
            }
        }

        // Remove associated range and string options
        ranges_.erase(name);
        stringOptions_.erase(name);

        // If the variable has an alias, remove the alias entry as well
        if (!info.alias.empty()) {
            variables_.erase(info.alias);
            ranges_.erase(info.alias);  // Also remove potential alias entries
                                        // in other maps
            stringOptions_.erase(info.alias);
        }

        // Finally, remove the primary variable entry
        variables_.erase(it);

    } else {
        // Check if 'name' is an alias
        std::string primaryNameToRemove;
        std::string aliasToRemove = name;  // The name provided is the alias
        for (auto const& [primaryName, info] : variables_) {
            if (info.alias == aliasToRemove) {
                primaryNameToRemove = primaryName;
                break;
            }
        }

        if (!primaryNameToRemove.empty()) {
            // Found the primary variable associated with the alias, call
            // removeVariable with the primary name
            removeVariable(primaryNameToRemove);
        } else {
            LOG_F(WARNING, "Variable or alias not found: {}", name);
        }
    }
}

auto VariableManager::getAllVariables() const -> std::vector<std::string> {
    LOG_F(INFO, "Getting all primary variables");
    std::vector<std::string> variableNames;
    variableNames.reserve(variables_.size());  // Reserve might be slightly more
                                               // than needed if aliases exist
    for (const auto& [name, info] : variables_) {
        // Only add primary variable names (those whose name is not an alias of
        // another)
        bool isAlias = false;
        for (const auto& [otherName, otherInfo] : variables_) {
            // Check if 'name' exists as an alias in any other VariableInfo
            if (name != otherName && otherInfo.alias == name) {
                isAlias = true;
                break;
            }
        }
        if (!isAlias) {
            variableNames.push_back(name);
        }
    }
    return variableNames;
}

auto VariableManager::getVariablesByGroup(const std::string& group) const
    -> std::vector<std::string> {
    LOG_F(INFO, "Getting variables for group: {}", group);
    std::vector<std::string> result;

    auto it = groups_.find(group);
    if (it != groups_.end()) {
        result.reserve(it->second.size());

        // Find all primary variable names in the group
        for (const auto& name : it->second) {
            // Check if name is a primary variable (not just an alias)
            auto varIt = variables_.find(name);
            if (varIt != variables_.end()) {
                // Check if this name isn't an alias of another variable
                bool isAlias = false;
                for (const auto& [primaryName, info] : variables_) {
                    if (primaryName != name && info.alias == name) {
                        isAlias = true;
                        break;
                    }
                }

                if (!isAlias) {
                    // This is a primary variable name, add it to the result
                    result.push_back(name);
                }
            }
        }
    } else {
        LOG_F(INFO, "Group not found: {}", group);
    }

    return result;
}

void VariableManager::exportVariablesToJson(const std::string& filePath) const {
    LOG_F(INFO, "Exporting variables to JSON file: {}", filePath);

    nlohmann::json jsonData;

    for (const auto& [name, info] : variables_) {
        // Check if the current 'name' is an alias. If so, skip it.
        // We only want to export primary variables.
        bool isAlias = false;
        for (const auto& [otherName, otherInfo] : variables_) {
            if (name != otherName && otherInfo.alias == name) {
                isAlias = true;
                break;
            }
        }
        if (isAlias) {
            continue;  // Skip aliases, export only primary variables
        }

        nlohmann::json varData;
        varData["description"] = info.description;
        varData["alias"] = info.alias;  // Export the alias if it exists
        varData["group"] = info.group;

        try {
            // Use if-else if structure for type checking
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
                LOG_F(WARNING,
                      "Unknown type for variable '{}', value not exported",
                      name);
            }
        } catch (const std::bad_any_cast& e) {
            LOG_F(WARNING, "Failed to cast variable '{}': {}", name, e.what());
            varData["type"] = "unknown";
            varData["value"] = nullptr;
        }

        // Export range if it exists
        auto rangeIt = ranges_.find(name);
        if (rangeIt != ranges_.end()) {
            try {
                // Check the type stored in varData, not info.variable.type()
                // again
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
                // No range for bool or string typically, but handle potential
                // bad_any_cast
            } catch (const std::bad_any_cast&) {
                LOG_F(WARNING,
                      "Failed to cast range for variable '{}' with type '{}'",
                      name, varData["type"].get<std::string>());
            }
        }

        // Export string options if they exist
        auto optionsIt = stringOptions_.find(name);
        if (optionsIt != stringOptions_.end()) {
            // Ensure the type is actually string before exporting options
            if (varData["type"] == "string") {
                varData["options"] = optionsIt->second;
            } else {
                LOG_F(WARNING,
                      "Found string options for non-string variable '{}' "
                      "(type: {}), options not exported.",
                      name, varData["type"].get<std::string>());
            }
        }

        jsonData[name] = varData;  // Add data for the primary variable name
    }

    try {
        std::ofstream file(filePath);
        if (!file) {
            LOG_F(ERROR, "Failed to open file for writing: {}", filePath);
            // Consider throwing an exception or returning a bool/status code
            return;
        }
        file << jsonData.dump(4);  // Use 4 spaces for indentation
        file.close();              // Ensure file is closed
        LOG_F(INFO, "Successfully exported variables to {}", filePath);
    } catch (const std::exception& e) {
        // Catch potential exceptions during file writing or JSON dumping
        LOG_F(ERROR, "Failed to export variables to JSON: {}", e.what());
        // Consider re-throwing or returning an error status
    }
}

void VariableManager::importVariablesFromJson(const std::string& filePath) {
    LOG_F(INFO, "Importing variables from JSON file: {}", filePath);

    std::ifstream file(filePath);
    if (!file) {
        LOG_F(ERROR, "Failed to open file for reading: {}", filePath);
        // Consider throwing an exception or returning a bool/status code
        return;
    }

    nlohmann::json jsonData;
    try {
        file >> jsonData;
        file.close();  // Close the file after reading
    } catch (const nlohmann::json::parse_error& e) {
        LOG_F(ERROR, "Failed to parse JSON file '{}': {}", filePath, e.what());
        file.close();  // Ensure file is closed even on error
        return;        // Stop import if JSON is invalid
    } catch (
        const std::exception& e) {  // Catch other potential ifstream errors
        LOG_F(ERROR, "Failed read from file '{}': {}", filePath, e.what());
        file.close();
        return;
    }

    try {
        for (auto it = jsonData.begin(); it != jsonData.end(); ++it) {
            const std::string& name = it.key();
            const nlohmann::json& varData = it.value();

            // Basic validation of varData structure
            if (!varData.is_object() || !varData.contains("type") ||
                !varData.contains("value")) {
                LOG_F(WARNING,
                      "Skipping invalid entry for variable '{}' in JSON.",
                      name);
                continue;
            }

            std::string type = varData.value("type", "unknown");
            std::string description = varData.value("description", "");
            std::string alias = varData.value("alias", "");
            std::string group = varData.value("group", "");

            // Check if the variable 'name' or its potential 'alias' already
            // exists
            bool nameExists = has(name);
            bool aliasExists = !alias.empty() && has(alias);

            if (nameExists) {
                // Variable exists, update its value
                LOG_F(INFO, "Variable '{}' already exists, updating value.",
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
                        // Re-apply string options if they exist in JSON, even
                        // for existing vars
                        if (varData.contains("options")) {
                            try {
                                std::vector<std::string> options =
                                    varData["options"]
                                        .get<std::vector<std::string>>();
                                // Use the public setStringOptions which handles
                                // validation
                                setStringOptions(name, options);
                            } catch (const std::exception& e) {
                                LOG_F(WARNING,
                                      "Failed to set string options for "
                                      "existing variable '{}': {}",
                                      name, e.what());
                            }
                        }
                    } else {
                        LOG_F(WARNING,
                              "Unknown type '{}' for existing variable '{}', "
                              "cannot update value.",
                              type, name);
                    }
                    // Note: We don't update description, alias, group, or range
                    // for existing variables here. Decide if this is the
                    // desired behavior. If updates are needed, add logic here.

                } catch (const std::exception&
                             e) {  // Catch errors during setValue (type
                                   // mismatch, range error, etc.)
                    LOG_F(ERROR, "Failed to update value for variable '{}': {}",
                          name, e.what());
                }
            } else if (aliasExists) {
                // The primary name doesn't exist, but the alias does. This is a
                // conflict.
                LOG_F(WARNING,
                      "Skipping import for variable '{}': its alias '{}' "
                      "already exists as a variable or alias.",
                      name, alias);
            } else {
                // Variable does not exist and alias (if provided) doesn't
                // conflict, add it.
                LOG_F(INFO, "Adding new variable '{}' from JSON.", name);
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
                            // Use the public setStringOptions which handles
                            // validation
                            setStringOptions(name, options);
                        }
                    } else {
                        LOG_F(WARNING,
                              "Unknown type '{}' for new variable '{}', "
                              "skipping import.",
                              type, name);
                    }
                } catch (const std::exception&
                             e) {  // Catch errors during addVariable, setRange,
                                   // setStringOptions
                    LOG_F(ERROR, "Failed to add variable '{}' from JSON: {}",
                          name, e.what());
                    // Consider if partial addition should be rolled back
                    // (tricky without transactions) If addVariable succeeded
                    // but setRange/setStringOptions failed, the variable exists
                    // but is incomplete. We might need to remove the partially
                    // added variable.
                    if (has(name)) {  // Check if addVariable succeeded before
                                      // the error
                        try {
                            removeVariable(name);  // Attempt cleanup
                            LOG_F(INFO,
                                  "Cleaned up partially added variable '{}'",
                                  name);
                        } catch (const std::exception& remove_e) {
                            LOG_F(ERROR,
                                  "Failed to cleanup partially added variable "
                                  "'{}': {}",
                                  name, remove_e.what());
                        }
                    }
                }
            }
        }
        LOG_F(INFO, "Finished importing variables from JSON file: {}",
              filePath);
    } catch (const std::exception& e) {
        // Catch errors during the loop (e.g., JSON access errors not caught
        // earlier)
        LOG_F(ERROR, "An error occurred during JSON import process: {}",
              e.what());
    }
}

// Definition for setStringOptions using std::span
void VariableManager::setStringOptions(const std::string& name,
                                       std::span<const std::string> options) {
    LOG_F(INFO, "Setting string options for variable: {}", name);

    // Find the variable, considering aliases
    std::string primaryName = name;
    std::shared_ptr<Trackable<std::string>> trackableVar = nullptr;
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        // 'name' is the primary name
        try {
            trackableVar =
                std::any_cast<std::shared_ptr<Trackable<std::string>>>(
                    it->second.variable);
        } catch (const std::bad_any_cast&) {
            LOG_F(ERROR, "Variable '{}' is not of type string.", name);
            THROW_TYPE_ERROR("Variable '{}' is not of type string.", name);
        }
    } else {
        // Check if 'name' is an alias
        bool foundAlias = false;
        for (const auto& [p_name, info] : variables_) {
            if (info.alias == name) {
                try {
                    trackableVar =
                        std::any_cast<std::shared_ptr<Trackable<std::string>>>(
                            info.variable);
                    primaryName = p_name;  // Store the primary name
                    foundAlias = true;
                    break;
                } catch (const std::bad_any_cast&) {
                    LOG_F(ERROR,
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
            LOG_F(WARNING, "Variable or alias not found: {}", name);
            THROW_OBJ_NOT_EXIST(name);
        }
    }

    // Store options using the primary name
    stringOptions_[primaryName].assign(options.begin(), options.end());

    // Validate the current value against the new options
    const std::string& currentValue = trackableVar->get();
    const auto& opts =
        stringOptions_[primaryName];  // Use primary name to get options
    if (std::find(opts.begin(), opts.end(), currentValue) == opts.end()) {
        LOG_F(ERROR,
              "Current value '{}' is not valid with the new options for "
              "variable '{}'.",
              currentValue, primaryName);
        // Decide on behavior: throw, reset to default, log warning? Throwing
        // seems consistent. Clear the options we just set because the state is
        // invalid.
        stringOptions_.erase(primaryName);
        THROW_INVALID_ARGUMENT(
            "Current value '{}' is not valid with the new options for variable "
            "'{}'",
            currentValue, primaryName);
    }

    // Subscribe to future changes using the primary name for map access
    // Use a capture list that copies necessary data or uses stable references.
    // Capturing 'this' and 'primaryName' (by value) is safe.
    trackableVar->subscribe(
        [this, primaryName]([[maybe_unused]] const std::string& oldValue,
                            const std::string& newValue) {
            // Use primaryName to access the options map inside the lambda
            const auto& currentOpts = stringOptions_[primaryName];
            if (std::find(currentOpts.begin(), currentOpts.end(), newValue) ==
                currentOpts.end()) {
                // Throw using the original name/alias the user interacted with
                // might be better, but requires capturing 'name' too. Let's
                // stick to primaryName for consistency internally.
                THROW_INVALID_ARGUMENT("Invalid option '{}' for variable '{}'",
                                       newValue, primaryName);
            }
        });
    LOG_F(INFO,
          "Successfully set string options for variable '{}' (primary name: "
          "'{}')",
          name, primaryName);
}

// Note: Template function definitions should typically reside in the header
// file (var.hpp) unless they are explicitly instantiated in the .cpp file for
// all required types. Since the definitions for addVariable, setRange,
// getVariable, setValue<T>, forEachVariable are already in var.hpp according to
// the provided context, they should remain there. This .cpp file should only
// contain definitions for non-template member functions.
