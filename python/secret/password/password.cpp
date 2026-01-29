/*
 * password.cpp
 *
 * Python bindings for password module.
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 * License: GPL3
 */

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/secret/password/breach_checker.hpp"
#include "atom/secret/password/entry.hpp"
#include "atom/secret/password/generator.hpp"
#include "atom/secret/password/validator.hpp"

namespace py = pybind11;
using namespace atom::secret;

void bind_password(py::module& m) {
    // PasswordCategory enum
    py::enum_<PasswordCategory>(m, "PasswordCategory",
                                R"pbdoc(
        Categories for password entries.
        )pbdoc")
        .value("General", PasswordCategory::General)
        .value("Finance", PasswordCategory::Finance)
        .value("Social", PasswordCategory::Social)
        .value("Work", PasswordCategory::Work)
        .value("Email", PasswordCategory::Email)
        .value("Shopping", PasswordCategory::Shopping)
        .value("Development", PasswordCategory::Development)
        .value("Entertainment", PasswordCategory::Entertainment)
        .value("Personal", PasswordCategory::Personal)
        .value("Other", PasswordCategory::Other)
        .export_values();

    // PasswordStrength enum
    py::enum_<PasswordStrength>(m, "PasswordStrength",
                                R"pbdoc(
        Password strength levels.
        )pbdoc")
        .value("VeryWeak", PasswordStrength::VeryWeak)
        .value("Weak", PasswordStrength::Weak)
        .value("Medium", PasswordStrength::Medium)
        .value("Strong", PasswordStrength::Strong)
        .value("VeryStrong", PasswordStrength::VeryStrong)
        .export_values();

    // CustomField struct
    py::class_<CustomField>(m, "CustomField",
                            R"pbdoc(
        Custom field for password entries.
        )pbdoc")
        .def(py::init<>())
        .def(py::init<const std::string&, const std::string&, bool, bool>(),
             py::arg("name"), py::arg("value"), py::arg("is_protected") = false,
             py::arg("is_multiline") = false)
        .def_readwrite("name", &CustomField::name)
        .def_readwrite("value", &CustomField::value)
        .def_readwrite("is_protected", &CustomField::isProtected)
        .def_readwrite("is_multiline", &CustomField::isMultiline);

    // PasswordEntry class
    py::class_<PasswordEntry>(m, "PasswordEntry",
                              R"pbdoc(
        Represents a password entry with metadata.
        )pbdoc")
        .def(py::init<>())
        .def_static("create", &PasswordEntry::create,
                    "Creates a new entry with generated ID")
        .def_readwrite("id", &PasswordEntry::id)
        .def_readwrite("title", &PasswordEntry::title)
        .def_readwrite("username", &PasswordEntry::username)
        .def_readwrite("password", &PasswordEntry::password)
        .def_readwrite("url", &PasswordEntry::url)
        .def_readwrite("email", &PasswordEntry::email)
        .def_readwrite("notes", &PasswordEntry::notes)
        .def_readwrite("category", &PasswordEntry::category)
        .def_readwrite("tags", &PasswordEntry::tags)
        .def_readwrite("is_favorite", &PasswordEntry::isFavorite)
        .def_readwrite("is_archived", &PasswordEntry::isArchived)
        .def_readwrite("created", &PasswordEntry::created)
        .def_readwrite("modified", &PasswordEntry::modified)
        .def_readwrite("expires", &PasswordEntry::expires)
        .def_readwrite("last_accessed", &PasswordEntry::lastAccessed)
        .def_readwrite("access_count", &PasswordEntry::accessCount)
        .def_readwrite("custom_fields", &PasswordEntry::customFields)
        .def("generate_id", &PasswordEntry::generateId)
        .def("is_empty", &PasswordEntry::isEmpty)
        .def("is_expired", &PasswordEntry::isExpired)
        .def("is_expiring_soon", &PasswordEntry::isExpiringSoon,
             py::arg("days"))
        .def("add_tag", &PasswordEntry::addTag, py::arg("tag"))
        .def("remove_tag", &PasswordEntry::removeTag, py::arg("tag"))
        .def("has_tag", &PasswordEntry::hasTag, py::arg("tag"))
        .def("add_custom_field", &PasswordEntry::addCustomField,
             py::arg("field"))
        .def("get_custom_field", &PasswordEntry::getCustomField,
             py::arg("name"), py::return_value_policy::reference)
        .def("update_password", &PasswordEntry::updatePassword,
             py::arg("new_password"), py::arg("max_history") = 10)
        .def("mark_accessed", &PasswordEntry::markAccessed);

    // Helper functions
    m.def("category_to_string", &categoryToString, py::arg("category"));
    m.def("string_to_category", &stringToCategory, py::arg("str"));
    m.def("strength_to_string", &strengthToString, py::arg("strength"));

    // PasswordGeneratorOptions struct
    py::class_<PasswordGeneratorOptions>(m, "PasswordGeneratorOptions",
                                         R"pbdoc(
        Options for password generation.
        )pbdoc")
        .def(py::init<>())
        .def_static("defaults", &PasswordGeneratorOptions::defaults)
        .def_static("strong", &PasswordGeneratorOptions::strong)
        .def_static("pin", &PasswordGeneratorOptions::pin,
                    py::arg("length") = 6)
        .def_static("readable", &PasswordGeneratorOptions::readable)
        .def_readwrite("length", &PasswordGeneratorOptions::length)
        .def_readwrite("include_lowercase",
                       &PasswordGeneratorOptions::includeLowercase)
        .def_readwrite("include_uppercase",
                       &PasswordGeneratorOptions::includeUppercase)
        .def_readwrite("include_digits",
                       &PasswordGeneratorOptions::includeDigits)
        .def_readwrite("include_special",
                       &PasswordGeneratorOptions::includeSpecial)
        .def_readwrite("exclude_ambiguous",
                       &PasswordGeneratorOptions::excludeAmbiguous)
        .def_readwrite("exclude_brackets",
                       &PasswordGeneratorOptions::excludeBrackets)
        .def_readwrite("custom_characters",
                       &PasswordGeneratorOptions::customCharacters)
        .def_readwrite("exclude_characters",
                       &PasswordGeneratorOptions::excludeCharacters)
        .def_readwrite("min_lowercase", &PasswordGeneratorOptions::minLowercase)
        .def_readwrite("min_uppercase", &PasswordGeneratorOptions::minUppercase)
        .def_readwrite("min_digits", &PasswordGeneratorOptions::minDigits)
        .def_readwrite("min_special", &PasswordGeneratorOptions::minSpecial)
        .def_readwrite("start_with_letter",
                       &PasswordGeneratorOptions::startWithLetter)
        .def_readwrite("no_repeating_chars",
                       &PasswordGeneratorOptions::noRepeatingChars);

    // PasswordGenerator class
    py::class_<PasswordGenerator>(m, "PasswordGenerator",
                                  R"pbdoc(
        Secure password generation utilities.
        )pbdoc")
        .def_static("generate",
                    py::overload_cast<>(&PasswordGenerator::generate),
                    "Generates a password with default options")
        .def_static("generate",
                    py::overload_cast<const PasswordGeneratorOptions&>(
                        &PasswordGenerator::generate),
                    py::arg("options"),
                    "Generates a password with custom options")
        .def_static("generate_pin", &PasswordGenerator::generatePin,
                    py::arg("length") = 6, "Generates a PIN")
        .def_static("generate_passphrase",
                    &PasswordGenerator::generatePassphrase,
                    py::arg("word_count") = 4, py::arg("separator") = "-",
                    py::arg("capitalize") = true,
                    py::arg("include_number") = true, "Generates a passphrase")
        .def_static(
            "generate_pronounceable", &PasswordGenerator::generatePronounceable,
            py::arg("length") = 12, "Generates a pronounceable password")
        .def_static("validate_options", &PasswordGenerator::validateOptions,
                    py::arg("options"), "Validates generation options");

    // PasswordPolicy struct
    py::class_<PasswordPolicy>(m, "PasswordPolicy",
                               R"pbdoc(
        Password policy for validation.
        )pbdoc")
        .def(py::init<>())
        .def_readwrite("min_length", &PasswordPolicy::minLength)
        .def_readwrite("max_length", &PasswordPolicy::maxLength)
        .def_readwrite("require_uppercase", &PasswordPolicy::requireUppercase)
        .def_readwrite("require_lowercase", &PasswordPolicy::requireLowercase)
        .def_readwrite("require_digit", &PasswordPolicy::requireDigit)
        .def_readwrite("require_special", &PasswordPolicy::requireSpecial)
        .def_readwrite("min_unique_chars", &PasswordPolicy::minUniqueChars)
        .def_readwrite("check_common", &PasswordPolicy::checkCommon)
        .def_readwrite("check_history", &PasswordPolicy::checkHistory);

    // ValidationResult struct
    py::class_<ValidationResult>(m, "ValidationResult",
                                 R"pbdoc(
        Result of password validation.
        )pbdoc")
        .def(py::init<>())
        .def_readonly("is_valid", &ValidationResult::isValid)
        .def_readonly("score", &ValidationResult::score)
        .def_readonly("strength", &ValidationResult::strength)
        .def_readonly("entropy", &ValidationResult::entropy)
        .def_readonly("issues", &ValidationResult::issues)
        .def_readonly("suggestions", &ValidationResult::suggestions);

    // PasswordValidator class
    py::class_<PasswordValidator>(m, "PasswordValidator",
                                  R"pbdoc(
        Password validation and strength analysis.
        )pbdoc")
        .def_static(
            "validate",
            py::overload_cast<const std::string&>(&PasswordValidator::validate),
            py::arg("password"), "Validates a password with default policy")
        .def_static(
            "validate",
            py::overload_cast<const std::string&, const PasswordPolicy&>(
                &PasswordValidator::validate),
            py::arg("password"), py::arg("policy"),
            "Validates a password with custom policy")
        .def_static("calculate_strength", &PasswordValidator::calculateStrength,
                    py::arg("score"), "Calculates strength from score")
        .def_static("calculate_entropy", &PasswordValidator::calculateEntropy,
                    py::arg("password"), "Calculates password entropy")
        .def_static("has_repeating_characters",
                    &PasswordValidator::hasRepeatingCharacters,
                    py::arg("password"), py::arg("min_repeat") = 3,
                    "Checks for repeating characters")
        .def_static("has_sequential_characters",
                    &PasswordValidator::hasSequentialCharacters,
                    py::arg("password"), py::arg("min_sequence") = 3,
                    "Checks for sequential characters")
        .def_static("is_common_password", &PasswordValidator::isCommonPassword,
                    py::arg("password"), "Checks if password is common");

    // BreachChecker class
    py::class_<BreachChecker>(m, "BreachChecker",
                              R"pbdoc(
        Password breach checking utilities.
        )pbdoc")
        .def_static("is_common_breached_password",
                    &BreachChecker::isCommonBreachedPassword,
                    py::arg("password"),
                    "Checks if password is in common breached list")
        .def_static("get_hash_prefix", &BreachChecker::getHashPrefix,
                    py::arg("password"),
                    "Gets SHA-1 hash prefix for k-anonymity check")
        .def_static("get_hash_suffix", &BreachChecker::getHashSuffix,
                    py::arg("password"),
                    "Gets SHA-1 hash suffix for k-anonymity check")
        .def_static("add_to_common_list", &BreachChecker::addToCommonList,
                    py::arg("password"), "Adds a password to the common list")
        .def_static("clear_common_list", &BreachChecker::clearCommonList,
                    "Clears the common password list")
        .def_static("get_common_list_size", &BreachChecker::getCommonListSize,
                    "Gets the size of the common password list");
}
