#include "atom/async/messaging/eventstack.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Template for declaring EventStack with different types
template <typename T>
void declare_event_stack(py::module& m, const std::string& type_name) {
    using namespace atom::async;
    using EventStackType = EventStack<T>;

    std::string class_name = "EventStack" + type_name;

    py::class_<EventStackType>(m, class_name.c_str(),
                               R"pbdoc(
        A thread-safe stack data structure for managing events.

        This class provides methods for efficient event storage with thread-safety,
        filtering, querying, and transformations. The stack follows LIFO (Last In, First Out)
        semantics, where the most recently added event is the first to be removed.

        Args:
            None (default constructor)

        Examples:
            >>> from atom.async.eventstack import EventStackInt
            >>>
            >>> # Create an event stack
            >>> stack = EventStackInt()
            >>>
            >>> # Add some events
            >>> stack.push_event(42)
            >>> stack.push_event(100)
            >>> stack.push_event(7)
            >>>
            >>> # Access the top event without removing it
            >>> top = stack.peek_top_event()
            >>> print(top)  # Output: 7
            >>>
            >>> # Pop an event
            >>> event = stack.pop_event()
            >>> print(event)  # Output: 7
            >>>
            >>> # Check size
            >>> print(len(stack))  # Output: 2
        )pbdoc")
        .def(py::init<>())

        // Copy and move constructors
        .def(py::init<const EventStackType&>(), py::arg("other"),
             "Copy constructor - creates a new stack from an existing one")
        /*
       .def(py::init<EventStackType&&>(), py::arg("other"),
        "Move constructor - creates a new stack by taking ownership of "
        "another stack")
        */

        // Core stack operations
        .def("push_event", &EventStackType::pushEvent, py::arg("event"),
             R"pbdoc(
             Pushes an event onto the stack.

             Args:
                 event: The event to push onto the stack

             Raises:
                 RuntimeError: If memory allocation fails
             )pbdoc")
        .def("pop_event", &EventStackType::popEvent,
             R"pbdoc(
             Pops an event from the stack.

             Returns:
                 The popped event, or None if the stack is empty
             )pbdoc")
        .def("peek_top_event", &EventStackType::peekTopEvent,
             R"pbdoc(
             Returns the top event in the stack without removing it.

             Returns:
                 The top event, or None if the stack is empty

             Raises:
                 RuntimeError: If the stack is empty and exceptions are enabled
             )pbdoc")

        // Stack information
        .def("is_empty", &EventStackType::isEmpty,
             "Checks if the stack is empty")
        .def("size", &EventStackType::size,
             "Returns the number of events in the stack")
        .def("clear_events", &EventStackType::clearEvents,
             "Clears all events from the stack")
        .def("copy_stack", &EventStackType::copyStack,
             "Creates and returns a copy of the stack")

        // Filtering and searching
        .def(
            "filter_events",
            [](EventStackType& self, py::function predicate) {
                self.filterEvents([predicate](const T& event) -> bool {
                    py::gil_scoped_acquire acquire;
                    return predicate(event).template cast<bool>();
                });
            },
            py::arg("predicate"),
            R"pbdoc(
             Filters events based on a custom filter function.

             Args:
                 predicate: A function that takes an event and returns a boolean.
                            Events are kept if the function returns True.

             Examples:
                 >>> # Keep only events greater than 50
                 >>> stack.filter_events(lambda event: event > 50)
             )pbdoc")
        .def(
            "find_event",
            [](const EventStackType& self, py::function predicate) {
                return self.findEvent([predicate](const T& event) -> bool {
                    py::gil_scoped_acquire acquire;
                    return predicate(event).template cast<bool>();
                });
            },
            py::arg("predicate"),
            R"pbdoc(
             Finds the first event that satisfies a predicate.

             Args:
                 predicate: A function that takes an event and returns a boolean

             Returns:
                 The first event satisfying the predicate, or None if not found

             Examples:
                 >>> # Find first event divisible by 10
                 >>> event = stack.find_event(lambda e: e % 10 == 0)
             )pbdoc")

        // Statistics and checks
        .def(
            "count_events",
            [](const EventStackType& self, py::function predicate) {
                return self.countEvents([predicate](const T& event) -> bool {
                    py::gil_scoped_acquire acquire;
                    return predicate(event).template cast<bool>();
                });
            },
            py::arg("predicate"),
            R"pbdoc(
             Counts the number of events that satisfy a predicate.

             Args:
                 predicate: A function that takes an event and returns a boolean

             Returns:
                 The count of events satisfying the predicate

             Examples:
                 >>> # Count events less than 100
                 >>> count = stack.count_events(lambda e: e < 100)
             )pbdoc")
        .def(
            "any_event",
            [](const EventStackType& self, py::function predicate) {
                return self.anyEvent([predicate](const T& event) -> bool {
                    py::gil_scoped_acquire acquire;
                    return predicate(event).template cast<bool>();
                });
            },
            py::arg("predicate"),
            R"pbdoc(
             Checks if any event in the stack satisfies a predicate.

             Args:
                 predicate: A function that takes an event and returns a boolean

             Returns:
                 True if any event satisfies the predicate, False otherwise
             )pbdoc")
        .def(
            "all_events",
            [](const EventStackType& self, py::function predicate) {
                return self.allEvents([predicate](const T& event) -> bool {
                    py::gil_scoped_acquire acquire;
                    return predicate(event).template cast<bool>();
                });
            },
            py::arg("predicate"),
            R"pbdoc(
             Checks if all events in the stack satisfy a predicate.

             Args:
                 predicate: A function that takes an event and returns a boolean

             Returns:
                 True if all events satisfy the predicate, False otherwise
             )pbdoc")

        // Transformations
        .def(
            "transform_events",
            [](EventStackType& self, py::function transform_func) {
                self.transformEvents([transform_func](T& event) {
                    py::gil_scoped_acquire acquire;
                    py::object result = transform_func(event);
                    if (!result.is_none()) {
                        event = result.cast<T>();
                    }
                });
            },
            py::arg("transform_func"),
            R"pbdoc(
             Transforms events using the provided function.

             Args:
                 transform_func: A function that takes an event and returns a new event or None

             Examples:
                 >>> # Double all event values
                 >>> stack.transform_events(lambda e: e * 2)
             )pbdoc")
        .def(
            "sort_events",
            [](EventStackType& self, py::function compare_func) {
                self.sortEvents([compare_func](const T& a, const T& b) -> bool {
                    py::gil_scoped_acquire acquire;
                    return compare_func(a, b).template cast<bool>();
                });
            },
            py::arg("compare_func"),
            R"pbdoc(
             Sorts the events in the stack based on a custom comparison function.

             Args:
                 compare_func: A function that takes two events and returns a boolean.
                               Returns true if the first argument should be placed before the second.

             Examples:
                 >>> # Sort in descending order
                 >>> stack.sort_events(lambda a, b: a > b)
             )pbdoc")
        .def("reverse_events", &EventStackType::reverseEvents,
             "Reverses the order of events in the stack")

        // Utilities
        .def(
            "for_each",
            [](const EventStackType& self, py::function func) {
                self.forEach([func](const T& event) {
                    py::gil_scoped_acquire acquire;
                    func(event);
                });
            },
            py::arg("func"),
            R"pbdoc(
             Applies a function to each event in the stack.

             Args:
                 func: A function to apply to each event

             Examples:
                 >>> # Print each event
                 >>> stack.for_each(lambda e: print(e))
             )pbdoc")

        // Type-specific operations
        .def("remove_duplicates", &EventStackType::removeDuplicates,
             "Removes duplicate events from the stack")

        // Serialization support (for string-compatible types)
        .def(
            "serialize_stack",
            [](const EventStackType& self) -> py::str {
                if constexpr (std::is_same_v<T, std::string> ||
                              std::is_same_v<T, int> ||
                              std::is_same_v<T, float> ||
                              std::is_same_v<T, double>) {
                    return self.serializeStack();
                } else {
                    throw py::type_error(
                        "Serialization not supported for this type");
                }
            },
            R"pbdoc(
             Serializes the stack into a string.

             Returns:
                 The serialized stack as a string

             Raises:
                 TypeError: If serialization is not supported for this type
                 RuntimeError: If serialization fails

             Examples:
                 >>> serialized = stack.serialize_stack()
                 >>> print(serialized)
             )pbdoc")
        .def(
            "deserialize_stack",
            [](EventStackType& self, const std::string& serialized_data) {
                if constexpr (std::is_same_v<T, std::string> ||
                              std::is_same_v<T, int> ||
                              std::is_same_v<T, float> ||
                              std::is_same_v<T, double>) {
                    self.deserializeStack(serialized_data);
                } else {
                    throw py::type_error(
                        "Deserialization not supported for this type");
                }
            },
            py::arg("serialized_data"),
            R"pbdoc(
             Deserializes a string into the stack.

             Args:
                 serialized_data: The serialized stack data

             Raises:
                 TypeError: If deserialization is not supported for this type
                 RuntimeError: If deserialization fails

             Examples:
                 >>> stack.deserialize_stack(serialized_data)
             )pbdoc")

        // Advanced view operations
        .def(
            "get_events_view",
            [](const EventStackType& self) -> py::list {
                auto view = self.getEventsView();
                py::list result;
                for (const auto& event : view) {
                    result.append(event);
                }
                return result;
            },
            R"pbdoc(
             Returns a view of all events in the stack.

             Returns:
                 A list containing all events in the stack

             Examples:
                 >>> events = stack.get_events_view()
                 >>> print(f"All events: {events}")
             )pbdoc")

        // Python-specific methods
        .def("__len__", &EventStackType::size)
        .def("__bool__",
             [](const EventStackType& self) { return !self.isEmpty(); })
        .def("__iter__",
             [](const EventStackType& self) {
                 auto stack_copy = self.copyStack();
                 auto stack_data = stack_copy.getEventsView();
                 return py::make_iterator(
                     stack_data.begin(), stack_data.end(),
                     py::return_value_policy::reference_internal);
             })
        .def("__repr__",
             [](const EventStackType& self) {
                 return "<EventStack size=" + std::to_string(self.size()) + ">";
             })
        .def("__str__", [](const EventStackType& self) {
            return "EventStack(size=" + std::to_string(self.size()) + ")";
        });
}

