#ifndef ATOM_TYPE_JSON_SCHEMA_HPP
#define ATOM_TYPE_JSON_SCHEMA_HPP

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "atom/macro.hpp"
#include "atom/type/json.hpp"

namespace atom::type {

using json = nlohmann::json;

/**
 * @brief Enum representing JSON Schema specification versions
 */
enum class SchemaVersion {
    DRAFT4,
    DRAFT6,
    DRAFT7,
    DRAFT2019_09,
    DRAFT2020_12,
    AUTO_DETECT
};

/**
 * @brief Structure to store validation error information
 */
struct ValidationError {
    std::string message;
    std::string path;
    std::string schema_path;
    std::string instance_snippet;
    std::string error_code;

    ValidationError() = default;

    ValidationError(std::string msg, std::string p = "", std::string sp = "",
                    std::string is = "", std::string ec = "")
        : message(std::move(msg)),
          path(std::move(p)),
          schema_path(std::move(sp)),
          instance_snippet(std::move(is)),
          error_code(std::move(ec)) {}

    /**
     * @brief Converts error to JSON format
     * @return JSON object representing the error
     */
    [[nodiscard]] json toJson() const {
        json error_json = {{"message", message}, {"path", path}};

        if (!schema_path.empty())
            error_json["schemaPath"] = schema_path;
        if (!instance_snippet.empty())
            error_json["instanceSnippet"] = instance_snippet;
        if (!error_code.empty())
            error_json["errorCode"] = error_code;

        return error_json;
    }
} ATOM_ALIGNAS(64);

/**
 * @brief Configuration options for JSON Schema validation
 */
struct ValidationOptions {
    bool fail_fast{false};
    bool validate_schema{true};
    bool ignore_format{false};
    bool allow_undefined_formats{true};
    size_t max_errors{100};
    size_t max_recursion_depth{64};
    size_t max_reference_depth{16};
    std::string base_uri{};
    SchemaVersion schema_version{SchemaVersion::AUTO_DETECT};
};

/**
 * @brief Custom exception for schema validation errors
 */
class SchemaValidationException : public std::runtime_error {
public:
    explicit SchemaValidationException(std::string message)
        : std::runtime_error(std::move(message)) {}
};

class SchemaManager;

/**
 * @brief Enhanced JSON Schema validator with full JSON Schema draft support
 */
class JsonValidator {
public:
    using Format_validator = std::function<bool(const std::string&)>;

    /**
     * @brief Constructor
     * @param options Validation options
     */
    explicit JsonValidator(ValidationOptions options = {})
        : options_(std::move(options)) {
        initializeFormatValidators();
    }

    /**
     * @brief Sets the root schema
     * @param schema_json JSON formatted schema
     * @param id Optional schema ID
     * @throws SchemaValidationException if schema is invalid
     */
    void setRootSchema(const json& schema_json, const std::string& id = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        root_schema_ = schema_json;
        schema_id_ = id.empty() ? extractId(schema_json) : id;

        resetState();

        if (options_.schema_version == SchemaVersion::AUTO_DETECT) {
            detectSchemaVersion(schema_json);
        }

        if (options_.validate_schema) {
            validateSchemaAgainstMetaSchema(schema_json);
        }

        compileSchema(schema_json);
    }

    /**
     * @brief Validates the given JSON instance against the schema
     * @param instance JSON instance to validate
     * @return true if validation passes, false if validation fails
     * @throws SchemaValidationException for critical errors
     */
    [[nodiscard]] bool validate(const json& instance) {
        std::lock_guard<std::mutex> lock(mutex_);
        resetValidationState();

        if (root_schema_.is_null()) {
            errors_.emplace_back("No schema has been set", "");
            return false;
        }

        current_recursion_depth_ = 0;
        current_ref_depth_ = 0;

        try {
            validateSchema(instance, root_schema_, "", "#");
        } catch (const SchemaValidationException& e) {
            errors_.emplace_back(std::string("Validation aborted: ") + e.what(),
                                 "");
        } catch (const std::exception& e) {
            errors_.emplace_back(std::string("Unexpected error: ") + e.what(),
                                 "");
        }

        return errors_.empty();
    }

    /**
     * @brief Gets the validation errors
     * @return Vector of validation errors
     */
    [[nodiscard]] const std::vector<ValidationError>& getErrors()
        const noexcept {
        return errors_;
    }

    /**
     * @brief Gets the validation errors as a JSON array
     * @return JSON array of validation errors
     */
    [[nodiscard]] json getErrorsAsJson() const {
        json error_array = json::array();
        for (const auto& error : errors_) {
            error_array.push_back(error.toJson());
        }
        return error_array;
    }

    /**
     * @brief Registers a custom format validator
     * @param format_name Name of the format
     * @param validator Function that validates strings against this format
     */
    void registerFormatValidator(std::string format_name,
                                 Format_validator validator) {
        std::lock_guard<std::mutex> lock(mutex_);
        format_validators_[std::move(format_name)] = std::move(validator);
    }

    /**
     * @brief Links this validator with a schema manager for $ref resolution
     * @param manager Pointer to schema manager
     */
    void setSchemaManager(std::shared_ptr<SchemaManager> manager) {
        std::lock_guard<std::mutex> lock(mutex_);
        schema_manager_ = std::move(manager);
    }

    /**
     * @brief Gets the detected schema version
     * @return Schema version
     */
    [[nodiscard]] SchemaVersion getSchemaVersion() const noexcept {
        return options_.schema_version;
    }

    /**
     * @brief Gets the schema ID
     * @return Schema ID as string
     */
    [[nodiscard]] const std::string& getSchemaId() const noexcept {
        return schema_id_;
    }

    /**
     * @brief Updates validation options
     * @param options New validation options
     */
    void setOptions(ValidationOptions options) {
        std::lock_guard<std::mutex> lock(mutex_);
        options_ = std::move(options);
    }

private:
    json root_schema_;
    std::string schema_id_;
    std::vector<ValidationError> errors_;
    ValidationOptions options_;
    std::unordered_map<std::string, Format_validator> format_validators_;
    std::weak_ptr<SchemaManager> schema_manager_;
    std::mutex mutex_;

