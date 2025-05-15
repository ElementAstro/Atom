#include "atom/system/env.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(env, m) {
    m.doc() = "Environment variable management module for the atom package";

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

    // Env class binding
    py::class_<atom::utils::Env, std::shared_ptr<atom::utils::Env>>(
        m, "Env",
        R"(Environment variable class for managing program environment variables,
command-line arguments, and other related information.

This class provides methods to access, modify, and manage environment variables
and command-line arguments.

Examples:
    >>> from atom.system import env
    >>> e = env.Env()
    >>> e.add("MY_VAR", "value")
    >>> print(e.get("MY_VAR"))
    value
)")
        .def(py::init<>(),
             "Default constructor that initializes environment variable "
             "information.")
        .def(py::init<int, char**>(), py::arg("argc"), py::arg("argv"),
             "Constructor that initializes environment variable information "
             "with command-line arguments.")
        .def_static(
            "create_shared", &atom::utils::Env::createShared, py::arg("argc"),
            py::arg("argv"),
            R"(Static method to create a shared pointer to an Env object.

Args:
    argc: Number of command-line arguments.
    argv: Array of command-line arguments.

Returns:
    Shared pointer to an Env object.
)")
        .def_static("environ", &atom::utils::Env::Environ,
                    R"(Static method to get the current environment variables.

Returns:
    Dictionary of environment variables.
)")
        .def("add", &atom::utils::Env::add, py::arg("key"), py::arg("val"),
             R"(Adds a key-value pair to the environment variables.

Args:
    key: The key name.
    val: The value associated with the key.
)")
        .def("add_multiple", &atom::utils::Env::addMultiple, py::arg("vars"),
             R"(Adds multiple key-value pairs to the environment variables.

Args:
    vars: The dictionary of key-value pairs to add.
)")
        .def("has", &atom::utils::Env::has, py::arg("key"),
             R"(Checks if a key exists in the environment variables.

Args:
    key: The key name.

Returns:
    True if the key exists, otherwise False.
)")
        .def("has_all", &atom::utils::Env::hasAll, py::arg("keys"),
             R"(Checks if all keys exist in the environment variables.

Args:
    keys: The list of key names.

Returns:
    True if all keys exist, otherwise False.
)")
        .def("has_any", &atom::utils::Env::hasAny, py::arg("keys"),
             R"(Checks if any of the keys exist in the environment variables.

Args:
    keys: The list of key names.

Returns:
    True if any key exists, otherwise False.
)")
        .def("del", &atom::utils::Env::del, py::arg("key"),
             R"(Deletes a key-value pair from the environment variables.

Args:
    key: The key name.
)")
        .def("del_multiple", &atom::utils::Env::delMultiple, py::arg("keys"),
             R"(Deletes multiple key-value pairs from the environment variables.

Args:
    keys: The list of key names to delete.
)")
        .def(
            "get", &atom::utils::Env::get, py::arg("key"),
            py::arg("default_value") = "",
            R"(Gets the value associated with a key, or returns a default value if the key does not exist.

Args:
    key: The key name.
    default_value: The default value to return if the key does not exist.

Returns:
    The value associated with the key, or the default value.
)")
        .def(
            "get_as",
            [](atom::utils::Env& self, const std::string& key,
               py::object default_value) {
                // Check the type of default_value to determine what type we
                // should return
                if (py::isinstance<py::int_>(default_value))
                    return py::cast(
                        self.getAs<int>(key, default_value.cast<int>()));
                else if (py::isinstance<py::float_>(default_value))
                    return py::cast(
                        self.getAs<double>(key, default_value.cast<double>()));
                else if (py::isinstance<py::bool_>(default_value))
                    return py::cast(
                        self.getAs<bool>(key, default_value.cast<bool>()));
                else
                    return py::cast(
                        self.get(key, default_value.cast<std::string>()));
            },
            py::arg("key"), py::arg("default_value") = py::none(),
            R"(Gets the value associated with a key and converts it to the specified type.

Args:
    key: The key name.
    default_value: The default value to return if the key does not exist or conversion fails.
                  The type of this parameter determines the return type.

Returns:
    The value converted to the appropriate type, or the default value.

Examples:
    >>> from atom.system import env
    >>> e = env.Env()
    >>> e.add("INT_VAR", "42")
    >>> print(e.get_as("INT_VAR", 0))  # Returns as int
    42
    >>> print(e.get_as("FLOAT_VAR", 3.14))  # Returns as float
    3.14
)")
        .def(
            "get_optional",
            [](atom::utils::Env& self, const std::string& key,
               py::object type_hint) {
                // Use type_hint to determine what type we should return
                if (py::isinstance<py::type>(type_hint)) {
                    if (type_hint.is(py::int_()))
                        return py::cast(self.getOptional<int>(key));
                    else if (type_hint.is(py::float_()))
                        return py::cast(self.getOptional<double>(key));
                    else if (type_hint.is(py::bool_()))
                        return py::cast(self.getOptional<bool>(key));
                    else if (type_hint.is(py::str()))
                        return py::cast(self.getOptional<std::string>(key));
                    else
                        throw std::invalid_argument(
                            "Unsupported type hint provided.");
                }
                // Default to string
                return py::cast(self.getOptional<std::string>(key));
            },
            py::arg("key"), py::arg("type_hint") = py::type::of<std::string>(),
            R"(Gets the value associated with a key as an optional type.

Args:
    key: The key name.
    type_hint: Optional type hint (int, float, bool, or str) to determine conversion.

Returns:
    The value if it exists and can be converted, otherwise None.

Examples:
    >>> from atom.system import env
    >>> e = env.Env()
    >>> e.add("INT_VAR", "42")
    >>> print(e.get_optional("INT_VAR", int))
    42
    >>> print(e.get_optional("MISSING_VAR", int))
    None
)")
        .def("set_env", &atom::utils::Env::setEnv, py::arg("key"),
             py::arg("val"),
             R"(Sets the value of an environment variable.

Args:
    key: The key name.
    val: The value to set.

Returns:
    True if the environment variable was set successfully, otherwise False.
)")
        .def("set_env_multiple", &atom::utils::Env::setEnvMultiple,
             py::arg("vars"),
             R"(Sets multiple environment variables.

Args:
    vars: The dictionary of key-value pairs to set.

Returns:
    True if all environment variables were set successfully, otherwise False.
)")
        .def(
            "get_env", &atom::utils::Env::getEnv, py::arg("key"),
            py::arg("default_value") = "",
            R"(Gets the value of an environment variable, or returns a default value if the variable does not exist.

Args:
    key: The key name.
    default_value: The default value to return if the variable does not exist.

Returns:
    The value of the environment variable, or the default value.
)")
        .def(
            "get_env_as",
            [](atom::utils::Env& self, const std::string& key,
               py::object default_value) {
                if (py::isinstance<py::int_>(default_value))
                    return py::cast(
                        self.getEnvAs<int>(key, default_value.cast<int>()));
                else if (py::isinstance<py::float_>(default_value))
                    return py::cast(self.getEnvAs<double>(
                        key, default_value.cast<double>()));
                else if (py::isinstance<py::bool_>(default_value))
                    return py::cast(
                        self.getEnvAs<bool>(key, default_value.cast<bool>()));
                else
                    return py::cast(
                        self.getEnv(key, default_value.cast<std::string>()));
            },
            py::arg("key"), py::arg("default_value") = py::none(),
            R"(Gets the value of an environment variable and converts it to the specified type.

Args:
    key: The key name.
    default_value: The default value to return if the variable does not exist or conversion fails.
                  The type of this parameter determines the return type.

Returns:
    The value converted to the appropriate type, or the default value.
)")
        .def("unset_env", &atom::utils::Env::unsetEnv, py::arg("name"),
             R"(Unsets an environment variable.

Args:
    name: The name of the environment variable to unset.
)")
        .def("unset_env_multiple", &atom::utils::Env::unsetEnvMultiple,
             py::arg("names"),
             R"(Unsets multiple environment variables.

Args:
    names: The list of environment variable names to unset.
)")
        .def_static("list_variables", &atom::utils::Env::listVariables,
                    R"(Lists all environment variables.

Returns:
    A list of environment variable names.
)")
        .def_static("filter_variables", &atom::utils::Env::filterVariables,
                    py::arg("predicate"),
                    R"(Filters environment variables based on a predicate.

Args:
    predicate: The predicate function that takes a key-value pair and returns a boolean.

Returns:
    A dictionary of filtered environment variables.

Examples:
    >>> from atom.system import env
    >>> # Get all variables with values containing 'python'
    >>> vars = env.Env.filter_variables(lambda k, v: 'python' in v.lower())
)")
        .def_static(
            "get_variables_with_prefix",
            &atom::utils::Env::getVariablesWithPrefix, py::arg("prefix"),
            R"(Gets all environment variables that start with a given prefix.

Args:
    prefix: The prefix to filter by.

Returns:
    A dictionary of environment variables with the given prefix.

Examples:
    >>> from atom.system import env
    >>> # Get all PATH-related variables
    >>> path_vars = env.Env.get_variables_with_prefix("PATH")
)")
        .def_static(
            "save_to_file", &atom::utils::Env::saveToFile, py::arg("file_path"),
            py::arg("vars") = std::unordered_map<std::string, std::string>{},
            R"(Saves environment variables to a file.

Args:
    file_path: The path to the file.
    vars: The dictionary of variables to save, or all environment variables if empty.

Returns:
    True if the save was successful, otherwise False.
)")
        .def_static("load_from_file", &atom::utils::Env::loadFromFile,
                    py::arg("file_path"), py::arg("overwrite") = false,
                    R"(Loads environment variables from a file.

Args:
    file_path: The path to the file.
    overwrite: Whether to overwrite existing variables.

Returns:
    True if the load was successful, otherwise False.
)")
        .def("get_executable_path", &atom::utils::Env::getExecutablePath,
             R"(Gets the executable path.

Returns:
    The full path of the executable file.
)")
        .def("get_working_directory", &atom::utils::Env::getWorkingDirectory,
             R"(Gets the working directory.

Returns:
    The working directory.
)")
        .def("get_program_name", &atom::utils::Env::getProgramName,
             R"(Gets the program name.

Returns:
    The program name.
)")
        .def("get_all_args", &atom::utils::Env::getAllArgs,
             R"(Gets all command-line arguments.

Returns:
    The dictionary of command-line arguments.
)")
#if ATOM_ENABLE_DEBUG
        .def_static("print_all_variables", &atom::utils::Env::printAllVariables,
                    "Prints all environment variables.")
        .def("print_all_args", &atom::utils::Env::printAllArgs,
             "Prints all command-line arguments.")
