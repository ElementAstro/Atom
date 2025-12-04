#include "atom/secret/password_manager.hpp"

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void bind_password_manager(py::module& m) {
    // PasswordManager::Statistics struct
    py::class_<atom::secret::PasswordManager::Statistics>(m, "Statistics",
                                                          R"pbdoc(
        Statistics about stored passwords.

        Attributes:
            total_entries (int): Total number of password entries
            expired_entries (int): Number of expired entries
            weak_passwords (int): Number of weak passwords
            duplicate_passwords (int): Number of duplicate passwords
            last_modified (datetime): Last modification timestamp
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("total_entries",
                       &atom::secret::PasswordManager::Statistics::totalEntries,
                       "Total number of password entries")
        .def_readwrite(
            "expired_entries",
            &atom::secret::PasswordManager::Statistics::expiredEntries,
            "Number of expired entries")
        .def_readwrite(
            "weak_passwords",
            &atom::secret::PasswordManager::Statistics::weakPasswords,
            "Number of weak passwords")
        .def_readwrite(
            "duplicate_passwords",
            &atom::secret::PasswordManager::Statistics::duplicatePasswords,
            "Number of duplicate passwords")
        .def_readwrite("last_modified",
                       &atom::secret::PasswordManager::Statistics::lastModified,
                       "Last modification timestamp");

    // PasswordManager class
    py::class_<atom::secret::PasswordManager>(m, "PasswordManager",
                                              R"pbdoc(
        Main password manager class providing secure password storage and management.

        Provides a complete password management solution with:
        - Secure password storage with encryption
        - Master password protection with PBKDF2 key derivation
        - Cross-platform secure storage backends
        - Password generation and strength analysis
        - Search and filtering capabilities
        - Import/export functionality
        - Auto-lock and session management
        - Password expiration tracking

        Example:
            >>> # Create and initialize a password manager
            >>> manager = PasswordManager()
            >>> settings = PasswordManagerSettings()
            >>> settings.auto_lock_timeout_seconds = 300
            >>> manager.initialize("MyMasterPassword123!", settings)
            >>>
            >>> # Store a password
            >>> entry = PasswordEntry()
            >>> entry.password = "SecurePassword123!"
            >>> entry.username = "user@example.com"
            >>> entry.url = "https://example.com"
            >>> manager.store_password("example.com", entry)
            >>>
            >>> # Retrieve a password
            >>> retrieved = manager.retrieve_password("example.com")
            >>> print(f"Username: {retrieved.username}")
            >>>
            >>> # Search passwords
            >>> results = manager.search_passwords("example")
            >>>
            >>> # Lock when done
            >>> manager.lock()
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def("initialize", &atom::secret::PasswordManager::initialize,
             py::arg("master_password"),
             py::arg("settings") = atom::secret::PasswordManagerSettings{},
             R"pbdoc(
            Initializes the password manager with a master password.

            Args:
                master_password: The master password for encryption.
                settings: Optional password manager settings.

            Returns:
                bool: True if initialization succeeded, False otherwise.
            )pbdoc")
        .def("lock", &atom::secret::PasswordManager::lock,
             R"pbdoc(
            Locks the password manager, clearing sensitive data from memory.
            )pbdoc")
        .def("unlock", &atom::secret::PasswordManager::unlock,
             py::arg("master_password"),
             R"pbdoc(
            Unlocks the password manager with the master password.

            Args:
                master_password: The master password.

            Returns:
                bool: True if unlock succeeded, False otherwise.
            )pbdoc")
        .def("is_locked", &atom::secret::PasswordManager::isLocked,
             R"pbdoc(
            Checks if the password manager is currently locked.

            Returns:
                bool: True if locked, False if unlocked.
            )pbdoc")
        .def("change_master_password",
             &atom::secret::PasswordManager::changeMasterPassword,
             py::arg("current_password"), py::arg("new_password"),
             R"pbdoc(
            Changes the master password.

            Args:
                current_password: Current master password.
                new_password: New master password.

            Returns:
                bool: True if change succeeded, False otherwise.
            )pbdoc")
        .def("store_password", &atom::secret::PasswordManager::storePassword,
             py::arg("key"), py::arg("entry"),
             R"pbdoc(
            Stores a password entry.

            Args:
                key: Unique key for the entry.
                entry: Password entry to store.

            Returns:
                bool: True if storage succeeded, False otherwise.
            )pbdoc")
        .def("retrieve_password",
             &atom::secret::PasswordManager::retrievePassword, py::arg("key"),
             R"pbdoc(
            Retrieves a password entry.

            Args:
                key: Key of the entry to retrieve.

            Returns:
                PasswordEntry: Retrieved password entry or empty entry if not found.
            )pbdoc")
        .def("remove_password", &atom::secret::PasswordManager::removePassword,
             py::arg("key"),
             R"pbdoc(
            Removes a password entry.

            Args:
                key: Key of the entry to remove.

            Returns:
                bool: True if removal succeeded, False otherwise.
            )pbdoc")
        .def("get_all_keys", &atom::secret::PasswordManager::getAllKeys,
             R"pbdoc(
            Gets all stored password keys.

            Returns:
                list[str]: List of all password keys.
            )pbdoc")
        .def(
            "search_passwords", &atom::secret::PasswordManager::searchPasswords,
            py::arg("query"), py::arg("search_in_title") = true,
            py::arg("search_in_username") = true,
            py::arg("search_in_url") = true, py::arg("search_in_notes") = false,
            py::arg("search_in_tags") = true,
            R"pbdoc(
            Searches for password entries by various criteria.

            Args:
                query: Search query.
                search_in_title: Search in entry titles (default: True).
                search_in_username: Search in usernames (default: True).
                search_in_url: Search in URLs (default: True).
                search_in_notes: Search in notes (default: False).
                search_in_tags: Search in tags (default: True).

            Returns:
                list[tuple[str, PasswordEntry]]: List of matching entries with their keys.
            )pbdoc")
        .def("filter_by_category",
             &atom::secret::PasswordManager::filterByCategory,
             py::arg("category"),
             R"pbdoc(
            Filters password entries by category.

            Args:
                category: Category to filter by.

            Returns:
                list[tuple[str, PasswordEntry]]: List of matching entries with their keys.
            )pbdoc")
        .def("get_expiring_passwords",
             &atom::secret::PasswordManager::getExpiringPasswords,
             py::arg("days_ahead") = 30,
             R"pbdoc(
            Gets password entries that are expiring soon.

            Args:
                days_ahead: Number of days ahead to check (default: 30).

            Returns:
                list[tuple[str, PasswordEntry]]: List of expiring entries with their keys.
            )pbdoc")
        .def("generate_password",
             &atom::secret::PasswordManager::generatePassword,
             py::arg("length") = 0, py::arg("include_uppercase") = true,
             py::arg("include_numbers") = true,
             py::arg("include_special") = true,
             R"pbdoc(
            Generates a secure password.

            Args:
                length: Password length (0 uses settings default).
                include_uppercase: Include uppercase letters (default: True).
                include_numbers: Include numbers (default: True).
                include_special: Include special characters (default: True).

            Returns:
                str: Generated password or empty string on failure.
            )pbdoc")
        .def("analyze_password",
             &atom::secret::PasswordManager::analyzePassword,
             py::arg("password"),
             R"pbdoc(
            Analyzes password strength.

            Args:
                password: Password to analyze.

            Returns:
                AnalysisResult: Password analysis results.
            )pbdoc")
        .def("export_to_json", &atom::secret::PasswordManager::exportToJson,
             R"pbdoc(
            Exports all password entries to JSON.

            Returns:
                Result[str]: JSON string or error message.
            )pbdoc")
        .def("import_from_json", &atom::secret::PasswordManager::importFromJson,
             py::arg("json"), py::arg("overwrite_existing") = false,
             R"pbdoc(
            Imports password entries from JSON.

            Args:
                json: JSON string containing password entries.
                overwrite_existing: Whether to overwrite existing entries (default: False).

            Returns:
                Result[int]: Number of imported entries or error message.
            )pbdoc")
        .def("get_settings", &atom::secret::PasswordManager::getSettings,
             R"pbdoc(
            Gets the current password manager settings.

            Returns:
                PasswordManagerSettings: Current settings.
            )pbdoc")
        .def("update_settings", &atom::secret::PasswordManager::updateSettings,
             py::arg("settings"),
             R"pbdoc(
            Updates password manager settings.

            Args:
                settings: New settings to apply.

            Returns:
                bool: True if update succeeded, False otherwise.
            )pbdoc")
        .def("get_statistics", &atom::secret::PasswordManager::getStatistics,
             R"pbdoc(
            Gets statistics about stored passwords.

            Returns:
                Statistics: Statistics structure.
            )pbdoc");
}