    std::unordered_map<std::string, json> uri_to_schema_map_;
    std::unordered_map<std::string, std::regex> regex_cache_;

    std::atomic<size_t> current_recursion_depth_{0};
    std::atomic<size_t> current_ref_depth_{0};

    /**
     * @brief Resets the validator state
     */
    void resetState() {
        errors_.clear();
        uri_to_schema_map_.clear();
        regex_cache_.clear();
    }

    /**
     * @brief Resets only the validation state (errors and counters)
     */
    void resetValidationState() {
        errors_.clear();
        current_recursion_depth_ = 0;
        current_ref_depth_ = 0;
    }

    /**
     * @brief Extract schema ID from schema
     * @param schema Schema to extract ID from
     * @return Schema ID or empty string if not found
     */
    [[nodiscard]] std::string extractId(const json& schema) const {
        if (schema.is_object()) {
            if (schema.contains("$id"))
                return schema["$id"].get<std::string>();
            if (schema.contains("id"))
                return schema["id"].get<std::string>();
        }
        return "";
    }

    /**
     * @brief Detect schema version from schema
     * @param schema Schema to analyze
     */
    void detectSchemaVersion(const json& schema) {
        if (schema.is_object()) {
            if (schema.contains("$schema")) {
                const std::string& schema_uri =
                    schema["$schema"].get<std::string>();

                if (schema_uri.find("draft/2020-12") != std::string::npos) {
                    options_.schema_version = SchemaVersion::DRAFT2020_12;
                } else if (schema_uri.find("draft/2019-09") !=
                           std::string::npos) {
                    options_.schema_version = SchemaVersion::DRAFT2019_09;
                } else if (schema_uri.find("draft-07") != std::string::npos) {
                    options_.schema_version = SchemaVersion::DRAFT7;
                } else if (schema_uri.find("draft-06") != std::string::npos) {
                    options_.schema_version = SchemaVersion::DRAFT6;
                } else if (schema_uri.find("draft-04") != std::string::npos) {
                    options_.schema_version = SchemaVersion::DRAFT4;
                } else {
                    options_.schema_version = SchemaVersion::DRAFT2020_12;
                }
                return;
            }

            if (schema.contains("$id")) {
                options_.schema_version = SchemaVersion::DRAFT7;
            } else if (schema.contains("id")) {
                options_.schema_version = SchemaVersion::DRAFT4;
            } else {
                options_.schema_version = SchemaVersion::DRAFT2020_12;
            }
        }
    }

    /**
     * @brief Validates schema against appropriate meta-schema
     * @param schema Schema to validate
     * @throws SchemaValidationException if schema is invalid
     */
    void validateSchemaAgainstMetaSchema(const json& schema) {
        if (!schema.is_object()) {
            throw SchemaValidationException("Schema must be a JSON object");
        }
    }

    /**
     * @brief Pre-compile schema to optimize validation
     * @param schema Schema to compile
     */
    void compileSchema(const json& schema) {
        if (schema.is_object()) {
            std::string id = extractId(schema);
            if (!id.empty()) {
                uri_to_schema_map_[id] = schema;
            }

            if (schema.contains("pattern")) {
                try {
                    const std::string& pattern =
                        schema["pattern"].get<std::string>();
                    regex_cache_[pattern] = std::regex(pattern);
                } catch (const std::exception&) {
                }
            }

            for (const auto& [key, value] : schema.items()) {
                if (value.is_object() || value.is_array()) {
                    compileSchemaRecursive(value);
                }
            }
        } else if (schema.is_array()) {
            for (const auto& item : schema) {
                if (item.is_object() || item.is_array()) {
                    compileSchemaRecursive(item);
                }
            }
        }
    }

    /**
     * @brief Recursively compile schema parts
     * @param schema_part Schema or subschema to compile
     */
    void compileSchemaRecursive(const json& schema_part) {
        if (schema_part.is_object()) {
            compileSchema(schema_part);
        } else if (schema_part.is_array()) {
            for (const auto& item : schema_part) {
                if (item.is_object() || item.is_array()) {
                    compileSchemaRecursive(item);
                }
            }
        }
    }

    /**
     * @brief Recursively validates JSON instance against the schema
     * @param instance Current JSON instance part
     * @param schema Current schema part
     * @param instance_path Current path for error messages
     * @param schema_path Current schema path for error reporting
     * @throws SchemaValidationException when validation cannot continue
     */
    void validateSchema(const json& instance, const json& schema,
                        const std::string& instance_path,
                        const std::string& schema_path) {
        if (++current_recursion_depth_ > options_.max_recursion_depth) {
            throw SchemaValidationException("Maximum recursion depth exceeded");
        }

        if (options_.fail_fast && !errors_.empty()) {
            --current_recursion_depth_;
            return;
        }

        if (errors_.size() >= options_.max_errors) {
            --current_recursion_depth_;
            return;
        }

        if (schema.is_object() && schema.contains("$ref")) {
            validateReference(instance, schema, instance_path, schema_path);
            --current_recursion_depth_;
            return;
        }

        if (schema.is_object()) {
            validateType(instance, schema, instance_path, schema_path);

            if (instance.is_object()) {
                validateObject(instance, schema, instance_path, schema_path);
            }

            if (instance.is_array()) {
                validateArray(instance, schema, instance_path, schema_path);
            }

            if (instance.is_string()) {
                validateString(instance, schema, instance_path, schema_path);
            }

            if (instance.is_number()) {
                validateNumber(instance, schema, instance_path, schema_path);
            }

            validateEnum(instance, schema, instance_path, schema_path);
            validateConst(instance, schema, instance_path, schema_path);
            validateConditionals(instance, schema, instance_path, schema_path);
            validateCombinations(instance, schema, instance_path, schema_path);
        }

        --current_recursion_depth_;
    }

