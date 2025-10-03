#include "atom/extra/dotenv/dotenv.hpp"
#include "atom/extra/dotenv/loader.hpp"
#include "atom/extra/dotenv/parser.hpp"
#include "atom/extra/dotenv/validator.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

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
    py::class_<dotenv::LoadOptions>(m, "LoadOptions",
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
        .def_readwrite("override_existing", &dotenv::LoadOptions::override_existing,
                       "If true, override existing environment variables with loaded values")
        .def_readwrite("create_if_missing", &dotenv::LoadOptions::create_if_missing,
                       "If true, create the .env file if it does not exist")
        .def_readwrite("encoding", &dotenv::LoadOptions::encoding,
                       "The expected encoding of the .env file (e.g., 'utf-8')")
        .def_readwrite("search_paths", &dotenv::LoadOptions::search_paths,
                       "List of directories to search for .env files")
        .def_readwrite("file_patterns", &dotenv::LoadOptions::file_patterns,
                       "List of file name patterns to match when searching for .env files");

    // ParseOptions struct
    py::class_<dotenv::ParseOptions>(m, "ParseOptions",
                                     R"(Configuration options for parsing .env files.

This struct defines various options that control how .env file content is parsed,
including comment handling, variable expansion, and validation settings.)")
        .def(py::init<>(), "Create default parse options")
        .def_readwrite("allow_comments", &dotenv::ParseOptions::allow_comments,
                       "Whether to allow comments in .env files")
        .def_readwrite("trim_whitespace", &dotenv::ParseOptions::trim_whitespace,
                       "Whether to trim whitespace from values")
        .def_readwrite("expand_variables", &dotenv::ParseOptions::expand_variables,
                       "Whether to expand variable references")
        .def_readwrite("strict_mode", &dotenv::ParseOptions::strict_mode,
                       "Whether to use strict parsing mode");

    // DotenvOptions struct
    py::class_<dotenv::DotenvOptions>(m, "DotenvOptions",
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
    py::class_<dotenv::LoadResult>(m, "LoadResult",
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
        .def("add_error", &dotenv::LoadResult::addError,
             py::arg("error"),
             R"(Add an error message and mark the result as unsuccessful.

Args:
    error: Error message to add.
)")
        .def("add_warning", &dotenv::LoadResult::addWarning,
             py::arg("warning"),
             R"(Add a warning message.

Args:
    warning: Warning message to add.
)");

    // Dotenv class binding
    py::class_<dotenv::Dotenv>(m, "Dotenv",
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
        .def("load", &dotenv::Dotenv::load,
             py::arg("filepath") = ".env",
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
        .def("auto_load", &dotenv::Dotenv::autoLoad,
             py::arg("base_path") = ".",
             R"(Automatically discover and load .env files from search paths.

Args:
    base_path: Base directory for file discovery (default: ".").

Returns:
    LoadResult containing discovered variables and status.
)")
        .def("load_from_string", &dotenv::Dotenv::loadFromString,
             py::arg("content"),
             R"(Load environment variables from a string containing .env content.

Args:
    content: The .env file content as a string.

Returns:
    LoadResult containing parsed variables and status.
)")
        .def("apply_to_environment", &dotenv::Dotenv::applyToEnvironment,
             py::arg("variables"), py::arg("override_existing") = false,
             R"(Apply loaded variables to the system environment.

Args:
    variables: Dictionary of variables to apply.
    override_existing: If true, override existing environment variables.
)")
        .def("save", &dotenv::Dotenv::save,
             py::arg("filepath"), py::arg("variables"),
             R"(Save environment variables to a .env file.

Args:
    filepath: Output file path.
    variables: Dictionary of variables to save.
)")
        .def("get_options", &dotenv::Dotenv::getOptions,
             R"(Get the current configuration options.

Returns:
    Reference to the current DotenvOptions.
)")
        .def("set_options", &dotenv::Dotenv::setOptions,
             py::arg("options"),
             R"(Update the configuration options.

Args:
    options: New configuration options to set.
)")
        .def_static("quick_load", &dotenv::Dotenv::quickLoad,
                    py::arg("filepath") = ".env",
                    R"(Quickly load environment variables from a file with default options.

Args:
    filepath: Path to the .env file (default: ".env").

Returns:
    LoadResult containing loaded variables and status.
)")
        .def_static("config", &dotenv::Dotenv::config,
                    py::arg("filepath"), py::arg("override_existing") = false,
                    R"(Quickly load and apply environment variables to the system environment.

Args:
    filepath: Path to the .env file.
    override_existing: If true, override existing environment variables.

Raises:
    RuntimeError: If configuration fails.
)");
}
