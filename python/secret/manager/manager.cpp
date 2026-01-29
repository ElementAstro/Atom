/*
 * manager.cpp
 *
 * Python bindings for manager module.
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 * License: GPL3
 */

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/secret/manager/audit_log.hpp"
#include "atom/secret/manager/password_manager.hpp"
#include "atom/secret/manager/session.hpp"

namespace py = pybind11;
using namespace atom::secret;

void bind_manager(py::module& m) {
    // SessionConfig struct
    py::class_<SessionConfig>(m, "SessionConfig",
                              R"pbdoc(
        Configuration for session management.
        )pbdoc")
        .def(py::init<>())
        .def_static("defaults", &SessionConfig::defaults)
        .def_readwrite("timeout_seconds", &SessionConfig::timeoutSeconds)
        .def_readwrite("auto_lock_on_idle", &SessionConfig::autoLockOnIdle);

    // SessionManager class
    py::class_<SessionManager>(m, "SessionManager",
                               R"pbdoc(
        Session management for password manager.
        )pbdoc")
        .def(py::init<>())
        .def(py::init<const SessionConfig&>(), py::arg("config"))
        .def("unlock", &SessionManager::unlock)
        .def("lock", &SessionManager::lock)
        .def("is_unlocked", &SessionManager::isUnlocked)
        .def("is_expired", &SessionManager::isExpired)
        .def("record_activity", &SessionManager::recordActivity)
        .def("get_last_activity", &SessionManager::getLastActivity)
        .def("get_session_duration", &SessionManager::getSessionDuration)
        .def("get_config", &SessionManager::getConfig)
        .def("update_config", &SessionManager::updateConfig, py::arg("config"));

    // AuditAction enum
    py::enum_<AuditAction>(m, "AuditAction",
                           R"pbdoc(
        Actions that can be audited.
        )pbdoc")
        .value("SessionUnlock", AuditAction::SessionUnlock)
        .value("SessionLock", AuditAction::SessionLock)
        .value("EntryCreate", AuditAction::EntryCreate)
        .value("EntryRead", AuditAction::EntryRead)
        .value("EntryUpdate", AuditAction::EntryUpdate)
        .value("EntryDelete", AuditAction::EntryDelete)
        .value("PasswordCopied", AuditAction::PasswordCopied)
        .value("PasswordChanged", AuditAction::PasswordChanged)
        .value("Import", AuditAction::Import)
        .value("Export", AuditAction::Export)
        .value("BackupCreated", AuditAction::BackupCreated)
        .value("BackupRestored", AuditAction::BackupRestored)
        .export_values();

    // AuditEntry struct
    py::class_<AuditEntry>(m, "AuditEntry",
                           R"pbdoc(
        An entry in the audit log.
        )pbdoc")
        .def(py::init<>())
        .def_readonly("timestamp", &AuditEntry::timestamp)
        .def_readonly("action", &AuditEntry::action)
        .def_readonly("entry_id", &AuditEntry::entryId)
        .def_readonly("entry_title", &AuditEntry::entryTitle)
        .def_readonly("details", &AuditEntry::details);

    // AuditLog class
    py::class_<AuditLog>(m, "AuditLog",
                         R"pbdoc(
        Audit logging for password manager operations.
        )pbdoc")
        .def(py::init<const std::filesystem::path&>(), py::arg("log_path"))
        .def("log",
             py::overload_cast<AuditAction, const std::string&,
                               const std::string&, const std::string&>(
                 &AuditLog::log),
             py::arg("action"), py::arg("entry_id") = "",
             py::arg("entry_title") = "", py::arg("details") = "",
             "Logs an action")
        .def("get_entries", &AuditLog::getEntries)
        .def("get_entry_count", &AuditLog::getEntryCount)
        .def("query_by_action", &AuditLog::queryByAction, py::arg("action"))
        .def("query_by_entry_id", &AuditLog::queryByEntryId,
             py::arg("entry_id"))
        .def("query_by_time_range", &AuditLog::queryByTimeRange,
             py::arg("start"), py::arg("end"))
        .def("clear", &AuditLog::clear)
        .def("export_to_csv", &AuditLog::exportToCsv, py::arg("path"))
        .def("export_to_json", &AuditLog::exportToJson, py::arg("path"));

    m.def("audit_action_to_string", &auditActionToString, py::arg("action"));

    // SearchFilter struct
    py::class_<SearchFilter>(m, "SearchFilter",
                             R"pbdoc(
        Filter for searching password entries.
        )pbdoc")
        .def(py::init<>())
        .def_readwrite("query", &SearchFilter::query)
        .def_readwrite("categories", &SearchFilter::categories)
        .def_readwrite("tags", &SearchFilter::tags)
        .def_readwrite("favorites_only", &SearchFilter::favoritesOnly)
        .def_readwrite("exclude_archived", &SearchFilter::excludeArchived);

    // PasswordManagerSettings struct
    py::class_<PasswordManagerSettings>(m, "PasswordManagerSettings",
                                        R"pbdoc(
        Settings for the password manager.
        )pbdoc")
        .def(py::init<>())
        .def_readwrite("min_password_length",
                       &PasswordManagerSettings::minPasswordLength)
        .def_readwrite("session_timeout_seconds",
                       &PasswordManagerSettings::sessionTimeoutSeconds)
        .def_readwrite("max_password_history",
                       &PasswordManagerSettings::maxPasswordHistory)
        .def_readwrite("auto_lock_enabled",
                       &PasswordManagerSettings::autoLockEnabled)
        .def_readwrite("audit_enabled", &PasswordManagerSettings::auditEnabled);

    // PasswordManager class
    py::class_<PasswordManager>(m, "PasswordManager",
                                R"pbdoc(
        Main password manager interface.
        )pbdoc")
        .def(py::init<std::unique_ptr<SecureStorage>>(), py::arg("storage"))
        .def("initialize", &PasswordManager::initialize,
             py::arg("master_password"), "Initializes the password manager")
        .def("unlock", &PasswordManager::unlock, py::arg("master_password"),
             "Unlocks the password manager")
        .def("lock", &PasswordManager::lock, "Locks the password manager")
        .def("is_unlocked", &PasswordManager::isUnlocked,
             "Returns whether the manager is unlocked")
        .def("add_entry", &PasswordManager::addEntry, py::arg("entry"),
             "Adds a password entry")
        .def("get_entry", &PasswordManager::getEntry, py::arg("id"),
             "Gets a password entry by ID")
        .def("update_entry", &PasswordManager::updateEntry, py::arg("entry"),
             "Updates a password entry")
        .def("delete_entry", &PasswordManager::deleteEntry, py::arg("id"),
             "Deletes a password entry")
        .def("get_all_entries", &PasswordManager::getAllEntries,
             "Gets all password entries")
        .def("search", &PasswordManager::search, py::arg("filter"),
             "Searches password entries")
        .def("get_entry_count", &PasswordManager::getEntryCount,
             "Gets the number of entries")
        .def("get_password", &PasswordManager::getPassword, py::arg("id"),
             "Gets the password for an entry")
        .def("update_password", &PasswordManager::updatePassword, py::arg("id"),
             py::arg("new_password"), "Updates the password for an entry")
        .def("export_to_json", &PasswordManager::exportToJson,
             "Exports all entries to JSON")
        .def("import_from_json", &PasswordManager::importFromJson,
             py::arg("json"), "Imports entries from JSON")
        .def("change_master_password", &PasswordManager::changeMasterPassword,
             py::arg("current_password"), py::arg("new_password"),
             "Changes the master password");
}