    /**
     * @brief Validates a reference ($ref keyword)
     */
    void validateReference(const json& instance, const json& schema,
                           const std::string& instance_path,
                           const std::string& schema_path) {
        if (++current_ref_depth_ > options_.max_reference_depth) {
            addError("Maximum reference depth exceeded", instance_path,
                     schema_path);
            --current_ref_depth_;
            return;
        }

        const std::string& ref = schema["$ref"].get<std::string>();

        if (ref.starts_with("#")) {
            json referenced_schema =
                resolvePointer(root_schema_, ref.substr(1));
            if (!referenced_schema.is_null()) {
                validateSchema(instance, referenced_schema, instance_path,
                               schema_path + "/" + ref);
            } else {
                addError("Invalid reference: " + ref, instance_path,
                         schema_path + "/$ref");
            }
        } else {
            auto manager = schema_manager_.lock();
            if (manager) {
                addError("External references not yet implemented: " + ref,
                         instance_path, schema_path + "/$ref");
            } else {
                addError(
                    "Cannot resolve external reference without schema "
                    "manager: " +
                        ref,
                    instance_path, schema_path + "/$ref");
            }
        }

        --current_ref_depth_;
    }

    /**
     * @brief Helper to add an error with error code
     */
    void addError(const std::string& message, const std::string& instance_path,
                  const std::string& schema_path,
                  const std::string& error_code = "") {
        std::string snippet;
        errors_.emplace_back(message, instance_path, schema_path, snippet,
                             error_code);
    }

    /**
     * @brief Resolves a JSON pointer
     * @param doc Document to search in
     * @param pointer JSON pointer (without the leading #)
     * @return Referenced JSON or null if not found
     */
    json resolvePointer(const json& doc, const std::string& pointer) {
        if (pointer.empty() || pointer == "/")
            return doc;

        try {
            return doc.at(json::json_pointer(pointer));
        } catch (const std::exception&) {
            return nullptr;
        }
    }

    /**
     * @brief Validates type keyword
     */
    void validateType(const json& instance, const json& schema,
                      const std::string& instance_path,
                      const std::string& schema_path) {
        if (!schema.contains("type"))
            return;

        const auto& type_spec = schema["type"];

        if (!validateTypeValue(instance, type_spec)) {
            addError("Type mismatch, expected: " + typeToString(type_spec),
                     instance_path, schema_path + "/type", "type");
        }
    }

    /**
     * @brief Validates instance against type specification
     */
    [[nodiscard]] bool validateTypeValue(const json& instance,
                                         const json& type_spec) const noexcept {
        if (type_spec.is_string()) {
            return checkTypeString(instance, type_spec.get<std::string>());
        }
        if (type_spec.is_array()) {
            for (const auto& type : type_spec) {
                if (type.is_string() &&
                    checkTypeString(instance, type.get<std::string>())) {
                    return true;
                }
            }
            return false;
        }
        return false;
    }

    /**
     * @brief Checks if instance matches a specific type string
     */
    [[nodiscard]] static bool checkTypeString(const json& instance,
                                              std::string_view type) noexcept {
        if (type == "object")
            return instance.is_object();
        if (type == "array")
            return instance.is_array();
        if (type == "string")
            return instance.is_string();
        if (type == "number")
            return instance.is_number();
        if (type == "integer")
            return instance.is_number_integer();
        if (type == "boolean")
            return instance.is_boolean();
        if (type == "null")
            return instance.is_null();
        return false;
    }

    /**
     * @brief Converts type specification to string
     */
    [[nodiscard]] static std::string typeToString(const json& type_spec) {
        if (type_spec.is_string()) {
            return type_spec.get<std::string>();
        }

        if (type_spec.is_array()) {
            std::string result = "[";
            for (size_t i = 0; i < type_spec.size(); ++i) {
                if (i > 0)
                    result += ", ";
                result += type_spec[i].get<std::string>();
            }
            result += "]";
            return result;
        }

        return "unknown";
    }

