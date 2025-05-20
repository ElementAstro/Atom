#include "atom/system/wregistry.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <windows.h>

namespace py = pybind11;

PYBIND11_MODULE(wregistry, m) {
    m.doc() = "Windows Registry functions module for the atom package";

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

    // Define the predefined root keys as module constants
    m.attr("HKEY_CLASSES_ROOT") = reinterpret_cast<intptr_t>(HKEY_CLASSES_ROOT);
    m.attr("HKEY_CURRENT_USER") = reinterpret_cast<intptr_t>(HKEY_CURRENT_USER);
    m.attr("HKEY_LOCAL_MACHINE") =
        reinterpret_cast<intptr_t>(HKEY_LOCAL_MACHINE);
    m.attr("HKEY_USERS") = reinterpret_cast<intptr_t>(HKEY_USERS);
    m.attr("HKEY_CURRENT_CONFIG") =
        reinterpret_cast<intptr_t>(HKEY_CURRENT_CONFIG);

    // Bind the functions
    m.def(
        "get_registry_subkeys",
        [](intptr_t hRootKey, const std::string& subKey) {
            std::vector<std::string> subKeys;
            bool success = atom::system::getRegistrySubKeys(
                reinterpret_cast<HKEY>(hRootKey), subKey, subKeys);

            if (!success) {
                throw py::value_error("Failed to get registry subkeys for: " +
                                      subKey);
            }

            return subKeys;
        },
        py::arg("hRootKey"), py::arg("subKey"),
        R"(Get all subkeys of a specified registry key.

Args:
    hRootKey: Root key handle (use predefined constants like HKEY_LOCAL_MACHINE).
    subKey: The subkey path to examine, with components separated by backslashes.

Returns:
    List of subkey names.

Raises:
    ValueError: If the operation fails.

Examples:
    >>> from atom.system import wregistry
    >>> # List all subkeys in HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft
    >>> subkeys = wregistry.get_registry_subkeys(
    ...     wregistry.HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft"
    ... )
    >>> print(subkeys)
    ['Windows', 'Office', ...]
)");

    m.def(
        "get_registry_values",
        [](intptr_t hRootKey, const std::string& subKey) {
            std::vector<std::pair<std::string, std::string>> values;
            bool success = atom::system::getRegistryValues(
                reinterpret_cast<HKEY>(hRootKey), subKey, values);

            if (!success) {
                throw py::value_error("Failed to get registry values for: " +
                                      subKey);
            }

            // Convert to Python dictionary for more natural usage
            py::dict result;
            for (const auto& [name, value] : values) {
                result[py::str(name)] = py::str(value);
            }

            return result;
        },
        py::arg("hRootKey"), py::arg("subKey"),
        R"(Get all value names and data for a specified registry key.

Args:
    hRootKey: Root key handle (use predefined constants like HKEY_LOCAL_MACHINE).
    subKey: The subkey path to examine, with components separated by backslashes.

Returns:
    Dictionary mapping value names to their data.

Raises:
    ValueError: If the operation fails.

Examples:
    >>> from atom.system import wregistry
    >>> # Get values from HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run
    >>> values = wregistry.get_registry_values(
    ...     wregistry.HKEY_CURRENT_USER, 
    ...     "Software\\Microsoft\\Windows\\CurrentVersion\\Run"
    ... )
    >>> for name, value in values.items():
    ...     print(f"{name}: {value}")
)");

    m.def(
        "modify_registry_value",
        [](intptr_t hRootKey, const std::string& subKey,
           const std::string& valueName, const std::string& newValue) {
            bool success = atom::system::modifyRegistryValue(
                reinterpret_cast<HKEY>(hRootKey), subKey, valueName, newValue);

            if (!success) {
                throw py::value_error("Failed to modify registry value: " +
                                      subKey + "\\" + valueName);
            }

            return true;
        },
        py::arg("hRootKey"), py::arg("subKey"), py::arg("valueName"),
        py::arg("newValue"),
        R"(Modify a specific registry value.

Args:
    hRootKey: Root key handle (use predefined constants like HKEY_LOCAL_MACHINE).
    subKey: The subkey path, with components separated by backslashes.
    valueName: The name of the value to modify.
    newValue: The new data for the value.

Returns:
    True if the operation was successful.

Raises:
    ValueError: If the operation fails.

Examples:
    >>> from atom.system import wregistry
    >>> # Modify a value (requires appropriate permissions)
    >>> try:
    ...     wregistry.modify_registry_value(
    ...         wregistry.HKEY_CURRENT_USER,
    ...         "Software\\MyApp\\Settings",
    ...         "LastRun",
    ...         "2023-06-17"
    ...     )
    ...     print("Value modified successfully")
    ... except ValueError as e:
    ...     print(f"Error: {e}")
)");

    m.def(
        "delete_registry_subkey",
        [](intptr_t hRootKey, const std::string& subKey) {
            bool success = atom::system::deleteRegistrySubKey(
                reinterpret_cast<HKEY>(hRootKey), subKey);

            if (!success) {
                throw py::value_error("Failed to delete registry subkey: " +
                                      subKey);
            }

            return true;
        },
        py::arg("hRootKey"), py::arg("subKey"),
        R"(Delete a registry key and all its subkeys.

Args:
    hRootKey: Root key handle (use predefined constants like HKEY_LOCAL_MACHINE).
    subKey: The subkey path to delete, with components separated by backslashes.

Returns:
    True if the operation was successful.

Raises:
    ValueError: If the operation fails.

Examples:
    >>> from atom.system import wregistry
    >>> # Delete a registry key (requires appropriate permissions)
    >>> try:
    ...     wregistry.delete_registry_subkey(
    ...         wregistry.HKEY_CURRENT_USER,
    ...         "Software\\TemporaryApp"
    ...     )
    ...     print("Registry key deleted successfully")
    ... except ValueError as e:
    ...     print(f"Error: {e}")
)");

    m.def(
        "delete_registry_value",
        [](intptr_t hRootKey, const std::string& subKey,
           const std::string& valueName) {
            bool success = atom::system::deleteRegistryValue(
                reinterpret_cast<HKEY>(hRootKey), subKey, valueName);

            if (!success) {
                throw py::value_error("Failed to delete registry value: " +
                                      subKey + "\\" + valueName);
            }

            return true;
        },
        py::arg("hRootKey"), py::arg("subKey"), py::arg("valueName"),
        R"(Delete a specific registry value.

Args:
    hRootKey: Root key handle (use predefined constants like HKEY_LOCAL_MACHINE).
    subKey: The subkey path, with components separated by backslashes.
    valueName: The name of the value to delete.

Returns:
    True if the operation was successful.

Raises:
    ValueError: If the operation fails.

Examples:
    >>> from atom.system import wregistry
    >>> # Delete a registry value (requires appropriate permissions)
    >>> try:
    ...     wregistry.delete_registry_value(
    ...         wregistry.HKEY_CURRENT_USER,
    ...         "Software\\MyApp\\Settings",
    ...         "TemporaryData"
    ...     )
    ...     print("Registry value deleted successfully")
    ... except ValueError as e:
    ...     print(f"Error: {e}")
)");

    m.def(
        "recursively_enumerate_registry_subkeys",
        [](intptr_t hRootKey, const std::string& subKey) {
            atom::system::recursivelyEnumerateRegistrySubKeys(
                reinterpret_cast<HKEY>(hRootKey), subKey);
        },
        py::arg("hRootKey"), py::arg("subKey"),
        R"(Recursively enumerate all subkeys and values under a registry key.

This function prints all found keys and values to standard output.

Args:
    hRootKey: Root key handle (use predefined constants like HKEY_LOCAL_MACHINE).
    subKey: The subkey path to enumerate, with components separated by backslashes.

Examples:
    >>> from atom.system import wregistry
    >>> # Enumerate all keys under HKEY_CURRENT_USER\Software\Microsoft\Windows
    >>> wregistry.recursively_enumerate_registry_subkeys(
    ...     wregistry.HKEY_CURRENT_USER,
    ...     "Software\\Microsoft\\Windows"
    ... )
)");

    m.def(
        "backup_registry",
        [](intptr_t hRootKey, const std::string& subKey,
           const std::string& backupFilePath) {
            bool success = atom::system::backupRegistry(
                reinterpret_cast<HKEY>(hRootKey), subKey, backupFilePath);

            if (!success) {
                throw py::value_error("Failed to backup registry key: " +
                                      subKey + " to file: " + backupFilePath);
            }

            return true;
        },
        py::arg("hRootKey"), py::arg("subKey"), py::arg("backupFilePath"),
        R"(Backup a registry key and all its subkeys to a file.

Args:
    hRootKey: Root key handle (use predefined constants like HKEY_LOCAL_MACHINE).
    subKey: The subkey path to backup, with components separated by backslashes.
    backupFilePath: The full path to the backup file to create.

Returns:
    True if the operation was successful.

Raises:
    ValueError: If the operation fails.

Examples:
    >>> from atom.system import wregistry
    >>> # Backup a registry key to a file
    >>> try:
    ...     wregistry.backup_registry(
    ...         wregistry.HKEY_CURRENT_USER,
    ...         "Software\\Microsoft\\Windows",
    ...         "C:\\Temp\\windows_settings_backup.reg"
    ...     )
    ...     print("Registry backup completed successfully")
    ... except ValueError as e:
    ...     print(f"Error: {e}")
)");

    m.def(
        "find_registry_key",
        [](intptr_t hRootKey, const std::string& subKey,
           const std::string& searchKey) {
            atom::system::findRegistryKey(reinterpret_cast<HKEY>(hRootKey),
                                          subKey, searchKey);
        },
        py::arg("hRootKey"), py::arg("subKey"), py::arg("searchKey"),
        R"(Recursively search for subkeys containing the specified string.

This function prints all matching keys to standard output.

Args:
    hRootKey: Root key handle (use predefined constants like HKEY_LOCAL_MACHINE).
    subKey: The subkey path to start searching from, with components separated by backslashes.
    searchKey: The string to search for in key names.

Examples:
    >>> from atom.system import wregistry
    >>> # Find all registry keys containing "Microsoft" under HKEY_CURRENT_USER\Software
    >>> wregistry.find_registry_key(
    ...     wregistry.HKEY_CURRENT_USER,
    ...     "Software",
    ...     "Microsoft"
    ... )
)");

    m.def(
        "find_registry_value",
        [](intptr_t hRootKey, const std::string& subKey,
           const std::string& searchValue) {
            atom::system::findRegistryValue(reinterpret_cast<HKEY>(hRootKey),
                                            subKey, searchValue);
        },
        py::arg("hRootKey"), py::arg("subKey"), py::arg("searchValue"),
        R"(Recursively search for registry values containing the specified string.

This function prints all matching values to standard output.

Args:
    hRootKey: Root key handle (use predefined constants like HKEY_LOCAL_MACHINE).
    subKey: The subkey path to start searching from, with components separated by backslashes.
    searchValue: The string to search for in value names or data.

Examples:
    >>> from atom.system import wregistry
    >>> # Find all registry values containing "update" under HKEY_LOCAL_MACHINE\SOFTWARE
    >>> wregistry.find_registry_value(
    ...     wregistry.HKEY_LOCAL_MACHINE,
    ...     "SOFTWARE",
    ...     "update"
    ... )
)");

    m.def(
        "export_registry",
        [](intptr_t hRootKey, const std::string& subKey,
           const std::string& exportFilePath) {
            bool success = atom::system::exportRegistry(
                reinterpret_cast<HKEY>(hRootKey), subKey, exportFilePath);

            if (!success) {
                throw py::value_error("Failed to export registry key: " +
                                      subKey + " to file: " + exportFilePath);
            }

            return true;
        },
        py::arg("hRootKey"), py::arg("subKey"), py::arg("exportFilePath"),
        R"(Export a registry key and all its subkeys to a REG file.

Args:
    hRootKey: Root key handle (use predefined constants like HKEY_LOCAL_MACHINE).
    subKey: The subkey path to export, with components separated by backslashes.
    exportFilePath: The full path to the .reg file to create.

Returns:
    True if the operation was successful.

Raises:
    ValueError: If the operation fails.

Examples:
    >>> from atom.system import wregistry
    >>> # Export a registry key to a .reg file
    >>> try:
    ...     wregistry.export_registry(
    ...         wregistry.HKEY_CURRENT_USER,
    ...         "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
    ...         "C:\\Temp\\startup_programs.reg"
    ...     )
    ...     print("Registry export completed successfully")
    ... except ValueError as e:
    ...     print(f"Error: {e}")
)");

    // Helper function to check if a registry key exists
    m.def(
        "key_exists",
        [](intptr_t hRootKey, const std::string& subKey) {
            HKEY hKey;
            LONG result = RegOpenKeyExA(reinterpret_cast<HKEY>(hRootKey),
                                        subKey.c_str(), 0, KEY_READ, &hKey);

            if (result == ERROR_SUCCESS) {
                RegCloseKey(hKey);
                return true;
            }

            return false;
        },
        py::arg("hRootKey"), py::arg("subKey"),
        R"(Check if a registry key exists.

Args:
    hRootKey: Root key handle (use predefined constants like HKEY_LOCAL_MACHINE).
    subKey: The subkey path to check, with components separated by backslashes.

Returns:
    True if the key exists, False otherwise.

Examples:
    >>> from atom.system import wregistry
    >>> # Check if a registry key exists
    >>> if wregistry.key_exists(
    ...     wregistry.HKEY_CURRENT_USER,
    ...     "Software\\Microsoft\\Windows"
    ... ):
    ...     print("Registry key exists")
    ... else:
    ...     print("Registry key does not exist")
)");

    // Helper function to check if a registry value exists
    m.def(
        "value_exists",
        [](intptr_t hRootKey, const std::string& subKey,
           const std::string& valueName) {
            HKEY hKey;
            LONG result = RegOpenKeyExA(reinterpret_cast<HKEY>(hRootKey),
                                        subKey.c_str(), 0, KEY_READ, &hKey);

            if (result != ERROR_SUCCESS) {
                return false;
            }

            DWORD type;
            DWORD dataSize = 0;
            result = RegQueryValueExA(hKey, valueName.c_str(), nullptr, &type,
                                      nullptr, &dataSize);

            RegCloseKey(hKey);
            return (result == ERROR_SUCCESS);
        },
        py::arg("hRootKey"), py::arg("subKey"), py::arg("valueName"),
        R"(Check if a registry value exists.

Args:
    hRootKey: Root key handle (use predefined constants like HKEY_LOCAL_MACHINE).
    subKey: The subkey path, with components separated by backslashes.
    valueName: The name of the value to check.

Returns:
    True if the value exists, False otherwise.

Examples:
    >>> from atom.system import wregistry
    >>> # Check if a registry value exists
    >>> if wregistry.value_exists(
    ...     wregistry.HKEY_CURRENT_USER,
    ...     "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
    ...     "OneDrive"
    ... ):
    ...     print("Registry value exists")
    ... else:
    ...     print("Registry value does not exist")
)");

    // Helper function to create a registry key
    m.def(
        "create_key",
        [](intptr_t hRootKey, const std::string& subKey) {
            HKEY hKey;
            DWORD disposition;
            LONG result = RegCreateKeyExA(reinterpret_cast<HKEY>(hRootKey),
                                          subKey.c_str(), 0, nullptr,
                                          REG_OPTION_NON_VOLATILE, KEY_WRITE,
                                          nullptr, &hKey, &disposition);

            if (result != ERROR_SUCCESS) {
                throw py::value_error("Failed to create registry key: " +
                                      subKey);
            }

            RegCloseKey(hKey);
            return disposition == REG_CREATED_NEW_KEY;
        },
        py::arg("hRootKey"), py::arg("subKey"),
        R"(Create a new registry key.

Args:
    hRootKey: Root key handle (use predefined constants like HKEY_LOCAL_MACHINE).
    subKey: The subkey path to create, with components separated by backslashes.

Returns:
    True if a new key was created, False if the key already existed.

Raises:
    ValueError: If the operation fails.

Examples:
    >>> from atom.system import wregistry
    >>> # Create a new registry key (requires appropriate permissions)
    >>> try:
    ...     is_new = wregistry.create_key(
    ...         wregistry.HKEY_CURRENT_USER,
    ...         "Software\\MyApp\\Settings"
    ...     )
    ...     if is_new:
    ...         print("New registry key created")
    ...     else:
    ...         print("Registry key already existed")
    ... except ValueError as e:
    ...     print(f"Error: {e}")
)");

    // Helper function to set a string registry value
    m.def(
        "set_string_value",
        [](intptr_t hRootKey, const std::string& subKey,
           const std::string& valueName, const std::string& data) {
            HKEY hKey;
            LONG result = RegOpenKeyExA(reinterpret_cast<HKEY>(hRootKey),
                                        subKey.c_str(), 0, KEY_WRITE, &hKey);

            if (result != ERROR_SUCCESS) {
                throw py::value_error("Failed to open registry key: " + subKey);
            }

            result =
                RegSetValueExA(hKey, valueName.c_str(), 0, REG_SZ,
                               reinterpret_cast<const BYTE*>(data.c_str()),
                               static_cast<DWORD>(data.length() +
                                                  1)  // Include null terminator
                );

            RegCloseKey(hKey);

            if (result != ERROR_SUCCESS) {
                throw py::value_error("Failed to set registry value: " +
                                      subKey + "\\" + valueName);
            }

            return true;
        },
        py::arg("hRootKey"), py::arg("subKey"), py::arg("valueName"),
        py::arg("data"),
        R"(Set a string (REG_SZ) registry value.

Args:
    hRootKey: Root key handle (use predefined constants like HKEY_LOCAL_MACHINE).
    subKey: The subkey path, with components separated by backslashes.
    valueName: The name of the value to set.
    data: The string data to set.

Returns:
    True if the operation was successful.

Raises:
    ValueError: If the operation fails.

Examples:
    >>> from atom.system import wregistry
    >>> # Set a string registry value (requires appropriate permissions)
    >>> try:
    ...     wregistry.set_string_value(
    ...         wregistry.HKEY_CURRENT_USER,
    ...         "Software\\MyApp\\Settings",
    ...         "InstallPath",
    ...         "C:\\Program Files\\MyApp"
    ...     )
    ...     print("Registry value set successfully")
    ... except ValueError as e:
    ...     print(f"Error: {e}")
)");

    // Helper function to set a DWORD registry value
    m.def(
        "set_dword_value",
        [](intptr_t hRootKey, const std::string& subKey,
           const std::string& valueName, DWORD data) {
            HKEY hKey;
            LONG result = RegOpenKeyExA(reinterpret_cast<HKEY>(hRootKey),
                                        subKey.c_str(), 0, KEY_WRITE, &hKey);

            if (result != ERROR_SUCCESS) {
                throw py::value_error("Failed to open registry key: " + subKey);
            }

            result = RegSetValueExA(hKey, valueName.c_str(), 0, REG_DWORD,
                                    reinterpret_cast<const BYTE*>(&data),
                                    sizeof(DWORD));

            RegCloseKey(hKey);

            if (result != ERROR_SUCCESS) {
                throw py::value_error("Failed to set registry value: " +
                                      subKey + "\\" + valueName);
            }

            return true;
        },
        py::arg("hRootKey"), py::arg("subKey"), py::arg("valueName"),
        py::arg("data"),
        R"(Set a DWORD (REG_DWORD) registry value.

Args:
    hRootKey: Root key handle (use predefined constants like HKEY_LOCAL_MACHINE).
    subKey: The subkey path, with components separated by backslashes.
    valueName: The name of the value to set.
    data: The integer data to set (32-bit).

Returns:
    True if the operation was successful.

Raises:
    ValueError: If the operation fails.

Examples:
    >>> from atom.system import wregistry
    >>> # Set a DWORD registry value (requires appropriate permissions)
    >>> try:
    ...     wregistry.set_dword_value(
    ...         wregistry.HKEY_CURRENT_USER,
    ...         "Software\\MyApp\\Settings",
    ...         "MaxConnections",
    ...         10
    ...     )
    ...     print("Registry value set successfully")
    ... except ValueError as e:
    ...     print(f"Error: {e}")
)");

    // Helper function to get a string registry value
    m.def(
        "get_string_value",
        [](intptr_t hRootKey, const std::string& subKey,
           const std::string& valueName) {
            HKEY hKey;
            LONG result = RegOpenKeyExA(reinterpret_cast<HKEY>(hRootKey),
                                        subKey.c_str(), 0, KEY_READ, &hKey);

            if (result != ERROR_SUCCESS) {
                throw py::value_error("Failed to open registry key: " + subKey);
            }

            DWORD type;
            DWORD dataSize = 0;

            // Get required buffer size
            result = RegQueryValueExA(hKey, valueName.c_str(), nullptr, &type,
                                      nullptr, &dataSize);

            if (result != ERROR_SUCCESS) {
                RegCloseKey(hKey);
                throw py::value_error("Failed to query registry value: " +
                                      subKey + "\\" + valueName);
            }

            if (type != REG_SZ && type != REG_EXPAND_SZ) {
                RegCloseKey(hKey);
                throw py::type_error("Registry value is not a string: " +
                                     subKey + "\\" + valueName);
            }

            std::vector<char> buffer(dataSize);

            // Get actual data
            result = RegQueryValueExA(hKey, valueName.c_str(), nullptr, &type,
                                      reinterpret_cast<BYTE*>(buffer.data()),
                                      &dataSize);

            RegCloseKey(hKey);

            if (result != ERROR_SUCCESS) {
                throw py::value_error("Failed to read registry value: " +
                                      subKey + "\\" + valueName);
            }

            return std::string(buffer.data());
        },
        py::arg("hRootKey"), py::arg("subKey"), py::arg("valueName"),
        R"(Get a string (REG_SZ or REG_EXPAND_SZ) registry value.

Args:
    hRootKey: Root key handle (use predefined constants like HKEY_LOCAL_MACHINE).
    subKey: The subkey path, with components separated by backslashes.
    valueName: The name of the value to get.

Returns:
    The string value.

Raises:
    ValueError: If the registry key or value cannot be accessed.
    TypeError: If the registry value is not a string type.

Examples:
    >>> from atom.system import wregistry
    >>> # Get a string registry value
    >>> try:
    ...     install_path = wregistry.get_string_value(
    ...         wregistry.HKEY_LOCAL_MACHINE,
    ...         "SOFTWARE\\Microsoft\\Windows\\CurrentVersion",
    ...         "ProgramFilesDir"
    ...     )
    ...     print(f"Program Files directory: {install_path}")
    ... except (ValueError, TypeError) as e:
    ...     print(f"Error: {e}")
)");

    // Helper function to get a DWORD registry value
    m.def(
        "get_dword_value",
        [](intptr_t hRootKey, const std::string& subKey,
           const std::string& valueName) {
            HKEY hKey;
            LONG result = RegOpenKeyExA(reinterpret_cast<HKEY>(hRootKey),
                                        subKey.c_str(), 0, KEY_READ, &hKey);

            if (result != ERROR_SUCCESS) {
                throw py::value_error("Failed to open registry key: " + subKey);
            }

            DWORD type;
            DWORD data;
            DWORD dataSize = sizeof(data);

            result =
                RegQueryValueExA(hKey, valueName.c_str(), nullptr, &type,
                                 reinterpret_cast<BYTE*>(&data), &dataSize);

            RegCloseKey(hKey);

            if (result != ERROR_SUCCESS) {
                throw py::value_error("Failed to query registry value: " +
                                      subKey + "\\" + valueName);
            }

            if (type != REG_DWORD) {
                throw py::type_error("Registry value is not a DWORD: " +
                                     subKey + "\\" + valueName);
            }

            return static_cast<uint32_t>(data);
        },
        py::arg("hRootKey"), py::arg("subKey"), py::arg("valueName"),
        R"(Get a DWORD (REG_DWORD) registry value.

Args:
    hRootKey: Root key handle (use predefined constants like HKEY_LOCAL_MACHINE).
    subKey: The subkey path, with components separated by backslashes.
    valueName: The name of the value to get.

Returns:
    The integer value.

Raises:
    ValueError: If the registry key or value cannot be accessed.
    TypeError: If the registry value is not a DWORD type.

Examples:
    >>> from atom.system import wregistry
    >>> # Get a DWORD registry value
    >>> try:
    ...     max_conn = wregistry.get_dword_value(
    ...         wregistry.HKEY_CURRENT_USER,
    ...         "Software\\MyApp\\Settings",
    ...         "MaxConnections"
    ...     )
    ...     print(f"Maximum connections: {max_conn}")
    ... except (ValueError, TypeError) as e:
    ...     print(f"Error: {e}")
)");

    // Helper function to get registry value type
    m.def(
        "get_value_type",
        [](intptr_t hRootKey, const std::string& subKey,
           const std::string& valueName)
            -> std::string {  // Explicitly specify return type
            HKEY hKey;
            LONG result = RegOpenKeyExA(reinterpret_cast<HKEY>(hRootKey),
                                        subKey.c_str(), 0, KEY_READ, &hKey);

            if (result != ERROR_SUCCESS) {
                throw py::value_error("Failed to open registry key: " + subKey);
            }

            DWORD type;
            DWORD dataSize = 0;

            result = RegQueryValueExA(hKey, valueName.c_str(), nullptr, &type,
                                      nullptr, &dataSize);

            RegCloseKey(hKey);

            if (result != ERROR_SUCCESS) {
                throw py::value_error("Failed to query registry value: " +
                                      subKey + "\\" + valueName);
            }

            // Return type as string for better user experience
            switch (type) {
                case REG_SZ:
                    return "REG_SZ";
                case REG_EXPAND_SZ:
                    return "REG_EXPAND_SZ";
                case REG_BINARY:
                    return "REG_BINARY";
                case REG_DWORD:
                    return "REG_DWORD";
                case REG_QWORD:
                    return "REG_QWORD";
                case REG_MULTI_SZ:
                    return "REG_MULTI_SZ";
                default:
                    return "Unknown type (" + std::to_string(type) + ")";
            }
        },
        py::arg("hRootKey"), py::arg("subKey"), py::arg("valueName"),
        R"(Get the type of a registry value.

Args:
    hRootKey: Root key handle (use predefined constants like HKEY_LOCAL_MACHINE).
    subKey: The subkey path, with components separated by backslashes.
    valueName: The name of the value to check.

Returns:
    String representation of the registry value type (e.g., "REG_SZ", "REG_DWORD").

Raises:
    ValueError: If the registry key or value cannot be accessed.

Examples:
    >>> from atom.system import wregistry
    >>> # Get the type of a registry value
    >>> try:
    ...     value_type = wregistry.get_value_type(
    ...         wregistry.HKEY_LOCAL_MACHINE,
    ...         "SOFTWARE\\Microsoft\\Windows\\CurrentVersion",
    ...         "ProgramFilesDir"
    ...     )
    ...     print(f"Registry value type: {value_type}")
    ... except ValueError as e:
    ...     print(f"Error: {e}")
)");
}