#include "atom/secret/common.hpp"

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void bind_common(py::module& m) {
    // PasswordStrength enum
    py::enum_<atom::secret::PasswordStrength>(m, "PasswordStrength",
                                              R"pbdoc(
        Password strength levels.

        Represents the overall strength of a password based on various
        characteristics including length, character diversity, and patterns.

        Values:
            VeryWeak: Very weak password, easily crackable
            Weak: Weak password, vulnerable to attacks
            Medium: Medium strength password, acceptable for low-security uses
            Strong: Strong password, suitable for most applications
            VeryStrong: Very strong password, highly secure
        )pbdoc")
        .value("VeryWeak", atom::secret::PasswordStrength::VeryWeak,
               "Very weak password")
        .value("Weak", atom::secret::PasswordStrength::Weak, "Weak password")
        .value("Medium", atom::secret::PasswordStrength::Medium,
               "Medium strength password")
        .value("Strong", atom::secret::PasswordStrength::Strong,
               "Strong password")
        .value("VeryStrong", atom::secret::PasswordStrength::VeryStrong,
               "Very strong password")
        .export_values();

    // PasswordCategory enum
    py::enum_<atom::secret::PasswordCategory>(m, "PasswordCategory",
                                              R"pbdoc(
        Password categories for organization.

        Used to categorize and organize password entries for easier
        management and filtering.

        Values:
            General: General purpose passwords
            Finance: Financial and banking passwords
            Work: Work-related passwords
            Personal: Personal account passwords
            Social: Social media passwords
            Entertainment: Entertainment service passwords
            Other: Other uncategorized passwords
        )pbdoc")
        .value("General", atom::secret::PasswordCategory::General,
               "General purpose")
        .value("Finance", atom::secret::PasswordCategory::Finance,
               "Financial accounts")
        .value("Work", atom::secret::PasswordCategory::Work, "Work-related")
        .value("Personal", atom::secret::PasswordCategory::Personal,
               "Personal accounts")
        .value("Social", atom::secret::PasswordCategory::Social, "Social media")
        .value("Entertainment", atom::secret::PasswordCategory::Entertainment,
               "Entertainment services")
        .value("Other", atom::secret::PasswordCategory::Other, "Other")
        .export_values();

    // EncryptionMethod enum (nested in EncryptionOptions)
    py::enum_<atom::secret::EncryptionOptions::Method>(m, "EncryptionMethod",
                                                       R"pbdoc(
        Encryption method enumeration.

        Specifies the encryption algorithm to use for data protection.

        Values:
            AES_GCM: AES-GCM (default, AEAD encryption with authentication)
            AES_CBC: AES-CBC (traditional block cipher mode)
            CHACHA20_POLY1305: ChaCha20-Poly1305 (modern AEAD cipher)
        )pbdoc")
        .value("AES_GCM", atom::secret::EncryptionOptions::Method::AES_GCM,
               "AES-GCM (Authenticated Encryption with Associated Data)")
        .value("AES_CBC", atom::secret::EncryptionOptions::Method::AES_CBC,
               "AES-CBC (Cipher Block Chaining)")
        .value("CHACHA20_POLY1305",
               atom::secret::EncryptionOptions::Method::CHACHA20_POLY1305,
               "ChaCha20-Poly1305 (Modern AEAD cipher)")
        .export_values();

    // EncryptionOptions struct
    py::class_<atom::secret::EncryptionOptions>(m, "EncryptionOptions",
                                                R"pbdoc(
        Structure for encryption options.

        Configures encryption parameters including hardware acceleration,
        key derivation iterations, and encryption method.

        Attributes:
            use_hardware_acceleration (bool): Whether to use hardware acceleration
            key_iterations (int): PBKDF2 iteration count (default: 100000)
            encryption_method (EncryptionMethod): The encryption method to use
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def_readwrite(
            "use_hardware_acceleration",
            &atom::secret::EncryptionOptions::useHardwareAcceleration,
            "Whether to use hardware acceleration")
        .def_readwrite("key_iterations",
                       &atom::secret::EncryptionOptions::keyIterations,
                       "PBKDF2 iteration count")
        .def_readwrite("encryption_method",
                       &atom::secret::EncryptionOptions::encryptionMethod,
                       "The encryption method to use");

    // PasswordManagerSettings struct
    py::class_<atom::secret::PasswordManagerSettings>(m,
                                                      "PasswordManagerSettings",
                                                      R"pbdoc(
        Settings for the Password Manager.

        Configures various aspects of password manager behavior including
        auto-lock timeout, password requirements, and encryption options.

        Attributes:
            auto_lock_timeout_seconds (int): Auto-lock timeout in seconds
            notify_on_password_expiry (bool): Enable password expiry notifications
            password_expiry_days (int): Password validity period in days
            min_password_length (int): Minimum password length requirement
            require_special_chars (bool): Require special characters in passwords
            require_numbers (bool): Require numbers in passwords
            require_mixed_case (bool): Require mixed case letters in passwords
            encryption_options (EncryptionOptions): Encryption options
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def_readwrite(
            "auto_lock_timeout_seconds",
            &atom::secret::PasswordManagerSettings::autoLockTimeoutSeconds,
            "Auto-lock timeout in seconds (default: 300)")
        .def_readwrite(
            "notify_on_password_expiry",
            &atom::secret::PasswordManagerSettings::notifyOnPasswordExpiry,
            "Enable password expiry notifications (default: true)")
        .def_readwrite(
            "password_expiry_days",
            &atom::secret::PasswordManagerSettings::passwordExpiryDays,
            "Password validity period in days (default: 90)")
        .def_readwrite(
            "min_password_length",
            &atom::secret::PasswordManagerSettings::minPasswordLength,
            "Minimum password length requirement (default: 12)")
        .def_readwrite(
            "require_special_chars",
            &atom::secret::PasswordManagerSettings::requireSpecialChars,
            "Require special characters in passwords (default: true)")
        .def_readwrite("require_numbers",
                       &atom::secret::PasswordManagerSettings::requireNumbers,
                       "Require numbers in passwords (default: true)")
        .def_readwrite(
            "require_mixed_case",
            &atom::secret::PasswordManagerSettings::requireMixedCase,
            "Require mixed case letters in passwords (default: true)")
        .def_readwrite(
            "encryption_options",
            &atom::secret::PasswordManagerSettings::encryptionOptions,
            "Encryption options");

    // PreviousPassword struct
    py::class_<atom::secret::PreviousPassword>(m, "PreviousPassword",
                                               R"pbdoc(
        Structure representing a previous password entry with change timestamp.

        Attributes:
            password (str): The previous password value
            changed (datetime): When the password was changed
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def_readwrite("password", &atom::secret::PreviousPassword::password,
                       "The previous password value")
        .def_readwrite("changed", &atom::secret::PreviousPassword::changed,
                       "When the password was changed");
}
