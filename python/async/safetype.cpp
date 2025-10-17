#include "atom/async/safetype.hpp"

#include <pybind11/functional.h>
#include <pybind11/operators.h>  // For operators like == if needed explicitly for py::object with concepts
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Specialization of std::hash for py::object
// This allows py::object to be used as a key in std::unordered_map,
// and satisfies the HashTableKeyValue concept for LockFreeHashTable.
namespace std {
template <>
struct hash<py::object> {
    std::size_t operator()(const py::object& obj) const {
        try {
            // py::hash correctly handles Python's hash, including
            // negative results, and converts it to std::size_t.
            return py::hash(obj);
        } catch (const py::error_already_set& e) {
            // If the Python object is unhashable, py::hash_value will throw.
            // We rethrow the Python error, which pybind11 will convert to a C++
            // exception. This makes the error visible at the C++ level. Ensure
            // that only hashable Python objects are used as keys.
            throw;
        }
    }
};
}  // namespace std

PYBIND11_MODULE(safetype, m) {
    m.doc() = R"pbdoc(
        Thread-Safe and Lock-Free Data Structures Module
        -----------------------------------------------

        This module provides high-performance thread-safe and lock-free data structures
        for concurrent programming with advanced synchronization features.

        Features:
          - Lock-free data structures (stack, hash table, list)
          - Thread-safe data structures (vector, SafeType wrapper)
          - High-performance concurrent access patterns
          - Memory-efficient implementations with atomic operations
          - Python integration with proper exception handling

        The module includes:
          - LockFreeStack: Lock-free stack for concurrent push/pop operations
          - LockFreeHashTable: Lock-free hash table for concurrent key-value storage
          - LockFreeList: Lock-free linked list for concurrent operations
          - ThreadSafeVector: Thread-safe dynamic array with mutex protection
          - SafeType: Thread-safe wrapper for any type with read-write locks

        Example:
            >>> from atom.async.safetype import LockFreeStack, SafeType
            >>>
            >>> # Lock-free stack usage
            >>> stack = LockFreeStack()
            >>> stack.push("item1")
            >>> stack.push("item2")
            >>> item = stack.pop()  # Returns "item2"
            >>>
            >>> # Thread-safe type wrapper
            >>> safe_counter = SafeType(0)
            >>> safe_counter.modify(lambda x: x + 1)
            >>> value = safe_counter.get()  # Returns 1
    )pbdoc";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const py::error_already_set& e) {  // Catch pybind11 errors
            // Let pybind11 handle it or convert to PyExc_Exception if not
            // already set
            if (!PyErr_Occurred()) {
                PyErr_SetString(PyExc_Exception,
                                "An unknown pybind11 error occurred");
            }
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
               const py::object& key) -> py::object {  // Key should be const&
                auto result = self.find(key);
                if (result) {
                    return py::object(
                        result->get());  // Ensure a new py::object is returned
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
               const py::object& key,
               const py::object& value) {  // const& for args
                self.insert(key, value);
            },
            py::arg("key"), py::arg("value"),
            R"(Insert a key-value pair into the hash table.

Args:
    key: The key.
    value: The value to associate with the key.
)")
        .def(
            "erase",
            [](atom::async::LockFreeHashTable<py::object, py::object>& self,
               const py::object& key) {
                return self.erase(key);
            },  // const& for key
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
            [](const atom::async::LockFreeHashTable<py::object, py::object>&
                   self,                 // const self
               const py::object& key) {  // const& for key
                auto result = self.find(key);
                if (result) {
                    return py::object(
                        result->get());  // Ensure a new py::object
                }
                throw py::key_error("Key not found: " +
                                    py::str(key).cast<std::string>());
            },
            py::arg("key"), "Support for table[key] access.")
        .def(
            "__setitem__",
            [](atom::async::LockFreeHashTable<py::object, py::object>& self,
               const py::object& key,
               const py::object& value) {  // const& for args
                self.insert(key, value);
            },
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
               const py::object& key) {
                return self.find(key).has_value();
            },  // const& for key
            py::arg("key"), "Support for 'key in table' membership test.")
        .def(
            "__iter__",
            [](const atom::async::LockFreeHashTable<py::object, py::object>&
                   self) {
                // For LockFreeHashTable, iterating over keys is common.
                // The C++ iterator yields std::pair<const Key&, Value&>.
                // py::make_iterator will convert this to a Python tuple (key,
                // value). If you want to iterate over keys like a Python dict,
                // you'd do: return py::make_key_iterator(self.begin(),
                // self.end()); For items (key, value pairs):
                return py::make_iterator(self.begin(), self.end());
            },
            py::keep_alive<0, 1>(),
            "Support for iteration over table items (key, value pairs).");

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
               const py::object& value) {
                self.pushBack(value);
            },  // const& for value
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
            [](const atom::async::ThreadSafeVector<py::object>& self,
               size_t index) {  // const self
                if (index >= self.getSize()) {
                    throw py::index_error("Index out of range");
                }
                // The C++ operator[] returns by value (T), which is py::object.
                // This is fine.
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
                // Create a temporary vector to hold all elements for safe
                // iteration as the underlying vector can change.
                std::vector<py::object> items;
                items.reserve(self.getSize());  // Pre-allocate
                for (size_t i = 0; i < self.getSize(); ++i) {
                    items.push_back(self.at(i));  // at() is thread-safe
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
            [](atom::async::LockFreeList<py::object>& self,
               const py::object& value) {  // const&
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

    // SafeType class binding - Thread-safe wrapper for any type
    py::class_<atom::async::SafeType<py::object>>(m, "SafeType",
                                                  R"pbdoc(
        Thread-safe wrapper for any type with read-write lock protection.

        SafeType provides thread-safe access to any wrapped value using shared_mutex
        for optimal read performance. Multiple readers can access the value concurrently,
        but writers have exclusive access.

        Examples:
            >>> from atom.async.safetype import SafeType
            >>> safe_counter = SafeType(0)
            >>> safe_counter.set(42)
            >>> value = safe_counter.get()  # Returns 42
            >>>
            >>> # Modify with a function
            >>> safe_counter.modify(lambda x: x + 1)
            >>> print(safe_counter.get())  # Returns 43
            >>>
            >>> # Read-only access with a function
            >>> result = safe_counter.read(lambda x: x * 2)
            >>> print(result)  # Returns 86 (43 * 2)
        )pbdoc")
        .def(py::init<>(),
             R"pbdoc(
             Create a SafeType with default-constructed value.

             Examples:
                 >>> safe_value = SafeType()  # Contains None
             )pbdoc")
        .def(py::init<const py::object&>(), py::arg("value"),
             R"pbdoc(
             Create a SafeType with an initial value.

             Args:
                 value: Initial value to store

             Examples:
                 >>> safe_string = SafeType("hello")
                 >>> safe_list = SafeType([1, 2, 3])
             )pbdoc")
        .def("get", &atom::async::SafeType<py::object>::get,
             R"pbdoc(
             Get a copy of the current value (thread-safe read).

             Returns:
                 Copy of the current value

             Examples:
                 >>> safe_value = SafeType(42)
                 >>> current = safe_value.get()
                 >>> print(current)  # 42
             )pbdoc")
        .def("set",
             static_cast<void (atom::async::SafeType<py::object>::*)(
                 const py::object&)>(&atom::async::SafeType<py::object>::set),
             py::arg("value"),
             R"pbdoc(
             Set a new value (thread-safe write).

             Args:
                 value: New value to store

             Examples:
                 >>> safe_value = SafeType(0)
                 >>> safe_value.set(100)
                 >>> print(safe_value.get())  # 100
             )pbdoc")
        .def(
            "modify",
            [](atom::async::SafeType<py::object>& self,
               py::function func) -> py::object {
                return self.modify([func](py::object& value) -> py::object {
                    py::gil_scoped_acquire acquire;
                    try {
                        return func(value);
                    } catch (const py::error_already_set& e) {
                        throw;
                    }
                });
            },
            py::arg("func"),
            R"pbdoc(
             Modify the value using a function (thread-safe write).

             Args:
                 func: Function that takes the current value and returns a new result

             Returns:
                 Result of the function (if any)

             Examples:
                 >>> safe_counter = SafeType(10)
                 >>> safe_counter.modify(lambda x: x + 5)  # Increments by 5
                 >>> print(safe_counter.get())  # 15
                 >>>
                 >>> # Function with return value
                 >>> result = safe_counter.modify(lambda x: x * 2)
                 >>> print(result)  # Returns 30, value becomes 30
             )pbdoc")
        .def(
            "read",
            [](const atom::async::SafeType<py::object>& self,
               py::function func) -> py::object {
                return self.read([func](const py::object& value) -> py::object {
                    py::gil_scoped_acquire acquire;
                    try {
                        return func(value);
                    } catch (const py::error_already_set& e) {
                        throw;
                    }
                });
            },
            py::arg("func"),
            R"pbdoc(
             Read the value using a function (thread-safe read-only access).

             Args:
                 func: Function that takes the current value and returns a result

             Returns:
                 Result of the function

             Examples:
                 >>> safe_list = SafeType([1, 2, 3, 4, 5])
                 >>> length = safe_list.read(lambda x: len(x))
                 >>> print(length)  # 5
                 >>>
                 >>> # Complex read operation
                 >>> sum_even = safe_list.read(lambda x: sum(i for i in x if i % 2 == 0))
                 >>> print(sum_even)  # 6 (2 + 4)
             )pbdoc")
        .def(
            "swap",
            [](atom::async::SafeType<py::object>& self,
               atom::async::SafeType<py::object>& other) { self.swap(other); },
            py::arg("other"),
            R"pbdoc(
             Swap values with another SafeType (thread-safe).

             Args:
                 other: Another SafeType to swap values with

             Examples:
                 >>> safe1 = SafeType("hello")
                 >>> safe2 = SafeType("world")
                 >>> safe1.swap(safe2)
                 >>> print(safe1.get())  # "world"
                 >>> print(safe2.get())  # "hello"
             )pbdoc");

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
                    items.size());                  // Initialize with capacity
            for (py::handle item_handle : items) {  // item_handle is py::handle
                vec->pushBack(
                    item_handle.cast<py::object>());  // Cast to py::object
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
             auto list_ptr =  // Renamed to avoid conflict with std::list
                 std::make_shared<atom::async::LockFreeList<py::object>>();
             // Add items in reverse order to maintain original list order when
             // pushing to front
             for (ssize_t i = static_cast<ssize_t>(items.size()) - 1; i >= 0;
                  --i) {
                 list_ptr->pushFront(
                     items[i].cast<py::object>());  // items[i] is py::handle
             }
             return list_ptr;
         },
         py::arg("items") = py::list(),
         R"(Create a new LockFreeList with initial elements.

Args:
    items: Initial items to add to the list (optional).

Returns:
    A new LockFreeList containing the provided items.

Examples:
    >>> from atom.async import create_lock_free_list
    >>> lst = create_lock_free_list(["item1", "item2", "item3"]) # Becomes item3 -> item2 -> item1
    >>> lst.size()
    3
    >>> lst.front() # Should be item3
)")

        .def(
            "create_safe_type",
            [](py::object initial_value) {
                return std::make_shared<atom::async::SafeType<py::object>>(
                    initial_value);
            },
            py::arg("initial_value") = py::none(),
            R"pbdoc(
        Create a new SafeType with an initial value.

        Args:
            initial_value: Initial value to store (default: None)

        Returns:
            A new SafeType instance

        Examples:
            >>> from atom.async.safetype import create_safe_type
            >>> safe_counter = create_safe_type(0)
            >>> safe_list = create_safe_type([1, 2, 3])
        )pbdoc");

    // Utility functions for performance testing and benchmarking
    m.def(
         "benchmark_lock_free_stack",
         [](size_t num_operations, size_t num_threads) -> py::dict {
             using namespace std::chrono;

             py::dict results;
             auto stack =
                 std::make_shared<atom::async::LockFreeStack<py::object>>();

             // Benchmark push operations
             auto start = high_resolution_clock::now();

             for (size_t i = 0; i < num_operations; ++i) {
                 stack->push(py::cast(i));
             }

             auto end = high_resolution_clock::now();
             auto push_duration = duration_cast<microseconds>(end - start);

             // Benchmark pop operations
             start = high_resolution_clock::now();
             size_t successful_pops = 0;

             for (size_t i = 0; i < num_operations; ++i) {
                 auto result = stack->pop();
                 if (result.has_value()) {
                     successful_pops++;
                 }
             }

             end = high_resolution_clock::now();
             auto pop_duration = duration_cast<microseconds>(end - start);

             // Calculate statistics
             double push_ops_per_second =
                 (num_operations * 1000000.0) / push_duration.count();
             double pop_ops_per_second =
                 (num_operations * 1000000.0) / pop_duration.count();

             results[py::str("num_operations")] = num_operations;
             results[py::str("num_threads")] = num_threads;
             results[py::str("push_time_us")] = push_duration.count();
             results[py::str("pop_time_us")] = pop_duration.count();
             results[py::str("push_ops_per_second")] = push_ops_per_second;
             results[py::str("pop_ops_per_second")] = pop_ops_per_second;
             results[py::str("successful_pops")] = successful_pops;
             results[py::str("final_stack_size")] = stack->size();

             return results;
         },
         py::arg("num_operations") = 10000, py::arg("num_threads") = 1,
         R"pbdoc(
        Benchmark lock-free stack performance.

        Args:
            num_operations: Number of push/pop operations to perform (default: 10,000)
            num_threads: Number of threads to simulate (default: 1)

        Returns:
            dict: Benchmark results with timing and throughput metrics

        Examples:
            >>> results = benchmark_lock_free_stack(5000, 4)
            >>> print(f"Push ops/sec: {results['push_ops_per_second']:.2f}")
            >>> print(f"Pop ops/sec: {results['pop_ops_per_second']:.2f}")
        )pbdoc")

        .def(
            "benchmark_safe_type",
            [](size_t num_operations) -> py::dict {
                using namespace std::chrono;

                py::dict results;
                atom::async::SafeType<py::object> safe_value(py::cast(0));

                // Benchmark read operations
                auto start = high_resolution_clock::now();

                for (size_t i = 0; i < num_operations; ++i) {
                    volatile auto value = safe_value.get();
                    (void)value;  // Suppress unused variable warning
                }

                auto end = high_resolution_clock::now();
                auto read_duration = duration_cast<microseconds>(end - start);

                // Benchmark write operations
                start = high_resolution_clock::now();

                for (size_t i = 0; i < num_operations; ++i) {
                    safe_value.set(py::cast(i));
                }

                end = high_resolution_clock::now();
                auto write_duration = duration_cast<microseconds>(end - start);

                // Benchmark modify operations
                start = high_resolution_clock::now();

                for (size_t i = 0; i < num_operations; ++i) {
                    safe_value.modify([](py::object& value) {
                        auto int_val = value.cast<int>();
                        value = py::cast(int_val + 1);
                    });
                }

                end = high_resolution_clock::now();
                auto modify_duration = duration_cast<microseconds>(end - start);

                // Calculate statistics
                double read_ops_per_second =
                    (num_operations * 1000000.0) / read_duration.count();
                double write_ops_per_second =
                    (num_operations * 1000000.0) / write_duration.count();
                double modify_ops_per_second =
                    (num_operations * 1000000.0) / modify_duration.count();

                results[py::str("num_operations")] = num_operations;
                results[py::str("read_time_us")] = read_duration.count();
                results[py::str("write_time_us")] = write_duration.count();
                results[py::str("modify_time_us")] = modify_duration.count();
                results[py::str("read_ops_per_second")] = read_ops_per_second;
                results[py::str("write_ops_per_second")] = write_ops_per_second;
                results[py::str("modify_ops_per_second")] =
                    modify_ops_per_second;
                results[py::str("final_value")] = safe_value.get();

                return results;
            },
            py::arg("num_operations") = 10000,
            R"pbdoc(
        Benchmark SafeType performance for read, write, and modify operations.

        Args:
            num_operations: Number of operations to perform (default: 10,000)

        Returns:
            dict: Benchmark results with timing and throughput metrics

        Examples:
            >>> results = benchmark_safe_type(5000)
            >>> print(f"Read ops/sec: {results['read_ops_per_second']:.2f}")
            >>> print(f"Write ops/sec: {results['write_ops_per_second']:.2f}")
            >>> print(f"Modify ops/sec: {results['modify_ops_per_second']:.2f}")
        )pbdoc");

    // Add version and feature information
    m.attr("__version__") = "1.0.0";

    // Feature detection
    m.attr("HAS_LOCK_FREE_SUPPORT") = true;
    m.attr("HAS_SHARED_MUTEX") = true;

#ifdef ATOM_USE_BOOST_LOCKFREE
    m.attr("HAS_BOOST_LOCKFREE") = true;
#else
    m.attr("HAS_BOOST_LOCKFREE") = false;
#endif

    // Platform information
#ifdef ATOM_PLATFORM_WINDOWS
    m.attr("PLATFORM") = "Windows";
#elif defined(ATOM_PLATFORM_APPLE)
    m.attr("PLATFORM") = "macOS";
#elif defined(ATOM_PLATFORM_LINUX)
    m.attr("PLATFORM") = "Linux";
#else
    m.attr("PLATFORM") = "Unknown";
#endif
}