    /**
     * @brief Validates object-specific keywords
     */
    void validateObject(const json& instance, const json& schema,
                        const std::string& instance_path,
                        const std::string& schema_path) {
        if (schema.contains("required")) {
            const auto& required = schema["required"];
            for (const auto& prop_name : required) {
                if (!instance.contains(prop_name)) {
                    addError("Missing required property: " +
                                 prop_name.get<std::string>(),
                             instance_path, schema_path + "/required",
                             "required");
                }
            }
        }

        if (schema.contains("properties")) {
            const auto& properties = schema["properties"];
            for (const auto& [prop_name, prop_schema] : properties.items()) {
                if (instance.contains(prop_name)) {
                    std::string prop_path =
                        instance_path.empty() ? prop_name
                                              : instance_path + "/" + prop_name;
                    validateSchema(instance[prop_name], prop_schema, prop_path,
                                   schema_path + "/properties/" + prop_name);
                }
            }
        }

        if (schema.contains("patternProperties")) {
            const auto& pattern_props = schema["patternProperties"];

            for (const auto& [pattern_str, pattern_schema] :
                 pattern_props.items()) {
                std::regex pattern;

                auto it = regex_cache_.find(pattern_str);
                if (it != regex_cache_.end()) {
                    pattern = it->second;
                } else {
                    try {
                        pattern = std::regex(pattern_str);
                        regex_cache_[pattern_str] = pattern;
                    } catch (const std::regex_error&) {
                        addError(
                            "Invalid regex pattern: " + pattern_str,
                            instance_path,
                            schema_path + "/patternProperties/" + pattern_str,
                            "patternProperties");
                        continue;
                    }
                }

                for (const auto& [prop_name, prop_value] : instance.items()) {
                    if (std::regex_search(prop_name, pattern)) {
                        std::string prop_path =
                            instance_path.empty()
                                ? prop_name
                                : instance_path + "/" + prop_name;
                        validateSchema(
                            prop_value, pattern_schema, prop_path,
                            schema_path + "/patternProperties/" + pattern_str);
                    }
                }
            }
        }

        if (schema.contains("additionalProperties")) {
            const auto& additional_props = schema["additionalProperties"];

            for (const auto& [prop_name, prop_value] : instance.items()) {
                bool validated = false;

                if (schema.contains("properties") &&
                    schema["properties"].contains(prop_name)) {
                    validated = true;
                }

                if (!validated && schema.contains("patternProperties")) {
                    for (const auto& [pattern_str, _] :
                         schema["patternProperties"].items()) {
                        auto it = regex_cache_.find(pattern_str);
                        std::regex pattern;

                        if (it != regex_cache_.end()) {
                            pattern = it->second;
                        } else {
                            try {
                                pattern = std::regex(pattern_str);
                                regex_cache_[pattern_str] = pattern;
                            } catch (const std::regex_error&) {
                                continue;
                            }
                        }

                        if (std::regex_search(prop_name, pattern)) {
                            validated = true;
                            break;
                        }
                    }
                }

                if (!validated) {
                    if (additional_props.is_boolean() &&
                        additional_props == false) {
                        addError(
                            "Additional property not allowed: " + prop_name,
                            instance_path,
                            schema_path + "/additionalProperties",
                            "additionalProperties");
                    } else if (additional_props.is_object()) {
                        std::string prop_path =
                            instance_path.empty()
                                ? prop_name
                                : instance_path + "/" + prop_name;
                        validateSchema(prop_value, additional_props, prop_path,
                                       schema_path + "/additionalProperties");
                    }
                }
            }
        }

        if (schema.contains("propertyNames") &&
            schema["propertyNames"].is_object()) {
            const auto& prop_names_schema = schema["propertyNames"];

            for (const auto& [prop_name, _] : instance.items()) {
                json prop_name_json = prop_name;
                std::string pseudo_path = instance_path + "/{propertyName}";
                validateSchema(prop_name_json, prop_names_schema, pseudo_path,
                               schema_path + "/propertyNames");
            }
        }

        if (schema.contains("minProperties")) {
            const auto min_props = schema["minProperties"].get<std::size_t>();
            if (instance.size() < min_props) {
                addError("Object has too few properties, minimum: " +
                             std::to_string(min_props),
                         instance_path, schema_path + "/minProperties",
                         "minProperties");
            }
        }

        if (schema.contains("maxProperties")) {
            const auto max_props = schema["maxProperties"].get<std::size_t>();
            if (instance.size() > max_props) {
                addError("Object has too many properties, maximum: " +
                             std::to_string(max_props),
                         instance_path, schema_path + "/maxProperties",
                         "maxProperties");
            }
        }

        validateDependencies(instance, schema, instance_path, schema_path);
    }

    /**
     * @brief Validates dependencies/dependentRequired/dependentSchemas keywords
     */
    void validateDependencies(const json& instance, const json& schema,
                              const std::string& instance_path,
                              const std::string& schema_path) {
        if (schema.contains("dependencies")) {
            const auto& dependencies = schema["dependencies"];

            for (const auto& [prop_name, dependency] : dependencies.items()) {
                if (instance.contains(prop_name)) {
                    if (dependency.is_array()) {
                        for (const auto& required_prop : dependency) {
                            if (!instance.contains(required_prop)) {
                                addError(
                                    "Missing dependency: " +
                                        required_prop.get<std::string>(),
                                    instance_path,
                                    schema_path + "/dependencies/" + prop_name,
                                    "dependencies");
                            }
                        }
                    } else if (dependency.is_object()) {
                        validateSchema(
                            instance, dependency, instance_path,
                            schema_path + "/dependencies/" + prop_name);
                    }
                }
            }
        }

        if (schema.contains("dependentRequired")) {
            const auto& dependent_required = schema["dependentRequired"];

            for (const auto& [prop_name, required_props] :
                 dependent_required.items()) {
                if (instance.contains(prop_name) && required_props.is_array()) {
                    for (const auto& required_prop : required_props) {
                        if (!instance.contains(required_prop)) {
                            addError(
                                "Missing dependent property: " +
                                    required_prop.get<std::string>(),
                                instance_path,
                                schema_path + "/dependentRequired/" + prop_name,
                                "dependentRequired");
                        }
                    }
                }
            }
        }

        if (schema.contains("dependentSchemas")) {
            const auto& dependent_schemas = schema["dependentSchemas"];

            for (const auto& [prop_name, dep_schema] :
                 dependent_schemas.items()) {
                if (instance.contains(prop_name)) {
                    validateSchema(
                        instance, dep_schema, instance_path,
                        schema_path + "/dependentSchemas/" + prop_name);
                }
            }
        }
    }

