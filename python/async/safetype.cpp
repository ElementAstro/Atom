#include "atom/async/safetype.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(safetype, m) {
    m.doc() =
        "Lock-free data structures implementation module for the atom package";

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

    // LockFreeStack class binding
    py::class_<atom::async::LockFreeStack<py::object>>(
        m, "LockFreeStack",
        R"(Lock-free stack implementation for concurrent access.

This class provides a thread-safe stack that can be accessed by multiple threads
simultaneously without explicit locking mechanisms.

Examples:
    >>> from atom.async import LockFreeStack
    >>> stack = LockFreeStack()
    >>> stack.push("item1")
    >>> stack.push("item2")
    >>> item = stack.pop()
    >>> print(item)
    item2
)")
        .def(py::init<>(), "Creates a new empty LockFreeStack.")
        .def(
            "push",
            [](atom::async::LockFreeStack<py::object>& self, py::object value) {
                self.push(value);
            },
            py::arg("value"),
            R"(Push an element onto the stack.

Args:
    value: The element to push onto the stack.
)")
        .def("pop", &atom::async::LockFreeStack<py::object>::pop,
             R"(Pop an element from the stack.

Returns:
    The top element from the stack, or None if the stack is empty.
)")
        .def("top", &atom::async::LockFreeStack<py::object>::top,
             R"(Get the top element without removing it.

Returns:
    The top element, or None if the stack is empty.
)")
        .def("empty", &atom::async::LockFreeStack<py::object>::empty,
             R"(Check if the stack is empty.

Returns:
    True if the stack is empty, False otherwise.
)")
        .def("size", &atom::async::LockFreeStack<py::object>::size,
             R"(Get the approximate size of the stack.

Returns:
    The approximate number of elements in the stack.
)")
        // Python-specific methods
        .def("__len__", &atom::async::LockFreeStack<py::object>::size,
             "Support for len() function.")
        .def(
            "__bool__",
            [](const atom::async::LockFreeStack<py::object>& self) {
                return !self.empty();
            },
            "Support for boolean evaluation.");

    // LockFreeHashTable class binding
    py::class_<atom::async::LockFreeHashTable<py::object, py::object>>(
        m, "LockFreeHashTable",
        R"(Lock-free hash table implementation for concurrent access.

This class provides a thread-safe hash table that can be accessed by multiple threads
simultaneously without explicit locking mechanisms.

Args:
    num_buckets: Initial number of buckets in the hash table (default: 16).

Examples:
    >>> from atom.async import LockFreeHashTable
    >>> table = LockFreeHashTable()
    >>> table.insert("key1", "value1")
    >>> table["key2"] = "value2"
    >>> print(table.find("key1"))
    value1
    >>> print(table["key2"])
    value2
)")
        .def(py::init<size_t>(), py::arg("num_buckets") = 16,
             "Creates a new empty LockFreeHashTable with the specified number "
             "of buckets.")
        .def(
            "find",
            [](const atom::async::LockFreeHashTable<py::object, py::object>&
                   self,
               py::object key) {
                auto result = self.find(key);
                if (result) {
                    return result->get();
                }
                return py::none();
            },
            py::arg("key"),
            R"(Find a value by key.

Args:
    key: The key to look up.

Returns:
    The associated value, or None if the key is not found.
)")
        .def(
            "insert",
            [](atom::async::LockFreeHashTable<py::object, py::object>& self,
               py::object key, py::object value) { self.insert(key, value); },
            py::arg("key"), py::arg("value"),
            R"(Insert a key-value pair into the hash table.

Args:
    key: The key.
    value: The value to associate with the key.
)")
        .def(
            "erase",
            [](atom::async::LockFreeHashTable<py::object, py::object>& self,
               py::object key) { return self.erase(key); },
            py::arg("key"),
            R"(Remove a key-value pair from the hash table.

Args:
    key: The key to remove.

Returns:
    True if the key was found and removed, False otherwise.
)")
        .def("empty",
             &atom::async::LockFreeHashTable<py::object, py::object>::empty,
             R"(Check if the hash table is empty.

Returns:
    True if the hash table is empty, False otherwise.
)")
        .def("size",
             &atom::async::LockFreeHashTable<py::object, py::object>::size,
             R"(Get the number of elements in the hash table.

Returns:
    The number of key-value pairs in the hash table.
)")
        .def("clear",
             &atom::async::LockFreeHashTable<py::object, py::object>::clear,
             R"(Clear all elements from the hash table.)")
        .def(
            "__getitem__",
            [](atom::async::LockFreeHashTable<py::object, py::object>& self,
               py::object key) {
                auto result = self.find(key);
                if (result) {
                    return result->get();
                }
                throw py::key_error("Key not found: " +
                                    py::str(key).cast<std::string>());
            },
            py::arg("key"), "Support for table[key] access.")
        .def(
            "__setitem__",
            [](atom::async::LockFreeHashTable<py::object, py::object>& self,
               py::object key, py::object value) { self.insert(key, value); },
            py::arg("key"), py::arg("value"),
            "Support for table[key] = value assignment.")
        .def("__len__",
             &atom::async::LockFreeHashTable<py::object, py::object>::size,
             "Support for len() function.")
        .def(
            "__bool__",
            [](const atom::async::LockFreeHashTable<py::object, py::object>&
                   self) { return !self.empty(); },
            "Support for boolean evaluation.")
        .def(
            "__contains__",
            [](const atom::async::LockFreeHashTable<py::object, py::object>&
                   self,
               py::object key) { return self.find(key).has_value(); },
            py::arg("key"), "Support for 'key in table' membership test.")
        .def(
            "__iter__",
            [](const atom::async::LockFreeHashTable<py::object, py::object>&
                   self) {
                return py::make_iterator(self.begin(), self.end());
            },
            py::keep_alive<0, 1>(), "Support for iteration over table items.");

    // ThreadSafeVector class binding
    py::class_<atom::async::ThreadSafeVector<py::object>>(
        m, "ThreadSafeVector",
        R"(Thread-safe vector implementation for concurrent access.

This class provides a thread-safe dynamic array that can be accessed by multiple threads
simultaneously.

Args:
    initial_capacity: Initial capacity of the vector (default: 16).

Examples:
    >>> from atom.async import ThreadSafeVector
    >>> vec = ThreadSafeVector()
    >>> vec.push_back("item1")
    >>> vec.push_back("item2")
    >>> print(vec.at(0))
    item1
    >>> print(vec[1])
    item2
)")
        .def(py::init<size_t>(), py::arg("initial_capacity") = 16,
             "Creates a new empty ThreadSafeVector with the specified initial "
             "capacity.")
        .def(
            "push_back",
            [](atom::async::ThreadSafeVector<py::object>& self,
               py::object value) { self.pushBack(value); },
            py::arg("value"),
            R"(Add an element to the end of the vector.

Args:
    value: The element to add.
)")
        .def("pop_back", &atom::async::ThreadSafeVector<py::object>::popBack,
             R"(Remove and return the last element.

Returns:
    The last element, or None if the vector is empty.
)")
        .def("at", &atom::async::ThreadSafeVector<py::object>::at,
             py::arg("index"),
             R"(Access an element at the specified index.

Args:
    index: Zero-based index of the element.

Returns:
    The element at the specified position.

Raises:
    IndexError: If index is out of range.
)")
        .def(
            "try_at", &atom::async::ThreadSafeVector<py::object>::try_at,
            py::arg("index"),
            R"(Try to access an element at the specified index without throwing exceptions.

Args:
    index: Zero-based index of the element.

Returns:
    The element at the specified position, or None if index is out of range.
)")
        .def("empty", &atom::async::ThreadSafeVector<py::object>::empty,
             R"(Check if the vector is empty.

Returns:
    True if the vector is empty, False otherwise.
)")
        .def("size", &atom::async::ThreadSafeVector<py::object>::getSize,
             R"(Get the size of the vector.

Returns:
    The number of elements in the vector.
)")
        .def("capacity",
             &atom::async::ThreadSafeVector<py::object>::getCapacity,
             R"(Get the capacity of the vector.

Returns:
    The current capacity of the vector.
)")
        .def("clear", &atom::async::ThreadSafeVector<py::object>::clear,
             R"(Clear all elements from the vector.)")
        .def("shrink_to_fit",
             &atom::async::ThreadSafeVector<py::object>::shrinkToFit,
             R"(Reduce memory usage by freeing unused capacity.)")
        .def("front", &atom::async::ThreadSafeVector<py::object>::front,
             R"(Get the first element of the vector.

Returns:
    The first element.

Raises:
    IndexError: If the vector is empty.
)")
        .def("try_front", &atom::async::ThreadSafeVector<py::object>::try_front,
             R"(Try to get the first element without throwing exceptions.

Returns:
    The first element, or None if the vector is empty.
)")
        .def("back", &atom::async::ThreadSafeVector<py::object>::back,
             R"(Get the last element of the vector.

Returns:
    The last element.

Raises:
    IndexError: If the vector is empty.
)")
        .def("try_back", &atom::async::ThreadSafeVector<py::object>::try_back,
             R"(Try to get the last element without throwing exceptions.

Returns:
    The last element, or None if the vector is empty.
)")
        .def(
            "__getitem__",
            [](atom::async::ThreadSafeVector<py::object>& self, size_t index) {
                if (index >= self.getSize()) {
                    throw py::index_error("Index out of range");
                }
                return self[index];
            },
            py::arg("index"), "Support for vector[index] access.")
        .def("__len__", &atom::async::ThreadSafeVector<py::object>::getSize,
             "Support for len() function.")
        .def(
            "__bool__",
            [](const atom::async::ThreadSafeVector<py::object>& self) {
                return !self.empty();
            },
            "Support for boolean evaluation.")
        .def(
            "__iter__",
            [](const atom::async::ThreadSafeVector<py::object>& self) {
                // Create a temporary vector to hold all elements
                std::vector<py::object> items;
                for (size_t i = 0; i < self.getSize(); ++i) {
                    items.push_back(self.at(i));
                }
                return py::make_iterator(items.begin(), items.end());
            },
            py::keep_alive<0, 1>(),
            "Support for iteration over vector elements.");

    // LockFreeList class binding
    py::class_<atom::async::LockFreeList<py::object>>(
        m, "LockFreeList",
        R"(Lock-free linked list implementation for concurrent access.

This class provides a thread-safe singly-linked list that can be accessed by multiple threads
simultaneously without explicit locking mechanisms.

Examples:
    >>> from atom.async import LockFreeList
    >>> lst = LockFreeList()
    >>> lst.push_front("item1")
    >>> lst.push_front("item2")
    >>> print(lst.front())
    item2
    >>> item = lst.pop_front()
    >>> print(item)
    item2
)")
        .def(py::init<>(), "Creates a new empty LockFreeList.")
        .def(
            "push_front",
            [](atom::async::LockFreeList<py::object>& self, py::object value) {
                self.pushFront(value);
            },
            py::arg("value"),
            R"(Add an element to the front of the list.

Args:
    value: The element to add.
)")
        .def("pop_front", &atom::async::LockFreeList<py::object>::popFront,
             R"(Remove and return the front element.

Returns:
    The front element, or None if the list is empty.
)")
        .def("front", &atom::async::LockFreeList<py::object>::front,
             R"(Get the front element without removing it.

Returns:
    The front element, or None if the list is empty.
)")
        .def("empty", &atom::async::LockFreeList<py::object>::empty,
             R"(Check if the list is empty.

Returns:
    True if the list is empty, False otherwise.
)")
        .def("size", &atom::async::LockFreeList<py::object>::size,
             R"(Get the size of the list.

Returns:
    The number of elements in the list.
)")
        .def("clear", &atom::async::LockFreeList<py::object>::clear,
             R"(Clear all elements from the list.)")
        // Python-specific methods
        .def("__len__", &atom::async::LockFreeList<py::object>::size,
             "Support for len() function.")
        .def(
            "__bool__",
            [](const atom::async::LockFreeList<py::object>& self) {
                return !self.empty();
            },
            "Support for boolean evaluation.")
        .def(
            "__iter__",
            [](const atom::async::LockFreeList<py::object>& self) {
                return py::make_iterator(self.begin(), self.end());
            },
            py::keep_alive<0, 1>(),
            "Support for iteration over list elements.");

    // Factory functions
    m.def(
        "create_lock_free_stack",
        []() {
            return std::make_shared<atom::async::LockFreeStack<py::object>>();
        },
        R"(Create a new empty LockFreeStack.

Returns:
    A new LockFreeStack instance.

Examples:
    >>> from atom.async import create_lock_free_stack
    >>> stack = create_lock_free_stack()
)");

    m.def(
        "create_lock_free_hash_table",
        [](size_t num_buckets) {
            return std::make_shared<
                atom::async::LockFreeHashTable<py::object, py::object>>(
                num_buckets);
        },
        py::arg("num_buckets") = 16,
        R"(Create a new empty LockFreeHashTable.

Args:
    num_buckets: Initial number of buckets in the hash table (default: 16).

Returns:
    A new LockFreeHashTable instance.

Examples:
    >>> from atom.async import create_lock_free_hash_table
    >>> table = create_lock_free_hash_table(32)  # 32 buckets
)");

    m.def(
        "create_thread_safe_vector",
        [](const py::list& items) {
            auto vec =
                std::make_shared<atom::async::ThreadSafeVector<py::object>>(
                    items.size());
            for (auto item : items) {
                vec->pushBack(item);
            }
            return vec;
        },
        py::arg("items") = py::list(),
        R"(Create a new ThreadSafeVector with initial elements.

Args:
    items: Initial items to add to the vector (optional).

Returns:
    A new ThreadSafeVector containing the provided items.

Examples:
    >>> from atom.async import create_thread_safe_vector
    >>> vec = create_thread_safe_vector(["item1", "item2", "item3"])
    >>> vec.size()
    3
)");

    m.def(
        "create_lock_free_list",
        [](const py::list& items) {
            auto list =
                std::make_shared<atom::async::LockFreeList<py::object>>();
            // Add items in reverse order to maintain original list order
            for (auto it = items.rbegin(); it != items.rend(); ++it) {
                list->pushFront(*it);
            }
            return list;
        },
        py::arg("items") = py::list(),
        R"(Create a new LockFreeList with initial elements.

Args:
    items: Initial items to add to the list (optional).

Returns:
    A new LockFreeList containing the provided items.

Examples:
    >>> from atom.async import create_lock_free_list
    >>> lst = create_lock_free_list(["item1", "item2", "item3"])
    >>> lst.size()
    3
)");
}