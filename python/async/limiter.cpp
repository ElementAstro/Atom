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
        .def("print_log", &atom::async::RateLimiter::printLog,
             "Prints the log of requests (for debugging)")
        .def("get_rejected_requests",
             &atom::async::RateLimiter::getRejectedRequests,
             py::arg("function_name"),
             R"(Gets the number of rejected requests for a specific function.

Args:
    function_name: Name of the function

Returns:
    Number of rejected requests
)");

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

    // 添加RateLimiter的Awaiter对象，用于异步Python代码
    py::class_<atom::async::RateLimiter::Awaiter>(
        m, "RateLimiterAwaiter",
        "Internal awaiter class for RateLimiter in coroutines")
        .def("__await__", [](atom::async::RateLimiter::Awaiter& awaiter) {
            return py::make_iterator(
                py::cpp_function([&awaiter]() {
                    if (awaiter.await_ready()) {
                        return py::object(py::cast(false));
                    }
                    // 模拟协程暂停/继续
                    try {
                        awaiter.await_resume();
                        return py::object(py::cast(true));
                    } catch (const atom::async::RateLimitExceededException& e) {
                        throw py::error_already_set();
                    }
                }),
                py::cpp_function([&awaiter]() { return py::none(); }),
                py::return_value_policy::reference_internal);
        });

    // 添加RateLimiter的acquire方法，返回Awaiter
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

    // 添加版本信息
    m.attr("__version__") = "1.0.0";
}