    /**
     * @brief Validates array-specific keywords
     */
    void validateArray(const json& instance, const json& schema,
                       const std::string& instance_path,
                       const std::string& schema_path) {
        if (schema.contains("items")) {
            const auto& items = schema["items"];

            if (items.is_object()) {
                for (std::size_t i = 0; i < instance.size(); ++i) {
                    std::string item_path =
                        instance_path + "/" + std::to_string(i);
                    validateSchema(instance[i], items, item_path,
                                   schema_path + "/items");
                }
            } else if (items.is_array()) {
                for (std::size_t i = 0;
                     i < std::min(items.size(), instance.size()); ++i) {
                    std::string item_path =
                        instance_path + "/" + std::to_string(i);
                    validateSchema(instance[i], items[i], item_path,
                                   schema_path + "/items/" + std::to_string(i));
                }

                if (instance.size() > items.size() &&
                    schema.contains("additionalItems")) {
                    const auto& additional_items = schema["additionalItems"];

                    if (additional_items.is_boolean() &&
                        additional_items == false) {
                        addError("Additional items not allowed", instance_path,
                                 schema_path + "/additionalItems",
                                 "additionalItems");
                    } else if (additional_items.is_object()) {
                        for (std::size_t i = items.size(); i < instance.size();
                             ++i) {
                            std::string item_path =
                                instance_path + "/" + std::to_string(i);
                            validateSchema(instance[i], additional_items,
                                           item_path,
                                           schema_path + "/additionalItems");
                        }
                    }
                }
            }
        }

        if (schema.contains("prefixItems") &&
            schema["prefixItems"].is_array()) {
            const auto& prefix_items = schema["prefixItems"];

            for (std::size_t i = 0;
                 i < std::min(prefix_items.size(), instance.size()); ++i) {
                std::string item_path = instance_path + "/" + std::to_string(i);
                validateSchema(
                    instance[i], prefix_items[i], item_path,
                    schema_path + "/prefixItems/" + std::to_string(i));
            }

            if (instance.size() > prefix_items.size() &&
                schema.contains("items")) {
                const auto& items = schema["items"];

                if (items.is_boolean()) {
                    if (items == false) {
                        addError("Additional items not allowed", instance_path,
                                 schema_path + "/items", "items");
                    }
                } else if (items.is_object()) {
                    for (std::size_t i = prefix_items.size();
                         i < instance.size(); ++i) {
                        std::string item_path =
                            instance_path + "/" + std::to_string(i);
                        validateSchema(instance[i], items, item_path,
                                       schema_path + "/items");
                    }
                }
            }
        }

        if (schema.contains("contains")) {
            const auto& contains_schema = schema["contains"];
            bool any_valid = false;

            std::size_t valid_count = 0;
            std::size_t min_contains = 1;
            std::size_t max_contains = std::numeric_limits<std::size_t>::max();

            if (schema.contains("minContains") &&
                schema["minContains"].is_number_unsigned()) {
                min_contains = schema["minContains"].get<std::size_t>();
            }

            if (schema.contains("maxContains") &&
                schema["maxContains"].is_number_unsigned()) {
                max_contains = schema["maxContains"].get<std::size_t>();
            }

            for (std::size_t i = 0; i < instance.size(); ++i) {
                std::size_t error_count = errors_.size();

                std::string item_path = instance_path + "/" + std::to_string(i);
                validateSchema(instance[i], contains_schema, item_path,
                               schema_path + "/contains");

                if (errors_.size() == error_count) {
                    any_valid = true;
                    valid_count++;

                    errors_.resize(error_count);

                    if (valid_count >= max_contains) {
                        break;
                    }
                } else {
                    errors_.resize(error_count);
                }
            }

            if (instance.size() > 0 &&
                (!any_valid || valid_count < min_contains)) {
                addError(
                    "Array doesn't contain required number of matching items "
                    "(min: " +
                        std::to_string(min_contains) + ")",
                    instance_path, schema_path + "/contains", "contains");
            }

            if (valid_count > max_contains) {
                addError("Array contains too many matching items (max: " +
                             std::to_string(max_contains) + ")",
                         instance_path, schema_path + "/maxContains",
                         "maxContains");
            }
        }

        if (schema.contains("minItems")) {
            const auto min_items = schema["minItems"].get<std::size_t>();
            if (instance.size() < min_items) {
                addError("Array has too few items, minimum: " +
                             std::to_string(min_items),
                         instance_path, schema_path + "/minItems", "minItems");
            }
        }

        if (schema.contains("maxItems")) {
            const auto max_items = schema["maxItems"].get<std::size_t>();
            if (instance.size() > max_items) {
                addError("Array has too many items, maximum: " +
                             std::to_string(max_items),
                         instance_path, schema_path + "/maxItems", "maxItems");
            }
        }

        if (schema.contains("uniqueItems") &&
            schema["uniqueItems"].get<bool>()) {
            std::set<json> unique_items;
            for (std::size_t i = 0; i < instance.size(); ++i) {
                const auto& item = instance[i];
                if (!unique_items.insert(item).second) {
                    addError("Array items must be unique", instance_path,
                             schema_path + "/uniqueItems", "uniqueItems");
                    break;
                }
            }
        }
    }

    /**
     * @brief Validates string-specific keywords
     */
    void validateString(const json& instance, const json& schema,
                        const std::string& instance_path,
                        const std::string& schema_path) {
        const std::string& str = instance.get<std::string>();

        if (schema.contains("minLength")) {
            const auto min_length = schema["minLength"].get<std::size_t>();
            if (str.length() < min_length) {
                addError("String is too short, minimum length: " +
                             std::to_string(min_length),
                         instance_path, schema_path + "/minLength",
                         "minLength");
            }
        }

        if (schema.contains("maxLength")) {
            const auto max_length = schema["maxLength"].get<std::size_t>();
            if (str.length() > max_length) {
                addError("String is too long, maximum length: " +
                             std::to_string(max_length),
                         instance_path, schema_path + "/maxLength",
                         "maxLength");
            }
        }

        if (schema.contains("pattern")) {
            const std::string& pattern_str =
                schema["pattern"].get<std::string>();

            std::regex pattern;
            auto it = regex_cache_.find(pattern_str);

            if (it != regex_cache_.end()) {
                pattern = it->second;
            } else {
                try {
                    pattern = std::regex(pattern_str);
                    regex_cache_[pattern_str] = pattern;
                } catch (const std::regex_error&) {
                    addError("Invalid regex pattern: " + pattern_str,
                             instance_path, schema_path + "/pattern",
                             "pattern");
                    return;
                }
            }

            if (!std::regex_search(str, pattern)) {
                addError("String does not match pattern: " + pattern_str,
                         instance_path, schema_path + "/pattern", "pattern");
            }
        }

        if (!options_.ignore_format && schema.contains("format")) {
            const std::string& format = schema["format"].get<std::string>();
            validateFormat(str, format, instance_path, schema_path);
        }
    }

    /**
     * @brief Validates a string against a format
     */
    void validateFormat(const std::string& str, const std::string& format,
                        const std::string& instance_path,
                        const std::string& schema_path) {
        auto it = format_validators_.find(format);

        if (it != format_validators_.end()) {
            if (!it->second(str)) {
                addError("String does not match format: " + format,
                         instance_path, schema_path + "/format", "format");
            }
        } else if (!options_.allow_undefined_formats) {
            addError("Unknown format: " + format, instance_path,
                     schema_path + "/format", "format");
        }
    }

