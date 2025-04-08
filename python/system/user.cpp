#include "atom/system/user.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(user, m) {
    m.doc() = "User information and management module for the atom package";

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

    // User information functions
    m.def("get_user_groups", &atom::system::getUserGroups,
          R"(Get user groups.

Returns:
    List of group names the current user belongs to.

Examples:
    >>> from atom.system import user
    >>> groups = user.get_user_groups()
    >>> print(groups)
)");

    m.def("get_username", &atom::system::getUsername,
          R"(Get the username of the current user.

Returns:
    Username of the current user.

Examples:
    >>> from atom.system import user
    >>> username = user.get_username()
    >>> print(f"Current user: {username}")
)");

    m.def("get_hostname", &atom::system::getHostname,
          R"(Get the hostname of the system.

Returns:
    Hostname of the system.

Examples:
    >>> from atom.system import user
    >>> hostname = user.get_hostname()
    >>> print(f"Host: {hostname}")
)");

    m.def("get_user_id", &atom::system::getUserId,
          R"(Get the user ID of the current user.

Returns:
    User ID of the current user.

Examples:
    >>> from atom.system import user
    >>> uid = user.get_user_id()
    >>> print(f"UID: {uid}")
)");

    m.def("get_group_id", &atom::system::getGroupId,
          R"(Get the group ID of the current user.

Returns:
    Group ID of the current user.

Examples:
    >>> from atom.system import user
    >>> gid = user.get_group_id()
    >>> print(f"GID: {gid}")
)");

    m.def("get_home_directory", &atom::system::getHomeDirectory,
          R"(Get the home directory of the current user.

Returns:
    Home directory path of the current user.

Examples:
    >>> from atom.system import user
    >>> home = user.get_home_directory()
    >>> print(f"Home directory: {home}")
)");

    m.def("get_current_working_directory",
          &atom::system::getCurrentWorkingDirectory,
          R"(Get the current working directory.

Returns:
    Current working directory path.

Examples:
    >>> from atom.system import user
    >>> cwd = user.get_current_working_directory()
    >>> print(f"Current directory: {cwd}")
)");

    m.def("get_login_shell", &atom::system::getLoginShell,
          R"(Get the login shell of the current user.

Returns:
    Login shell path of the current user.

Examples:
    >>> from atom.system import user
    >>> shell = user.get_login_shell()
    >>> print(f"Shell: {shell}")
)");

