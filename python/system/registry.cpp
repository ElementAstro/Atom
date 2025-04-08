#include "atom/system/lregistry.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(registry, m) {
    m.doc() = "Registry management module for the atom package";

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

    // Define RegistryFormat enum
    py::enum_<atom::system::RegistryFormat>(m, "Format",
                                            "Registry data format types")
        .value("TEXT", atom::system::RegistryFormat::TEXT, "Plain text format")
        .value("JSON", atom::system::RegistryFormat::JSON, "JSON format")
        .value("XML", atom::system::RegistryFormat::XML, "XML format")
        .value("BINARY", atom::system::RegistryFormat::BINARY, "Binary format")
        .export_values();

    // Define RegistryResult enum
    py::enum_<atom::system::RegistryResult>(m, "Result",
                                            "Registry operation result codes")
        .value("SUCCESS", atom::system::RegistryResult::SUCCESS,
               "Operation successful")
        .value("KEY_NOT_FOUND", atom::system::RegistryResult::KEY_NOT_FOUND,
               "Key not found")
        .value("VALUE_NOT_FOUND", atom::system::RegistryResult::VALUE_NOT_FOUND,
               "Value not found")
        .value("FILE_ERROR", atom::system::RegistryResult::FILE_ERROR,
               "File error")
        .value("PERMISSION_DENIED",
               atom::system::RegistryResult::PERMISSION_DENIED,
               "Permission denied")
        .value("INVALID_FORMAT", atom::system::RegistryResult::INVALID_FORMAT,
               "Invalid format")
        .value("ENCRYPTION_ERROR",
               atom::system::RegistryResult::ENCRYPTION_ERROR,
               "Encryption error")
        .value("ALREADY_EXISTS", atom::system::RegistryResult::ALREADY_EXISTS,
               "Already exists")
        .value("UNKNOWN_ERROR", atom::system::RegistryResult::UNKNOWN_ERROR,
               "Unknown error")
        .export_values();

    // Define RegistryValueInfo struct
    py::class_<atom::system::RegistryValueInfo>(m, "ValueInfo",
                                                "Registry value metadata")
        .def_readwrite("name", &atom::system::RegistryValueInfo::name,
                       "Value name")
        .def_readwrite("type", &atom::system::RegistryValueInfo::type,
                       "Value type")
        .def_readwrite("last_modified",
                       &atom::system::RegistryValueInfo::lastModified,
                       "Last modified timestamp")
        .def_readwrite("size", &atom::system::RegistryValueInfo::size,
                       "Value size in bytes");

    // Define Registry class
    py::class_<atom::system::Registry>(
        m, "Registry",
        R"(Registry management class for storing and retrieving configuration data.

This class provides methods to create, read, update, and delete values in a registry store.
It supports different storage formats and includes features like transactions, encryption,
and event callbacks.

Examples:
    >>> from atom.system import registry
    >>> reg = registry.Registry()
    >>> reg.initialize("config.reg")
    >>> reg.create_key("app/settings")
    >>> reg.set_value("app/settings", "theme", "dark")
    >>> theme = reg.get_value("app/settings", "theme")
    >>> print(theme)
    dark
)")
        .def(py::init<>(), "Constructs a new Registry object.")

        // Initialization methods
        .def("initialize", &atom::system::Registry::initialize,
             py::arg("file_path"), py::arg("use_encryption") = false,
             R"(Initializes the registry with specified settings.

Args:
    file_path: Path to registry file
    use_encryption: Whether to use encryption (default: False)

Returns:
    Result code indicating success or failure
)")

        .def("load_registry_from_file",
             &atom::system::Registry::loadRegistryFromFile,
             py::arg("file_path") = "",
             py::arg("format") = atom::system::RegistryFormat::TEXT,
             R"(Loads registry data from a file.

Args:
    file_path: Path to the registry file
    format: Format of the registry file (default: TEXT)

Returns:
    Result code indicating success or failure
)")

        // Key operations
        .def("create_key", &atom::system::Registry::createKey,
             py::arg("key_path"),
             R"(Creates a new key in the registry.

Args:
    key_path: The path of the key to create

Returns:
    Result code indicating success or failure
)")
        .def("delete_key", &atom::system::Registry::deleteKey,
             py::arg("key_path"),
             R"(Deletes a key from the registry.

Args:
    key_path: The path of the key to delete

Returns:
    Result code indicating success or failure
)")
        .def("key_exists", &atom::system::Registry::keyExists,
             py::arg("key_path"),
             R"(Checks if a key exists in the registry.

Args:
    key_path: The path of the key to check for existence

Returns:
    True if the key exists, False otherwise
)")
        .def("get_all_keys", &atom::system::Registry::getAllKeys,
             R"(Gets all keys in the registry.

Returns:
    List of key paths
)")
        .def("search_keys", &atom::system::Registry::searchKeys,
             py::arg("pattern"),
             R"(Searches for keys matching a pattern.

Args:
    pattern: Search pattern

Returns:
    List of matching key paths
)")

        // Value operations
        .def("set_value", &atom::system::Registry::setValue,
             py::arg("key_path"), py::arg("value_name"), py::arg("data"),
             R"(Sets a value for a key in the registry.

Args:
    key_path: The path of the key
    value_name: The name of the value to set
    data: The data to set for the value

Returns:
    Result code indicating success or failure
)")
        .def("set_typed_value", &atom::system::Registry::setTypedValue,
             py::arg("key_path"), py::arg("value_name"), py::arg("data"),
             py::arg("type"),
             R"(Sets a value with a specific type for a key in the registry.

Args:
    key_path: The path of the key
    value_name: The name of the value to set
    data: The data to set for the value
    type: The data type

Returns:
    Result code indicating success or failure
)")
        .def(
            "get_value",
            [](atom::system::Registry& self, const std::string& keyPath,
               const std::string& valueName) {
                auto value = self.getValue(keyPath, valueName);
                if (value.has_value()) {
                    return py::cast(value.value());
                }
                return py::none();
            },
            py::arg("key_path"), py::arg("value_name"),
            R"(Gets the value associated with a key and value name from the registry.

Args:
    key_path: The path of the key
    value_name: The name of the value to retrieve

Returns:
    The value if found, None otherwise
)")
        .def(
            "get_typed_value",
            [](atom::system::Registry& self, const std::string& keyPath,
               const std::string& valueName) {
                std::string type;
                auto value = self.getTypedValue(keyPath, valueName, type);
                if (value.has_value()) {
                    return py::make_tuple(value.value(), type);
                }
                return py::make_tuple(py::none(), "");
            },
            py::arg("key_path"), py::arg("value_name"),
            R"(Gets the value and type associated with a key and value name.

Args:
    key_path: The path of the key
    value_name: The name of the value to retrieve

Returns:
    Tuple of (value, type) if found, (None, "") otherwise
)")
        .def("delete_value", &atom::system::Registry::deleteValue,
             py::arg("key_path"), py::arg("value_name"),
             R"(Deletes a value from a key in the registry.

Args:
    key_path: The path of the key
    value_name: The name of the value to delete

Returns:
    Result code indicating success or failure
)")
        .def("value_exists", &atom::system::Registry::valueExists,
             py::arg("key_path"), py::arg("value_name"),
             R"(Checks if a value exists for a key in the registry.

Args:
    key_path: The path of the key
    value_name: The name of the value to check for existence

Returns:
    True if the value exists, False otherwise
)")
        .def("get_value_names", &atom::system::Registry::getValueNames,
             py::arg("key_path"),
             R"(Retrieves all value names for a given key from the registry.

Args:
    key_path: The path of the key

Returns:
    List of value names associated with the given key
)")
        .def(
            "get_value_info",
            [](atom::system::Registry& self, const std::string& keyPath,
               const std::string& valueName) {
                auto info = self.getValueInfo(keyPath, valueName);
                if (info.has_value()) {
                    return py::cast(info.value());
                }
                return py::none();
            },
            py::arg("key_path"), py::arg("value_name"),
            R"(Gets detailed information about a registry value.

Args:
    key_path: The path of the key
    value_name: The name of the value

Returns:
    ValueInfo object if found, None otherwise
)")
        .def("search_values", &atom::system::Registry::searchValues,
             py::arg("value_pattern"),
             R"(Searches for values matching criteria.

Args:
    value_pattern: Pattern to match against value content

Returns:
    List of key-value pairs that match
)")

        // Backup and restore
        .def("backup_registry_data",
             &atom::system::Registry::backupRegistryData,
             py::arg("backup_path") = "",
             R"(Backs up the registry data.

Args:
    backup_path: Path for the backup file (default: auto-generated)

Returns:
    Result code indicating success or failure
)")
        .def("restore_registry_data",
             &atom::system::Registry::restoreRegistryData,
             py::arg("backup_file"),
             R"(Restores the registry data from a backup file.

Args:
    backup_file: The backup file to restore data from

Returns:
    Result code indicating success or failure
)")

        // Import and export
        .def("export_registry", &atom::system::Registry::exportRegistry,
             py::arg("file_path"), py::arg("format"),
             R"(Exports registry data to a specified format.

Args:
    file_path: Export file path
    format: Format to export to

Returns:
    Result code indicating success or failure
)")
        .def("import_registry", &atom::system::Registry::importRegistry,
             py::arg("file_path"), py::arg("format"),
             py::arg("merge_existing") = false,
             R"(Imports registry data from a file.

Args:
    file_path: Import file path
    format: Format to import from
    merge_existing: How to handle conflicts (True: merge, False: replace)

Returns:
    Result code indicating success or failure
)")

        // Transaction control
        .def("begin_transaction", &atom::system::Registry::beginTransaction,
             R"(Begins a transaction for atomic operations.

Returns:
    True if transaction started successfully
)")
        .def("commit_transaction", &atom::system::Registry::commitTransaction,
             R"(Commits the current transaction.

Returns:
    Result code indicating success or failure
)")
        .def("rollback_transaction",
             &atom::system::Registry::rollbackTransaction,
             R"(Rolls back the current transaction.

Returns:
    Result code indicating success or failure
)")

        // Event callbacks
        .def("register_event_callback",
             &atom::system::Registry::registerEventCallback,
             py::arg("callback"),
             R"(Registers a callback for registry events.

Args:
    callback: The function to call on events, with signature fn(key_path, value_name)

Returns:
    Unique ID for the callback registration

Examples:
    >>> def on_registry_change(key, value):
    ...     print(f"Changed: {key} - {value}")
    >>> callback_id = reg.register_event_callback(on_registry_change)
)")
        .def("unregister_event_callback",
             &atom::system::Registry::unregisterEventCallback,
             py::arg("callback_id"),
             R"(Unregisters a previously registered callback.

Args:
    callback_id: The ID returned from register_event_callback

Returns:
    True if successfully unregistered
)")

        // Settings
        .def("set_auto_save", &atom::system::Registry::setAutoSave,
             py::arg("enable"),
             R"(Enables or disables automatic saving.

Args:
    enable: Whether to enable auto-save
)")
        .def("get_last_error", &atom::system::Registry::getLastError,
             R"(Gets the error message for the last failed operation.

Returns:
    Error message string
)");

    // Add helper function for easier result checking
    m.def(
        "is_success",
        [](atom::system::RegistryResult result) {
            return result == atom::system::RegistryResult::SUCCESS;
        },
        py::arg("result"),
        R"(Checks if a registry operation was successful.

Args:
    result: Result code from a registry operation

Returns:
    True if the operation was successful, False otherwise

Examples:
    >>> from atom.system import registry
    >>> reg = registry.Registry()
    >>> result = reg.initialize("config.reg")
    >>> if registry.is_success(result):
    ...     print("Registry initialized successfully")
)");
}