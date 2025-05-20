#include "atom/system/env.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;
std::shared_ptr<atom::utils::Env> create_env_shared_wrapper(
    int argc, py::list argv_list) {
    // 将Python列表转换为char**
    std::vector<std::string> argv_strings;
    std::vector<char*> argv_ptrs;

    for (auto item : argv_list) {
        std::string str = item.cast<std::string>();
        argv_strings.push_back(str);
    }

    for (auto& s : argv_strings) {
        argv_ptrs.push_back(&s[0]);
    }
    argv_ptrs.push_back(nullptr);  // 结尾需要NULL

    return atom::utils::Env::createShared(argc, argv_ptrs.data());
}

atom::utils::Env* create_env_wrapper(int argc, py::list argv_list) {
    std::vector<std::string> argv_strings;
    std::vector<char*> argv_ptrs;

    for (auto item : argv_list) {
        std::string str = item.cast<std::string>();
        argv_strings.push_back(str);
    }

    for (auto& s : argv_strings) {
        argv_ptrs.push_back(&s[0]);
    }
    argv_ptrs.push_back(nullptr);

    return new atom::utils::Env(argc, argv_ptrs.data());
}

PYBIND11_MODULE(env, m) {
    m.doc() = "Environment variable management module for the atom package";

    // 注册异常转换
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

    // 注册环境变量格式枚举
    py::enum_<atom::utils::VariableFormat>(m, "VariableFormat",
                                           "Environment variable format")
        .value("UNIX", atom::utils::VariableFormat::UNIX,
               "Unix format (${VAR} or $VAR)")
        .value("WINDOWS", atom::utils::VariableFormat::WINDOWS,
               "Windows format (%VAR%)")
        .value("AUTO", atom::utils::VariableFormat::AUTO,
               "Auto-detect based on platform")
        .export_values();

    // 注册环境变量持久化级别枚举
    py::enum_<atom::utils::PersistLevel>(
        m, "PersistLevel", "Environment variable persistence level")
        .value("PROCESS", atom::utils::PersistLevel::PROCESS,
               "Only valid for current process")
        .value("USER", atom::utils::PersistLevel::USER,
               "User level persistence")
        .value("SYSTEM", atom::utils::PersistLevel::SYSTEM,
               "System level persistence (requires admin)")
        .export_values();

    // 绑定ScopedEnv类
    py::class_<atom::utils::Env::ScopedEnv,
               std::shared_ptr<atom::utils::Env::ScopedEnv>>(
        m, "ScopedEnv",
        R"(Temporary environment variable scope.
        
When this object is created, it sets the specified environment variable.
When the object is destroyed, the original value is restored.)")
        .def(py::init<const atom::utils::String&, const atom::utils::String&>(),
             py::arg("key"), py::arg("value"),
             R"(Constructor that sets a temporary environment variable.

Args:
    key: The environment variable name.
    value: The value to set.
)");

    // Env 类绑定
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
        .def(py::init(&create_env_wrapper), py::arg("argc"), py::arg("argv"),
             "Constructor that initializes environment variable information "
             "with command-line arguments.")
        .def_static(
            "create_shared", &create_env_shared_wrapper, py::arg("argc"),
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
            py::arg("default_value") = std::string(""),
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
                        throw py::type_error("Unsupported type hint");
                }
                return py::cast(self.getOptional<std::string>(key));
            },
            py::arg("key"), py::arg("type_hint") = py::str().get_type(),
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
        .def_static("set_env", &atom::utils::Env::setEnv, py::arg("key"),
                    py::arg("val"),
                    R"(Sets the value of an environment variable.

Args:
    key: The key name.
    val: The value to set.

Returns:
    True if the environment variable was set successfully, otherwise False.
)")
        .def_static("set_env_multiple", &atom::utils::Env::setEnvMultiple,
                    py::arg("vars"),
                    R"(Sets multiple environment variables.

Args:
    vars: The dictionary of key-value pairs to set.

Returns:
    True if all environment variables were set successfully, otherwise False.
)")
        .def_static(
            "get_env", &atom::utils::Env::getEnv, py::arg("key"),
            py::arg("default_value") = std::string(""),
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
                else if (py::isinstance<py::str>(default_value))
                    return py::cast(
                        self.getEnv(key, default_value.cast<std::string>()));
                else
                    throw py::type_error("Unsupported type hint");
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
        .def_static("unset_env", &atom::utils::Env::unsetEnv, py::arg("name"),
                    R"(Unsets an environment variable.

Args:
    name: The name of the environment variable to unset.
)")
        .def_static("unset_env_multiple", &atom::utils::Env::unsetEnvMultiple,
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
        .def_static(
            "filter_variables",
            [](const py::function& py_predicate) {
                // 创建一个lambda来包装Python谓词函数
                auto cpp_predicate = [&py_predicate](const std::string& key,
                                                     const std::string& val) {
                    py::gil_scoped_acquire gil;
                    return py_predicate(key, val).cast<bool>();
                };
                return atom::utils::Env::filterVariables(cpp_predicate);
            },
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
            "save_to_file",
            [](const std::filesystem::path& file_path,
               const std::unordered_map<std::string, std::string>& vars) {
                return atom::utils::Env::saveToFile(file_path, vars);
            },
            py::arg("file_path"),
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
        // 添加新增的静态方法
        .def_static("get_home_dir", &atom::utils::Env::getHomeDir,
                    R"(Gets the user's home directory.

Returns:
    The path to the user's home directory.
)")
        .def_static("get_temp_dir", &atom::utils::Env::getTempDir,
                    R"(Gets the system temporary directory.

Returns:
    The path to the system temporary directory.
)")
        .def_static("get_config_dir", &atom::utils::Env::getConfigDir,
                    R"(Gets the system configuration directory.

Returns:
    The path to the system configuration directory.
)")
        .def_static("get_data_dir", &atom::utils::Env::getDataDir,
                    R"(Gets the user data directory.

Returns:
    The path to the user data directory.
)")
        .def_static("expand_variables", &atom::utils::Env::expandVariables,
                    py::arg("str"),
                    py::arg("format") = atom::utils::VariableFormat::AUTO,
                    R"(Expands environment variables in a string.

Args:
    str: The string containing environment variable references (e.g., "$HOME/file" or "%PATH%;newpath").
    format: The environment variable format (Unix style ${VAR} or Windows style %VAR%).

Returns:
    The expanded string.
)")
        .def_static("set_persistent_env", &atom::utils::Env::setPersistentEnv,
                    py::arg("key"), py::arg("val"),
                    py::arg("level") = atom::utils::PersistLevel::USER,
                    R"(Persistently sets an environment variable.

Args:
    key: The environment variable name.
    val: The value to set.
    level: The persistence level (PROCESS, USER, or SYSTEM).

Returns:
    True if the variable was successfully set, otherwise False.
)")
        .def_static("delete_persistent_env",
                    &atom::utils::Env::deletePersistentEnv, py::arg("key"),
                    py::arg("level") = atom::utils::PersistLevel::USER,
                    R"(Persistently deletes an environment variable.

Args:
    key: The environment variable name.
    level: The persistence level (PROCESS, USER, or SYSTEM).

Returns:
    True if the variable was successfully deleted, otherwise False.
)")
        .def_static("add_to_path", &atom::utils::Env::addToPath,
                    py::arg("path"), py::arg("prepend") = false,
                    R"(Adds a path to the PATH environment variable.

Args:
    path: The path to add.
    prepend: Whether to add the path to the beginning (True) or end (False) of PATH.

Returns:
    True if the path was successfully added, otherwise False.
)")
        .def_static("remove_from_path", &atom::utils::Env::removeFromPath,
                    py::arg("path"),
                    R"(Removes a path from the PATH environment variable.

Args:
    path: The path to remove.

Returns:
    True if the path was successfully removed, otherwise False.
)")
        .def_static("is_in_path", &atom::utils::Env::isInPath, py::arg("path"),
                    R"(Checks if a path is in the PATH environment variable.

Args:
    path: The path to check.

Returns:
    True if the path is in PATH, otherwise False.
)")
        .def_static("get_path_entries", &atom::utils::Env::getPathEntries,
                    R"(Gets all paths in the PATH environment variable.

Returns:
    A list of all paths in PATH.
)")
        .def_static("diff_environments", &atom::utils::Env::diffEnvironments,
                    py::arg("env1"), py::arg("env2"),
                    R"(Compares two environment variable sets.

Args:
    env1: First environment variable set.
    env2: Second environment variable set.

Returns:
    A tuple of (added, removed, modified) variables.
)")
        .def_static("merge_environments", &atom::utils::Env::mergeEnvironments,
                    py::arg("base_env"), py::arg("overlay_env"),
                    py::arg("override") = true,
                    R"(Merges two environment variable sets.

Args:
    base_env: Base environment variable set.
    overlay_env: Overlay environment variable set.
    override: Whether to override base variables with overlay variables when conflicts occur.

Returns:
    The merged environment variable set.
)")
        .def_static("get_system_name", &atom::utils::Env::getSystemName,
                    R"(Gets the system name.

Returns:
    The system name (e.g., "Windows", "Linux", "MacOS").
)")
        .def_static("get_system_arch", &atom::utils::Env::getSystemArch,
                    R"(Gets the system architecture.

Returns:
    The system architecture (e.g., "x86_64", "arm64").
)")
        .def_static("get_current_user", &atom::utils::Env::getCurrentUser,
                    R"(Gets the current user name.

Returns:
    The current user name.
)")
        .def_static("get_host_name", &atom::utils::Env::getHostName,
                    R"(Gets the host name.

Returns:
    The host name.
)")
        .def_static(
            "register_change_notification",
            [](const py::function& py_callback) {
                auto cpp_callback = [py_callback](const std::string& key,
                                                  const std::string& oldValue,
                                                  const std::string& newValue) {
                    py::gil_scoped_acquire gil;
                    py_callback(key, oldValue, newValue);
                };
                return atom::utils::Env::registerChangeNotification(
                    cpp_callback);
            },
            py::arg("callback"),
            R"(Registers a notification for environment variable changes.

Args:
    callback: A function that takes (key, old_value, new_value) parameters.

Returns:
    A notification ID that can be used to unregister the notification.
)")
        .def_static("unregister_change_notification",
                    &atom::utils::Env::unregisterChangeNotification,
                    py::arg("id"),
                    R"(Unregisters an environment variable change notification.

Args:
    id: The notification ID to unregister.

Returns:
    True if the notification was successfully unregistered, otherwise False.
)")
        .def_static("create_scoped_env", &atom::utils::Env::createScopedEnv,
                    py::arg("key"), py::arg("value"),
                    R"(Creates a temporary environment variable scope.

Args:
    key: The environment variable name.
    value: The value to set.

Returns:
    A ScopedEnv object that will restore the original value when destroyed.

Example:
    >>> from atom.system import env
    >>> # Create a temporary environment variable
    >>> with env.Env.create_scoped_env("TEMP_VAR", "temp_value") as scoped:
    ...     # TEMP_VAR is set to "temp_value" here
    ...     print(env.get_env("TEMP_VAR"))
    ... # TEMP_VAR is restored to its original value here
)")
#if ATOM_ENABLE_DEBUG
        .def_static("print_all_variables", &atom::utils::Env::printAllVariables,
                    "Prints all environment variables.")
        .def("print_all_args", &atom::utils::Env::printAllArgs,
             "Prints all command-line arguments.")
#endif
        .def(
            "__getitem__",
            [](atom::utils::Env& self, const std::string& key) {
                return self.get(key, "");
            },
            py::arg("key"), "Support for dictionary-like access: env[key]")
        .def("__setitem__", &atom::utils::Env::add, py::arg("key"),
             py::arg("val"),
             "Support for dictionary-like assignment: env[key] = val")
        .def("__contains__", &atom::utils::Env::has, py::arg("key"),
             "Support for the 'in' operator: key in env");

    // 额外的模块级别工具函数
    m.def(
        "get_env",
        [](const std::string& key, const std::string& default_value) {
            return atom::utils::Env().getEnv(key, default_value);
        },
        py::arg("key"), py::arg("default_value") = std::string(""),
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

    // 添加新增的便捷模块级函数
    m.def(
        "expand_variables",
        [](const std::string& str, atom::utils::VariableFormat format =
                                       atom::utils::VariableFormat::AUTO) {
            return atom::utils::Env::expandVariables(str, format);
        },
        py::arg("str"), py::arg("format") = atom::utils::VariableFormat::AUTO,
        R"(Expands environment variables in a string.

Args:
    str: The string containing environment variable references.
    format: The environment variable format (UNIX, WINDOWS, or AUTO).

Returns:
    The expanded string.

Examples:
    >>> from atom.system import env
    >>> path = env.expand_variables("$HOME/documents")
    >>> print(path)
)");

    m.def("get_home_dir", &atom::utils::Env::getHomeDir,
          R"(Gets the user's home directory.

Returns:
    The path to the user's home directory.

Examples:
    >>> from atom.system import env
    >>> home = env.get_home_dir()
    >>> print(f"Home directory: {home}")
)");

    m.def(
        "get_system_info",
        []() {
            return py::dict(
                py::arg("system") = atom::utils::Env::getSystemName(),
                py::arg("arch") = atom::utils::Env::getSystemArch(),
                py::arg("user") = atom::utils::Env::getCurrentUser(),
                py::arg("host") = atom::utils::Env::getHostName());
        },
        R"(Gets system information.

Returns:
    A dictionary containing system name, architecture, user name, and host name.

Examples:
    >>> from atom.system import env
    >>> info = env.get_system_info()
    >>> print(f"System: {info['system']} ({info['arch']}) ")
    >>> print(f"User: {info['user']} on {info['host']}")
)");
}