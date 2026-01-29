/*
 * storage.cpp
 *
 * Python bindings for storage module.
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 * License: GPL3
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/secret/storage/backup.hpp"
#include "atom/secret/storage/file_storage.hpp"
#include "atom/secret/storage/storage.hpp"

namespace py = pybind11;
using namespace atom::secret;

void bind_storage(py::module& m) {
    // StorageBackend enum
    py::enum_<StorageBackend>(m, "StorageBackend",
                              R"pbdoc(
        Storage backend types.
        )pbdoc")
        .value("Memory", StorageBackend::Memory)
        .value("File", StorageBackend::File)
        .value("System", StorageBackend::System)
        .export_values();

    // SecureStorage class
    py::class_<SecureStorage, std::shared_ptr<SecureStorage>>(m,
                                                              "SecureStorage",
                                                              R"pbdoc(
        Secure storage interface.
        )pbdoc")
        .def_static("create",
                    py::overload_cast<StorageBackend, const std::string&>(
                        &SecureStorage::create),
                    py::arg("backend") = StorageBackend::Memory,
                    py::arg("path") = "", "Creates a secure storage instance")
        .def_static("is_backend_available", &SecureStorage::isBackendAvailable,
                    py::arg("backend"), "Checks if a backend is available")
        .def("store", &SecureStorage::store, py::arg("key"), py::arg("data"),
             "Stores binary data")
        .def("store_string", &SecureStorage::storeString, py::arg("key"),
             py::arg("data"), "Stores string data")
        .def("retrieve", &SecureStorage::retrieve, py::arg("key"),
             "Retrieves binary data")
        .def("retrieve_string", &SecureStorage::retrieveString, py::arg("key"),
             "Retrieves string data")
        .def("remove", &SecureStorage::remove, py::arg("key"), "Removes data")
        .def("exists", &SecureStorage::exists, py::arg("key"),
             "Checks if key exists")
        .def("list_keys", &SecureStorage::listKeys, "Lists all keys")
        .def("clear", &SecureStorage::clear, "Clears all data")
        .def("get_location", &SecureStorage::getLocation,
             "Gets storage location");

    // FileStorage class
    py::class_<FileStorage, SecureStorage, std::shared_ptr<FileStorage>>(
        m, "FileStorage",
        R"pbdoc(
        File-based secure storage.
        )pbdoc")
        .def(py::init<const std::string&>(), py::arg("path"));

    // BackupConfig struct
    py::class_<BackupConfig>(m, "BackupConfig",
                             R"pbdoc(
        Configuration for backup operations.
        )pbdoc")
        .def(py::init<>())
        .def_readwrite("backup_directory", &BackupConfig::backupDirectory)
        .def_readwrite("compress", &BackupConfig::compress)
        .def_readwrite("encrypt", &BackupConfig::encrypt)
        .def_readwrite("encryption_password", &BackupConfig::encryptionPassword)
        .def_readwrite("max_backups", &BackupConfig::maxBackups);

    // BackupManager class
    py::class_<BackupManager>(m, "BackupManager",
                              R"pbdoc(
        Backup management utilities.
        )pbdoc")
        .def_static("create_backup", &BackupManager::createBackup,
                    py::arg("storage"), py::arg("config"), "Creates a backup")
        .def_static("restore_backup", &BackupManager::restoreBackup,
                    py::arg("storage"), py::arg("backup_path"),
                    py::arg("config"), "Restores from a backup")
        .def_static("list_backups", &BackupManager::listBackups,
                    py::arg("backup_directory"), "Lists available backups")
        .def_static("delete_old_backups", &BackupManager::deleteOldBackups,
                    py::arg("backup_directory"), py::arg("max_backups"),
                    "Deletes old backups");
}
