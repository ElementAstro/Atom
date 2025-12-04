#include "atom/secret/storage.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void bind_storage(py::module& m) {
    // SecureStorage interface
    py::class_<atom::secret::SecureStorage,
               std::unique_ptr<atom::secret::SecureStorage>>(m, "SecureStorage",
                                                             R"pbdoc(
        Interface for platform-specific secure storage.

        Provides a unified interface for storing encrypted data in the
        platform's secure storage system (Windows Credential Manager,
        macOS Keychain, Linux Secret Service, or file-based fallback).

        Example:
            >>> # Create a secure storage instance
            >>> storage = SecureStorage.create("MyApp")
            >>>
            >>> # Store encrypted data
            >>> storage.store("my_key", "encrypted_data_here")
            >>>
            >>> # Retrieve data
            >>> data = storage.retrieve("my_key")
            >>>
            >>> # Get all keys
            >>> keys = storage.get_all_keys()
            >>>
            >>> # Remove data
            >>> storage.remove("my_key")
        )pbdoc")
        .def("store", &atom::secret::SecureStorage::store, py::arg("key"),
             py::arg("data"),
             R"pbdoc(
            Stores encrypted data in the platform's secure storage.

            Args:
                key: The key or identifier for the data.
                data: The encrypted data to store.

            Returns:
                bool: True on success, False otherwise.
            )pbdoc")
        .def("retrieve", &atom::secret::SecureStorage::retrieve, py::arg("key"),
             R"pbdoc(
            Retrieves encrypted data from the platform's secure storage.

            Args:
                key: The key or identifier for the data.

            Returns:
                str: The retrieved data or an empty string if not found/error.
            )pbdoc")
        .def("remove", &atom::secret::SecureStorage::remove, py::arg("key"),
             R"pbdoc(
            Deletes data from the platform's secure storage.

            Args:
                key: The key or identifier for the data to delete.

            Returns:
                bool: True on success, False otherwise.
            )pbdoc")
        .def("get_all_keys", &atom::secret::SecureStorage::getAllKeys,
             R"pbdoc(
            Gets all keys/identifiers stored in the platform's secure storage.

            Returns:
                list[str]: A list of key strings.
            )pbdoc")
        .def_static("create", &atom::secret::SecureStorage::create,
                    py::arg("app_name"),
                    R"pbdoc(
            Creates and returns a platform-appropriate instance of SecureStorage.

            Args:
                app_name: The application name for storage categorization.

            Returns:
                SecureStorage: A unique_ptr to a SecureStorage instance.
            )pbdoc");
}