PYBIND11_MODULE(eventstack, m) {
    m.doc() = R"pbdoc(
        Thread-Safe Event Stack Module
        -----------------------------

        This module provides a thread-safe stack data structure for managing events
        with support for various filtering, transformation, and querying operations.

        Features:
          - Thread-safe event storage with LIFO (Last In, First Out) semantics
          - Atomic operations for push, pop, and peek
          - Event filtering and transformation
          - Search functionality
          - Statistical queries
          - Support for various data types

        The module includes implementations for common data types:
          - EventStackInt: For integer events
          - EventStackFloat: For floating-point events
          - EventStackString: For string events
          - EventStackBool: For boolean events

        Example:
            >>> from atom.async.eventstack import EventStackInt
            >>>
            >>> # Create an event stack
            >>> stack = EventStackInt()
            >>>
            >>> # Add events
            >>> for i in range(10):
            >>>     stack.push_event(i)
            >>>
            >>> # Check if any event satisfies a condition
            >>> has_even = stack.any_event(lambda e: e % 2 == 0)
            >>> print(f"Has even numbers: {has_even}")
            >>>
            >>> # Find an event
            >>> five = stack.find_event(lambda e: e == 5)
            >>> if five is not None:
            >>>     print(f"Found: {five}")
            >>>
            >>> # Filter events
            >>> stack.filter_events(lambda e: e > 5)
            >>> print(f"Events after filtering: {stack.size()}")
    )pbdoc";

    // Register exception translations
    py::register_exception<atom::async::EventStackException>(
        m, "EventStackException", PyExc_RuntimeError);
    py::register_exception<atom::async::EventStackEmptyException>(
        m, "EventStackEmptyException", PyExc_RuntimeError);
    py::register_exception<atom::async::EventStackSerializationException>(
        m, "EventStackSerializationException", PyExc_RuntimeError);

    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::async::EventStackEmptyException& e) {
            PyErr_SetString(PyExc_IndexError, e.what());
        } catch (const atom::async::EventStackSerializationException& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::out_of_range& e) {
            PyErr_SetString(PyExc_IndexError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Register EventStack with different types
    declare_event_stack<int>(m, "Int");
    declare_event_stack<float>(m, "Float");
    declare_event_stack<double>(m, "Double");
    declare_event_stack<std::string>(m, "String");
    declare_event_stack<bool>(m, "Bool");

    // Add utility functions for stack operations
    m.def(
         "merge_stacks",
         [](const atom::async::EventStack<int>& stack1,
            const atom::async::EventStack<int>& stack2) {
             auto merged = stack1.copyStack();
             auto stack2_copy = stack2.copyStack();

             // Pop all events from stack2_copy and push to merged
             while (!stack2_copy.isEmpty()) {
                 auto event = stack2_copy.popEvent();
                 if (event) {
                     merged.pushEvent(*event);
                 }
             }
             return merged;
         },
         py::arg("stack1"), py::arg("stack2"),
         R"pbdoc(
         Merge two integer event stacks.

         Args:
             stack1: First event stack
             stack2: Second event stack

         Returns:
             A new EventStack containing events from both stacks

         Examples:
             >>> merged = merge_stacks(stack1, stack2)
         )pbdoc")

        .def(
            "stack_statistics",
            [](const atom::async::EventStack<int>& stack) -> py::dict {
                py::dict stats;
                stats["size"] = stack.size();
                stats["is_empty"] = stack.isEmpty();

                if (!stack.isEmpty()) {
                    auto view = stack.getEventsView();
                    if (!view.empty()) {
                        int min_val =
                            *std::min_element(view.begin(), view.end());
                        int max_val =
                            *std::max_element(view.begin(), view.end());
                        double sum =
                            std::accumulate(view.begin(), view.end(), 0.0);
                        double mean = sum / view.size();

                        stats["min"] = min_val;
                        stats["max"] = max_val;
                        stats["sum"] = sum;
                        stats["mean"] = mean;
                    }
                }

                return stats;
            },
            py::arg("stack"),
            R"pbdoc(
         Calculate statistics for an integer event stack.

         Args:
             stack: The event stack to analyze

         Returns:
             A dictionary containing statistics (size, min, max, sum, mean)

         Examples:
             >>> stats = stack_statistics(stack)
             >>> print(f"Mean: {stats['mean']}")
         )pbdoc");

    // Utility function to create appropriate event stack based on input type
    m.def(
        "create_event_stack",
        [&m](py::object sample_event) -> py::object {
            if (py::isinstance<py::int_>(sample_event)) {
                return m.attr("EventStackInt")();
            } else if (py::isinstance<py::float_>(sample_event)) {
                return m.attr("EventStackFloat")();
            } else if (py::isinstance<py::str>(sample_event)) {
                return m.attr("EventStackString")();
            } else if (py::isinstance<py::bool_>(sample_event)) {
                return m.attr("EventStackBool")();
            } else {
                throw py::type_error(
                    "Unsupported event type. Use int, float, str, or bool.");
            }
        },
        py::arg("sample_event"),
        R"pbdoc(
          Factory function to create an appropriate EventStack based on the input type.

          Args:
              sample_event: An example event of the type you want to store
                            (used only to determine the type)

          Returns:
              A new EventStack of the appropriate type

          Raises:
              TypeError: If the event type is not supported

          Examples:
              >>> # Create an integer event stack
              >>> int_stack = create_event_stack(42)
              >>>
              >>> # Create a string event stack
              >>> str_stack = create_event_stack("hello")
          )pbdoc");

    // Add version information
    m.attr("__version__") = "1.0.0";

    // Add feature detection
#if HAS_EXECUTION_HEADER
    m.attr("PARALLEL_EXECUTION_SUPPORTED") = true;
#else
    m.attr("PARALLEL_EXECUTION_SUPPORTED") = false;
#endif

#if ATOM_ASYNC_USE_LOCKFREE
    m.attr("LOCKFREE_STACK_ENABLED") = true;
#else
    m.attr("LOCKFREE_STACK_ENABLED") = false;
#endif

    // Add debugging support detection
#if ENABLE_DEBUG
    m.attr("DEBUG_ENABLED") = true;
#else
    m.attr("DEBUG_ENABLED") = false;
#endif

    // Add performance benchmarking utilities
    m.def(
         "benchmark_stack_operations",
         [](size_t num_operations) -> py::dict {
             using namespace std::chrono;
             atom::async::EventStack<int> stack;

             py::dict results;

             // Benchmark push operations
             auto start = high_resolution_clock::now();
             for (size_t i = 0; i < num_operations; ++i) {
                 stack.pushEvent(static_cast<int>(i));
             }
             auto end = high_resolution_clock::now();
             auto push_duration = duration_cast<microseconds>(end - start);
             results["push_time_us"] = push_duration.count();
             results["push_ops_per_sec"] =
                 (num_operations * 1000000.0) / push_duration.count();

             // Benchmark pop operations
             start = high_resolution_clock::now();
             for (size_t i = 0; i < num_operations; ++i) {
                 stack.popEvent();
             }
             end = high_resolution_clock::now();
             auto pop_duration = duration_cast<microseconds>(end - start);
             results["pop_time_us"] = pop_duration.count();
             results["pop_ops_per_sec"] =
                 (num_operations * 1000000.0) / pop_duration.count();

             results["total_operations"] = num_operations;
             return results;
         },
         py::arg("num_operations") = 10000,
         R"pbdoc(
         Benchmark stack operations performance.

         Args:
             num_operations: Number of operations to perform for benchmarking

         Returns:
             A dictionary containing performance metrics

         Examples:
             >>> results = benchmark_stack_operations(100000)
             >>> print(f"Push ops/sec: {results['push_ops_per_sec']}")
         )pbdoc")

        .def(
            "create_test_stack",
            [](const std::string& stack_type, size_t size) -> py::object {
                if (stack_type == "int") {
                    auto stack = atom::async::EventStack<int>();
                    for (size_t i = 0; i < size; ++i) {
                        stack.pushEvent(static_cast<int>(i));
                    }
                    return py::cast(stack);
                } else if (stack_type == "string") {
                    auto stack = atom::async::EventStack<std::string>();
                    for (size_t i = 0; i < size; ++i) {
                        stack.pushEvent("event_" + std::to_string(i));
                    }
                    return py::cast(stack);
                } else {
                    throw py::value_error(
                        "Unsupported stack type. Use 'int' or 'string'.");
                }
            },
            py::arg("stack_type"), py::arg("size") = 10,
            R"pbdoc(
         Create a test stack with predefined data.

         Args:
             stack_type: Type of stack to create ("int" or "string")
             size: Number of test events to add

         Returns:
             A new EventStack with test data

         Examples:
             >>> test_stack = create_test_stack("int", 100)
             >>> print(f"Test stack size: {test_stack.size()}")
         )pbdoc");
}
