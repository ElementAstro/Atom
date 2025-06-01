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
                    std::string is = "", std::string ec = "") noexcept
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
    using FormatValidator = std::function<bool(std::string_view)>;

    /**
     * @brief Constructor
     * @param options Validation options
     */
    explicit JsonValidator(ValidationOptions options = {}) noexcept
        : options_(std::move(options)) {
        initializeFormatValidators();
    }

    /**
     * @brief Sets the root schema
     * @param schema_json JSON formatted schema
     * @param id Optional schema ID
     * @throws SchemaValidationException if schema is invalid
     */
    void setRootSchema(const json& schema_json, std::string_view id = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        root_schema_ = schema_json;
        schema_id_ = id.empty() ? extractId(schema_json) : std::string(id);

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
                                 FormatValidator validator) {
        std::lock_guard<std::mutex> lock(mutex_);
        format_validators_[std::move(format_name)] = std::move(validator);
    }

    /**
     * @brief Links this validator with a schema manager for $ref resolution
     * @param manager Pointer to schema manager
     */
    void setSchemaManager(std::shared_ptr<SchemaManager> manager) noexcept {
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
    void setOptions(ValidationOptions options) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        options_ = std::move(options);
    }

private:
    json root_schema_;
    std::string schema_id_;
    std::vector<ValidationError> errors_;
    ValidationOptions options_;
    std::unordered_map<std::string, FormatValidator> format_validators_;
    std::weak_ptr<SchemaManager> schema_manager_;
    mutable std::mutex mutex_;

    std::unordered_map<std::string, json> uri_to_schema_map_;
    mutable std::unordered_map<std::string, std::regex> regex_cache_;

    std::atomic<size_t> current_recursion_depth_{0};
    std::atomic<size_t> current_ref_depth_{0};

    /**
     * @brief Resets the validator state
     */
    void resetState() noexcept {
        errors_.clear();
        uri_to_schema_map_.clear();
        regex_cache_.clear();
    }

    /**
     * @brief Resets only the validation state (errors and counters)
     */
    void resetValidationState() noexcept {
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
            if (auto it = schema.find("$id");
                it != schema.end() && it->is_string())
                return it->get<std::string>();
            if (auto it = schema.find("id");
                it != schema.end() && it->is_string())
                return it->get<std::string>();
        }
        return "";
    }

    /**
     * @brief Detect schema version from schema
     * @param schema Schema to analyze
     */
    void detectSchemaVersion(const json& schema) noexcept {
        if (!schema.is_object()) {
            options_.schema_version = SchemaVersion::DRAFT2020_12;
            return;
        }

        if (auto it = schema.find("$schema");
            it != schema.end() && it->is_string()) {
            std::string_view schema_uri = it->get_ref<const std::string&>();

            if (schema_uri.find("draft/2020-12") != std::string_view::npos) {
                options_.schema_version = SchemaVersion::DRAFT2020_12;
            } else if (schema_uri.find("draft/2019-09") !=
                       std::string_view::npos) {
                options_.schema_version = SchemaVersion::DRAFT2019_09;
            } else if (schema_uri.find("draft-07") != std::string_view::npos) {
                options_.schema_version = SchemaVersion::DRAFT7;
            } else if (schema_uri.find("draft-06") != std::string_view::npos) {
                options_.schema_version = SchemaVersion::DRAFT6;
            } else if (schema_uri.find("draft-04") != std::string_view::npos) {
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
                uri_to_schema_map_[std::move(id)] = schema;
            }

            if (auto it = schema.find("pattern");
                it != schema.end() && it->is_string()) {
                const std::string& pattern = it->get_ref<const std::string&>();
                try {
                    regex_cache_[pattern] = std::regex(pattern);
                } catch (const std::exception&) {
                    // Ignore invalid patterns during compilation
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
                        std::string_view instance_path,
                        std::string_view schema_path) {
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
                           std::string_view instance_path,
                           std::string_view schema_path) {
        if (++current_ref_depth_ > options_.max_reference_depth) {
            addError("Maximum reference depth exceeded", instance_path,
                     schema_path);
            --current_ref_depth_;
            return;
        }

        const std::string& ref = schema["$ref"].get_ref<const std::string&>();

        if (ref.starts_with("#")) {
            json referenced_schema =
                resolvePointer(root_schema_, ref.substr(1));
            if (!referenced_schema.is_null()) {
                std::string new_schema_path =
                    std::string(schema_path) + "/" + ref;
                validateSchema(instance, referenced_schema, instance_path,
                               new_schema_path);
            } else {
                std::string error_path = std::string(schema_path) + "/$ref";
                addError("Invalid reference: " + ref, instance_path,
                         error_path);
            }
        } else {
            auto manager = schema_manager_.lock();
            std::string error_path = std::string(schema_path) + "/$ref";
            if (manager) {
                addError("External references not yet implemented: " + ref,
                         instance_path, error_path);
            } else {
                addError(
                    "Cannot resolve external reference without schema "
                    "manager: " +
                        ref,
                    instance_path, error_path);
            }
        }

        --current_ref_depth_;
    }

    /**
     * @brief Helper to add an error with error code
     */
    void addError(std::string_view message, std::string_view instance_path,
                  std::string_view schema_path,
                  std::string_view error_code = "") {
        errors_.emplace_back(std::string(message), std::string(instance_path),
                             std::string(schema_path), "",
                             std::string(error_code));
    }

    /**
     * @brief Resolves a JSON pointer
     * @param doc Document to search in
     * @param pointer JSON pointer (without the leading #)
     * @return Referenced JSON or null if not found
     */
    json resolvePointer(const json& doc,
                        std::string_view pointer) const noexcept {
        if (pointer.empty() || pointer == "/")
            return doc;

        try {
            return doc.at(json::json_pointer(std::string(pointer)));
        } catch (const std::exception&) {
            return nullptr;
        }
    }

    /**
     * @brief Validates type keyword
     */
    void validateType(const json& instance, const json& schema,
                      std::string_view instance_path,
                      std::string_view schema_path) {
        auto it = schema.find("type");
        if (it == schema.end())
            return;

        if (!validateTypeValue(instance, *it)) {
            std::string error_path = std::string(schema_path) + "/type";
            addError("Type mismatch, expected: " + typeToString(*it),
                     instance_path, error_path, "type");
        }
    }

    /**
     * @brief Validates instance against type specification
     */
    [[nodiscard]] bool validateTypeValue(const json& instance,
                                         const json& type_spec) const noexcept {
        if (type_spec.is_string()) {
            return checkTypeString(instance,
                                   type_spec.get_ref<const std::string&>());
        }
        if (type_spec.is_array()) {
            for (const auto& type : type_spec) {
                if (type.is_string() &&
                    checkTypeString(instance,
                                    type.get_ref<const std::string&>())) {
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
                        std::string_view instance_path,
                        std::string_view schema_path) {
        if (auto it = schema.find("required");
            it != schema.end() && it->is_array()) {
            for (const auto& prop_name : *it) {
                if (!instance.contains(prop_name)) {
                    std::string error_path =
                        std::string(schema_path) + "/required";
                    addError("Missing required property: " +
                                 prop_name.get<std::string>(),
                             instance_path, error_path, "required");
                }
            }
        }

        if (auto it = schema.find("properties");
            it != schema.end() && it->is_object()) {
            for (const auto& [prop_name, prop_schema] : it->items()) {
                if (instance.contains(prop_name)) {
                    std::string prop_path =
                        instance_path.empty()
                            ? prop_name
                            : std::string(instance_path) + "/" + prop_name;
                    std::string new_schema_path =
                        std::string(schema_path) + "/properties/" + prop_name;
                    validateSchema(instance[prop_name], prop_schema, prop_path,
                                   new_schema_path);
                }
            }
        }

        validatePatternProperties(instance, schema, instance_path, schema_path);
        validateAdditionalProperties(instance, schema, instance_path,
                                     schema_path);
        validatePropertyNames(instance, schema, instance_path, schema_path);
        validatePropertyCounts(instance, schema, instance_path, schema_path);
        validateDependencies(instance, schema, instance_path, schema_path);
    }

    /**
     * @brief Validates pattern properties
     */
    void validatePatternProperties(const json& instance, const json& schema,
                                   std::string_view instance_path,
                                   std::string_view schema_path) {
        auto it = schema.find("patternProperties");
        if (it == schema.end() || !it->is_object())
            return;

        for (const auto& [pattern_str, pattern_schema] : it->items()) {
            std::regex pattern =
                getOrCompileRegex(pattern_str, instance_path, schema_path);

            for (const auto& [prop_name, prop_value] : instance.items()) {
                if (std::regex_search(prop_name, pattern)) {
                    std::string prop_path =
                        instance_path.empty()
                            ? prop_name
                            : std::string(instance_path) + "/" + prop_name;
                    std::string new_schema_path = std::string(schema_path) +
                                                  "/patternProperties/" +
                                                  pattern_str;
                    validateSchema(prop_value, pattern_schema, prop_path,
                                   new_schema_path);
                }
            }
        }
    }

    /**
     * @brief Gets or compiles a regex pattern
     */
    std::regex getOrCompileRegex(const std::string& pattern_str,
                                 std::string_view instance_path,
                                 std::string_view schema_path) {
        auto it = regex_cache_.find(pattern_str);
        if (it != regex_cache_.end()) {
            return it->second;
        }

        try {
            std::regex pattern(pattern_str);
            regex_cache_[pattern_str] = pattern;
            return pattern;
        } catch (const std::regex_error&) {
            std::string error_path =
                std::string(schema_path) + "/patternProperties/" + pattern_str;
            addError("Invalid regex pattern: " + pattern_str, instance_path,
                     error_path, "patternProperties");
            return std::regex(".*");  // Fallback regex that matches everything
        }
    }

    /**
     * @brief Validates additional properties
     */
    void validateAdditionalProperties(const json& instance, const json& schema,
                                      std::string_view instance_path,
                                      std::string_view schema_path) {
        auto it = schema.find("additionalProperties");
        if (it == schema.end())
            return;

        for (const auto& [prop_name, prop_value] : instance.items()) {
            bool validated = false;

            // Check if validated by properties
            if (auto props_it = schema.find("properties");
                props_it != schema.end() && props_it->contains(prop_name)) {
                validated = true;
            }

            // Check if validated by pattern properties
            if (!validated) {
                if (auto pattern_it = schema.find("patternProperties");
                    pattern_it != schema.end() && pattern_it->is_object()) {
                    for (const auto& [pattern_str, _] : pattern_it->items()) {
                        try {
                            std::regex pattern = getOrCompileRegex(
                                pattern_str, instance_path, schema_path);
                            if (std::regex_search(prop_name, pattern)) {
                                validated = true;
                                break;
                            }
                        } catch (const std::regex_error&) {
                            continue;
                        }
                    }
                }
            }

            if (!validated) {
                if (it->is_boolean() && *it == false) {
                    std::string error_path =
                        std::string(schema_path) + "/additionalProperties";
                    addError("Additional property not allowed: " + prop_name,
                             instance_path, error_path, "additionalProperties");
                } else if (it->is_object()) {
                    std::string prop_path =
                        instance_path.empty()
                            ? prop_name
                            : std::string(instance_path) + "/" + prop_name;
                    std::string error_path =
                        std::string(schema_path) + "/additionalProperties";
                    validateSchema(prop_value, *it, prop_path, error_path);
                }
            }
        }
    }

    /**
     * @brief Validates property names
     */
    void validatePropertyNames(const json& instance, const json& schema,
                               std::string_view instance_path,
                               std::string_view schema_path) {
        auto it = schema.find("propertyNames");
        if (it == schema.end() || !it->is_object())
            return;

        for (const auto& [prop_name, _] : instance.items()) {
            json prop_name_json = prop_name;
            std::string pseudo_path =
                std::string(instance_path) + "/{propertyName}";
            std::string error_path =
                std::string(schema_path) + "/propertyNames";
            validateSchema(prop_name_json, *it, pseudo_path, error_path);
        }
    }

    /**
     * @brief Validates property count constraints
     */
    void validatePropertyCounts(const json& instance, const json& schema,
                                std::string_view instance_path,
                                std::string_view schema_path) {
        if (auto it = schema.find("minProperties");
            it != schema.end() && it->is_number_unsigned()) {
            const auto min_props = it->get<std::size_t>();
            if (instance.size() < min_props) {
                std::string error_path =
                    std::string(schema_path) + "/minProperties";
                addError("Object has too few properties, minimum: " +
                             std::to_string(min_props),
                         instance_path, error_path, "minProperties");
            }
        }

        if (auto it = schema.find("maxProperties");
            it != schema.end() && it->is_number_unsigned()) {
            const auto max_props = it->get<std::size_t>();
            if (instance.size() > max_props) {
                std::string error_path =
                    std::string(schema_path) + "/maxProperties";
                addError("Object has too many properties, maximum: " +
                             std::to_string(max_props),
                         instance_path, error_path, "maxProperties");
            }
        }
    }

    /**
     * @brief Validates dependencies/dependentRequired/dependentSchemas keywords
     */
    void validateDependencies(const json& instance, const json& schema,
                              std::string_view instance_path,
                              std::string_view schema_path) {
        // Legacy dependencies keyword
        if (auto it = schema.find("dependencies");
            it != schema.end() && it->is_object()) {
            for (const auto& [prop_name, dependency] : it->items()) {
                if (instance.contains(prop_name)) {
                    std::string dep_path =
                        std::string(schema_path) + "/dependencies/" + prop_name;
                    if (dependency.is_array()) {
                        for (const auto& required_prop : dependency) {
                            if (!instance.contains(required_prop)) {
                                addError("Missing dependency: " +
                                             required_prop.get<std::string>(),
                                         instance_path, dep_path,
                                         "dependencies");
                            }
                        }
                    } else if (dependency.is_object()) {
                        validateSchema(instance, dependency, instance_path,
                                       dep_path);
                    }
                }
            }
        }

        // Draft 2019-09+ dependentRequired
        if (auto it = schema.find("dependentRequired");
            it != schema.end() && it->is_object()) {
            for (const auto& [prop_name, required_props] : it->items()) {
                if (instance.contains(prop_name) && required_props.is_array()) {
                    std::string dep_path = std::string(schema_path) +
                                           "/dependentRequired/" + prop_name;
                    for (const auto& required_prop : required_props) {
                        if (!instance.contains(required_prop)) {
                            addError("Missing dependent property: " +
                                         required_prop.get<std::string>(),
                                     instance_path, dep_path,
                                     "dependentRequired");
                        }
                    }
                }
            }
        }

        // Draft 2019-09+ dependentSchemas
        if (auto it = schema.find("dependentSchemas");
            it != schema.end() && it->is_object()) {
            for (const auto& [prop_name, dep_schema] : it->items()) {
                if (instance.contains(prop_name)) {
                    std::string dep_path = std::string(schema_path) +
                                           "/dependentSchemas/" + prop_name;
                    validateSchema(instance, dep_schema, instance_path,
                                   dep_path);
                }
            }
        }
    }

    /**
     * @brief Validates array-specific keywords
     */
    void validateArray(const json& instance, const json& schema,
                       std::string_view instance_path,
                       std::string_view schema_path) {
        validateArrayItems(instance, schema, instance_path, schema_path);
        validateArrayContains(instance, schema, instance_path, schema_path);
        validateArrayConstraints(instance, schema, instance_path, schema_path);
    }

    /**
     * @brief Validates array items
     */
    void validateArrayItems(const json& instance, const json& schema,
                            std::string_view instance_path,
                            std::string_view schema_path) {
        // Handle prefixItems (Draft 2020-12)
        if (auto prefix_it = schema.find("prefixItems");
            prefix_it != schema.end() && prefix_it->is_array()) {
            const auto& prefix_items = *prefix_it;
            for (std::size_t i = 0;
                 i < std::min(prefix_items.size(), instance.size()); ++i) {
                std::string item_path =
                    std::string(instance_path) + "/" + std::to_string(i);
                std::string item_schema_path = std::string(schema_path) +
                                               "/prefixItems/" +
                                               std::to_string(i);
                validateSchema(instance[i], prefix_items[i], item_path,
                               item_schema_path);
            }

            // Handle additional items beyond prefixItems
            if (instance.size() > prefix_items.size()) {
                if (auto items_it = schema.find("items");
                    items_it != schema.end()) {
                    if (items_it->is_boolean() && *items_it == false) {
                        std::string error_path =
                            std::string(schema_path) + "/items";
                        addError("Additional items not allowed", instance_path,
                                 error_path, "items");
                    } else if (items_it->is_object()) {
                        for (std::size_t i = prefix_items.size();
                             i < instance.size(); ++i) {
                            std::string item_path = std::string(instance_path) +
                                                    "/" + std::to_string(i);
                            std::string error_path =
                                std::string(schema_path) + "/items";
                            validateSchema(instance[i], *items_it, item_path,
                                           error_path);
                        }
                    }
                }
            }
        }
        // Handle legacy items keyword
        else if (auto items_it = schema.find("items");
                 items_it != schema.end()) {
            if (items_it->is_object()) {
                for (std::size_t i = 0; i < instance.size(); ++i) {
                    std::string item_path =
                        std::string(instance_path) + "/" + std::to_string(i);
                    std::string error_path =
                        std::string(schema_path) + "/items";
                    validateSchema(instance[i], *items_it, item_path,
                                   error_path);
                }
            } else if (items_it->is_array()) {
                for (std::size_t i = 0;
                     i < std::min(items_it->size(), instance.size()); ++i) {
                    std::string item_path =
                        std::string(instance_path) + "/" + std::to_string(i);
                    std::string item_schema_path = std::string(schema_path) +
                                                   "/items/" +
                                                   std::to_string(i);
                    validateSchema(instance[i], (*items_it)[i], item_path,
                                   item_schema_path);
                }

                // Handle additionalItems
                if (instance.size() > items_it->size()) {
                    if (auto add_items_it = schema.find("additionalItems");
                        add_items_it != schema.end()) {
                        if (add_items_it->is_boolean() &&
                            *add_items_it == false) {
                            std::string error_path =
                                std::string(schema_path) + "/additionalItems";
                            addError("Additional items not allowed",
                                     instance_path, error_path,
                                     "additionalItems");
                        } else if (add_items_it->is_object()) {
                            for (std::size_t i = items_it->size();
                                 i < instance.size(); ++i) {
                                std::string item_path =
                                    std::string(instance_path) + "/" +
                                    std::to_string(i);
                                std::string error_path =
                                    std::string(schema_path) +
                                    "/additionalItems";
                                validateSchema(instance[i], *add_items_it,
                                               item_path, error_path);
                            }
                        }
                    }
                }
            }
        }
    }

    /**
     * @brief Validates array contains keyword
     */
    void validateArrayContains(const json& instance, const json& schema,
                               std::string_view instance_path,
                               std::string_view schema_path) {
        auto contains_it = schema.find("contains");
        if (contains_it == schema.end())
            return;

        std::size_t valid_count = 0;
        std::size_t min_contains = 1;
        std::size_t max_contains = std::numeric_limits<std::size_t>::max();

        if (auto it = schema.find("minContains");
            it != schema.end() && it->is_number_unsigned()) {
            min_contains = it->get<std::size_t>();
        }

        if (auto it = schema.find("maxContains");
            it != schema.end() && it->is_number_unsigned()) {
            max_contains = it->get<std::size_t>();
        }

        for (std::size_t i = 0; i < instance.size(); ++i) {
            std::size_t error_count = errors_.size();
            std::string item_path =
                std::string(instance_path) + "/" + std::to_string(i);
            std::string error_path = std::string(schema_path) + "/contains";

            validateSchema(instance[i], *contains_it, item_path, error_path);

            if (errors_.size() == error_count) {
                valid_count++;
                if (valid_count >= max_contains)
                    break;
            } else {
                errors_.resize(error_count);
            }
        }

        if (instance.size() > 0 && valid_count < min_contains) {
            std::string error_path = std::string(schema_path) + "/contains";
            addError(
                "Array doesn't contain required number of matching items "
                "(min: " +
                    std::to_string(min_contains) + ")",
                instance_path, error_path, "contains");
        }

        if (valid_count > max_contains) {
            std::string error_path = std::string(schema_path) + "/maxContains";
            addError("Array contains too many matching items (max: " +
                         std::to_string(max_contains) + ")",
                     instance_path, error_path, "maxContains");
        }
    }

    /**
     * @brief Validates array constraints (minItems, maxItems, uniqueItems)
     */
    void validateArrayConstraints(const json& instance, const json& schema,
                                  std::string_view instance_path,
                                  std::string_view schema_path) {
        if (auto it = schema.find("minItems");
            it != schema.end() && it->is_number_unsigned()) {
            const auto min_items = it->get<std::size_t>();
            if (instance.size() < min_items) {
                std::string error_path = std::string(schema_path) + "/minItems";
                addError("Array has too few items, minimum: " +
                             std::to_string(min_items),
                         instance_path, error_path, "minItems");
            }
        }

        if (auto it = schema.find("maxItems");
            it != schema.end() && it->is_number_unsigned()) {
            const auto max_items = it->get<std::size_t>();
            if (instance.size() > max_items) {
                std::string error_path = std::string(schema_path) + "/maxItems";
                addError("Array has too many items, maximum: " +
                             std::to_string(max_items),
                         instance_path, error_path, "maxItems");
            }
        }

        if (auto it = schema.find("uniqueItems");
            it != schema.end() && it->is_boolean() && it->get<bool>()) {
            std::set<json> unique_items;
            for (std::size_t i = 0; i < instance.size(); ++i) {
                if (!unique_items.insert(instance[i]).second) {
                    std::string error_path =
                        std::string(schema_path) + "/uniqueItems";
                    addError("Array items must be unique", instance_path,
                             error_path, "uniqueItems");
                    break;
                }
            }
        }
    }

    /**
     * @brief Validates string-specific keywords
     */
    void validateString(const json& instance, const json& schema,
                        std::string_view instance_path,
                        std::string_view schema_path) {
        const std::string& str = instance.get_ref<const std::string&>();

        if (auto it = schema.find("minLength");
            it != schema.end() && it->is_number_unsigned()) {
            const auto min_length = it->get<std::size_t>();
            if (str.length() < min_length) {
                std::string error_path =
                    std::string(schema_path) + "/minLength";
                addError("String is too short, minimum length: " +
                             std::to_string(min_length),
                         instance_path, error_path, "minLength");
            }
        }

        if (auto it = schema.find("maxLength");
            it != schema.end() && it->is_number_unsigned()) {
            const auto max_length = it->get<std::size_t>();
            if (str.length() > max_length) {
                std::string error_path =
                    std::string(schema_path) + "/maxLength";
                addError("String is too long, maximum length: " +
                             std::to_string(max_length),
                         instance_path, error_path, "maxLength");
            }
        }

        if (auto it = schema.find("pattern");
            it != schema.end() && it->is_string()) {
            const std::string& pattern_str = it->get_ref<const std::string&>();
            std::regex pattern =
                getOrCompileRegex(pattern_str, instance_path, schema_path);

            if (!std::regex_search(str, pattern)) {
                std::string error_path = std::string(schema_path) + "/pattern";
                addError("String does not match pattern: " + pattern_str,
                         instance_path, error_path, "pattern");
            }
        }

        if (!options_.ignore_format) {
            if (auto it = schema.find("format");
                it != schema.end() && it->is_string()) {
                const std::string& format = it->get_ref<const std::string&>();
                validateFormat(str, format, instance_path, schema_path);
            }
        }
    }

    /**
     * @brief Validates a string against a format
     */
    void validateFormat(std::string_view str, std::string_view format,
                        std::string_view instance_path,
                        std::string_view schema_path) {
        auto it = format_validators_.find(std::string(format));

        if (it != format_validators_.end()) {
            if (!it->second(str)) {
                std::string error_path = std::string(schema_path) + "/format";
                addError("String does not match format: " + std::string(format),
                         instance_path, error_path, "format");
            }
        } else if (!options_.allow_undefined_formats) {
            std::string error_path = std::string(schema_path) + "/format";
            addError("Unknown format: " + std::string(format), instance_path,
                     error_path, "format");
        }
    }

    /**
     * @brief Validates number-specific keywords
     */
    void validateNumber(const json& instance, const json& schema,
                        std::string_view instance_path,
                        std::string_view schema_path) {
        const auto num_value = instance.get<double>();

        if (auto it = schema.find("minimum");
            it != schema.end() && it->is_number()) {
            const auto minimum = it->get<double>();
            if (num_value < minimum) {
                std::string error_path = std::string(schema_path) + "/minimum";
                addError(
                    "Value is less than minimum: " + std::to_string(minimum),
                    instance_path, error_path, "minimum");
            }
        }

        if (auto it = schema.find("exclusiveMinimum"); it != schema.end()) {
            if (it->is_boolean() && it->get<bool>()) {
                if (auto min_it = schema.find("minimum");
                    min_it != schema.end() && min_it->is_number()) {
                    const auto minimum = min_it->get<double>();
                    if (num_value <= minimum) {
                        std::string error_path =
                            std::string(schema_path) + "/exclusiveMinimum";
                        addError(
                            "Value must be greater than exclusive minimum: " +
                                std::to_string(minimum),
                            instance_path, error_path, "exclusiveMinimum");
                    }
                }
            } else if (it->is_number()) {
                const auto ex_minimum = it->get<double>();
                if (num_value <= ex_minimum) {
                    std::string error_path =
                        std::string(schema_path) + "/exclusiveMinimum";
                    addError("Value must be greater than exclusive minimum: " +
                                 std::to_string(ex_minimum),
                             instance_path, error_path, "exclusiveMinimum");
                }
            }
        }

        if (auto it = schema.find("maximum");
            it != schema.end() && it->is_number()) {
            const auto maximum = it->get<double>();
            if (num_value > maximum) {
                std::string error_path = std::string(schema_path) + "/maximum";
                addError(
                    "Value is greater than maximum: " + std::to_string(maximum),
                    instance_path, error_path, "maximum");
            }
        }

        if (auto it = schema.find("exclusiveMaximum"); it != schema.end()) {
            if (it->is_boolean() && it->get<bool>()) {
                if (auto max_it = schema.find("maximum");
                    max_it != schema.end() && max_it->is_number()) {
                    const auto maximum = max_it->get<double>();
                    if (num_value >= maximum) {
                        std::string error_path =
                            std::string(schema_path) + "/exclusiveMaximum";
                        addError("Value must be less than exclusive maximum: " +
                                     std::to_string(maximum),
                                 instance_path, error_path, "exclusiveMaximum");
                    }
                }
            } else if (it->is_number()) {
                const auto ex_maximum = it->get<double>();
                if (num_value >= ex_maximum) {
                    std::string error_path =
                        std::string(schema_path) + "/exclusiveMaximum";
                    addError("Value must be less than exclusive maximum: " +
                                 std::to_string(ex_maximum),
                             instance_path, error_path, "exclusiveMaximum");
                }
            }
        }

        if (auto it = schema.find("multipleOf");
            it != schema.end() && it->is_number()) {
            const auto multiple = it->get<double>();
            const auto quotient = num_value / multiple;
            const auto rounded = std::round(quotient);

            constexpr double epsilon = 1e-10;
            if (std::abs(quotient - rounded) > epsilon) {
                std::string error_path =
                    std::string(schema_path) + "/multipleOf";
                addError(
                    "Value is not a multiple of: " + std::to_string(multiple),
                    instance_path, error_path, "multipleOf");
            }
        }
    }

    /**
     * @brief Validates enum keyword
     */
    void validateEnum(const json& instance, const json& schema,
                      std::string_view instance_path,
                      std::string_view schema_path) {
        auto it = schema.find("enum");
        if (it == schema.end() || !it->is_array())
            return;

        for (const auto& value : *it) {
            if (instance == value)
                return;
        }

        std::string error_path = std::string(schema_path) + "/enum";
        addError("Value not found in enumeration", instance_path, error_path,
                 "enum");
    }

    /**
     * @brief Validates const keyword
     */
    void validateConst(const json& instance, const json& schema,
                       std::string_view instance_path,
                       std::string_view schema_path) {
        auto it = schema.find("const");
        if (it == schema.end())
            return;

        if (instance != *it) {
            std::string error_path = std::string(schema_path) + "/const";
            addError("Value does not match const value", instance_path,
                     error_path, "const");
        }
    }

    /**
     * @brief Validates conditional keywords (if/then/else)
     */
    void validateConditionals(const json& instance, const json& schema,
                              std::string_view instance_path,
                              std::string_view schema_path) {
        auto if_it = schema.find("if");
        if (if_it == schema.end() || !if_it->is_object())
            return;

        std::vector<ValidationError> temp_errors = std::move(errors_);
        errors_.clear();

        std::string if_path = std::string(schema_path) + "/if";
        validateSchema(instance, *if_it, instance_path, if_path);
        bool condition_passed = errors_.empty();

        errors_ = std::move(temp_errors);

        if (condition_passed) {
            if (auto then_it = schema.find("then"); then_it != schema.end()) {
                std::string then_path = std::string(schema_path) + "/then";
                validateSchema(instance, *then_it, instance_path, then_path);
            }
        } else {
            if (auto else_it = schema.find("else"); else_it != schema.end()) {
                std::string else_path = std::string(schema_path) + "/else";
                validateSchema(instance, *else_it, instance_path, else_path);
            }
        }
    }

    /**
     * @brief Validates combination keywords (allOf/anyOf/oneOf/not)
     */
    void validateCombinations(const json& instance, const json& schema,
                              std::string_view instance_path,
                              std::string_view schema_path) {
        validateAllOf(instance, schema, instance_path, schema_path);
        validateAnyOf(instance, schema, instance_path, schema_path);
        validateOneOf(instance, schema, instance_path, schema_path);
        validateNot(instance, schema, instance_path, schema_path);
    }

    /**
     * @brief Validates allOf keyword
     */
    void validateAllOf(const json& instance, const json& schema,
                       std::string_view instance_path,
                       std::string_view schema_path) {
        auto it = schema.find("allOf");
        if (it == schema.end() || !it->is_array())
            return;

        for (std::size_t i = 0; i < it->size(); ++i) {
            std::string sub_path =
                std::string(schema_path) + "/allOf/" + std::to_string(i);
            validateSchema(instance, (*it)[i], instance_path, sub_path);
        }
    }

    /**
     * @brief Validates anyOf keyword
     */
    void validateAnyOf(const json& instance, const json& schema,
                       std::string_view instance_path,
                       std::string_view schema_path) {
        auto it = schema.find("anyOf");
        if (it == schema.end() || !it->is_array())
            return;

        std::vector<ValidationError> original_errors = std::move(errors_);
        errors_.clear();

        for (std::size_t i = 0; i < it->size(); ++i) {
            std::vector<ValidationError> schema_errors = std::move(errors_);
            errors_.clear();

            std::string sub_path =
                std::string(schema_path) + "/anyOf/" + std::to_string(i);
            validateSchema(instance, (*it)[i], instance_path, sub_path);

            if (errors_.empty()) {
                errors_ = std::move(original_errors);
                return;
            }

            if (i == 0 || errors_.size() < schema_errors.size()) {
                schema_errors = std::move(errors_);
            }
            errors_ = std::move(schema_errors);
        }

        errors_ = std::move(original_errors);
        std::string error_path = std::string(schema_path) + "/anyOf";
        addError("Value does not match any schema in anyOf", instance_path,
                 error_path, "anyOf");
    }

    /**
     * @brief Validates oneOf keyword
     */
    void validateOneOf(const json& instance, const json& schema,
                       std::string_view instance_path,
                       std::string_view schema_path) {
        auto it = schema.find("oneOf");
        if (it == schema.end() || !it->is_array())
            return;

        std::vector<ValidationError> original_errors = std::move(errors_);
        int valid_count = 0;

        for (std::size_t i = 0; i < it->size(); ++i) {
            errors_.clear();
            std::string sub_path =
                std::string(schema_path) + "/oneOf/" + std::to_string(i);
            validateSchema(instance, (*it)[i], instance_path, sub_path);

            if (errors_.empty()) {
                valid_count++;
                if (valid_count > 1)
                    break;
            }
        }

        errors_ = std::move(original_errors);
        std::string error_path = std::string(schema_path) + "/oneOf";

        if (valid_count == 0) {
            addError(
                "Value does not match exactly one schema in oneOf (matched 0)",
                instance_path, error_path, "oneOf");
        } else if (valid_count > 1) {
            addError("Value matches more than one schema in oneOf (matched " +
                         std::to_string(valid_count) + ")",
                     instance_path, error_path, "oneOf");
        }
    }

    /**
     * @brief Validates not keyword
     */
    void validateNot(const json& instance, const json& schema,
                     std::string_view instance_path,
                     std::string_view schema_path) {
        auto it = schema.find("not");
        if (it == schema.end() || !it->is_object())
            return;

        std::vector<ValidationError> original_errors = std::move(errors_);
        errors_.clear();

        std::string not_path = std::string(schema_path) + "/not";
        validateSchema(instance, *it, instance_path, not_path);

        bool not_valid = !errors_.empty();
        errors_ = std::move(original_errors);

        if (!not_valid) {
            addError("Value should not validate against schema in 'not'",
                     instance_path, not_path, "not");
        }
    }

    /**
     * @brief Initialize standard format validators
     */
    void initializeFormatValidators() {
        registerFormatValidator("date-time", [](std::string_view s) {
            static const std::regex date_time_regex(
                R"(^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d(\.\d+)?(Z|[+-]\d\d:\d\d)$)");
            return std::regex_match(s.begin(), s.end(), date_time_regex);
        });

        registerFormatValidator("date", [](std::string_view s) {
            static const std::regex date_regex(R"(^\d{4}-\d\d-\d\d$)");
            return std::regex_match(s.begin(), s.end(), date_regex);
        });

        registerFormatValidator("time", [](std::string_view s) {
            static const std::regex time_regex(R"(^\d\d:\d\d:\d\d(\.\d+)?$)");
            return std::regex_match(s.begin(), s.end(), time_regex);
        });

        registerFormatValidator("email", [](std::string_view s) {
            static const std::regex email_regex(
                R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
            return std::regex_match(s.begin(), s.end(), email_regex);
        });

        registerFormatValidator("uri", [](std::string_view s) {
            static const std::regex uri_regex(
                R"(^[a-zA-Z][a-zA-Z0-9+.-]*://(?:[a-zA-Z0-9-._~!$&'()*+,;=:]|%[0-9a-fA-F]{2})*$)");
            return std::regex_match(s.begin(), s.end(), uri_regex);
        });

        registerFormatValidator("ipv4", [](std::string_view s) {
            static const std::regex ipv4_regex(
                R"(^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)");
            return std::regex_match(s.begin(), s.end(), ipv4_regex);
        });

        registerFormatValidator("hostname", [](std::string_view s) {
            static const std::regex hostname_regex(
                R"(^[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?(?:\.[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*$)");
            return std::regex_match(s.begin(), s.end(), hostname_regex);
        });

        registerFormatValidator("uuid", [](std::string_view s) {
            static const std::regex uuid_regex(
                R"(^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$)");
            return std::regex_match(s.begin(), s.end(), uuid_regex);
        });

        registerFormatValidator("regex", [](std::string_view s) {
            try {
                std::regex re(s.begin(), s.end());
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
    explicit SchemaManager(ValidationOptions options = {}) noexcept
        : options_(std::move(options)) {}

    /**
     * @brief Adds a schema to the manager
     * @param schema JSON schema to add
     * @param id Optional ID for the schema (if not specified, extracted from
     * schema)
     * @return true if schema was added successfully
     */
    bool addSchema(const json& schema, std::string_view id = "") {
        if (!schema.is_object())
            return false;

        std::string schema_id =
            id.empty() ? extractSchemaId(schema) : std::string(id);
        if (schema_id.empty()) {
            schema_id = "schema_" + std::to_string(next_id_++);
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
    bool validate(const json& data, std::string_view schema_id) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = validators_.find(std::string(schema_id));
        return it != validators_.end() && it->second->validate(data);
    }

    /**
     * @brief Gets validation errors from the last validation
     * @param schema_id ID of the schema
     * @return Vector of validation errors or empty vector if schema not found
     */
    std::vector<ValidationError> getErrors(std::string_view schema_id) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = validators_.find(std::string(schema_id));
        return it != validators_.end() ? it->second->getErrors()
                                       : std::vector<ValidationError>{};
    }

    /**
     * @brief Gets a schema by ID
     * @param schema_id ID of the schema
     * @return Schema JSON or null if not found
     */
    json getSchema(std::string_view schema_id) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = schema_map_.find(std::string(schema_id));
        return it != schema_map_.end() ? it->second : json(nullptr);
    }

    /**
     * @brief Gets a validator by ID
     * @param schema_id ID of the schema
     * @return Shared pointer to validator or null if not found
     */
    std::shared_ptr<JsonValidator> getValidator(std::string_view schema_id) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = validators_.find(std::string(schema_id));
        return it != validators_.end() ? it->second : nullptr;
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
            auto it = schema_map_.find(std::string(base_id));
            if (it == schema_map_.end())
                return nullptr;

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

        auto it = schema_map_.find(std::string(ref));
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
     * @brief Extract schema ID from schema
     * @param schema Schema to extract ID from
     * @return Schema ID or empty string if not found
     */
    [[nodiscard]] static std::string extractSchemaId(const json& schema) {
        if (schema.is_object()) {
            if (auto it = schema.find("$id");
                it != schema.end() && it->is_string())
                return it->get<std::string>();
            if (auto it = schema.find("id");
                it != schema.end() && it->is_string())  // Draft 4
                return it->get<std::string>();
        }
        return "";
    }

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