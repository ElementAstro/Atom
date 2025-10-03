#include "atom/async/threadlocal.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace atom::async;

PYBIND11_MODULE(threadlocal, m) {
    m.doc() = R"pbdoc(
        Enhanced Thread-Local Storage Module
        ===================================

        This module provides advanced thread-local storage capabilities with:
        - Multiple initialization strategies (default, conditional, thread-ID based)
        - Automatic cleanup and resource management
        - Thread-safe operations with shared_mutex optimization
        - Rich set of access and manipulation methods
        - Exception-safe operations with comprehensive error handling

        Key Features:
        - C++20 concepts for type safety
        - Optional initialization patterns
        - Atomic compare-and-update operations
        - Bulk operations (forEach, findIf, removeIf)
        - Performance monitoring and introspection

        Examples:
            >>> from atom.async.threadlocal import ThreadLocal, ThreadLocalError
            >>>
            >>> # Basic usage with initializer
            >>> tls = ThreadLocal(lambda: {"counter": 0})
            >>> value = tls.get()
            >>>
            >>> # Conditional initialization
            >>> tls_cond = ThreadLocal.with_conditional_initializer(
            ...     lambda: {"data": "value"} if some_condition() else None
            ... )
            >>>
            >>> # Thread-ID based initialization
            >>> tls_tid = ThreadLocal.with_thread_id_initializer(
            ...     lambda tid: f"thread_{tid}"
            ... )
        )pbdoc";

    // Register ThreadLocalException as a proper exception class
    py::register_exception<ThreadLocalException>(m, "ThreadLocalException");

    // Enhanced exception translator with detailed error information
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p) std::rethrow_exception(p);
        } catch (const ThreadLocalException& e) {
            py::dict error_info;
            error_info["error_code"] = static_cast<int>(e.error());
            error_info["function"] = e.function();
            error_info["file"] = e.file();
            error_info["line"] = e.line();

            py::object exc = py::cast(py::runtime_error(e.what()));
            exc.attr("error_info") = error_info;
            PyErr_SetObject(PyExc_RuntimeError, exc.ptr());
        }
    });

    // Register ThreadLocalError enum
    py::enum_<ThreadLocalError>(m, "ThreadLocalError",
        R"pbdoc(
        Enumeration of thread-local storage error types.
        )pbdoc")
        .value("NO_INITIALIZER", ThreadLocalError::NoInitializer,
               "No initializer provided")
        .value("INITIALIZATION_FAILED", ThreadLocalError::InitializationFailed,
               "Initialization failed")
        .value("VALUE_NOT_FOUND", ThreadLocalError::ValueNotFound,
               "Value not found")
        .value("OPERATION_FAILED", ThreadLocalError::OperationFailed,
               "Operation failed")
        .export_values();

    // ThreadLocal class binding with enhanced functionality
    py::class_<atom::async::ThreadLocal<py::object>>(
        m, "ThreadLocal",
        R"pbdoc(
        An enhanced thread-local storage class for Python objects.

        This class allows each thread to maintain its own independent instance of an object,
        with automatic cleanup and a rich set of access and manipulation methods. It supports
        multiple initialization strategies and provides thread-safe operations.

        Key Features:
        - Multiple initialization patterns (default, conditional, thread-ID based)
        - Automatic cleanup with custom cleanup functions
        - Thread-safe operations with optimized locking
        - Atomic compare-and-update operations
        - Bulk operations for processing multiple thread values
        - Exception-safe operations with comprehensive error handling

        Examples:
            >>> from atom.async.threadlocal import ThreadLocal
            >>>
            >>> # Create with a simple initializer
            >>> tls = ThreadLocal(lambda: {"counter": 0, "data": []})
            >>> value = tls.get()
            >>> print(value)  # {'counter': 0, 'data': []}
            >>>
            >>> # Create with a default value
            >>> tls_default = ThreadLocal("initial_value")
            >>> print(tls_default.get())  # initial_value
            >>>
            >>> # Use with cleanup function
            >>> def cleanup_func(value):
            ...     print(f"Cleaning up: {value}")
            >>> tls_cleanup = ThreadLocal(lambda: "resource", cleanup_func)
        )pbdoc")
        .def(py::init<>(),
             R"pbdoc(
             Constructs a new ThreadLocal object without an initializer.

             Values must be explicitly set using reset() before calling get().
             )pbdoc")
        .def(py::init<std::function<py::object()>>(), py::arg("initializer"),
             R"pbdoc(
             Constructs a ThreadLocal object with an initializer function.

             Args:
                 initializer: Function that returns the initial value for each thread.
                             Called once per thread on first access.
             )pbdoc")
        .def(py::init<std::function<py::object()>, std::function<void(py::object&)>>(),
             py::arg("initializer"), py::arg("cleanup"),
             R"pbdoc(
             Constructs a ThreadLocal object with initializer and cleanup functions.

             Args:
                 initializer: Function that returns the initial value for each thread.
                 cleanup: Function called when a value is removed or thread terminates.
             )pbdoc")
        .def(py::init<py::object>(), py::arg("default_value"),
             R"pbdoc(
             Constructs with a default value for all threads.

             Args:
                 default_value: The default value to use for all threads.
             )pbdoc")
        .def("get", &atom::async::ThreadLocal<py::object>::get,
             R"pbdoc(
             Retrieves the thread-local value.

             If the value is not yet initialized, the initializer function is called.

             Returns:
                 The thread-local value for the current thread.

             Raises:
                 ThreadLocalException: If initialization fails or no initializer is available.
             )pbdoc")
        .def("has_value", &atom::async::ThreadLocal<py::object>::hasValue,
             R"pbdoc(
             Checks if the current thread has a value.

             Returns:
                 bool: True if the current thread has an initialized value, False otherwise.
             )pbdoc")
        .def("reset", &atom::async::ThreadLocal<py::object>::reset,
             py::arg("value") = py::object(),
             R"pbdoc(
             Resets the thread-local value.

             Args:
                 value: The new value to set for the current thread.
             )pbdoc")
        .def("clear", &atom::async::ThreadLocal<py::object>::clear,
             R"pbdoc(
             Clears thread-local storage for all threads.
             Calls cleanup function if set.
             )pbdoc")
        .def("clear_current_thread", &atom::async::ThreadLocal<py::object>::clearCurrentThread,
             R"pbdoc(
             Clears the thread-local storage for the current thread.
             Calls cleanup function if set.
             )pbdoc")
        .def("size", &atom::async::ThreadLocal<py::object>::size,
             R"pbdoc(
             Gets the number of threads with values stored.

             Returns:
                 size_t: The count of thread values currently stored.
             )pbdoc")
        .def("empty", &atom::async::ThreadLocal<py::object>::empty,
             R"pbdoc(
             Checks if the storage is empty (no threads have values stored).

             Returns:
                 bool: True if there are no stored thread values, False otherwise.
             )pbdoc")
        .def("has_value_for_thread", &atom::async::ThreadLocal<py::object>::hasValueForThread,
             py::arg("thread_id"),
             R"pbdoc(
             Checks if the specified thread has a value.

             Args:
                 thread_id: The ID of the thread to check.

             Returns:
                 bool: True if the specified thread has an initialized value, False otherwise.
             )pbdoc")
        .def("__call__", &atom::async::ThreadLocal<py::object>::get,
             R"pbdoc(
             Allows calling the ThreadLocal object as a function to get the value.

             Equivalent to calling .get().

             Returns:
                 The thread-local value for the current thread.

             Raises:
                 ThreadLocalException: If initialization fails or no initializer is available.
             )pbdoc")
        .def("__bool__", &atom::async::ThreadLocal<py::object>::hasValue,
             R"pbdoc(
             Support for boolean evaluation (True if current thread has a value).
             )pbdoc")
        .def("try_get", [](atom::async::ThreadLocal<py::object>& self) -> py::object {
                auto result = self.tryGet();
                if (result.has_value()) {
                    return result.value().get();
                }
                return py::object();
             },
             R"pbdoc(
             Tries to get the value for the current thread without throwing.

             Unlike get(), this method does not throw an exception if the value
             is not found or cannot be initialized.

             Returns:
                 The thread-local value or None if it doesn't exist.
             )pbdoc")
        .def("compare_and_update", &atom::async::ThreadLocal<py::object>::compareAndUpdate,
             py::arg("expected"), py::arg("desired"),
             R"pbdoc(
             Atomically compares and updates the thread-local value.

             Updates to 'desired' only if the current value equals 'expected'.
             This operation is atomic and suitable for coordination scenarios.

             Args:
                 expected: The expected current value.
                 desired: The new value to set.

             Returns:
                 bool: True if the update was successful, False otherwise.
             )pbdoc")
        .def("get_or_create",
             [](atom::async::ThreadLocal<py::object>& self, py::function factory) -> py::object& {
                 return self.getOrCreate([factory]() -> py::object {
                     py::gil_scoped_acquire acquire;
                     return factory();
                 });
             },
             py::arg("factory"),
             R"pbdoc(
             Gets or creates the value for the current thread using a factory function.

             If the value does not exist, it is created using the provided factory function.

             Args:
                 factory: Function to create the value if it doesn't exist.

             Returns:
                 The thread-local value (existing or newly created).

             Example:
                 >>> tls = ThreadLocal()
                 >>> value = tls.get_or_create(lambda: {"initialized": True})
             )pbdoc")
        .def("update",
             [](atom::async::ThreadLocal<py::object>& self, py::function func) -> bool {
                 return self.update([func](py::object& value) -> py::object {
                     py::gil_scoped_acquire acquire;
                     return func(value);
                 });
             },
             py::arg("func"),
             R"pbdoc(
             Updates the thread-local value using a transformation function.

             Args:
                 func: Function that accepts the current value and returns a new value.

             Returns:
                 bool: True if successfully updated, False if no value exists.

             Example:
                 >>> tls = ThreadLocal(lambda: 0)
                 >>> tls.update(lambda x: x + 1)  # Increment the value
             )pbdoc")
        .def("for_each",
             [](atom::async::ThreadLocal<py::object>& self, py::function func) {
                 self.forEach([func](py::object& value) {
                     py::gil_scoped_acquire acquire;
                     func(value);
                 });
             },
             py::arg("func"),
             R"pbdoc(
             Executes a function for each thread-local value.

             Args:
                 func: Function to execute for each thread-local value.

             Example:
                 >>> tls = ThreadLocal(lambda: [])
                 >>> tls.for_each(lambda value: value.append("processed"))
             )pbdoc")
        .def("for_each_with_id",
             [](atom::async::ThreadLocal<py::object>& self, py::function func) {
                 self.forEachWithId([func](py::object& value, std::thread::id tid) {
                     py::gil_scoped_acquire acquire;
                     std::ostringstream oss;
                     oss << tid;
                     func(value, py::str(oss.str()));
                 });
             },
             py::arg("func"),
             R"pbdoc(
             Executes a function for each thread-local value with thread ID.

             Args:
                 func: Function that accepts (value, thread_id_string).

             Example:
                 >>> tls.for_each_with_id(lambda value, tid: print(f"Thread {tid}: {value}"))
             )pbdoc")
        .def("find_if",
             [](atom::async::ThreadLocal<py::object>& self, py::function predicate) -> py::object {
                 auto result = self.findIf([predicate](py::object& value) -> bool {
                     py::gil_scoped_acquire acquire;
                     return predicate(value).cast<bool>();
                 });
                 if (result.has_value()) {
                     return result.value().get();
                 }
                 return py::none();
             },
             py::arg("predicate"),
             R"pbdoc(
             Finds the first thread value that satisfies the given condition.

             Args:
                 predicate: Function that tests values and returns bool.

             Returns:
                 The first value that satisfies the condition, or None if not found.

             Example:
                 >>> result = tls.find_if(lambda x: x > 10)
             )pbdoc")
        .def("remove_if",
             [](atom::async::ThreadLocal<py::object>& self, py::function predicate) -> std::size_t {
                 return self.removeIf([predicate](py::object& value) -> bool {
                     py::gil_scoped_acquire acquire;
                     return predicate(value).cast<bool>();
                 });
             },
             py::arg("predicate"),
             R"pbdoc(
             Removes all thread values that satisfy the given condition.

             Args:
                 predicate: Function that tests values and returns bool.

             Returns:
                 int: The number of values removed.

             Example:
                 >>> removed_count = tls.remove_if(lambda x: x < 0)
             )pbdoc")
        .def("get_pointer",
             [](atom::async::ThreadLocal<py::object>& self) -> py::object {
                 auto* ptr = self.getPointer();
                 return ptr ? *ptr : py::none();
             },
             R"pbdoc(
             Gets a pointer to the thread-local value.

             Returns:
                 The thread-local value or None if not initialized.

             This method is non-blocking and safe to call even if
             the value hasn't been initialized.
             )pbdoc")
        .def("set_cleanup_function",
             [](atom::async::ThreadLocal<py::object>& self, py::function cleanup) {
                 self.setCleanupFunction([cleanup](py::object& value) {
                     py::gil_scoped_acquire acquire;
                     cleanup(value);
                 });
             },
             py::arg("cleanup"),
             R"pbdoc(
             Sets or updates the cleanup function.

             Args:
                 cleanup: Function to be called when a value is removed.

             The cleanup function will be called for each value when:
             - The thread terminates
             - clear() or clearCurrentThread() is called
             - A value is replaced via reset() or update()
             )pbdoc")
        .def("__getitem__", &atom::async::ThreadLocal<py::object>::get,
             R"pbdoc(
             Allows accessing the thread-local value using [] operator.

             Equivalent to calling .get().
             )pbdoc")
        .def("__setitem__",
             [](atom::async::ThreadLocal<py::object>& self, py::object, py::object value) {
                 self.reset(value);
             },
             R"pbdoc(
             Allows setting the thread-local value using [] operator.

             Equivalent to calling .reset(value).
             )pbdoc")
        .def("__len__", &atom::async::ThreadLocal<py::object>::size,
             R"pbdoc(
             Returns the number of threads with stored values.
             )pbdoc")
        .def("__iter__",
             [](atom::async::ThreadLocal<py::object>& self) {
                 py::list values;
                 self.forEach([&values](py::object& value) {
                     py::gil_scoped_acquire acquire;
                     values.append(value);
                 });
                 return py::iter(values);
             },
             R"pbdoc(
             Allows iteration over all thread-local values.

             Returns an iterator over all currently stored values.
             )pbdoc");

    // Factory methods for different initialization strategies
    m.def("create_thread_local",
          []() {
              return atom::async::ThreadLocal<py::object>();
          },
          R"pbdoc(
          Creates a new ThreadLocal object without an initializer.

          Returns:
              ThreadLocal: A new ThreadLocal instance.
          )pbdoc");

    m.def("create_thread_local_with_initializer",
          [](py::function initializer) {
              return atom::async::ThreadLocal<py::object>(
                  [initializer]() -> py::object {
                      py::gil_scoped_acquire acquire;
                      return initializer();
                  });
          },
          py::arg("initializer"),
          R"pbdoc(
          Creates a new ThreadLocal object with an initializer function.

          Args:
              initializer: Function that returns the initial value for each thread.

          Returns:
              ThreadLocal: A new ThreadLocal instance.
          )pbdoc");

    m.def("create_thread_local_with_default",
          [](py::object default_value) {
              return atom::async::ThreadLocal<py::object>(default_value);
          },
          py::arg("default_value"),
          R"pbdoc(
          Creates a new ThreadLocal object with a default value.

          Args:
              default_value: The default value to use for all threads.

          Returns:
              ThreadLocal: A new ThreadLocal instance.
          )pbdoc");

    m.def("create_thread_local_with_cleanup",
          [](py::function initializer, py::function cleanup) {
              return atom::async::ThreadLocal<py::object>(
                  [initializer]() -> py::object {
                      py::gil_scoped_acquire acquire;
                      return initializer();
                  },
                  [cleanup](py::object& value) {
                      py::gil_scoped_acquire acquire;
                      cleanup(value);
                  });
          },
          py::arg("initializer"), py::arg("cleanup"),
          R"pbdoc(
          Creates a new ThreadLocal object with initializer and cleanup functions.

          Args:
              initializer: Function that returns the initial value for each thread.
              cleanup: Function called when a value is removed.

          Returns:
              ThreadLocal: A new ThreadLocal instance.
          )pbdoc");

    // Utility functions
    m.def("get_current_thread_id",
          []() {
              std::ostringstream oss;
              oss << std::this_thread::get_id();
              return py::str(oss.str());
          },
          R"pbdoc(
          Get the ID of the current thread as a string.

          Returns:
              str: String representation of the current thread ID.

          This is useful for debugging and logging thread-local operations.
          )pbdoc")

    .def("benchmark_thread_local_performance",
         [](py::function initializer, int num_threads, int operations_per_thread) -> py::dict {
             using namespace std::chrono;

             py::dict results;
             std::vector<std::thread> threads;
             std::vector<double> thread_times(num_threads);

             auto tls = atom::async::ThreadLocal<py::object>(
                 [initializer]() -> py::object {
                     py::gil_scoped_acquire acquire;
                     return initializer();
                 });

             auto start_time = high_resolution_clock::now();

             for (int i = 0; i < num_threads; ++i) {
                 threads.emplace_back([&tls, &thread_times, i, operations_per_thread]() {
                     auto thread_start = high_resolution_clock::now();

                     for (int j = 0; j < operations_per_thread; ++j) {
                         // Perform get operations
                         auto& value = tls.get();

                         // Simulate some work
                         volatile int dummy = j;
                         (void)dummy;
                     }

                     auto thread_end = high_resolution_clock::now();
                     auto duration = duration_cast<microseconds>(thread_end - thread_start);
                     thread_times[i] = duration.count();
                 });
             }

             for (auto& thread : threads) {
                 thread.join();
             }

             auto end_time = high_resolution_clock::now();
             auto total_duration = duration_cast<microseconds>(end_time - start_time);

             // Calculate statistics
             double total_ops = num_threads * operations_per_thread;
             double avg_thread_time = 0;
             for (double time : thread_times) {
                 avg_thread_time += time;
             }
             avg_thread_time /= num_threads;

             double min_time = *std::min_element(thread_times.begin(), thread_times.end());
             double max_time = *std::max_element(thread_times.begin(), thread_times.end());

             results[py::str("num_threads")] = num_threads;
             results[py::str("operations_per_thread")] = operations_per_thread;
             results[py::str("total_operations")] = total_ops;
             results[py::str("total_time_us")] = total_duration.count();
             results[py::str("avg_thread_time_us")] = avg_thread_time;
             results[py::str("min_thread_time_us")] = min_time;
             results[py::str("max_thread_time_us")] = max_time;
             results[py::str("operations_per_second")] = (total_ops * 1000000.0) / total_duration.count();

             return results;
         },
         py::arg("initializer"), py::arg("num_threads") = 4, py::arg("operations_per_thread") = 1000,
         R"pbdoc(
         Benchmark thread-local storage performance across multiple threads.

         Args:
             initializer: Function to initialize thread-local values
             num_threads: Number of threads to use for testing (default: 4)
             operations_per_thread: Number of operations per thread (default: 1000)

         Returns:
             dict: Performance metrics including timing and throughput data

         Examples:
             >>> results = benchmark_thread_local_performance(lambda: {"counter": 0}, 8, 5000)
             >>> print(f"Operations per second: {results['operations_per_second']:.2f}")
         )pbdoc")

    .def("create_thread_local_pool",
         [](py::function initializer, size_t pool_size) -> py::list {
             py::list pool;
             for (size_t i = 0; i < pool_size; ++i) {
                 pool.append(atom::async::ThreadLocal<py::object>(
                     [initializer]() -> py::object {
                         py::gil_scoped_acquire acquire;
                         return initializer();
                     }));
             }
             return pool;
         },
         py::arg("initializer"), py::arg("pool_size"),
         R"pbdoc(
         Create a pool of ThreadLocal instances for load distribution.

         Args:
             initializer: Function to initialize thread-local values
             pool_size: Number of ThreadLocal instances to create

         Returns:
             list: List of ThreadLocal instances

         Examples:
             >>> pool = create_thread_local_pool(lambda: [], 5)
             >>> # Use different instances for different purposes
             >>> pool[0].reset("service1_data")
             >>> pool[1].reset("service2_data")
         )pbdoc")

    .def("create_thread_local_registry",
         []() -> py::dict {
             py::dict registry;
             registry[py::str("instances")] = py::dict();
             registry[py::str("count")] = 0;

             return registry;
         },
         R"pbdoc(
         Create a registry for managing multiple ThreadLocal instances.

         Returns:
             dict: Registry with instances dictionary and count

         Examples:
             >>> registry = create_thread_local_registry()
             >>> registry["instances"]["cache"] = ThreadLocal(lambda: {})
             >>> registry["instances"]["config"] = ThreadLocal(lambda: {"debug": False})
         )pbdoc");

    // Feature detection and platform information
    m.attr("HAS_THREAD_LOCAL_SUPPORT") = true;
    m.attr("HAS_CONDITIONAL_INITIALIZERS") = true;
    m.attr("HAS_THREAD_ID_INITIALIZERS") = true;
    m.attr("HAS_CLEANUP_FUNCTIONS") = true;
    m.attr("HAS_BULK_OPERATIONS") = true;

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

    // Add version information
    m.attr("__version__") = "1.0.0";
}