#ifdef _WIN32
    m.def("get_user_profile_directory", &atom::system::getUserProfileDirectory,
          R"(Get the user profile directory (Windows only).

Returns:
    User profile directory path.

Examples:
    >>> from atom.system import user
    >>> profile = user.get_user_profile_directory()
    >>> print(f"Profile directory: {profile}")
)");
#endif

    m.def("get_login", &atom::system::getLogin,
          R"(Retrieves the login name of the user.

This function retrieves the login name of the user associated with the
current process.

Returns:
    The login name of the user.

Examples:
    >>> from atom.system import user
    >>> login = user.get_login()
    >>> print(f"Login name: {login}")
)");

    m.def("is_root", &atom::system::isRoot,
          R"(Check whether the current user has root/administrator privileges.

Returns:
    True if the current user has root/administrator privileges,
    False otherwise.

Examples:
    >>> from atom.system import user
    >>> if user.is_root():
    ...     print("Running with administrator privileges")
    ... else:
    ...     print("Running with standard user privileges")
)");

    m.def("get_environment_variable", &atom::system::getEnvironmentVariable,
          py::arg("name"),
          R"(Get the value of an environment variable.

Args:
    name: The name of the environment variable.

Returns:
    The value of the environment variable or an empty string if not found.

Examples:
    >>> from atom.system import user
    >>> path = user.get_environment_variable("PATH")
    >>> print(f"PATH: {path}")
)");

    m.def("get_all_environment_variables",
          &atom::system::getAllEnvironmentVariables,
          R"(Get all environment variables.

Returns:
    A dictionary containing all environment variables.

Examples:
    >>> from atom.system import user
    >>> env_vars = user.get_all_environment_variables()
    >>> for name, value in env_vars.items():
    ...     print(f"{name}={value}")
)");

    m.def("set_environment_variable", &atom::system::setEnvironmentVariable,
          py::arg("name"), py::arg("value"),
          R"(Set the value of an environment variable.

Args:
    name: The name of the environment variable.
    value: The value to set.

Returns:
    True if the environment variable was set successfully,
    False if the environment variable could not be set.

Examples:
    >>> from atom.system import user
    >>> success = user.set_environment_variable("MY_VAR", "my_value")
    >>> if success:
    ...     print("Variable set successfully")
)");

    m.def("get_system_uptime", &atom::system::getSystemUptime,
          R"(Get the system uptime in seconds.

Returns:
    System uptime in seconds.

Examples:
    >>> from atom.system import user
    >>> uptime = user.get_system_uptime()
    >>> print(f"System uptime: {uptime} seconds")
    >>> # Convert to days/hours/minutes
    >>> days = uptime // (24 * 3600)
    >>> uptime %= (24 * 3600)
    >>> hours = uptime // 3600
    >>> uptime %= 3600
    >>> minutes = uptime // 60
    >>> print(f"System has been up for {days} days, {hours} hours, {minutes} minutes")
)");

    m.def("get_logged_in_users", &atom::system::getLoggedInUsers,
          R"(Get the list of logged-in users.

Returns:
    A list of logged-in user names.

Examples:
    >>> from atom.system import user
    >>> users = user.get_logged_in_users()
    >>> print(f"Logged in users: {', '.join(users)}")
)");

    m.def("user_exists", &atom::system::userExists, py::arg("username"),
          R"(Check if a user exists.

Args:
    username: The username to check.

Returns:
    True if the user exists, False otherwise.

Examples:
    >>> from atom.system import user
    >>> if user.user_exists("admin"):
    ...     print("Admin user exists")
    ... else:
    ...     print("Admin user does not exist")
)");

    // Add convenient utilities
    m.def(
        "get_user_info",
        []() {
            py::dict result;
            result["username"] = atom::system::getUsername();
            result["uid"] = atom::system::getUserId();
            result["gid"] = atom::system::getGroupId();
            result["home"] = atom::system::getHomeDirectory();
            result["shell"] = atom::system::getLoginShell();
            result["is_admin"] = atom::system::isRoot();
            result["hostname"] = atom::system::getHostname();
            return result;
        },
        R"(Get a dictionary with basic user information.

Returns:
    A dictionary containing username, uid, gid, home directory, shell, admin status, and hostname.

Examples:
    >>> from atom.system import user
    >>> info = user.get_user_info()
    >>> print(f"User {info['username']} (UID: {info['uid']}) on {info['hostname']}")
)");

    m.def(
        "expand_path",
        [](const std::string& path) {
            std::string result = path;

            // Replace ~ with home directory
            if (!result.empty() && result[0] == '~') {
                std::string home = atom::system::getHomeDirectory();
                result.replace(0, 1, home);
            }

            // Replace $VAR or ${VAR} with environment variables
            size_t pos = 0;
            while ((pos = result.find('$', pos)) != std::string::npos) {
                if (pos + 1 >= result.length())
                    break;

                size_t start = pos + 1;
                size_t end;
                bool braces = false;

                if (result[start] == '{') {
                    braces = true;
                    start++;
                    if (start >= result.length())
                        break;

                    end = result.find('}', start);
                    if (end == std::string::npos)
                        break;
                } else {
                    // Find end of variable name (non-alphanumeric or
                    // underscore)
                    end = start;
                    while (end < result.length() &&
                           (std::isalnum(result[end]) || result[end] == '_')) {
                        end++;
                    }
                }

                if (start == end) {
                    pos++;
                    continue;
                }

                std::string var_name = result.substr(start, end - start);
                std::string var_value =
                    atom::system::getEnvironmentVariable(var_name);

                // Replace the variable
                size_t replace_start = pos;
                size_t replace_len = (braces ? end + 1 : end) - pos;
                result.replace(replace_start, replace_len, var_value);

                // Adjust position for next search
                pos = replace_start + var_value.length();
            }

            return result;
        },
        py::arg("path"),
        R"(Expand a path, replacing ~ with the home directory and $VAR with environment variables.

Args:
    path: The path to expand.

Returns:
    The expanded path.

Examples:
    >>> from atom.system import user
    >>> path = user.expand_path("~/Documents")
    >>> print(path)  # Expands to /home/username/Documents or similar
    >>> config_path = user.expand_path("$HOME/.config")
    >>> print(config_path)
)");

    // Context manager for environment variables
    py::class_<py::object>(m, "EnvironmentContext",
                           "A context manager for environment variables")
        .def(py::init([](py::dict variables) {
                 return py::object();  // Placeholder, actual implementation in
                                       // __enter__
             }),
             py::arg("variables"),
             "Create a context manager for temporarily setting environment "
             "variables")
        .def("__enter__",
             [](py::object& self, py::dict variables) {
                 py::dict old_values;
                 for (auto item : variables) {
                     std::string key = py::str(item.first);
                     std::string value = py::str(item.second);

                     // Save old value if it exists
                     std::string old_value =
                         atom::system::getEnvironmentVariable(key);
                     old_values[key.c_str()] = old_value;

                     // Set new value
                     atom::system::setEnvironmentVariable(key, value);
                 }
                 self.attr("_old_values") = old_values;
                 return self;
             })
        .def("__exit__", [](py::object& self, py::object exc_type,
                            py::object exc_val, py::object exc_tb) {
            py::dict old_values = self.attr("_old_values");
            for (auto item : old_values) {
                std::string key = py::str(item.first);
                std::string value = py::str(item.second);

                // Restore old value
                atom::system::setEnvironmentVariable(key, value);
            }
            return false;  // Don't suppress exceptions
        });

    m.def(
        "temp_env",
        [](py::dict variables) {
            return m.attr("EnvironmentContext")(variables);
        },
        py::arg("variables"),
        R"(Create a context manager for temporarily setting environment variables.

Args:
    variables: Dictionary of environment variables to set temporarily.

Returns:
    A context manager object.

Examples:
    >>> from atom.system import user
    >>> # Temporarily modify environment variables
    >>> with user.temp_env({"DEBUG": "1", "LOG_LEVEL": "INFO"}):
    ...     # Code that uses these environment variables
    ...     debug = user.get_environment_variable("DEBUG")
    ...     assert debug == "1"
    >>> # Original environment is restored after the with block
)");
}