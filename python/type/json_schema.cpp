#include "atom/type/json-schema.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace atom::type;

PYBIND11_MODULE(json_schema, m) {
    m.doc() = "JSON Schema validation module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const SchemaValidationException& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Bind SchemaVersion enum
    py::enum_<SchemaVersion>(m, "SchemaVersion", R"(
        JSON Schema specification versions.
        
        Enum values:
            DRAFT4: JSON Schema draft 4
            DRAFT6: JSON Schema draft 6
            DRAFT7: JSON Schema draft 7
            DRAFT2019_09: JSON Schema draft 2019-09
            DRAFT2020_12: JSON Schema draft 2020-12
            AUTO_DETECT: Automatically detect version from schema
    )")
        .value("DRAFT4", SchemaVersion::DRAFT4)
        .value("DRAFT6", SchemaVersion::DRAFT6)
        .value("DRAFT7", SchemaVersion::DRAFT7)
        .value("DRAFT2019_09", SchemaVersion::DRAFT2019_09)
        .value("DRAFT2020_12", SchemaVersion::DRAFT2020_12)
        .value("AUTO_DETECT", SchemaVersion::AUTO_DETECT)
        .export_values();

    // Bind ValidationError struct
    py::class_<ValidationError>(m, "ValidationError", R"(
        Structure representing a JSON Schema validation error.
        
        Attributes:
            message (str): Error message describing the validation failure
            path (str): JSON path to the location where validation failed
            schema_path (str): Path to the schema element that caused the failure
            instance_snippet (str): Snippet of the instance that failed validation
            error_code (str): Error code identifying the type of validation failure
    )")
        .def(py::init<std::string, std::string, std::string, std::string,
                      std::string>(),
             py::arg("message"), py::arg("path") = "",
             py::arg("schema_path") = "", py::arg("instance_snippet") = "",
             py::arg("error_code") = "")
        .def_readwrite("message", &ValidationError::message)
        .def_readwrite("path", &ValidationError::path)
        .def_readwrite("schema_path", &ValidationError::schema_path)
        .def_readwrite("instance_snippet", &ValidationError::instance_snippet)
        .def_readwrite("error_code", &ValidationError::error_code)
        .def("to_json", &ValidationError::toJson,
             "Convert error to JSON format")
        .def("__repr__", [](const ValidationError& err) {
            return "ValidationError(message='" + err.message + "', path='" +
                   err.path + "')";
        });

    // Bind ValidationOptions struct
    py::class_<ValidationOptions>(m, "ValidationOptions", R"(
        Configuration options for JSON Schema validation.
        
        Attributes:
            fail_fast (bool): Stop on first error
            validate_schema (bool): Validate schema against meta-schema
            ignore_format (bool): Ignore format validators
            allow_undefined_formats (bool): Allow undefined formats
            max_errors (int): Maximum number of errors to collect
            max_recursion_depth (int): Maximum recursion depth for schema validation
            max_reference_depth (int): Maximum depth for $ref resolution
            base_uri (str): Base URI for schema resolution
            schema_version (SchemaVersion): Schema version to use
    )")
        .def(py::init<>())
        .def_readwrite("fail_fast", &ValidationOptions::fail_fast)
        .def_readwrite("validate_schema", &ValidationOptions::validate_schema)
        .def_readwrite("ignore_format", &ValidationOptions::ignore_format)
        .def_readwrite("allow_undefined_formats",
                       &ValidationOptions::allow_undefined_formats)
        .def_readwrite("max_errors", &ValidationOptions::max_errors)
        .def_readwrite("max_recursion_depth",
                       &ValidationOptions::max_recursion_depth)
        .def_readwrite("max_reference_depth",
                       &ValidationOptions::max_reference_depth)
        .def_readwrite("base_uri", &ValidationOptions::base_uri)
        .def_readwrite("schema_version", &ValidationOptions::schema_version);

    // Bind the JsonValidator class
    py::class_<JsonValidator>(m, "JsonValidator", R"(
        Enhanced JSON Schema validator with full JSON Schema draft support.
        
        This class provides methods for validating JSON instances against JSON Schemas
        following various draft versions of the specification.
        
        Args:
            options: Validation options

        Examples:
            >>> from atom.json_schema import JsonValidator, ValidationOptions
            >>> validator = JsonValidator()
            >>> schema = {"type": "object", "properties": {"name": {"type": "string"}}}
            >>> validator.set_root_schema(schema)
            >>> validator.validate({"name": "test"})
            True
            >>> validator.validate({"name": 123})
            False
            >>> validator.get_errors()
            [ValidationError(message='Type mismatch, expected: string', path='/name')]
    )")
        .def(py::init<ValidationOptions>(),
             py::arg("options") = ValidationOptions())
        .def("set_root_schema", &JsonValidator::setRootSchema,
             py::arg("schema_json"), py::arg("id") = "",
             R"(Sets the root schema.

Args:
    schema_json: JSON formatted schema
    id: Optional schema ID. If not provided, extracted from schema.

Raises:
    ValueError: If schema is invalid.
)")
        .def("validate", &JsonValidator::validate, py::arg("instance"),
             R"(Validates the given JSON instance against the schema.

Args:
    instance: JSON instance to validate

Returns:
    bool: True if validation passes, False if validation fails

Raises:
    RuntimeError: For critical validation errors
)")
        .def("get_errors", &JsonValidator::getErrors,
             "Get list of validation errors",
             py::return_value_policy::reference_internal)
        .def("get_errors_as_json", &JsonValidator::getErrorsAsJson,
             "Get validation errors as a JSON array")
        .def("register_format_validator",
             &JsonValidator::registerFormatValidator, py::arg("format_name"),
             py::arg("validator"),
             R"(Registers a custom format validator.

Args:
    format_name: Name of the format
    validator: Function that validates strings against this format. 
               Should take a string and return a boolean.
)")
        .def("set_schema_manager", &JsonValidator::setSchemaManager,
             py::arg("manager"),
             "Links this validator with a schema manager for $ref resolution")
        .def("get_schema_version", &JsonValidator::getSchemaVersion,
             "Gets the detected schema version", py::return_value_policy::copy)
        .def("get_schema_id", &JsonValidator::getSchemaId, "Gets the schema ID",
             py::return_value_policy::reference_internal)
        .def("set_options", &JsonValidator::setOptions, py::arg("options"),
             "Updates validation options");

    // Bind SchemaManager class
    py::class_<SchemaManager, std::shared_ptr<SchemaManager>>(
        m, "SchemaManager", R"(
        Schema Manager for handling multiple schemas and references.
        
        This class manages multiple JSON schemas and resolves references between them.
        
        Args:
            options: Validation options to use for schemas

        Examples:
            >>> from atom.json_schema import SchemaManager
            >>> manager = SchemaManager()
            >>> schema1 = {"$id": "http://example.com/schema1", "type": "object"}
            >>> schema2 = {"$id": "http://example.com/schema2", "type": "string"}
            >>> manager.add_schema(schema1)
            True
            >>> manager.add_schema(schema2)
            True
            >>> manager.validate({"name": "test"}, "http://example.com/schema1")
            True
    )")
        .def(py::init<ValidationOptions>(),
             py::arg("options") = ValidationOptions())
        .def("add_schema", &SchemaManager::addSchema, py::arg("schema"),
             py::arg("id") = "",
             R"(Adds a schema to the manager.

Args:
    schema: JSON schema to add
    id: Optional ID for the schema (if not specified, extracted from schema)

Returns:
    bool: True if schema was added successfully
)")
        .def("validate", &SchemaManager::validate, py::arg("data"),
             py::arg("schema_id"),
             R"(Validates data against a schema by ID.

Args:
    data: JSON data to validate
    schema_id: ID of the schema to validate against

Returns:
    bool: True if validation passes, False if validation fails
)")
        .def("get_errors", &SchemaManager::getErrors, py::arg("schema_id"),
             R"(Gets validation errors from the last validation.

Args:
    schema_id: ID of the schema

Returns:
    List[ValidationError]: Validation errors or empty list if schema not found
)")
        .def("get_schema", &SchemaManager::getSchema, py::arg("schema_id"),
             R"(Gets a schema by ID.

Args:
    schema_id: ID of the schema

Returns:
    dict: Schema JSON or None if not found
)")
        .def("get_validator", &SchemaManager::getValidator,
             py::arg("schema_id"),
             R"(Gets a validator by ID.

Args:
    schema_id: ID of the schema

Returns:
    JsonValidator: Validator for the schema or None if not found
)")
        .def("resolve_reference", &SchemaManager::resolveReference,
             py::arg("base_id"), py::arg("ref"),
             R"(Resolves a JSON pointer within a schema.

Args:
    base_id: Base schema ID
    ref: Reference string (can be URI or JSON pointer)

Returns:
    dict: Referenced schema or None if not found
)");

    // Create a factory function that wraps schema validation in a more Pythonic
    // API
    m.def(
        "validate",
        [](const json& schema, const json& instance,
           ValidationOptions options) {
            JsonValidator validator(options);
            validator.setRootSchema(schema);

            bool is_valid = validator.validate(instance);
            if (is_valid) {
                return py::make_tuple(true, py::list());
            } else {
                py::list errors;
                for (const auto& err : validator.getErrors()) {
                    errors.append(err);
                }
                return py::make_tuple(false, errors);
            }
        },
        py::arg("schema"), py::arg("instance"),
        py::arg("options") = ValidationOptions(),
        R"(Convenience function to validate a JSON instance against a schema.

Args:
    schema: JSON schema to validate against
    instance: JSON instance to validate
    options: Validation options

Returns:
    tuple: (is_valid, errors) where is_valid is a boolean and errors is a list of ValidationError

Examples:
    >>> from atom.json_schema import validate
    >>> schema = {"type": "object", "properties": {"name": {"type": "string"}}}
    >>> is_valid, errors = validate(schema, {"name": "test"})
    >>> is_valid
    True
    >>> is_valid, errors = validate(schema, {"name": 123})
    >>> is_valid
    False
    >>> errors
    [ValidationError(message='Type mismatch, expected: string', path='/name')]
)");

    // Add helper function to create a schema manager and add schemas in one
    // step
    m.def(
        "create_schema_manager",
        [](const py::dict& schemas, ValidationOptions options) {
            auto manager = std::make_shared<SchemaManager>(options);
            for (const auto& item : schemas) {
                std::string id = py::cast<std::string>(item.first);
                json schema = py::cast<json>(item.second);
                manager->addSchema(schema, id);
            }
            return manager;
        },
        py::arg("schemas"), py::arg("options") = ValidationOptions(),
        R"(Create a SchemaManager and add multiple schemas in one step.

Args:
    schemas: Dict mapping schema IDs to schema objects
    options: Validation options

Returns:
    SchemaManager: Configured schema manager with all schemas loaded

Examples:
    >>> from atom.json_schema import create_schema_manager
    >>> schemas = {
    ...     "http://example.com/schema1": {"type": "object"},
    ...     "http://example.com/schema2": {"type": "string"}
    ... }
    >>> manager = create_schema_manager(schemas)
)");
}