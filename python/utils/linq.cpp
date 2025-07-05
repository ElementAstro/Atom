#include "atom/utils/linq.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "pybind11_hash.hpp"  // 添加我们的哈希特化实现

namespace py = pybind11;

// Helper function to convert a Python iterable to a vector
template <typename T>
std::vector<T> py_iterable_to_vector(const py::iterable& iterable) {
    std::vector<T> result;
    for (const auto& item : iterable) {
        result.push_back(item.cast<T>());
    }
    return result;
}

template <typename T>
void declare_enumerable(py::module& m, const std::string& type_name) {
    std::string class_name = "Enumerable" + type_name;

    auto cls =
        py::class_<atom::utils::Enumerable<T>>(
            m, class_name.c_str(),
            R"(A LINQ-style utility class for sequence operations in Python.

This class provides methods to perform various operations on sequences similar
to .NET's LINQ or JavaScript's array methods. It enables method chaining for
transforming data through multiple operations.

Args:
    elements: A sequence of elements to operate on.

Examples:
    >>> from atom.utils import from_list
    >>> data = from_list([1, 2, 3, 4, 5])
    >>> result = data.where(lambda x: x > 2).select(lambda x: x * 2).to_list()
    >>> print(result)  # [6, 8, 10]
)")
            .def(py::init<const std::vector<T>&>(), py::arg("elements"));

    // ======== Filters and Reorders ========
    cls.def(
        "where",
        [](const atom::utils::Enumerable<T>& self, py::function predicate) {
            return self.where([predicate](const T& element) {
                return predicate(element).template cast<bool>();
            });
        },
        py::arg("predicate"),
        R"(Filters elements based on a predicate function.

Args:
    predicate: A function that takes an element and returns a boolean.

Returns:
    A new Enumerable containing only elements for which the predicate returns True.

Examples:
    >>> data.where(lambda x: x > 5)
)");

    cls.def(
        "where_with_index",
        [](const atom::utils::Enumerable<T>& self, py::function predicate) {
            return self.whereI([predicate](const T& element, size_t index) {
                return predicate(element, index).template cast<bool>();
            });
        },
        py::arg("predicate"),
        R"(Filters elements based on a predicate function that includes the element's index.

Args:
    predicate: A function that takes (element, index) and returns a boolean.

Returns:
    A new Enumerable containing only elements for which the predicate returns True.

Examples:
    >>> data.where_with_index(lambda x, i: x > i)
)");

    cls.def("take", &atom::utils::Enumerable<T>::take, py::arg("count"),
            R"(Takes the first n elements from the sequence.

Args:
    count: Number of elements to take from the beginning.

Returns:
    A new Enumerable containing at most 'count' elements.

Examples:
    >>> data.take(3)  # First 3 elements
)");

    cls.def(
        "take_while",
        [](const atom::utils::Enumerable<T>& self, py::function predicate) {
            return self.takeWhile([predicate](const T& element) {
                return predicate(element).template cast<bool>();
            });
        },
        py::arg("predicate"),
        R"(Takes elements from the beginning until the predicate returns False.

Args:
    predicate: A function that takes an element and returns a boolean.

Returns:
    A new Enumerable containing elements until the predicate fails.

Examples:
    >>> data.take_while(lambda x: x < 10)
)");

    cls.def(
        "take_while_with_index",
        [](const atom::utils::Enumerable<T>& self, py::function predicate) {
            return self.takeWhileI([predicate](const T& element, size_t index) {
                return predicate(element, index).template cast<bool>();
            });
        },
        py::arg("predicate"),
        R"(Takes elements until the predicate function that includes the element's index returns False.

Args:
    predicate: A function that takes (element, index) and returns a boolean.

Returns:
    A new Enumerable containing elements until the predicate fails.

Examples:
    >>> data.take_while_with_index(lambda x, i: x > i)
)");

    cls.def("skip", &atom::utils::Enumerable<T>::skip, py::arg("count"),
            R"(Skips the first n elements from the sequence.

Args:
    count: Number of elements to skip from the beginning.

Returns:
    A new Enumerable without the first 'count' elements.

Examples:
    >>> data.skip(2)  # All elements except the first 2
)");

    cls.def(
        "skip_while",
        [](const atom::utils::Enumerable<T>& self, py::function predicate) {
            return self.skipWhile([predicate](const T& element) {
                return predicate(element).template cast<bool>();
            });
        },
        py::arg("predicate"),
        R"(Skips elements from the beginning while the predicate returns True.

Args:
    predicate: A function that takes an element and returns a boolean.

Returns:
    A new Enumerable without elements until the predicate fails.

Examples:
    >>> data.skip_while(lambda x: x < 10)
)");

    cls.def(
        "skip_while_with_index",
        [](const atom::utils::Enumerable<T>& self, py::function predicate) {
            return self.skipWhileI([predicate](const T& element, size_t index) {
                return predicate(element, index).template cast<bool>();
            });
        },
        py::arg("predicate"),
        R"(Skips elements while the predicate function that includes the element's index returns True.

Args:
    predicate: A function that takes (element, index) and returns a boolean.

Returns:
    A new Enumerable without elements until the predicate fails.

Examples:
    >>> data.skip_while_with_index(lambda x, i: x < i)
)");

    cls.def(
        "order_by",
        [](const atom::utils::Enumerable<T>& self) { return self.orderBy(); },
        R"(Orders elements in ascending order.

Returns:
    A new Enumerable with elements ordered.

Examples:
    >>> data.order_by()
)");

    cls.def(
        "order_by",
        [](const atom::utils::Enumerable<T>& self, py::function key_selector) {
            return self.orderBy([key_selector](const T& element) {
                return key_selector(element);
            });
        },
        py::arg("key_selector"),
        R"(Orders elements by a key selector function.

Args:
    key_selector: A function that extracts a key from each element for sorting.

Returns:
    A new Enumerable with elements ordered by the key.

Examples:
    >>> data.order_by(lambda x: x.name)  # Order by name field
)");

    cls.def(
        "distinct",
        [](const atom::utils::Enumerable<T>& self) { return self.distinct(); },
        R"(Returns distinct elements from the sequence.

Returns:
    A new Enumerable with duplicate elements removed.

Examples:
    >>> data.distinct()
)");

    cls.def(
        "distinct",
        [](const atom::utils::Enumerable<T>& self, py::function key_selector) {
            return self.distinct([key_selector](const T& element) {
                return key_selector(element);
            });
        },
        py::arg("key_selector"),
        R"(Returns elements with distinct keys from the sequence.

Args:
    key_selector: A function that extracts a key from each element for comparison.

Returns:
    A new Enumerable with elements having unique keys.

Examples:
    >>> data.distinct(lambda x: x.id)  # Distinct by id field
)");

    cls.def("append", &atom::utils::Enumerable<T>::append, py::arg("items"),
            R"(Appends a collection to the end of the sequence.

Args:
    items: A collection of items to append.

Returns:
    A new Enumerable with additional elements at the end.

Examples:
    >>> data.append([6, 7, 8])
)");

    cls.def("prepend", &atom::utils::Enumerable<T>::prepend, py::arg("items"),
            R"(Prepends a collection to the beginning of the sequence.

Args:
    items: A collection of items to prepend.

Returns:
    A new Enumerable with additional elements at the beginning.

Examples:
    >>> data.prepend([0, -1, -2])
)");

    cls.def("concat", &atom::utils::Enumerable<T>::concat, py::arg("other"),
            R"(Concatenates another Enumerable to this sequence.

Args:
    other: Another Enumerable to concatenate.

Returns:
    A new Enumerable with elements from both sequences.

Examples:
    >>> seq1.concat(seq2)
)");

    cls.def("reverse", &atom::utils::Enumerable<T>::reverse,
            R"(Reverses the order of elements in the sequence.

Returns:
    A new Enumerable with elements in reverse order.

Examples:
    >>> data.reverse()
)");

    // Transformers
    cls.def(
        "select",
        [](const atom::utils::Enumerable<T>& self, py::function transformer) {
            return self.template select<py::object>(
                [transformer](const T& element) {
                    return transformer(element);
                });
        },
        py::arg("transformer"),
        R"(Projects each element to a new form using a transformer function.

Args:
    transformer: A function that transforms each element.

Returns:
    A new Enumerable with transformed elements.

Examples:
    >>> data.select(lambda x: x * 2)
)");

    cls.def(
        "select_with_index",
        [](const atom::utils::Enumerable<T>& self, py::function transformer) {
            return self.template selectI<py::object>(
                [transformer](const T& element, size_t index) {
                    return transformer(element, index);
                });
        },
        py::arg("transformer"),
        R"(Projects each element using a transformer function that includes the element's index.

Args:
    transformer: A function that takes (element, index) and transforms the element.

Returns:
    A new Enumerable with transformed elements.

Examples:
    >>> data.select_with_index(lambda x, i: x * i)
)");

    cls.def(
        "group_by",
        [](const atom::utils::Enumerable<T>& self, py::function key_selector) {
            return self.template groupBy<py::object>(
                [key_selector](const T& element) {
                    return key_selector(element);
                });
        },
        py::arg("key_selector"),
        R"(Groups elements by a key selector function.

Args:
    key_selector: A function that extracts a key from each element for grouping.

Returns:
    A new Enumerable containing the group keys.

Examples:
    >>> data.group_by(lambda x: x.category)
)");

    cls.def(
        "select_many",
        [](const atom::utils::Enumerable<T>& self,
           py::function collection_selector) {
            return self.template selectMany<py::object>(
                [collection_selector](const T& element) {
                    return py_iterable_to_vector<py::object>(
                        collection_selector(element));
                });
        },
        py::arg("collection_selector"),
        R"(Projects each element to a sequence and flattens the resulting sequences.

Args:
    collection_selector: A function that returns a collection for each element.

Returns:
    A new Enumerable with flattened elements.

Examples:
    >>> data.select_many(lambda x: [x, x+1, x+2])
)");

    // Aggregators
    cls.def(
        "all",
        [](const atom::utils::Enumerable<T>& self, py::function predicate) {
            return self.all([predicate](const T& element) {
                return predicate(element).template cast<bool>();
            });
        },
        py::arg("predicate") = py::cpp_function([](const T&) { return true; }),
        R"(Determines whether all elements satisfy a condition.

Args:
    predicate: A function that tests each element. Default checks if all elements exist.

Returns:
    True if all elements pass the test, False otherwise.

Examples:
    >>> data.all(lambda x: x > 0)  # Check if all elements are positive
)");

    cls.def(
        "any",
        [](const atom::utils::Enumerable<T>& self, py::function predicate) {
            return self.any([predicate](const T& element) {
                return predicate(element).template cast<bool>();
            });
        },
        py::arg("predicate") = py::cpp_function([](const T&) { return true; }),
        R"(Determines whether any element satisfies a condition.

Args:
    predicate: A function that tests each element. Default checks if any elements exist.

Returns:
    True if any element passes the test, False otherwise.

Examples:
    >>> data.any(lambda x: x < 0)  # Check if any elements are negative
)");

    cls.def(
        "sum",
        [](const atom::utils::Enumerable<T>& self) { return self.sum(); },
        R"(Computes the sum of the sequence elements.

Returns:
    The sum of all elements.

Examples:
    >>> data.sum()
)");

    cls.def(
        "sum",
        [](const atom::utils::Enumerable<T>& self, py::function selector) {
            return self.template sum<double>([selector](const T& element) {
                return selector(element).template cast<double>();
            });
        },
        py::arg("selector"),
        R"(Computes the sum of the sequence elements after applying a selector function.

Args:
    selector: A function to extract a numeric value from each element.

Returns:
    The sum of the selected values.

Examples:
    >>> data.sum(lambda x: x.value)  # Sum the 'value' field of each element
)");

    cls.def(
        "avg",
        [](const atom::utils::Enumerable<T>& self) { return self.avg(); },
        R"(Computes the average of the sequence elements.

Returns:
    The average of all elements.

Examples:
    >>> data.avg()
)");

    cls.def(
        "avg",
        [](const atom::utils::Enumerable<T>& self, py::function selector) {
            return self.template avg<double>([selector](const T& element) {
                return selector(element).template cast<double>();
            });
        },
        py::arg("selector"),
        R"(Computes the average of the sequence elements after applying a selector function.

Args:
    selector: A function to extract a numeric value from each element.

Returns:
    The average of the selected values.

Examples:
    >>> data.avg(lambda x: x.value)  # Average the 'value' field of each element
)");

    cls.def(
        "min",
        [](const atom::utils::Enumerable<T>& self) { return self.min(); },
        R"(Returns the minimum element in the sequence.

Returns:
    The minimum element.

Examples:
    >>> data.min()
)");

    cls.def(
        "min",
        [](const atom::utils::Enumerable<T>& self, py::function selector) {
            return self.min(
                [selector](const T& element) { return selector(element); });
        },
        py::arg("selector"),
        R"(Returns the element with the minimum value after applying a selector function.

Args:
    selector: A function to extract a comparable value from each element.

Returns:
    The element with the minimum selected value.

Examples:
    >>> data.min(lambda x: x.age)  # Find element with minimum age
)");

    cls.def(
        "max",
        [](const atom::utils::Enumerable<T>& self) { return self.max(); },
        R"(Returns the maximum element in the sequence.

Returns:
    The maximum element.

Examples:
    >>> data.max()
)");

    cls.def(
        "max",
        [](const atom::utils::Enumerable<T>& self, py::function selector) {
            return self.max(
                [selector](const T& element) { return selector(element); });
        },
        py::arg("selector"),
        R"(Returns the element with the maximum value after applying a selector function.

Args:
    selector: A function to extract a comparable value from each element.

Returns:
    The element with the maximum selected value.

Examples:
    >>> data.max(lambda x: x.score)  # Find element with maximum score
)");

    cls.def(
        "count",
        [](const atom::utils::Enumerable<T>& self) { return self.count(); },
        R"(Returns the number of elements in the sequence.

Returns:
    The count of elements.

Examples:
    >>> data.count()
)");

    cls.def(
        "count",
        [](const atom::utils::Enumerable<T>& self, py::function predicate) {
            return self.count([predicate](const T& element) {
                return predicate(element).template cast<bool>();
            });
        },
        py::arg("predicate"),
        R"(Returns the number of elements that satisfy a condition.

Args:
    predicate: A function that tests each element.

Returns:
    The count of elements that satisfy the condition.

Examples:
    >>> data.count(lambda x: x % 2 == 0)  # Count even numbers
)");

    cls.def("contains", &atom::utils::Enumerable<T>::contains, py::arg("value"),
            R"(Determines whether the sequence contains a specified element.

Args:
    value: The value to locate.

Returns:
    True if found, False otherwise.

Examples:
    >>> data.contains(42)
)");

    cls.def("element_at", &atom::utils::Enumerable<T>::elementAt,
            py::arg("index"),
            R"(Returns the element at a specified index.

Args:
    index: The zero-based index of the element to retrieve.

Returns:
    The element at the specified position.

Raises:
    IndexError: If the index is out of range.

Examples:
    >>> data.element_at(2)  # Get the third element
)");

    cls.def(
        "first",
        [](const atom::utils::Enumerable<T>& self) { return self.first(); },
        R"(Returns the first element of the sequence.

Returns:
    The first element.

Raises:
    RuntimeError: If the sequence is empty.

Examples:
    >>> data.first()
)");

    cls.def(
        "first",
        [](const atom::utils::Enumerable<T>& self, py::function predicate) {
            return self.first([predicate](const T& element) {
                return predicate(element).template cast<bool>();
            });
        },
        py::arg("predicate"),
        R"(Returns the first element that satisfies a condition.

Args:
    predicate: A function that tests each element.

Returns:
    The first element that satisfies the condition.

Raises:
    RuntimeError: If no element satisfies the condition.

Examples:
    >>> data.first(lambda x: x > 10)  # First element greater than 10
)");

    cls.def(
        "first_or_default",
        [](const atom::utils::Enumerable<T>& self) {
            return self.firstOrDefault();
        },
        R"(Returns the first element or None if the sequence is empty.

Returns:
    The first element or None.

Examples:
    >>> data.first_or_default()
)");

    cls.def(
        "first_or_default",
        [](const atom::utils::Enumerable<T>& self, py::function predicate) {
            return self.firstOrDefault([predicate](const T& element) {
                return predicate(element).template cast<bool>();
            });
        },
        py::arg("predicate"),
        R"(Returns the first element that satisfies a condition or None if no such element exists.

Args:
    predicate: A function that tests each element.

Returns:
    The first element that satisfies the condition or None.

Examples:
    >>> data.first_or_default(lambda x: x > 100)
)");

    cls.def(
        "last",
        [](const atom::utils::Enumerable<T>& self) { return self.last(); },
        R"(Returns the last element of the sequence.

Returns:
    The last element.

Raises:
    RuntimeError: If the sequence is empty.

Examples:
    >>> data.last()
)");

    cls.def(
        "last",
        [](const atom::utils::Enumerable<T>& self, py::function predicate) {
            return self.last([predicate](const T& element) {
                return predicate(element).template cast<bool>();
            });
        },
        py::arg("predicate"),
        R"(Returns the last element that satisfies a condition.

Args:
    predicate: A function that tests each element.

Returns:
    The last element that satisfies the condition.

Raises:
    RuntimeError: If no element satisfies the condition.

Examples:
    >>> data.last(lambda x: x < 20)  # Last element less than 20
)");

    cls.def(
        "last_or_default",
        [](const atom::utils::Enumerable<T>& self) {
            return self.lastOrDefault();
        },
        R"(Returns the last element or None if the sequence is empty.

Returns:
    The last element or None.

Examples:
    >>> data.last_or_default()
)");

    cls.def(
        "last_or_default",
        [](const atom::utils::Enumerable<T>& self, py::function predicate) {
            return self.lastOrDefault([predicate](const T& element) {
                return predicate(element).template cast<bool>();
            });
        },
        py::arg("predicate"),
        R"(Returns the last element that satisfies a condition or None if no such element exists.

Args:
    predicate: A function that tests each element.

Returns:
    The last element that satisfies the condition or None.

Examples:
    >>> data.last_or_default(lambda x: x.is_valid())
)");

    // Conversion methods
    cls.def("to_set", &atom::utils::Enumerable<T>::toStdSet,
            R"(Converts the sequence to a set.

Returns:
    A set containing the elements from the sequence.

Examples:
    >>> data.to_set()
)");

    cls.def("to_list", &atom::utils::Enumerable<T>::toStdVector,
            R"(Converts the sequence to a list.

Returns:
    A list containing the elements from the sequence.

Examples:
    >>> data.to_list()
)");

    // Python-specific methods
    cls.def(
        "__len__",
        [](const atom::utils::Enumerable<T>& self) { return self.count(); },
        "Support for len() function.");

    cls.def(
        "__iter__",
        [](const atom::utils::Enumerable<T>& self) {
            return py::make_iterator(self.toStdVector().begin(),
                                     self.toStdVector().end());
        },
        py::keep_alive<0, 1>(), "Support for iteration.");

    // Reduce method with Python semantics
    cls.def(
        "reduce",
        [](const atom::utils::Enumerable<T>& self, py::object init_val,
           py::function func) {
            auto vec = self.toStdVector();
            if (vec.empty()) {
                return init_val;
            }

            py::object result = init_val;
            for (const auto& item : vec) {
                result = func(result, item);
            }
            return result;
        },
        py::arg("initial_value"), py::arg("function"),
        R"(Applies a function of two arguments cumulatively to the items in the sequence.

Args:
    initial_value: The initial accumulator value.
    function: A function that takes (accumulated_value, item) and returns a new accumulated value.

Returns:
    The final accumulated value.

Examples:
    >>> data.reduce(0, lambda acc, x: acc + x)  # Sum all elements
)");
}

