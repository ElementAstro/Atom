#include "atom/secret/password.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(password, m) {
    m.doc() = "Password management module for the atom package";

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

    // PasswordStrength enum
    py::enum_<atom::secret::PasswordStrength>(m, "PasswordStrength",
                                              R"(Password strength levels.

Represents different levels of password security strength.)")
        .value("VERY_WEAK", atom::secret::PasswordStrength::VeryWeak)
        .value("WEAK", atom::secret::PasswordStrength::Weak)
        .value("MEDIUM", atom::secret::PasswordStrength::Medium)
        .value("STRONG", atom::secret::PasswordStrength::Strong)
        .value("VERY_STRONG", atom::secret::PasswordStrength::VeryStrong)
        .export_values();

    // PasswordCategory enum
    py::enum_<atom::secret::PasswordCategory>(
        m, "PasswordCategory",
        R"(Categories for organizing passwords.

Helps organize passwords by their intended use.)")
        .value("GENERAL", atom::secret::PasswordCategory::General)
        .value("FINANCE", atom::secret::PasswordCategory::Finance)
        .value("WORK", atom::secret::PasswordCategory::Work)
        .value("PERSONAL", atom::secret::PasswordCategory::Personal)
        .value("SOCIAL", atom::secret::PasswordCategory::Social)
        .value("ENTERTAINMENT", atom::secret::PasswordCategory::Entertainment)
        .value("OTHER", atom::secret::PasswordCategory::Other)
        .export_values();

    // EncryptionOptions struct
    py::class_<atom::secret::EncryptionOptions>(
        m, "EncryptionOptions",
        R"(Configuration options for encryption.

Controls how passwords are encrypted and stored.)")
        .def(py::init<>())
        .def_readwrite(
            "use_hardware_acceleration",
            &atom::secret::EncryptionOptions::useHardwareAcceleration,
            "Whether to use hardware acceleration for encryption operations")
        .def_readwrite("key_iterations",
                       &atom::secret::EncryptionOptions::keyIterations,
                       "Number of iterations for PBKDF2 key derivation")
        .def_readwrite(
            "encryption_method",
            &atom::secret::EncryptionOptions::encryptionMethod,
            "Encryption method (0=AES-GCM, 1=AES-CBC, 2=ChaCha20-Poly1305)");

    // PasswordManagerSettings struct
    py::class_<atom::secret::PasswordManagerSettings>(
        m, "PasswordManagerSettings",
        R"(Settings for the password manager behavior.

Controls automatic locking, password requirements, and more.)")
        .def(py::init<>())
        .def_readwrite(
            "auto_lock_timeout_seconds",
            &atom::secret::PasswordManagerSettings::autoLockTimeoutSeconds,
            "Time in seconds before automatically locking")
        .def_readwrite(
            "notify_on_password_expiry",
            &atom::secret::PasswordManagerSettings::notifyOnPasswordExpiry,
            "Whether to notify when passwords expire")
        .def_readwrite(
            "password_expiry_days",
            &atom::secret::PasswordManagerSettings::passwordExpiryDays,
            "Number of days after which passwords are considered expired")
        .def_readwrite(
            "min_password_length",
            &atom::secret::PasswordManagerSettings::minPasswordLength,
            "Minimum required password length")
        .def_readwrite(
            "require_special_chars",
            &atom::secret::PasswordManagerSettings::requireSpecialChars,
            "Whether passwords must contain special characters")
        .def_readwrite("require_numbers",
                       &atom::secret::PasswordManagerSettings::requireNumbers,
                       "Whether passwords must contain numbers")
        .def_readwrite("require_mixed_case",
                       &atom::secret::PasswordManagerSettings::requireMixedCase,
                       "Whether passwords must contain mixed case letters")
        .def_readwrite(
            "encryption_options",
            &atom::secret::PasswordManagerSettings::encryptionOptions,
            "Encryption configuration options");

    // PasswordEntry struct
    py::class_<atom::secret::PasswordEntry>(
        m, "PasswordEntry",
        R"(Structure containing password and related information.

Stores a password along with associated metadata.)")
        .def(py::init<>())
        .def_readwrite("password", &atom::secret::PasswordEntry::password,
                       "The stored password")
        .def_readwrite("username", &atom::secret::PasswordEntry::username,
                       "Associated username")
        .def_readwrite("url", &atom::secret::PasswordEntry::url,
                       "Associated URL")
        .def_readwrite("notes", &atom::secret::PasswordEntry::notes,
                       "Additional notes")
        .def_readwrite("category", &atom::secret::PasswordEntry::category,
                       "Password category")
        .def_readwrite("created", &atom::secret::PasswordEntry::created,
                       "Creation timestamp")
        .def_readwrite("modified", &atom::secret::PasswordEntry::modified,
                       "Last modification timestamp")
        .def_readwrite("previous_passwords",
                       &atom::secret::PasswordEntry::previousPasswords,
                       "History of previously used passwords");

    // PasswordManager class
    py::class_<atom::secret::PasswordManager>(
        m, "PasswordManager",
        R"(Secure password management class.

Provides secure storage, retrieval, and management of passwords using platform-specific
credential storage mechanisms.

Examples:
    >>> from atom.secret import PasswordManager
    >>> manager = PasswordManager()
    >>> manager.initialize("master_password")
    True
    >>> manager.unlock("master_password")
    True
    >>> entry = manager.retrieve_password("example_key")
)")
        .def(py::init<>(), "Constructs a new PasswordManager object.")
        .def("initialize", &atom::secret::PasswordManager::initialize,
             py::arg("master_password"),
             py::arg("settings") = atom::secret::PasswordManagerSettings(),
             R"(Initialize the password manager with a master password.

Args:
    master_password: Master password for deriving encryption keys
    settings: Optional settings for the password manager

Returns:
    True if initialization was successful
)")
        .def("unlock", &atom::secret::PasswordManager::unlock,
             py::arg("master_password"),
             R"(Unlock the password manager.

Args:
    master_password: Master password for authentication

Returns:
    True if unlocked successfully
)")
        .def("lock", &atom::secret::PasswordManager::lock,
             "Lock the password manager and clear sensitive data from memory.")
        .def("change_master_password",
             &atom::secret::PasswordManager::changeMasterPassword,
             py::arg("current_password"), py::arg("new_password"),
             R"(Change the master password.

Args:
    current_password: Current master password
    new_password: New master password to set

Returns:
    True if the master password was changed successfully
)")
        .def("load_all_passwords",
             &atom::secret::PasswordManager::loadAllPasswords,
             "Load all passwords into memory (must be unlocked).")
        .def("store_password", &atom::secret::PasswordManager::storePassword,
             py::arg("platform_key"), py::arg("entry"),
             R"(Store a password entry.

Args:
    platform_key: Key to identify the stored password
    entry: PasswordEntry object containing the password and related information

Returns:
    True if stored successfully
)")
        .def("retrieve_password",
             &atom::secret::PasswordManager::retrievePassword,
             py::arg("platform_key"),
             R"(Retrieve a password entry.

Args:
    platform_key: Key that identifies the stored password

Returns:
    PasswordEntry object with the retrieved information
)")
        .def("delete_password", &atom::secret::PasswordManager::deletePassword,
             py::arg("platform_key"),
             R"(Delete a password.

Args:
    platform_key: Key that identifies the stored password

Returns:
    True if deleted successfully
)")
        .def("get_all_platform_keys",
             &atom::secret::PasswordManager::getAllPlatformKeys,
             R"(Get a list of all platform keys.

Returns:
    List of all platform keys stored in the password manager
)")
        .def("search_passwords",
             &atom::secret::PasswordManager::searchPasswords, py::arg("query"),
             R"(Search for password entries.

Args:
    query: Search keyword

Returns:
    List of platform keys matching the search query
)")
        .def("filter_by_category",
             &atom::secret::PasswordManager::filterByCategory,
             py::arg("category"),
             R"(Filter passwords by category.

Args:
    category: Category to filter by

Returns:
    List of platform keys belonging to the specified category
)")
        .def("generate_password",
             &atom::secret::PasswordManager::generatePassword,
             py::arg("length") = 16, py::arg("include_special") = true,
             py::arg("include_numbers") = true,
             py::arg("include_mixed_case") = true,
             R"(Generate a strong password.

Args:
    length: Length of the generated password
    include_special: Whether to include special characters
    include_numbers: Whether to include numbers
    include_mixed_case: Whether to include mixed case letters

Returns:
    Generated password string
)")
        .def("evaluate_password_strength",
             &atom::secret::PasswordManager::evaluatePasswordStrength,
             py::arg("password"),
             R"(Evaluate password strength.

Args:
    password: Password to evaluate

Returns:
    PasswordStrength enum value indicating strength level
)")
        .def("export_passwords",
             &atom::secret::PasswordManager::exportPasswords,
             py::arg("file_path"), py::arg("password"),
             R"(Export all password data (encrypted).

Args:
    file_path: Path to export file
    password: Additional encryption password

Returns:
    True if export was successful
)")
        .def("import_passwords",
             &atom::secret::PasswordManager::importPasswords,
             py::arg("file_path"), py::arg("password"),
             R"(Import password data from backup file.

Args:
    file_path: Path to backup file
    password: Decryption password

Returns:
    True if import was successful
)")
        .def("update_settings", &atom::secret::PasswordManager::updateSettings,
             py::arg("new_settings"),
             R"(Update password manager settings.

Args:
    new_settings: New settings object
)")
        .def("get_settings", &atom::secret::PasswordManager::getSettings,
             R"(Get current settings.

Returns:
    Current PasswordManagerSettings object
)")
        .def("check_expired_passwords",
             &atom::secret::PasswordManager::checkExpiredPasswords,
             R"(Check for expired passwords.

Returns:
    List of platform keys with expired passwords
)")
        .def("set_activity_callback",
             &atom::secret::PasswordManager::setActivityCallback,
             py::arg("callback"),
             R"(Set callback for activity updates.

Args:
    callback: Function to call when activity occurs
)");
}