#include "atom/async/limiter.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// 模板函数以创建Debounce对象
template <typename F>
atom::async::Debounce<F> create_debounce(
    F func, std::chrono::milliseconds delay, bool leading,
    std::optional<std::chrono::milliseconds> maxWait) {
    return atom::async::Debounce<F>(func, delay, leading, maxWait);
}

// 模板函数以创建Throttle对象
template <typename F>
atom::async::Throttle<F> create_throttle(
    F func, std::chrono::milliseconds interval, bool leading,
    std::optional<std::chrono::milliseconds> maxWait) {
    return atom::async::Throttle<F>(func, interval, leading, maxWait);
}

PYBIND11_MODULE(limiter, m) {
    m.doc() = R"pbdoc(
        Rate Limiting and Rate Control
        ----------------------------

        This module provides tools for controlling call rates, including rate limiting,
        debouncing, and throttling functions.

        The module includes:
          - RateLimiter for controlling call frequency with configurable limits
          - Debounce for delaying function execution after multiple calls
          - Throttle for limiting function execution to specific intervals

        Example:
            >>> from atom.async import limiter
            >>>
            >>> # Create a rate limiter
            >>> rate_limiter = limiter.RateLimiter()
            >>>
            >>> # Set limit for a specific function (5 calls per second)
            >>> rate_limiter.set_function_limit("my_api_call", 5, 1)
            >>>
            >>> # Create a debounced function (waits 500ms after last call)
            >>> debounced_fn = limiter.create_debounce(lambda: print("Debounced!"), 500)
            >>> debounced_fn()  # Will wait 500ms before executing
            >>>
            >>> # Create a throttled function (executes at most once every 1000ms)
            >>> throttled_fn = limiter.create_throttle(lambda: print("Throttled!"), 1000)
            >>> throttled_fn()  # Executes immediately
            >>> throttled_fn()  # Ignored until interval passes
    )pbdoc";

    // 注册异常类
    py::register_exception<atom::async::RateLimitExceededException>(
        m, "RateLimitExceededException", PyExc_RuntimeError);

    // 注册通用异常转换器
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::async::RateLimitExceededException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // 定义RateLimiter::Settings类
    py::class_<atom::async::RateLimiter::Settings>(
        m, "RateLimiterSettings",
        R"(Settings for the rate limiter.

Specifies the maximum number of requests allowed within a time window.

Args:
    max_requests: Maximum number of requests allowed in the time window
    time_window: Duration of the time window in seconds
)")
        .def(py::init<size_t, std::chrono::seconds>(),
             py::arg("max_requests") = 5,
             py::arg("time_window") = std::chrono::seconds(1),
             "Constructs a Settings object with the specified parameters");

    // 定义RateLimiter类
    py::class_<atom::async::RateLimiter>(
        m, "RateLimiter",
        R"(A rate limiter class to control the rate of function executions.

This class manages rate limiting for different functions based on configurable settings.

Examples:
    >>> limiter = RateLimiter()
    >>> limiter.set_function_limit("api_call", 10, 60)  # 10 calls per minute
    >>>
    >>> # In an async function:
    >>> async def call_api():
    >>>     await limiter.acquire("api_call")
    >>>     # Make the API call here
)")
        .def(py::init<>(), "Creates a new RateLimiter instance")
        .def("set_function_limit", &atom::async::RateLimiter::setFunctionLimit,
             py::arg("function_name"), py::arg("max_requests"),
             py::arg("time_window"),
             R"(Sets the rate limit for a specific function.

Args:
    function_name: Name of the function to be rate-limited
    max_requests: Maximum number of requests allowed in the time window
    time_window: Duration of the time window in seconds

Raises:
    ValueError: If max_requests is 0 or time_window is 0
)")
        .def("pause", &atom::async::RateLimiter::pause,
             "Temporarily disables rate limiting for all functions")
        .def("resume", &atom::async::RateLimiter::resume,
             "Resumes rate limiting after a pause")
        .def("get_rejected_requests",
             &atom::async::RateLimiter::getRejectedRequests,
             py::arg("function_name"),
             R"(Gets the number of rejected requests for a specific function.

Args:
    function_name: Name of the function

Returns:
    Number of rejected requests
)")
        .def("reset_function", &atom::async::RateLimiter::resetFunction,
             py::arg("function_name"),
             R"pbdoc(
             Reset the rate limit counter and rejected count for a specific function.

             Args:
                 function_name: The name of the function to reset

             Examples:
                 >>> limiter.reset_function("api_call")
             )pbdoc")
        .def("reset_all", &atom::async::RateLimiter::resetAll,
             R"pbdoc(
             Reset all rate limit counters and rejected counts.

             Examples:
                 >>> limiter.reset_all()
             )pbdoc")
        .def("process_waiters", &atom::async::RateLimiter::processWaiters,
             R"pbdoc(
             Process waiting coroutines manually.

             This method can be called to manually process any coroutines
             that are waiting for rate limit approval.

             Examples:
                 >>> limiter.process_waiters()
             )pbdoc")
        .def(
            "set_function_limits",
            [](atom::async::RateLimiter& self, py::list settings_list) {
                std::vector<std::pair<std::string_view,
                                      atom::async::RateLimiter::Settings>>
                    cpp_settings;
                cpp_settings.reserve(settings_list.size());

                for (auto item : settings_list) {
                    auto tuple = item.cast<py::tuple>();
                    if (tuple.size() != 2) {
                        throw std::invalid_argument(
                            "Each item must be a tuple of (function_name, "
                            "settings)");
                    }

                    std::string function_name = tuple[0].cast<std::string>();
                    auto settings =
                        tuple[1].cast<atom::async::RateLimiter::Settings>();
                    cpp_settings.emplace_back(function_name, settings);
                }

                self.setFunctionLimits(cpp_settings);
            },
            py::arg("settings_list"),
            R"pbdoc(
            Set rate limits for multiple functions in batch.

            Args:
                settings_list: List of tuples containing (function_name, settings)

            Examples:
                >>> settings = [
                ...     ("api_call", RateLimiterSettings(10, 60)),
                ...     ("db_query", RateLimiterSettings(100, 60))
                ... ]
                >>> limiter.set_function_limits(settings)
            )pbdoc")
        .def(
            "acquire_batch",
            [](atom::async::RateLimiter& self, py::list function_names) {
                std::vector<std::string> cpp_names;
                cpp_names.reserve(function_names.size());

                for (auto name : function_names) {
                    cpp_names.push_back(name.cast<std::string>());
                }

                auto awaiters = self.acquireBatch(cpp_names);
                py::list py_awaiters;
                for (auto& awaiter : awaiters) {
                    py_awaiters.append(std::move(awaiter));
                }
                return py_awaiters;
            },
            py::arg("function_names"),
            R"pbdoc(
            Acquire rate limiters in batch for multiple functions.

            Args:
                function_names: List of function names

            Returns:
                List of Awaiter objects

            Examples:
                >>> awaiters = limiter.acquire_batch(["api_call", "db_query"])
                >>> # Use awaiters in async context
            )pbdoc");

    // Python包装函数创建Debounce对象的lambda函数
    m.def(
        "create_debounce",
        [](py::function func, std::chrono::milliseconds delay, bool leading,
           std::optional<std::chrono::milliseconds> maxWait) {
            // 创建一个封装了Python函数的Lambda
            auto lambda = [func]() {
                py::gil_scoped_acquire acquire;
                try {
                    func();
                } catch (const py::error_already_set& e) {
                    // 记录Python异常但不传播
                    PyErr_Clear();
                }
            };

            // 使用共享指针存储Debounce对象，避免复制
            using DebouncerType = atom::async::Debounce<decltype(lambda)>;
            auto debouncer_ptr = std::make_shared<DebouncerType>(
                lambda, delay, leading, maxWait);

            // 返回一个Python callable对象，通过指针捕获
            return py::cpp_function([debouncer_ptr]() { (*debouncer_ptr)(); });
        },
        py::arg("func"), py::arg("delay"), py::arg("leading") = false,
        py::arg("max_wait") = py::none(),
        R"(Creates a debounced version of a function.

A debounced function delays its execution until after a specified delay has elapsed
since the last time it was invoked.

Args:
    func: The function to debounce
    delay: Time in milliseconds to wait before invoking the function
    leading: If True, call the function immediately on the first call
    max_wait: Optional maximum wait time before forced execution

Returns:
    A debounced version of the input function

Examples:
    >>> # Create a function that waits until 500ms after the last call
    >>> debounced = create_debounce(lambda: print("Called!"), 500)
    >>> debounced()  # Will wait 500ms before printing
    >>> debounced()  # Resets the timer
    >>>
    >>> # Leading execution (immediate first call)
    >>> debounced2 = create_debounce(lambda: print("Called!"), 500, leading=True)
    >>> debounced2()  # Executes immediately, then waits for subsequent calls
)");

    // Python包装函数创建Throttle对象的lambda函数
    m.def(
        "create_throttle",
        [](py::function func, std::chrono::milliseconds interval, bool leading,
           std::optional<std::chrono::milliseconds> maxWait) {
            // 创建一个封装了Python函数的Lambda
            auto lambda = [func]() {
                py::gil_scoped_acquire acquire;
                try {
                    func();
                } catch (const py::error_already_set& e) {
                    // 记录Python异常但不传播
                    PyErr_Clear();
                }
            };

            // 使用共享指针存储Throttle对象，避免复制
            using ThrottlerType = atom::async::Throttle<decltype(lambda)>;
            auto throttler_ptr = std::make_shared<ThrottlerType>(
                lambda, interval, leading, maxWait);

            // 返回一个Python callable对象，通过指针捕获
            return py::cpp_function([throttler_ptr]() { (*throttler_ptr)(); });
        },
        py::arg("func"), py::arg("interval"), py::arg("leading") = false,
        py::arg("max_wait") = py::none(),
        R"(Creates a throttled version of a function.

A throttled function executes at most once in a specified time interval,
ignoring additional calls during that interval.

Args:
    func: The function to throttle
    interval: Minimum time in milliseconds between function executions
    leading: If True, call the function immediately on the first call
    max_wait: Optional maximum wait time before forced execution

Returns:
    A throttled version of the input function

Examples:
    >>> # Create a function that executes at most once per second
    >>> throttled = create_throttle(lambda: print("Called!"), 1000)
    >>> throttled()  # Executes immediately
    >>> throttled()  # Ignored until 1000ms have passed
    >>>
    >>> # Force immediate execution on first call
    >>> throttled2 = create_throttle(lambda: print("Called!"), 1000, leading=True)
    >>> throttled2()  # Executes immediately
)");

    py::class_<atom::async::RateLimiter::Awaiter>(
        m, "RateLimiterAwaiter",
        "Internal awaiter class for RateLimiter in coroutines")
        .def("__await__", [](atom::async::RateLimiter::Awaiter& awaiter) {
            // 创建一个自定义迭代器类，用于支持Python await协议
            struct AwaiterIterator {
                atom::async::RateLimiter::Awaiter& awaiter;
                bool done = false;

                AwaiterIterator(atom::async::RateLimiter::Awaiter& a)
                    : awaiter(a) {}

                // 迭代器需要支持以下操作
                void operator++() {
                    done = true;
                }  // 迭代到下一状态（在这里只有一步）
                bool operator==(const AwaiterIterator& other) const {
                    return done == other.done;
                }
                bool operator!=(const AwaiterIterator& other) const {
                    return !(*this == other);
                }

                py::object operator*() {
                    if (awaiter.await_ready()) {
                        done = true;
                        return py::cast(false);
                    }

                    try {
                        awaiter.await_resume();
                        return py::cast(true);
                    } catch (const atom::async::RateLimitExceededException& e) {
                        throw py::error_already_set();
                    }
                }
            };

            AwaiterIterator begin(awaiter);
            AwaiterIterator end(awaiter);
            end.done = true;

            return py::make_iterator(
                begin, end, py::return_value_policy::reference_internal);
        });

    m.attr("RateLimiter").attr("acquire") = py::cpp_function(
        [](atom::async::RateLimiter& limiter, std::string_view function_name) {
            return limiter.acquire(function_name);
        },
        R"(Acquires the rate limiter for a specific function.

This method is intended to be used with Python's 'await' keyword.

Args:
    function_name: Name of the function to be rate-limited

Returns:
    An awaitable object

Raises:
    RateLimitExceededException: If the rate limit is exceeded

Examples:
    >>> async def my_function():
    >>>     await limiter.acquire("api_call")
    >>>     # Rate-limited code here
)");

    // Utility functions
    m.def(
         "benchmark_rate_limiter",
         [](size_t num_requests, size_t max_requests_per_second) -> py::dict {
             using namespace std::chrono;

             py::dict results;
             atom::async::RateLimiter limiter;

             // Set up rate limit
             limiter.setFunctionLimit("benchmark_function",
                                      max_requests_per_second,
                                      std::chrono::seconds(1));

             // Benchmark rate limiting performance
             auto start = high_resolution_clock::now();
             size_t successful_requests = 0;
             size_t rejected_requests = 0;

             for (size_t i = 0; i < num_requests; ++i) {
                 try {
                     auto awaiter = limiter.acquire("benchmark_function");
                     if (awaiter.await_ready()) {
                         awaiter.await_resume();
                         successful_requests++;
                     } else {
                         rejected_requests++;
                     }
                 } catch (const atom::async::RateLimitExceededException&) {
                     rejected_requests++;
                 }
             }

             auto end = high_resolution_clock::now();
             auto duration = duration_cast<microseconds>(end - start);

             // Calculate statistics
             double total_time_us = duration.count();
             double requests_per_second =
                 (num_requests * 1000000.0) / total_time_us;
             double success_rate =
                 (double)successful_requests / num_requests * 100.0;

             results[py::str("num_requests")] = num_requests;
             results[py::str("max_requests_per_second")] =
                 max_requests_per_second;
             results[py::str("successful_requests")] = successful_requests;
             results[py::str("rejected_requests")] = rejected_requests;
             results[py::str("total_time_us")] = total_time_us;
             results[py::str("requests_per_second")] = requests_per_second;
             results[py::str("success_rate_percent")] = success_rate;
             results[py::str("actual_rejected")] =
                 limiter.getRejectedRequests("benchmark_function");

             return results;
         },
         py::arg("num_requests") = 1000,
         py::arg("max_requests_per_second") = 100,
         R"pbdoc(
        Benchmark rate limiter performance.

        Args:
            num_requests: Number of requests to test (default: 1000)
            max_requests_per_second: Rate limit to test (default: 100)

        Returns:
            dict: Benchmark results with timing and success rate metrics

        Examples:
            >>> results = benchmark_rate_limiter(5000, 50)
            >>> print(f"Success rate: {results['success_rate_percent']:.2f}%")
            >>> print(f"Requests per second: {results['requests_per_second']:.2f}")
        )pbdoc")

        .def(
            "create_rate_limited_function",
            [](py::function func, std::string function_name,
               size_t max_requests,
               std::chrono::seconds time_window) -> py::object {
                // Create a shared rate limiter for this function
                auto limiter = std::make_shared<atom::async::RateLimiter>();
                limiter->setFunctionLimit(function_name, max_requests,
                                          time_window);

                // Return a wrapped function that applies rate limiting
                return py::cpp_function([limiter, func, function_name](
                                            py::args args,
                                            py::kwargs kwargs) -> py::object {
                    try {
                        auto awaiter = limiter->acquire(function_name);
                        if (!awaiter.await_ready()) {
                            // For synchronous usage, we'll just check and
                            // proceed or throw
                            awaiter.await_resume();
                        }

                        // Call the original function
                        py::gil_scoped_acquire acquire;
                        return func(*args, **kwargs);
                    } catch (const atom::async::RateLimitExceededException& e) {
                        throw py::value_error(e.what());
                    }
                });
            },
            py::arg("func"), py::arg("function_name"), py::arg("max_requests"),
            py::arg("time_window"),
            R"pbdoc(
        Create a rate-limited version of a function.

        Args:
            func: The function to rate limit
            function_name: Name identifier for the function
            max_requests: Maximum number of requests allowed
            time_window: Time window for the rate limit

        Returns:
            A rate-limited version of the input function

        Examples:
            >>> # Create a function that can only be called 5 times per minute
            >>> limited_func = create_rate_limited_function(
            ...     lambda x: print(f"Processing {x}"),
            ...     "process_data",
            ...     5,
            ...     60
            ... )
            >>> limited_func("test")  # Will work for first 5 calls
        )pbdoc")

        .def(
            "create_multi_tier_limiter",
            [](py::dict tier_settings) -> py::object {
                auto limiter = std::make_shared<atom::async::RateLimiter>();

                // Set up multiple tiers
                for (auto item : tier_settings) {
                    std::string tier_name = item.first.cast<std::string>();
                    auto settings = item.second.cast<py::tuple>();

                    if (settings.size() != 2) {
                        throw std::invalid_argument(
                            "Each tier setting must be a tuple of "
                            "(max_requests, time_window)");
                    }

                    size_t max_requests = settings[0].cast<size_t>();
                    auto time_window =
                        std::chrono::seconds(settings[1].cast<int>());

                    limiter->setFunctionLimit(tier_name, max_requests,
                                              time_window);
                }

                return py::cast(limiter);
            },
            py::arg("tier_settings"),
            R"pbdoc(
        Create a multi-tier rate limiter with different limits for different tiers.

        Args:
            tier_settings: Dictionary mapping tier names to (max_requests, time_window_seconds)

        Returns:
            A configured RateLimiter instance

        Examples:
            >>> tiers = {
            ...     "basic": (10, 60),      # 10 requests per minute
            ...     "premium": (100, 60),   # 100 requests per minute
            ...     "enterprise": (1000, 60) # 1000 requests per minute
            ... }
            >>> limiter = create_multi_tier_limiter(tiers)
        )pbdoc");

    // Add version and feature information
    m.attr("__version__") = "1.0.0";

#ifdef ATOM_USE_ASIO
    m.attr("HAS_ASIO") = true;
#else
    m.attr("HAS_ASIO") = false;
#endif

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
