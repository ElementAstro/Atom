#include "atom/secret/password_entry.hpp"

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void bind_password_entry(py::module& m) {
    py::class_<atom::secret::PasswordEntry>(m, "PasswordEntry",
                                            R"pbdoc(
        Structure representing a password entry.

        Contains all information associated with a stored password including
        credentials, metadata, timestamps, and password history.

        Attributes:
            password (str): The stored password
            username (str): Associated username
            url (str): Associated URL
            notes (str): Additional notes
            title (str): Entry title
            category (PasswordCategory): Password category
            tags (list[str]): Tags for categorization and search
            created (datetime): Creation timestamp
            modified (datetime): Last modification timestamp
            expires (datetime): Expiration timestamp
            previous_passwords (list[str]): Password history

        Example:
            >>> entry = PasswordEntry()
            >>> entry.password = "SecurePassword123!"
            >>> entry.username = "user@example.com"
            >>> entry.url = "https://example.com"
            >>> entry.category = PasswordCategory.Personal
            >>> entry.tags = ["important", "work"]
        )pbdoc")
        .def(py::init<>(), "Default constructor")
        .def(py::init<const atom::secret::PasswordEntry&>(), py::arg("other"),
             "Copy constructor")
        .def_readwrite("password", &atom::secret::PasswordEntry::password,
                       "The stored password")
        .def_readwrite("username", &atom::secret::PasswordEntry::username,
                       "Associated username")
        .def_readwrite("url", &atom::secret::PasswordEntry::url,
                       "Associated URL")
        .def_readwrite("notes", &atom::secret::PasswordEntry::notes,
                       "Additional notes")
        .def_readwrite("title", &atom::secret::PasswordEntry::title,
                       "Entry title")
        .def_readwrite("category", &atom::secret::PasswordEntry::category,
                       "Password category")
        .def_readwrite("tags", &atom::secret::PasswordEntry::tags,
                       "Tags for categorization and search")
        .def_readwrite("created", &atom::secret::PasswordEntry::created,
                       "Creation timestamp")
        .def_readwrite("modified", &atom::secret::PasswordEntry::modified,
                       "Last modification timestamp")
        .def_readwrite("expires", &atom::secret::PasswordEntry::expires,
                       "Expiration timestamp")
        .def_readwrite("previous_passwords",
                       &atom::secret::PasswordEntry::previousPasswords,
                       "Password history")
        .def("is_empty", &atom::secret::PasswordEntry::isEmpty,
             R"pbdoc(
            Checks if the entry is empty.

            Returns:
                bool: True if the entry is empty, False otherwise.
            )pbdoc")
        .def("__repr__", [](const atom::secret::PasswordEntry& entry) {
            return "<PasswordEntry: username='" + entry.username + "', url='" +
                   entry.url + "'>";
        });
}