#endif
        .def("__getitem__", &atom::utils::Env::get, py::arg("key"),
             "Support for dictionary-like access: env[key]")
        .def("__setitem__", &atom::utils::Env::add, py::arg("key"),
             py::arg("val"),
             "Support for dictionary-like assignment: env[key] = val")
        .def("__contains__", &atom::utils::Env::has, py::arg("key"),
             "Support for the 'in' operator: key in env");

    // Additional utility functions at module level
    m.def(
        "get_env",
        [](const std::string& key, const std::string& default_value) {
            return atom::utils::Env().getEnv(key, default_value);
        },
        py::arg("key"), py::arg("default_value") = "",
        R"(Gets the value of an environment variable.

Args:
    key: The environment variable name.
    default_value: Value to return if the variable doesn't exist.

Returns:
    The value of the environment variable, or the default value.

Examples:
    >>> from atom.system import env
    >>> home = env.get_env("HOME", "")
    >>> print(f"Home directory: {home}")
)");

    m.def(
        "set_env",
        [](const std::string& key, const std::string& val) {
            return atom::utils::Env().setEnv(key, val);
        },
        py::arg("key"), py::arg("val"),
        R"(Sets an environment variable.

Args:
    key: The environment variable name.
    val: The value to set.

Returns:
    True if successful, False otherwise.

Examples:
    >>> from atom.system import env
    >>> env.set_env("MY_CUSTOM_VAR", "my_value")
)");

    m.def(
        "unset_env",
        [](const std::string& name) { atom::utils::Env().unsetEnv(name); },
        py::arg("name"),
        R"(Unsets (removes) an environment variable.

Args:
    name: The environment variable name to remove.

Examples:
    >>> from atom.system import env
    >>> env.unset_env("MY_CUSTOM_VAR")
)");

    m.def(
        "get_all_env", []() { return atom::utils::Env::Environ(); },
        R"(Gets all environment variables.

Returns:
    Dictionary of all environment variables.

Examples:
    >>> from atom.system import env
    >>> all_vars = env.get_all_env()
    >>> for key, value in all_vars.items():
    ...     print(f"{key} = {value}")
)");
}