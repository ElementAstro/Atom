#include "atom/async/queue.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(queue, m) {
    m.doc() = "Thread-safe queue implementation module for the atom package";

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

    // Thread-safe queue class binding
    py::class_<atom::async::ThreadSafeQueue<py::object>>(
        m, "ThreadSafeQueue",
        R"(Thread-safe queue implementation for multi-threaded operations.

This class provides methods to safely manage a queue across multiple threads
with blocking and non-blocking operations, batch processing, and various
utility functions.

Examples:
    >>> from atom.async import ThreadSafeQueue
    >>> queue = ThreadSafeQueue()
    >>> queue.put("item1")
    >>> queue.put("item2")
    >>> queue.size()
    2
    >>> item = queue.take()
    >>> print(item)
    item1
)")
        .def(py::init<>(), "Creates a new empty ThreadSafeQueue.")
        .def(
            "put",
            [](atom::async::ThreadSafeQueue<py::object>& self,
               py::object element) { self.put(std::move(element)); },
            py::arg("element"),
            R"(Add an element to the queue.

Args:
    element: The element to be added to the queue.
)")
        .def(
            "take",
            [](atom::async::ThreadSafeQueue<py::object>& self) {
                auto result = self.take();
                if (result) {
                    return *result;
                }
                throw py::value_error("Queue is empty or being destroyed");
            },
            R"(Take an element from the queue, blocking until one is available.

Returns:
    The next element from the queue.

Raises:
    ValueError: If the queue is being destroyed.
)")
        .def(
            "try_take",
            [](atom::async::ThreadSafeQueue<py::object>& self) {
                auto result = self.tryTake();
                if (result) {
                    return *result;
                }
                throw py::value_error("Queue is empty");
            },
            R"(Try to take an element from the queue without waiting.

Returns:
    The next element from the queue.

Raises:
    ValueError: If the queue is empty.
)")
        .def(
            "take_for",
            [](atom::async::ThreadSafeQueue<py::object>& self,
               const std::chrono::duration<long, std::ratio<1>>& timeout) {
                auto result = self.takeFor(timeout);
                if (result) {
                    return *result;
                }
                throw py::value_error("Queue is empty or timeout expired");
            },
            py::arg("timeout"),
            R"(Try to take an element from the queue, waiting up to the specified timeout.

Args:
    timeout: Maximum time to wait in seconds.

Returns:
    The next element from the queue.

Raises:
    ValueError: If the timeout expires or the queue is empty.
)")
        .def("size", &atom::async::ThreadSafeQueue<py::object>::size,
             R"(Get the current size of the queue.

Returns:
    The number of elements currently in the queue.
)")
        .def("empty", &atom::async::ThreadSafeQueue<py::object>::empty,
             R"(Check if the queue is empty.

Returns:
    True if the queue is empty, False otherwise.
)")
        .def("clear", &atom::async::ThreadSafeQueue<py::object>::clear,
             R"(Clear all elements from the queue.)")
        .def(
            "front",
            [](atom::async::ThreadSafeQueue<py::object>& self) {
                auto result = self.front();
                if (result) {
                    return *result;
                }
                throw py::value_error("Queue is empty");
            },
            R"(Get the front element without removing it.

Returns:
    The front element.

Raises:
    ValueError: If the queue is empty.
)")
        .def(
            "back",
            [](atom::async::ThreadSafeQueue<py::object>& self) {
                auto result = self.back();
                if (result) {
                    return *result;
                }
                throw py::value_error("Queue is empty");
            },
            R"(Get the back element without removing it.

Returns:
    The back element.

Raises:
    ValueError: If the queue is empty.
)")
        .def("wait_until_empty",
             &atom::async::ThreadSafeQueue<py::object>::waitUntilEmpty,
             R"(Wait until the queue becomes empty.)")
        .def(
            "to_vector",
            [](atom::async::ThreadSafeQueue<py::object>& self) {
                return self.toVector();
            },
            R"(Convert queue contents to a list.

Returns:
    A list containing copies of all elements in the queue.
)")
        .def(
            "emplace",
            [](atom::async::ThreadSafeQueue<py::object>& self,
               py::object element) { self.emplace(std::move(element)); },
            py::arg("element"),
            R"(Construct an element in-place in the queue.

Args:
    element: The element to construct in the queue.
)")
        .def(
            "for_each",
            [](atom::async::ThreadSafeQueue<py::object>& self,
               py::function func, bool parallel = false) {
                self.forEach([func](py::object& obj) { func(obj); }, parallel);
            },
            py::arg("func"), py::arg("parallel") = false,
            R"(Apply a function to each element in the queue.

Args:
    func: The function to apply to each element.
    parallel: Whether to process in parallel for large queues (default: False).
)")
        .def(
            "filter",
            [](atom::async::ThreadSafeQueue<py::object>& self,
               py::function predicate) {
                self.filter([predicate](const py::object& obj) {
                    return predicate(obj).cast<bool>();
                });
            },
            py::arg("predicate"),
            R"(Filter the queue elements.

Args:
    predicate: A function that returns True for elements to keep and False for elements to discard.
)")
        .def(
            "wait_for",
            [](atom::async::ThreadSafeQueue<py::object>& self,
               py::function predicate) {
                auto result = self.waitFor([predicate](const py::object& obj) {
                    return predicate(obj).cast<bool>();
                });

                if (result) {
                    return *result;
                }
                throw py::value_error("Queue is being destroyed");
            },
            py::arg("predicate"),
            R"(Wait for an element that satisfies a predicate.

Args:
    predicate: A function that returns True when the condition is met.

Returns:
    The first element that satisfies the predicate.

Raises:
    ValueError: If the queue is being destroyed.
)")
        /*
        .def(
        "extract_if",
        [](atom::async::ThreadSafeQueue<py::object>& self,
           py::function predicate) {
            // Create a C++ compatible predicate function
            auto cpp_predicate =
                [predicate](const py::object& obj) -> bool {
                return predicate(obj).cast<bool>();
            };

            // Call the extractIf method with the wrapper predicate
            return self.extractIf(cpp_predicate);
        },
        py::arg("predicate"),
        R"(Extract elements that satisfy a predicate.

Args:
predicate: A function that returns True for elements to extract.

Returns:
A list of extracted elements.
)")
        */

        .def(
            "sort",
            [](atom::async::ThreadSafeQueue<py::object>& self,
               py::function comp) {
                self.sort([comp](const py::object& a, const py::object& b) {
                    return comp(a, b).cast<bool>();
                });
            },
            py::arg("comp"),
            R"(Sort the elements in the queue.

Args:
    comp: A comparison function that returns True if the first argument should come before the second.
)")
        .def(
            "transform",
            [](atom::async::ThreadSafeQueue<py::object>& self,
               py::function func) {
                return self.transform<py::object>(
                    [func](py::object obj) { return func(obj); });
            },
            py::arg("func"),
            R"(Transform elements using a function and return a new queue.

Args:
    func: A transformation function to apply to each element.

Returns:
    A new ThreadSafeQueue containing the transformed elements.
)")
        .def(
            "group_by",
            [](atom::async::ThreadSafeQueue<py::object>& self,
               py::function key_func) {
                return self.groupBy<py::object>(
                    [key_func](const py::object& obj) {
                        return key_func(obj);
                    });
            },
            py::arg("key_func"),
            R"(Group elements by a key.

Args:
    key_func: A function to extract the key from each element.

Returns:
    A list of ThreadSafeQueue objects, each containing elements with the same key.
)")
        .def(
            "process_batches",
            [](atom::async::ThreadSafeQueue<py::object>& self,
               size_t batch_size, py::function processor) {
                return self.processBatches(
                    batch_size, [processor](std::span<py::object> batch) {
                        // Convert span to list for Python
                        py::list batch_list;
                        for (auto& item : batch) {
                            batch_list.append(item);
                        }
                        processor(batch_list);
                    });
            },
            py::arg("batch_size"), py::arg("processor"),
            R"(Process batches of items in parallel.

Args:
    batch_size: Size of each batch.
    processor: Function to process each batch.

Returns:
    Number of processed batches.
)")
        // Python-specific methods
        .def("__len__", &atom::async::ThreadSafeQueue<py::object>::size,
             "Support for len() function.")
        .def(
            "__bool__",
            [](const atom::async::ThreadSafeQueue<py::object>& q) {
                return !q.empty();
            },
            "Return True if the queue is not empty, False otherwise.")
        .def(
            "__iter__",
            [](py::object self) { return py::iter(self.attr("to_vector")()); },
            "Support for iteration.");

    // Factory function to create a ThreadSafeQueue with initial elements
    m.def(
        "create_queue",
        [](const py::list& items) {
            auto queue =
                std::make_shared<atom::async::ThreadSafeQueue<py::object>>();
            for (const py::handle& item : items) {
                // Convert handle to object properly
                py::object obj = py::reinterpret_borrow<py::object>(item);
                queue->put(std::move(obj));
            }
            return queue;
        },
        py::arg("items") = py::list(),
        R"(Create a ThreadSafeQueue with initial elements.

Args:
    items: Initial items to add to the queue (optional).

Returns:
    A new ThreadSafeQueue containing the provided items.

Examples:
    >>> from atom.async import create_queue
    >>> queue = create_queue(["item1", "item2", "item3"])
    >>> queue.size()
    3
)");
}