    /**
     * @brief Validates number-specific keywords
     */
    void validateNumber(const json& instance, const json& schema,
                        const std::string& instance_path,
                        const std::string& schema_path) {
        const auto num_value = instance.get<double>();

        if (schema.contains("minimum")) {
            const auto minimum = schema["minimum"].get<double>();
            if (num_value < minimum) {
                addError(
                    "Value is less than minimum: " + std::to_string(minimum),
                    instance_path, schema_path + "/minimum", "minimum");
            }
        }

        if (schema.contains("exclusiveMinimum") && schema.contains("minimum")) {
            if (schema["exclusiveMinimum"].is_boolean() &&
                schema["exclusiveMinimum"].get<bool>()) {
                const auto minimum = schema["minimum"].get<double>();
                if (num_value <= minimum) {
                    addError("Value must be greater than exclusive minimum: " +
                                 std::to_string(minimum),
                             instance_path, schema_path + "/exclusiveMinimum",
                             "exclusiveMinimum");
                }
            }
        }

        if (schema.contains("exclusiveMinimum") &&
            schema["exclusiveMinimum"].is_number()) {
            const auto ex_minimum = schema["exclusiveMinimum"].get<double>();
            if (num_value <= ex_minimum) {
                addError("Value must be greater than exclusive minimum: " +
                             std::to_string(ex_minimum),
                         instance_path, schema_path + "/exclusiveMinimum",
                         "exclusiveMinimum");
            }
        }

        if (schema.contains("maximum")) {
            const auto maximum = schema["maximum"].get<double>();
            if (num_value > maximum) {
                addError(
                    "Value is greater than maximum: " + std::to_string(maximum),
                    instance_path, schema_path + "/maximum", "maximum");
            }
        }

        if (schema.contains("exclusiveMaximum") && schema.contains("maximum")) {
            if (schema["exclusiveMaximum"].is_boolean() &&
                schema["exclusiveMaximum"].get<bool>()) {
                const auto maximum = schema["maximum"].get<double>();
                if (num_value >= maximum) {
                    addError("Value must be less than exclusive maximum: " +
                                 std::to_string(maximum),
                             instance_path, schema_path + "/exclusiveMaximum",
                             "exclusiveMaximum");
                }
            }
        }

        if (schema.contains("exclusiveMaximum") &&
            schema["exclusiveMaximum"].is_number()) {
            const auto ex_maximum = schema["exclusiveMaximum"].get<double>();
            if (num_value >= ex_maximum) {
                addError("Value must be less than exclusive maximum: " +
                             std::to_string(ex_maximum),
                         instance_path, schema_path + "/exclusiveMaximum",
                         "exclusiveMaximum");
            }
        }

        if (schema.contains("multipleOf")) {
            const auto multiple = schema["multipleOf"].get<double>();

            const auto quotient = num_value / multiple;
            const auto rounded = std::round(quotient);

            constexpr double epsilon = 1e-10;
            if (std::abs(quotient - rounded) > epsilon) {
                addError(
                    "Value is not a multiple of: " + std::to_string(multiple),
                    instance_path, schema_path + "/multipleOf", "multipleOf");
            }
        }
    }

    /**
     * @brief Validates enum keyword
     */
    void validateEnum(const json& instance, const json& schema,
                      const std::string& instance_path,
                      const std::string& schema_path) {
        if (schema.contains("enum")) {
            const auto& enum_values = schema["enum"];
            bool valid = false;

            for (const auto& value : enum_values) {
                if (instance == value) {
                    valid = true;
                    break;
                }
            }

            if (!valid) {
                addError("Value not found in enumeration", instance_path,
                         schema_path + "/enum", "enum");
            }
        }
    }

    /**
     * @brief Validates const keyword
     */
    void validateConst(const json& instance, const json& schema,
                       const std::string& instance_path,
                       const std::string& schema_path) {
        if (schema.contains("const")) {
            const auto& const_value = schema["const"];
            if (instance != const_value) {
                addError("Value does not match const value", instance_path,
                         schema_path + "/const", "const");
            }
        }
    }

    /**
     * @brief Validates conditional keywords (if/then/else)
     */
    void validateConditionals(const json& instance, const json& schema,
                              const std::string& instance_path,
                              const std::string& schema_path) {
        if (schema.contains("if") && schema["if"].is_object()) {
            const auto& if_schema = schema["if"];

            std::vector<ValidationError> temp_errors = errors_;
            errors_.clear();

            validateSchema(instance, if_schema, instance_path,
                           schema_path + "/if");
            bool condition_passed = errors_.empty();

            errors_ = temp_errors;

            if (condition_passed && schema.contains("then")) {
                validateSchema(instance, schema["then"], instance_path,
                               schema_path + "/then");
            } else if (!condition_passed && schema.contains("else")) {
                validateSchema(instance, schema["else"], instance_path,
                               schema_path + "/else");
            }
        }
    }

