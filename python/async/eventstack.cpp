#include "atom/async/eventstack.hpp"

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
        .def(py::init<EventStackType&&>(), py::arg("other"),
             "Move constructor - creates a new stack by taking ownership of "
             "another stack")

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
                    return predicate(event).cast<bool>();
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
                    return predicate(event).cast<bool>();
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
                    return predicate(event).cast<bool>();
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
                    return predicate(event).cast<bool>();
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
                    return predicate(event).cast<bool>();
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
                    return compare_func(a, b).cast<bool>();
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

        // Python-specific methods
        .def("__len__", &EventStackType::size)
        .def("__bool__",
             [](const EventStackType& self) { return !self.isEmpty(); })
        .def("__iter__", [](const EventStackType& self) {
            auto stack_copy = self.copyStack();
            return py::make_iterator(stack_copy);
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
        } catch (const atom::async::EventStackException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
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

// Add information about parallel execution support
#if HAS_EXECUTION_HEADER
    m.attr("PARALLEL_EXECUTION_SUPPORTED") = true;
#else
    m.attr("PARALLEL_EXECUTION_SUPPORTED") = false;
#endif
}