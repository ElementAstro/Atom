#include "atom/extra/dotenv/dotenv.hpp"
#include "atom/extra/dotenv/exceptions.hpp"
#include "atom/extra/dotenv/loader.hpp"
#include "atom/extra/dotenv/parser.hpp"
#include "atom/extra/dotenv/validator.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

PYBIND11_MODULE(dotenv, m) {
    m.doc() = R"(Environment variable loading module for the atom package.

This module provides a modern C++ interface for loading, parsing, validating,
and applying environment variables from .env files. It supports advanced
features such as schema validation, file watching, and custom logging.

Examples:
    >>> from atom.extra.dotenv import dotenv
    >>>
    >>> # Quick load from .env file
    >>> result = dotenv.Dotenv.quick_load(".env")
    >>> if result.success:
    ...     print(f"Loaded {len(result.variables)} variables")
    ...     for key, value in result.variables.items():
    ...         print(f"{key}={value}")
    >>>
    >>> # Load with custom options
    >>> options = dotenv.DotenvOptions()
    >>> options.debug = True
    >>> loader = dotenv.Dotenv(options)
    >>> result = loader.load(".env")
    >>>
    >>> # Apply to environment
    >>> if result.success:
    ...     loader.apply_to_environment(result.variables, override_existing=True)
)";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // LoadOptions struct
    py::class_<dotenv::LoadOptions>(
        m, "LoadOptions",
        R"(Configuration options for loading .env files.

This struct defines various options that control how .env files are loaded,
including whether to override existing environment variables, whether to
create missing files, encoding settings, search paths, and file patterns.

Examples:
    >>> options = dotenv.LoadOptions()
    >>> options.override_existing = True
    >>> options.create_if_missing = False
    >>> options.encoding = "utf-8"
    >>> options.search_paths = [".", "./config"]
    >>> options.file_patterns = [".env", ".env.local"]
)")
        .def(py::init<>(), "Create default load options")
        .def_readwrite("override_existing",
                       &dotenv::LoadOptions::override_existing,
                       "If true, override existing environment variables with "
                       "loaded values")
        .def_readwrite("create_if_missing",
                       &dotenv::LoadOptions::create_if_missing,
                       "If true, create the .env file if it does not exist")
        .def_readwrite("encoding", &dotenv::LoadOptions::encoding,
                       "The expected encoding of the .env file (e.g., 'utf-8')")
        .def_readwrite("search_paths", &dotenv::LoadOptions::search_paths,
                       "List of directories to search for .env files")
        .def_readwrite("file_patterns", &dotenv::LoadOptions::file_patterns,
                       "List of file name patterns to match when searching for "
                       ".env files");

    // ParseOptions struct
    py::class_<dotenv::ParseOptions>(
        m, "ParseOptions",
        R"(Configuration options for parsing .env files.

This struct defines various options that control how .env file content is parsed,
including comment handling, variable expansion, and validation settings.)")
        .def(py::init<>(), "Create default parse options")
        .def_readwrite("ignore_comments",
                       &dotenv::ParseOptions::ignore_comments,
                       "Whether to ignore comments in .env files")
        .def_readwrite("trim_whitespace",
                       &dotenv::ParseOptions::trim_whitespace,
                       "Whether to trim whitespace from values")
        .def_readwrite("expand_variables",
                       &dotenv::ParseOptions::expand_variables,
                       "Whether to expand variable references")
        .def_readwrite("allow_empty_values",
                       &dotenv::ParseOptions::allow_empty_values,
                       "Whether to allow empty values")
        .def_readwrite("comment_char", &dotenv::ParseOptions::comment_char,
                       "Character used for comments (default: '#')")
        .def_readwrite("encoding", &dotenv::ParseOptions::encoding,
                       "Expected encoding of the file (default: 'utf-8')");

    // DotenvOptions struct
    py::class_<dotenv::DotenvOptions>(
        m, "DotenvOptions",
        R"(Configuration options for the Dotenv loader.

This struct encapsulates all configuration options for the Dotenv loader,
including parser options, loader options, debug mode, and a custom logger.

Examples:
    >>> options = dotenv.DotenvOptions()
    >>> options.debug = True
    >>> options.parse_options.allow_comments = True
    >>> options.load_options.override_existing = True
)")
        .def(py::init<>(), "Create default dotenv options")
        .def_readwrite("parse_options", &dotenv::DotenvOptions::parse_options,
                       "Options for parsing .env files")
        .def_readwrite("load_options", &dotenv::DotenvOptions::load_options,
                       "Options for loading .env files from disk")
        .def_readwrite("debug", &dotenv::DotenvOptions::debug,
                       "Enable debug logging if true");

    // LoadResult struct
    py::class_<dotenv::LoadResult>(
        m, "LoadResult",
        R"(Result of loading environment variables from .env files.

This struct contains the outcome of a load operation, including the loaded
variables, any errors or warnings encountered, and the list of files loaded.

Examples:
    >>> result = dotenv.Dotenv.quick_load(".env")
    >>> if result.success:
    ...     print(f"Loaded {len(result.variables)} variables")
    ...     for key, value in result.variables.items():
    ...         print(f"{key}={value}")
    ... else:
    ...     print("Load failed:")
    ...     for error in result.errors:
    ...         print(f"  Error: {error}")
)")
        .def(py::init<>(), "Create an empty load result")
        .def_readwrite("success", &dotenv::LoadResult::success,
                       "True if loading was successful, false otherwise")
        .def_readwrite("variables", &dotenv::LoadResult::variables,
                       "Map of loaded environment variables (key-value pairs)")
        .def_readwrite("errors", &dotenv::LoadResult::errors,
                       "List of error messages encountered during loading")
        .def_readwrite("warnings", &dotenv::LoadResult::warnings,
                       "List of warning messages encountered during loading")
        .def_readwrite("loaded_files", &dotenv::LoadResult::loaded_files,
                       "List of file paths that were loaded")
        .def("add_error", &dotenv::LoadResult::addError, py::arg("error"),
             R"(Add an error message and mark the result as unsuccessful.

Args:
    error: Error message to add.
)")
        .def("add_warning", &dotenv::LoadResult::addWarning, py::arg("warning"),
             R"(Add a warning message.

Args:
    warning: Warning message to add.
)");

    // Dotenv class binding
    py::class_<dotenv::Dotenv>(
        m, "Dotenv",
        R"(Main Dotenv class for loading and managing environment variables.

This class provides a modern C++ interface for loading, parsing, validating,
and applying environment variables from .env files. It supports advanced
features such as schema validation, file watching, and custom logging.

Examples:
    >>> # Create with default options
    >>> loader = dotenv.Dotenv()
    >>>
    >>> # Create with custom options
    >>> options = dotenv.DotenvOptions()
    >>> options.debug = True
    >>> loader = dotenv.Dotenv(options)
    >>>
    >>> # Load from file
    >>> result = loader.load(".env")
    >>> if result.success:
    ...     loader.apply_to_environment(result.variables)
)")
        .def(py::init<const dotenv::DotenvOptions&>(),
             py::arg("options") = dotenv::DotenvOptions{},
             R"(Construct a Dotenv loader with the specified options.

Args:
    options: Configuration options for the loader.
)")
        .def("load", &dotenv::Dotenv::load, py::arg("filepath") = ".env",
             R"(Load environment variables from a single .env file.

Args:
    filepath: Path to the .env file (default: ".env").

Returns:
    LoadResult containing loaded variables and status.
)")
        .def("load_multiple", &dotenv::Dotenv::loadMultiple,
             py::arg("filepaths"),
             R"(Load environment variables from multiple .env files.

Args:
    filepaths: List of file paths to load.

Returns:
    LoadResult containing combined variables and status.
)")
        .def("auto_load", &dotenv::Dotenv::autoLoad, py::arg("base_path") = ".",
             R"(Automatically discover and load .env files from search paths.

Args:
    base_path: Base directory for file discovery (default: ".").

Returns:
    LoadResult containing discovered variables and status.
)")
        .def(
            "load_from_string", &dotenv::Dotenv::loadFromString,
            py::arg("content"),
            R"(Load environment variables from a string containing .env content.

Args:
    content: The .env file content as a string.

Returns:
    LoadResult containing parsed variables and status.
)")
        .def("load_and_validate", &dotenv::Dotenv::loadAndValidate,
             py::arg("filepath"), py::arg("schema"),
             R"(Load and validate environment variables using a schema.

Args:
    filepath: Path to the .env file.
    schema: Validation schema to apply.

Returns:
    LoadResult containing validation results and variables.
)")
        .def("apply_to_environment", &dotenv::Dotenv::applyToEnvironment,
             py::arg("variables"), py::arg("override_existing") = false,
             R"(Apply loaded variables to the system environment.

Args:
    variables: Dictionary of variables to apply.
    override_existing: If true, override existing environment variables.
)")
        .def("save", &dotenv::Dotenv::save, py::arg("filepath"),
             py::arg("variables"),
             R"(Save environment variables to a .env file.

Args:
    filepath: Output file path.
    variables: Dictionary of variables to save.
)")
        .def("watch", &dotenv::Dotenv::watch, py::arg("filepath"),
             py::arg("callback"),
             R"(Watch a .env file for changes and reload automatically.

Args:
    filepath: File to watch for changes.
    callback: Callback function invoked when the file changes.
                The callback receives a LoadResult parameter.
)")
        .def("stop_watching", &dotenv::Dotenv::stopWatching,
             R"(Stop watching the file for changes.)")
        .def("get_options", &dotenv::Dotenv::getOptions,
             R"(Get the current configuration options.

Returns:
    Reference to the current DotenvOptions.
)")
        .def("set_options", &dotenv::Dotenv::setOptions, py::arg("options"),
             R"(Update the configuration options.

Args:
    options: New configuration options to set.
)")
        .def_static(
            "quick_load", &dotenv::Dotenv::quickLoad,
            py::arg("filepath") = ".env",
            R"(Quickly load environment variables from a file with default options.

Args:
    filepath: Path to the .env file (default: ".env").

Returns:
    LoadResult containing loaded variables and status.
)")
        .def_static(
            "config", &dotenv::Dotenv::config, py::arg("filepath"),
            py::arg("override_existing") = false,
            R"(Quickly load and apply environment variables to the system environment.

Args:
    filepath: Path to the .env file.
    override_existing: If true, override existing environment variables.

Raises:
    RuntimeError: If configuration fails.
)");

    // EnvEntry struct
    py::class_<dotenv::EnvEntry>(
        m, "EnvEntry",
        R"(Represents a parsed environment variable entry with metadata.)")
        .def(py::init<>(), "Create an empty environment entry")
        .def_readwrite("key", &dotenv::EnvEntry::key, "The variable name/key")
        .def_readwrite("value", &dotenv::EnvEntry::value, "The variable value")
        .def_readwrite("original_line", &dotenv::EnvEntry::original_line,
                       "The original line from the .env file")
        .def_readwrite("line_number", &dotenv::EnvEntry::line_number,
                       "The line number in the .env file")
        .def_readwrite("is_quoted", &dotenv::EnvEntry::is_quoted,
                       "Whether the value was quoted")
        .def_readwrite("quote_type", &dotenv::EnvEntry::quote_type,
                       "The type of quote used ('\"' or '\\'')");

    // Parser class
    py::class_<dotenv::Parser>(
        m, "Parser",
        R"(Parser for .env files with comprehensive feature support.

This class provides low-level parsing functionality for .env file content,
supporting comments, quotes, escape sequences, variable expansion, and more.

Examples:
    >>> parser = dotenv.Parser()
    >>> content = 'KEY=value\\nFOO=bar'
    >>> variables = parser.parse(content)
    >>> print(variables)
    {'KEY': 'value', 'FOO': 'bar'}
)")
        .def(py::init<const dotenv::ParseOptions&>(),
             py::arg("options") = dotenv::ParseOptions{},
             "Create a parser with the specified options")
        .def("parse", &dotenv::Parser::parse, py::arg("content"),
             R"(Parse .env file content into a dictionary of variables.

Args:
    content: The .env file content as a string.

Returns:
    Dictionary of parsed environment variables.
)")
        .def("parse_detailed", &dotenv::Parser::parseDetailed,
             py::arg("content"),
             R"(Parse .env file content and return detailed entry information.

Args:
    content: The .env file content as a string.

Returns:
    List of EnvEntry objects with detailed parsing information.
)");

    // FileLoader class
    py::class_<dotenv::FileLoader>(m, "FileLoader",
                                   R"(Cross-platform file loader for .env files.

This class provides methods to load, save, and discover .env files,
supporting encoding detection, file pattern matching, and accessibility checks.

Examples:
    >>> loader = dotenv.FileLoader()
    >>> content = loader.load('.env')
    >>> print(content)
)")
        .def(py::init<const dotenv::LoadOptions&>(),
             py::arg("options") = dotenv::LoadOptions{},
             "Create a file loader with the specified options")
        .def("load", &dotenv::FileLoader::load, py::arg("filepath"),
             R"(Load the content of a .env file.

Args:
    filepath: Path to the .env file.

Returns:
    The content of the file as a string.

Raises:
    RuntimeError: If the file cannot be loaded.
)")
        .def("load_multiple", &dotenv::FileLoader::loadMultiple,
             py::arg("filepaths"),
             R"(Load and combine content from multiple .env files.

Args:
    filepaths: List of file paths to load.

Returns:
    Combined content of all files as a single string.
)")
        .def("auto_load", &dotenv::FileLoader::autoLoad,
             py::arg("base_path") = ".",
             R"(Automatically discover and load .env files from search paths.

Args:
    base_path: Base directory to start searching from.

Returns:
    Combined content from all discovered files.
)")
        .def("save", &dotenv::FileLoader::save, py::arg("filepath"),
             py::arg("env_vars"),
             R"(Save environment variables to a .env file.

Args:
    filepath: Output file path.
    env_vars: Dictionary of environment variables to save.

Raises:
    RuntimeError: If the file cannot be written.
)")
        .def("is_accessible", &dotenv::FileLoader::isAccessible,
             py::arg("filepath"),
             R"(Check if a file exists and is readable.

Args:
    filepath: Path to the file.

Returns:
    True if the file exists and is readable, false otherwise.
)")
        .def("get_modification_time", &dotenv::FileLoader::getModificationTime,
             py::arg("filepath"),
             R"(Get the last modification time of a file.

Args:
    filepath: Path to the file.

Returns:
    The file's last modification time.

Raises:
    RuntimeError: If the file does not exist.
)");

    // ValidationRule class
    py::class_<dotenv::ValidationRule, std::shared_ptr<dotenv::ValidationRule>>(
        m, "ValidationRule",
        R"(Validation rule for environment variables.

This class represents a single validation rule that can be applied to
environment variable values.)")
        .def("validate", &dotenv::ValidationRule::validate, py::arg("value"),
             R"(Validate a value against this rule.

Args:
    value: The value to validate.

Returns:
    True if the value is valid, false otherwise.
)")
        .def("get_name", &dotenv::ValidationRule::getName,
             "Get the name of this validation rule")
        .def("get_error_message", &dotenv::ValidationRule::getErrorMessage,
             "Get the error message for this validation rule");

    // ValidationSchema class
    py::class_<dotenv::ValidationSchema>(
        m, "ValidationSchema",
        R"(Schema for validating environment variables.

This class allows you to define validation rules for environment variables,
including required variables, optional variables with defaults, and custom
validation rules.

Examples:
    >>> schema = dotenv.ValidationSchema()
    >>> schema.required('DATABASE_URL')
    >>> schema.optional('PORT', '3000')
    >>> schema.rule('PORT', dotenv.rules.integer())
)")
        .def(py::init<>(), "Create an empty validation schema")
        .def("required", &dotenv::ValidationSchema::required, py::arg("key"),
             py::return_value_policy::reference,
             R"(Add a required variable to the schema.

Args:
    key: The variable name.

Returns:
    Self for method chaining.
)")
        .def("optional", &dotenv::ValidationSchema::optional, py::arg("key"),
             py::arg("default_value") = "", py::return_value_policy::reference,
             R"(Add an optional variable with a default value.

Args:
    key: The variable name.
    default_value: The default value if not provided.

Returns:
    Self for method chaining.
)")
        .def("rule", &dotenv::ValidationSchema::rule, py::arg("key"),
             py::arg("rule"), py::return_value_policy::reference,
             R"(Add a validation rule for a variable.

Args:
    key: The variable name.
    rule: The validation rule to apply.

Returns:
    Self for method chaining.
)")
        .def("rules", &dotenv::ValidationSchema::rules, py::arg("key"),
             py::arg("rules"), py::return_value_policy::reference,
             R"(Add multiple validation rules for a variable.

Args:
    key: The variable name.
    rules: List of validation rules to apply.

Returns:
    Self for method chaining.
)")
        .def("is_required", &dotenv::ValidationSchema::isRequired,
             py::arg("key"),
             R"(Check if a variable is required.

Args:
    key: The variable name.

Returns:
    True if the variable is required, false otherwise.
)")
        .def("get_default", &dotenv::ValidationSchema::getDefault,
             py::arg("key"),
             R"(Get the default value for a variable.

Args:
    key: The variable name.

Returns:
    The default value, or empty string if not set.
)")
        .def("get_rules", &dotenv::ValidationSchema::getRules, py::arg("key"),
             R"(Get validation rules for a variable.

Args:
    key: The variable name.

Returns:
    List of validation rules for the variable.
)")
        .def("get_required_variables",
             &dotenv::ValidationSchema::getRequiredVariables,
             R"(Get all required variables.

Returns:
    List of required variable names.
)");

    // ValidationResult struct
    py::class_<dotenv::ValidationResult>(
        m, "ValidationResult",
        R"(Result of validating environment variables against a schema.)")
        .def(py::init<>(), "Create an empty validation result")
        .def_readwrite("is_valid", &dotenv::ValidationResult::is_valid,
                       "True if validation passed, false otherwise")
        .def_readwrite("errors", &dotenv::ValidationResult::errors,
                       "List of validation error messages")
        .def_readwrite("processed_vars",
                       &dotenv::ValidationResult::processed_vars,
                       "Processed variables (with defaults applied)")
        .def("add_error", &dotenv::ValidationResult::addError, py::arg("error"),
             R"(Add an error message and mark validation as failed.

Args:
    error: Error message to add.
)");

    // Validator class
    py::class_<dotenv::Validator>(m, "Validator",
                                  R"(Environment variable validator.

This class validates environment variables against a schema, checking for
required variables, applying defaults, and running validation rules.

Examples:
    >>> validator = dotenv.Validator()
    >>> schema = dotenv.ValidationSchema()
    >>> schema.required('API_KEY')
    >>> variables = {'API_KEY': 'secret123'}
    >>> result = validator.validate(variables, schema)
    >>> if result.is_valid:
    ...     print('Validation passed!')
)")
        .def(py::init<>(), "Create a new validator")
        .def("validate", &dotenv::Validator::validate, py::arg("env_vars"),
             py::arg("schema"),
             R"(Validate environment variables against a schema.

Args:
    env_vars: Dictionary of environment variables.
    schema: Validation schema to apply.

Returns:
    ValidationResult containing validation status and errors.
)")
        .def("validate_with_defaults", &dotenv::Validator::validateWithDefaults,
             py::arg("env_vars"), py::arg("schema"),
             R"(Validate environment variables and apply defaults.

Args:
    env_vars: Dictionary of environment variables (will be modified).
    schema: Validation schema to apply.

Returns:
    ValidationResult with defaults applied to processed_vars.
)");

    // Built-in validation rules namespace
    py::module_ rules_module =
        m.def_submodule("rules", "Built-in validation rules");

    rules_module.def("not_empty", &dotenv::rules::notEmpty,
                     "Create a rule that requires non-empty values");
    rules_module.def("min_length", &dotenv::rules::minLength,
                     py::arg("min_len"),
                     "Create a rule that requires minimum length");
    rules_module.def("max_length", &dotenv::rules::maxLength,
                     py::arg("max_len"),
                     "Create a rule that requires maximum length");
    rules_module.def("pattern", &dotenv::rules::pattern, py::arg("regex"),
                     py::arg("description") = "",
                     "Create a rule that matches a regex pattern");
    rules_module.def("numeric", &dotenv::rules::numeric,
                     "Create a rule that requires numeric values");
    rules_module.def("integer", &dotenv::rules::integer,
                     "Create a rule that requires integer values");
    rules_module.def("boolean", &dotenv::rules::boolean,
                     "Create a rule that requires boolean values");
    rules_module.def("url", &dotenv::rules::url,
                     "Create a rule that requires valid URLs");
    rules_module.def("email", &dotenv::rules::email,
                     "Create a rule that requires valid email addresses");
    rules_module.def("one_of", &dotenv::rules::oneOf, py::arg("allowed_values"),
                     "Create a rule that requires one of the allowed values");
    rules_module.def("custom", &dotenv::rules::custom, py::arg("validator"),
                     py::arg("error_message"),
                     "Create a custom validation rule");

    // Exception classes
    py::register_exception<dotenv::DotenvException>(m, "DotenvException");
    py::register_exception<dotenv::FileException>(m, "FileException");
    py::register_exception<dotenv::ParseException>(m, "ParseException");
    py::register_exception<dotenv::ValidationException>(m,
                                                        "ValidationException");
}