    /**
     * @brief Validates combination keywords (allOf/anyOf/oneOf/not)
     */
    void validateCombinations(const json& instance, const json& schema,
                              const std::string& instance_path,
                              const std::string& schema_path) {
        if (schema.contains("allOf") && schema["allOf"].is_array()) {
            const auto& all_of = schema["allOf"];
            for (std::size_t i = 0; i < all_of.size(); ++i) {
                validateSchema(instance, all_of[i], instance_path,
                               schema_path + "/allOf/" + std::to_string(i));
            }
        }

        if (schema.contains("anyOf") && schema["anyOf"].is_array()) {
            const auto& any_of = schema["anyOf"];

            std::vector<ValidationError> original_errors = errors_;
            errors_.clear();

            bool valid = false;

            for (std::size_t i = 0; i < any_of.size(); ++i) {
                std::vector<ValidationError> schema_errors = errors_;
                errors_.clear();

                validateSchema(instance, any_of[i], instance_path,
                               schema_path + "/anyOf/" + std::to_string(i));

                if (errors_.empty()) {
                    valid = true;
                    errors_.clear();
                    break;
                }

                if (i == 0 || errors_.size() < schema_errors.size()) {
                    schema_errors = std::move(errors_);
                }

                errors_ = std::move(schema_errors);
            }

            if (valid) {
                errors_ = original_errors;
            } else {
                std::vector<ValidationError> anyof_errors = errors_;
                errors_ = original_errors;
                errors_.emplace_back("Value does not match any schema in anyOf",
                                     instance_path, schema_path + "/anyOf",
                                     "anyOf");
            }
        }

        if (schema.contains("oneOf") && schema["oneOf"].is_array()) {
            const auto& one_of = schema["oneOf"];

            std::vector<ValidationError> original_errors = errors_;
            int valid_count = 0;
            std::size_t first_match_index = 0;

            std::vector<std::vector<ValidationError>> sub_errors_list;

            for (std::size_t i = 0; i < one_of.size(); ++i) {
                errors_.clear();

                validateSchema(instance, one_of[i], instance_path,
                               schema_path + "/oneOf/" + std::to_string(i));

                if (errors_.empty()) {
                    valid_count++;
                    if (valid_count == 1) {
                        first_match_index = i;
                    }
                    if (valid_count > 1) {
                        break;
                    }
                } else {
                    sub_errors_list.push_back(std::move(errors_));
                }
            }

            errors_ = std::move(original_errors);

            if (valid_count == 0) {
                addError(
                    "Value does not match exactly one schema in oneOf (matched "
                    "0)",
                    instance_path, schema_path + "/oneOf", "oneOf");
            } else if (valid_count > 1) {
                addError(
                    "Value matches more than one schema in oneOf (matched " +
                        std::to_string(valid_count) + ")",
                    instance_path, schema_path + "/oneOf", "oneOf");
            }
        }

        if (schema.contains("not") && schema["not"].is_object()) {
            const auto& not_schema = schema["not"];

            std::vector<ValidationError> original_errors = errors_;
            errors_.clear();

            validateSchema(instance, not_schema, instance_path,
                           schema_path + "/not");

            bool not_valid = !errors_.empty();
            errors_ = original_errors;

            if (!not_valid) {
                errors_.emplace_back(
                    "Value should not validate against schema in 'not'",
                    instance_path, schema_path + "/not", "not");
            }
        }
    }

    /**
     * @brief Initialize standard format validators
     */
    void initializeFormatValidators() {
        registerFormatValidator("date-time", [](const std::string& s) {
            std::regex date_time_regex(
                R"(^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d(\.\d+)?(Z|[+-]\d\d:\d\d)$)");
            return std::regex_match(s, date_time_regex);
        });

        registerFormatValidator("date", [](const std::string& s) {
            std::regex date_regex(R"(^\d{4}-\d\d-\d\d$)");
            return std::regex_match(s, date_regex);
        });

        registerFormatValidator("time", [](const std::string& s) {
            std::regex time_regex(R"(^\d\d:\d\d:\d\d(\.\d+)?$)");
            return std::regex_match(s, time_regex);
        });

        registerFormatValidator("email", [](const std::string& s) {
            std::regex email_regex(
                R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
            return std::regex_match(s, email_regex);
        });

        registerFormatValidator("idn-email", [](const std::string& s) {
            std::regex email_regex(
                R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
            return std::regex_match(s, email_regex);
        });

        registerFormatValidator("uri", [](const std::string& s) {
            std::regex uri_regex(
                R"(^[a-zA-Z][a-zA-Z0-9+.-]*://(?:[a-zA-Z0-9-._~!$&'()*+,;=:]|%[0-9a-fA-F]{2})*$)");
            return std::regex_match(s, uri_regex);
        });

        registerFormatValidator("uri-reference", [](const std::string& s) {
            std::regex uri_ref_regex(
                R"(^(?:[a-zA-Z][a-zA-Z0-9+.-]*:|)(?://?)?(?:[a-zA-Z0-9-._~!$&'()*+,;=:]|%[0-9a-fA-F]{2})*$)");
            return std::regex_match(s, uri_ref_regex);
        });

        registerFormatValidator("ipv4", [](const std::string& s) {
            std::regex ipv4_regex(
                R"(^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)");
            return std::regex_match(s, ipv4_regex);
        });

        registerFormatValidator("ipv6", [](const std::string& s) {
            std::regex ipv6_regex(
                R"(^(?:[0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}|::(?:[0-9a-fA-F]{1,4}:){0-6}[0-9a-fA-F]{1,4}|[0-9a-fA-F]{1,4}::(?:[0-9a-fA-F]{1,4}:){0-5}[0-9a-fA-F]{1,4}|[0-9a-fA-F]{1,4}:[0-9a-fA-F]{1,4}::(?:[0-9a-fA-F]{1,4}:){0-4}[0-9a-fA-F]{1,4}$)");
            return std::regex_match(s, ipv6_regex);
        });

        registerFormatValidator("hostname", [](const std::string& s) {
            std::regex hostname_regex(
                R"(^[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?(?:\.[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*$)");
            return std::regex_match(s, hostname_regex);
        });

        registerFormatValidator("uuid", [](const std::string& s) {
            std::regex uuid_regex(
                R"(^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$)");
            return std::regex_match(s, uuid_regex);
        });

        registerFormatValidator("regex", [](const std::string& s) {
            try {
                std::regex re(s);
                return true;
            } catch (const std::regex_error&) {
                return false;
            }
        });
    }
};

/**
 * @brief Schema Manager for handling multiple schemas and references
 */
class SchemaManager : public std::enable_shared_from_this<SchemaManager> {
public:
    /**
     * @brief Constructor
     * @param options Validation options to use for schemas
     */
    explicit SchemaManager(ValidationOptions options = {})
        : options_(std::move(options)) {}

    /**
     * @brief Adds a schema to the manager
     * @param schema JSON schema to add
     * @param id Optional ID for the schema (if not specified, extracted from
     * schema)
     * @return true if schema was added successfully
     */
    bool addSchema(const json& schema, const std::string& id = "") {
        if (!schema.is_object()) {
            return false;
        }

        std::string schema_id = id;
        if (schema_id.empty()) {
            if (schema.contains("$id")) {
                schema_id = schema["$id"].get<std::string>();
            } else if (schema.contains("id")) {
                schema_id = schema["id"].get<std::string>();
            } else {
                schema_id = "schema_" + std::to_string(next_id_++);
            }
        }

        std::lock_guard<std::mutex> lock(mutex_);

        auto validator = std::make_shared<JsonValidator>(options_);
        validator->setRootSchema(schema, schema_id);
        validator->setSchemaManager(shared_from_this());

        validators_[schema_id] = validator;

        indexSubschemas(schema, schema_id);

        return true;
    }

    /**
     * @brief Validates data against a schema by ID
     * @param data JSON data to validate
     * @param schema_id ID of the schema to validate against
     * @return true if validation passes
     */
    bool validate(const json& data, const std::string& schema_id) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = validators_.find(schema_id);
        if (it == validators_.end()) {
            return false;
        }

        return it->second->validate(data);
    }

    /**
     * @brief Gets validation errors from the last validation
     * @param schema_id ID of the schema
     * @return Vector of validation errors or empty vector if schema not found
     */
    std::vector<ValidationError> getErrors(const std::string& schema_id) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = validators_.find(schema_id);
        if (it == validators_.end()) {
            return {};
        }

        return it->second->getErrors();
    }