// Helper function for creating Enumerable instances from Python
template <typename T>
atom::utils::Enumerable<T> create_enumerable_from_iterable(
    const py::iterable& iterable) {
    return atom::utils::Enumerable<T>(py_iterable_to_vector<T>(iterable));
}

PYBIND11_MODULE(linq, m) {
    m.doc() = "LINQ-style utilities for Python sequences";

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

    // Create bindings for common types
    declare_enumerable<int>(m, "Int");
    declare_enumerable<double>(m, "Double");
    declare_enumerable<std::string>(m, "String");
    declare_enumerable<py::object>(m, "Object");

    // Factory functions
    m.def("from_list", &create_enumerable_from_iterable<py::object>,
          py::arg("iterable"),
          R"(Creates an Enumerable from a Python iterable.

Args:
    iterable: A Python iterable (list, tuple, etc.)

Returns:
    An Enumerable instance containing the elements from the iterable.

Examples:
    >>> from atom.utils import from_list
    >>> data = from_list([1, 2, 3, 4, 5])
)");

    m.def("from_int_list", &create_enumerable_from_iterable<int>,
          py::arg("iterable"),
          R"(Creates an Enumerable of integers from a Python iterable.

Args:
    iterable: A Python iterable of integers.

Returns:
    An Enumerable instance containing the integer elements.

Examples:
    >>> from atom.utils import from_int_list
    >>> data = from_int_list([1, 2, 3, 4, 5])
)");

    m.def(
        "from_float_list", &create_enumerable_from_iterable<double>,
        py::arg("iterable"),
        R"(Creates an Enumerable of floating-point numbers from a Python iterable.

Args:
    iterable: A Python iterable of floating-point numbers.

Returns:
    An Enumerable instance containing the floating-point elements.

Examples:
    >>> from atom.utils import from_float_list
    >>> data = from_float_list([1.1, 2.2, 3.3, 4.4, 5.5])
)");

    m.def("from_string_list", &create_enumerable_from_iterable<std::string>,
          py::arg("iterable"),
          R"(Creates an Enumerable of strings from a Python iterable.

Args:
    iterable: A Python iterable of strings.

Returns:
    An Enumerable instance containing the string elements.

Examples:
    >>> from atom.utils import from_string_list
    >>> data = from_string_list(["a", "b", "c", "d", "e"])
)");

    m.def(
        "range",
        [](int start, int end, int step = 1) {
            std::vector<int> vec;
            for (int i = start; i < end; i += step) {
                vec.push_back(i);
            }
            return atom::utils::Enumerable<int>(vec);
        },
        py::arg("start"), py::arg("end"), py::arg("step") = 1,
        R"(Creates an Enumerable of integers from a range.

Args:
    start: The start value (inclusive).
    end: The end value (exclusive).
    step: The step size. Default is 1.

Returns:
    An Enumerable instance containing the integer sequence.

Examples:
    >>> from atom.utils import range
    >>> data = range(0, 10, 2)  # [0, 2, 4, 6, 8]
)");

    // Helper function to flatten nested lists
    m.def(
        "flatten",
        [](py::list nested_list) {
            py::list result;
            for (const auto& item : nested_list) {
                if (py::isinstance<py::list>(item) ||
                    py::isinstance<py::tuple>(item)) {
                    for (const auto& subitem : item) {
                        result.append(subitem);
                    }
                } else {
                    throw py::type_error("All items must be lists or tuples");
                }
            }
            return result;
        },
        py::arg("nested_list"),
        R"(Flattens a list of lists into a single list.

Args:
    nested_list: A list containing other lists or tuples.

Returns:
    A flattened list containing all elements from the nested lists.

Examples:
    >>> from atom.utils import flatten
    >>> flatten([[1, 2], [3, 4], [5, 6]])  # [1, 2, 3, 4, 5, 6]
)");
}