    /**
     * @brief Gets a schema by ID
     * @param schema_id ID of the schema
     * @return Schema JSON or null if not found
     */
    json getSchema(const std::string& schema_id) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = schema_map_.find(schema_id);
        if (it == schema_map_.end()) {
            return nullptr;
        }

        return it->second;
    }

    /**
     * @brief Gets a validator by ID
     * @param schema_id ID of the schema
     * @return Shared pointer to validator or null if not found
     */
    std::shared_ptr<JsonValidator> getValidator(const std::string& schema_id) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = validators_.find(schema_id);
        if (it == validators_.end()) {
            return nullptr;
        }

        return it->second;
    }

    /**
     * @brief Resolves a JSON pointer within a schema
     * @param base_id Base schema ID
     * @param ref Reference string (can be URI or JSON pointer)
     * @return Referenced schema or null if not found
     */
    json resolveReference(const std::string& base_id, const std::string& ref) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (ref.starts_with("#")) {
            auto it = schema_map_.find(base_id);
            if (it == schema_map_.end()) {
                return nullptr;
            }

            try {
                return it->second.at(json::json_pointer(ref.substr(1)));
            } catch (const std::exception&) {
                return nullptr;
            }
        }

        size_t hash_pos = ref.find('#');
        if (hash_pos != std::string::npos) {
            std::string uri = ref.substr(0, hash_pos);
            std::string pointer = ref.substr(hash_pos + 1);

            if (uri.empty()) {
                uri = base_id;
            }

            auto it = schema_map_.find(uri);
            if (it == schema_map_.end()) {
                return nullptr;
            }

            try {
                return it->second.at(json::json_pointer(pointer));
            } catch (const std::exception&) {
                return nullptr;
            }
        }

        auto it = schema_map_.find(ref);
        if (it == schema_map_.end()) {
            return nullptr;
        }

        return it->second;
    }

private:
    std::unordered_map<std::string, json> schema_map_;
    std::unordered_map<std::string, std::shared_ptr<JsonValidator>> validators_;
    ValidationOptions options_;
    std::mutex mutex_;
    std::atomic<size_t> next_id_{0};

    /**
     * @brief Index subschemas for $ref resolution
     * @param schema Schema to index
     * @param base_id Base ID of the schema
     * @param path JSON pointer path to this position
     */
    void indexSubschemas(const json& schema, const std::string& base_id,
                         const std::string& path = "") {
        if (!schema.is_object()) {
            return;
        }

        if (!path.empty()) {
            std::string subschema_id = base_id + "#" + path;
            schema_map_[subschema_id] = schema;
        } else {
            schema_map_[base_id] = schema;
        }

        std::string scope_id = base_id;
        if (schema.contains("$id")) {
            std::string new_id = schema["$id"].get<std::string>();

            if (!new_id.empty() && new_id[0] != '#') {
                if (isAbsoluteUri(new_id)) {
                    scope_id = new_id;
                } else {
                    scope_id = resolveUri(base_id, new_id);
                }
                schema_map_[scope_id] = schema;
            }
        }

        for (auto it = schema.begin(); it != schema.end(); ++it) {
            const auto& key = it.key();
            const auto& value = it.value();

            if (value.is_object()) {
                std::string new_path =
                    path.empty() ? "/" + key : path + "/" + key;
                indexSubschemas(value, scope_id, new_path);
            } else if (value.is_array()) {
                std::string new_path =
                    path.empty() ? "/" + key : path + "/" + key;
                for (size_t i = 0; i < value.size(); i++) {
                    if (value[i].is_object()) {
                        indexSubschemas(value[i], scope_id,
                                        new_path + "/" + std::to_string(i));
                    }
                }
            }
        }
    }

    /**
     * @brief Checks if a URI is absolute
     * @param uri URI to check
     * @return true if URI is absolute
     */
    static bool isAbsoluteUri(const std::string& uri) {
        return uri.find("://") != std::string::npos;
    }

    /**
     * @brief Resolves a relative URI against a base URI
     * @param base Base URI
     * @param relative Relative URI
     * @return Resolved URI
     */
    static std::string resolveUri(const std::string& base,
                                  const std::string& relative) {
        size_t last_slash = base.find_last_of("/");
        if (last_slash == std::string::npos) {
            return relative;
        }

        return base.substr(0, last_slash + 1) + relative;
    }
};

}  // namespace atom::type

#endif  // ATOM_TYPE_JSON_SCHEMA_HPP