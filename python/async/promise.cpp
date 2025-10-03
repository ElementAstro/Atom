#include "atom/async/promise.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Forward declarations for PromiseAwaiter classes
template <typename T>
void declare_promise_awaiter(py::module& m, const std::string& type_name);
void declare_promise_awaiter_void(py::module& m);
void declare_promise_void(py::module& m);

// Helper function to create a resolved Promise
template <typename T>
auto createResolvedPromise(const T& value) {
    auto promise = std::make_shared<atom::async::Promise<T>>();
    promise->setValue(value);
    return promise;
}

// Helper function to create a rejected Promise
template <typename T>
auto createRejectedPromise(const py::object& reason) {
    auto promise = std::make_shared<atom::async::Promise<T>>();
    try {
        throw py::cast<std::string>(reason);
    } catch (...) {
        promise->setException(std::current_exception());
    }
    return promise;
}

// Helper function to create a cancelled Promise
template <typename T>
auto createCancelledPromise() {
    auto promise = std::make_shared<atom::async::Promise<T>>();
    promise->cancel();
    return promise;
}

// 用于实现Promise.all功能
auto promiseAll(
    const std::vector<std::shared_ptr<atom::async::Promise<py::object>>>&
        promises) {
    auto resultPromise =
        std::make_shared<atom::async::Promise<std::vector<py::object>>>();

    if (promises.empty()) {
        resultPromise->setValue(std::vector<py::object>());
        return resultPromise;
    }

    // 创建共享状态跟踪完成情况
    struct SharedState {
        std::mutex mutex;
        std::vector<py::object> results;
        size_t completedCount = 0;
        size_t totalCount;
        std::shared_ptr<atom::async::Promise<std::vector<py::object>>>
            resultPromise;

        explicit SharedState(
            size_t count,
            std::shared_ptr<atom::async::Promise<std::vector<py::object>>>
                promise)
            : totalCount(count), resultPromise(promise) {
            results.resize(count);
        }
    };

    auto state = std::make_shared<SharedState>(promises.size(), resultPromise);

    // 为每个Promise设置回调
    for (size_t i = 0; i < promises.size(); ++i) {
        promises[i]->onComplete([state, i](py::object value) {
            py::gil_scoped_acquire acquire;
            std::unique_lock lock(state->mutex);
            state->results[i] = value;
            state->completedCount++;

            if (state->completedCount == state->totalCount) {
                state->resultPromise->setValue(state->results);
            }
        });
    }

    return resultPromise;
}

// 用于实现Promise.race功能
auto promiseRace(
    const std::vector<std::shared_ptr<atom::async::Promise<py::object>>>&
        promises) {
    auto resultPromise = std::make_shared<atom::async::Promise<py::object>>();

    if (promises.empty()) {
        resultPromise->setException(std::make_exception_ptr(
            std::runtime_error("No promises provided to race")));
        return resultPromise;
    }

    // 创建共享状态
    struct SharedState {
        std::mutex mutex;
        bool settled = false;
        std::shared_ptr<atom::async::Promise<py::object>> resultPromise;

        explicit SharedState(
            std::shared_ptr<atom::async::Promise<py::object>> promise)
            : resultPromise(promise) {}
    };

    auto state = std::make_shared<SharedState>(resultPromise);

    // 为每个Promise设置完成回调
    for (auto& promise : promises) {
        promise->onComplete([state](py::object value) {
            py::gil_scoped_acquire acquire;
            std::unique_lock lock(state->mutex);
            if (!state->settled) {
                state->settled = true;
                state->resultPromise->setValue(value);
            }
        });
    }

    return resultPromise;
}

// PromiseAwaiter template for different return types
template <typename T>
void declare_promise_awaiter(py::module& m, const std::string& type_name) {
    using namespace atom::async;
    using PromiseAwaiterT = PromiseAwaiter<T>;

    std::string class_name = "PromiseAwaiter" + type_name;

    py::class_<PromiseAwaiterT>(m, class_name.c_str(),
        R"pbdoc(
        Coroutine-compatible awaiter for Promise objects.

        This class provides C++20 coroutine support for Promise objects,
        allowing them to be used with async/await syntax in compatible environments.
        It implements the awaitable protocol for efficient coroutine integration.

        Note: This class is primarily for advanced use cases and coroutine integration.
        For most Python use cases, use Promise directly.
        )pbdoc")
        .def(py::init<std::shared_future<T>>(), py::arg("future"),
             R"pbdoc(
             Constructs a PromiseAwaiter from a shared_future.

             Args:
                 future: The shared_future to wrap for coroutine support.
             )pbdoc")
        .def("await_ready", &PromiseAwaiterT::await_ready,
             R"pbdoc(
             Checks if the promise is ready without blocking.

             Returns:
                 bool: True if the promise is ready, False otherwise.

             Note: This is part of the coroutine awaitable protocol.
             )pbdoc")
        .def("await_resume", &PromiseAwaiterT::await_resume,
             R"pbdoc(
             Resumes execution and returns the result.

             Returns:
                 The result of the promise operation.

             Raises:
                 Exception: Any exception that occurred during execution.

             Note: This is part of the coroutine awaitable protocol.
             )pbdoc");
}

// PromiseAwaiter void specialization
void declare_promise_awaiter_void(py::module& m) {
    using namespace atom::async;
    using PromiseAwaiterVoid = PromiseAwaiter<void>;

    py::class_<PromiseAwaiterVoid>(m, "PromiseAwaiterVoid",
        R"pbdoc(
        Coroutine-compatible awaiter for Promise<void> objects.

        This class provides C++20 coroutine support for void Promise objects,
        allowing them to be used with async/await syntax in compatible environments.
        It implements the awaitable protocol for efficient coroutine integration.

        Note: This class is primarily for advanced use cases and coroutine integration.
        For most Python use cases, use PromiseVoid directly.
        )pbdoc")
        .def(py::init<std::shared_future<void>>(), py::arg("future"),
             R"pbdoc(
             Constructs a PromiseAwaiterVoid from a shared_future<void>.

             Args:
                 future: The shared_future<void> to wrap for coroutine support.
             )pbdoc")
        .def("await_ready", &PromiseAwaiterVoid::await_ready,
             R"pbdoc(
             Checks if the promise is ready without blocking.

             Returns:
                 bool: True if the promise is ready, False otherwise.

             Note: This is part of the coroutine awaitable protocol.
             )pbdoc")
        .def("await_resume", &PromiseAwaiterVoid::await_resume,
             R"pbdoc(
             Resumes execution after the promise completes.

             Raises:
                 Exception: Any exception that occurred during execution.

             Note: This is part of the coroutine awaitable protocol.
             )pbdoc");
}

// Promise<void> specialization
void declare_promise_void(py::module& m) {
    using namespace atom::async;
    using PromiseVoid = Promise<void>;

    py::class_<PromiseVoid, std::shared_ptr<PromiseVoid>>(m, "PromiseVoid",
        R"pbdoc(
        A Promise specialization for void operations.

        This class represents a promise that doesn't return a value but signals
        completion or failure of an asynchronous operation. It provides the same
        interface as Promise but is optimized for void operations.

        Examples:
            >>> from atom.async.promise import PromiseVoid
            >>> def async_task():
            ...     p = PromiseVoid()
            ...     # Simulate async operation
            ...     import threading
            ...     def resolver():
            ...         import time
            ...         time.sleep(1)
            ...         p.resolve()
            ...     threading.Thread(target=resolver).start()
            ...     return p
            >>> promise = async_task()
            >>> promise.wait()  # Blocks until resolved
        )pbdoc")
        .def(py::init<>(), "Creates a new pending PromiseVoid.")
        .def("resolve", &PromiseVoid::setValue,
             R"pbdoc(
             Resolves the promise (completes the void operation).

             Raises:
                 RuntimeError: If the promise is already settled.
             )pbdoc")
        .def(
            "reject",
            [](PromiseVoid& self, py::object reason) {
                try {
                    std::string reason_str = reason.cast<std::string>();
                    throw std::runtime_error(reason_str);
                } catch (...) {
                    self.setException(std::current_exception());
                }
            },
            py::arg("reason"),
            R"pbdoc(
            Rejects the promise with the given reason.

            Args:
                reason: The reason for rejection.

            Raises:
                RuntimeError: If the promise is already settled.
            )pbdoc")
        .def(
            "is_pending",
            [](const PromiseVoid& self) {
                return !self.isCancelled() &&
                       self.getFuture().wait_for(std::chrono::seconds(0)) ==
                           std::future_status::timeout;
            },
            R"pbdoc(
            Checks if the promise is still pending.

            Returns:
                bool: True if the promise has not been resolved or rejected yet.
            )pbdoc")
        .def(
            "is_fulfilled",
            [](const PromiseVoid& self) {
                if (self.isCancelled())
                    return false;
                try {
                    auto future = self.getFuture();
                    return future.valid() &&
                           future.wait_for(std::chrono::seconds(0)) ==
                               std::future_status::ready;
                } catch (...) {
                    return false;
                }
            },
            R"pbdoc(
            Checks if the promise has been resolved.

            Returns:
                bool: True if the promise has been resolved.
            )pbdoc")
        .def(
            "is_rejected",
            [](const PromiseVoid& self) {
                if (self.isCancelled())
                    return true;
                try {
                    auto future = self.getFuture();
                    if (future.valid() &&
                        future.wait_for(std::chrono::seconds(0)) ==
                            std::future_status::ready) {
                        try {
                            future.get();
                            return false;  // No exception, not rejected
                        } catch (...) {
                            return true;  // Has exception, is rejected
                        }
                    }
                    return false;  // Not ready yet, not rejected
                } catch (...) {
                    return false;
                }
            },
            R"pbdoc(
            Checks if the promise has been rejected.

            Returns:
                bool: True if the promise has been rejected.
            )pbdoc")
        .def(
            "wait",
            [](PromiseVoid& self, unsigned int timeout_ms) {
                auto future = self.getFuture();
                if (timeout_ms == 0) {
                    future.get();  // Wait indefinitely
                } else {
                    auto status =
                        future.wait_for(std::chrono::milliseconds(timeout_ms));
                    if (status == std::future_status::ready) {
                        future.get();
                    } else if (status == std::future_status::timeout) {
                        throw std::runtime_error("Promise wait timed out");
                    } else {
                        throw std::runtime_error(
                            "Promise wait failed with unknown status");
                    }
                }
            },
            py::arg("timeout_ms") = 0,
            R"pbdoc(
            Waits for the promise to be settled.

            Args:
                timeout_ms: Maximum time to wait in milliseconds. 0 means wait indefinitely.

            Raises:
                Exception: The rejection reason if the promise is rejected.
                TimeoutError: If the timeout is reached before the promise settles.
            )pbdoc")
        .def("cancel", &PromiseVoid::cancel,
             R"pbdoc(
             Cancels the promise.

             Returns:
                 bool: True if the promise was successfully cancelled, False otherwise.
             )pbdoc")
        .def("is_cancelled", &PromiseVoid::isCancelled,
             R"pbdoc(
             Checks if the promise has been cancelled.

             Returns:
                 bool: True if the promise has been cancelled, False otherwise.
             )pbdoc")
        .def(
            "on_complete",
            [](PromiseVoid& self, py::function callback) {
                self.onComplete([callback]() {
                    py::gil_scoped_acquire acquire;
                    callback();
                });
            },
            py::arg("callback"),
            R"pbdoc(
            Registers a callback to be called when the promise completes.

            Args:
                callback: Function to call when the promise resolves.
            )pbdoc")
        .def(
            "run_async",
            [](PromiseVoid& self, py::function func) {
                self.runAsync([func]() {
                    py::gil_scoped_acquire acquire;
                    func();
                });
            },
            py::arg("func"),
            R"pbdoc(
            Runs a function asynchronously and resolves the promise when it completes.

            Args:
                func: Function to execute asynchronously.
            )pbdoc");
}

PYBIND11_MODULE(promise, m) {
    m.doc() =
        "Promise implementation module for asynchronous operations in the atom "
        "package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::async::PromiseCancelledException& e) {
            throw py::value_error(e.what());
        } catch (const std::invalid_argument& e) {
            throw py::value_error(e.what());
        } catch (const std::runtime_error& e) {
            throw std::runtime_error(e.what());
        } catch (const std::exception& e) {
            throw std::runtime_error(e.what());
        }
    });

    // Declare PromiseAwaiter for different types (coroutine support)
    declare_promise_awaiter<int>(m, "Int");
    declare_promise_awaiter<float>(m, "Float");
    declare_promise_awaiter<double>(m, "Double");
    declare_promise_awaiter<std::string>(m, "String");
    declare_promise_awaiter<bool>(m, "Bool");
    declare_promise_awaiter<py::object>(m, "Object");
    declare_promise_awaiter_void(m);

    // Declare Promise<void> specialization
    declare_promise_void(m);

    // Promise类的绑定
    using PyPromise = std::shared_ptr<atom::async::Promise<py::object>>;
    py::class_<atom::async::Promise<py::object>, PyPromise>(
        m, "Promise",
        R"(A Promise represents a value that may be available in the future.

This class provides methods to handle asynchronous operations with callback-based
resolution and rejection mechanisms similar to JavaScript Promises.

Examples:
    >>> from atom.async import Promise
    >>> def async_task():
    ...     p = Promise()
    ...     # Simulate async operation
    ...     import threading
    ...     def resolver():
    ...         import time
    ...         time.sleep(1)
    ...         p.resolve("Done!")
    ...     threading.Thread(target=resolver).start()
    ...     return p
    >>> promise = async_task()
    >>> result = promise.wait()  # Blocks until resolved
    >>> print(result)
    Done!
)")
        .def(py::init<>(), "Creates a new pending Promise.")
        .def(
            "resolve",
            [](atom::async::Promise<py::object>& self, py::object value) {
                try {
                    self.setValue(value);
                } catch (const std::exception& e) {
                    throw std::runtime_error(e.what());
                }
            },
            py::arg("value"),
            R"(Resolves the promise with the given value.

Args:
    value: The value to resolve the promise with.

Raises:
    RuntimeError: If the promise is already settled (resolved or rejected).
)")
        .def(
            "reject",
            [](atom::async::Promise<py::object>& self, py::object reason) {
                try {
                    std::string reason_str = reason.cast<std::string>();
                    throw std::runtime_error(reason_str);
                } catch (...) {
                    self.setException(std::current_exception());
                }
            },
            py::arg("reason"),
            R"(Rejects the promise with the given reason.

Args:
    reason: The reason for rejection, typically an exception.

Raises:
    RuntimeError: If the promise is already settled (resolved or rejected).
)")
        .def(
            "is_pending",
            [](const atom::async::Promise<py::object>& self) {
                return !self.isCancelled() &&
                       self.getFuture().wait_for(std::chrono::seconds(0)) ==
                           std::future_status::timeout;
            },
            R"(Checks if the promise is still pending.

Returns:
    bool: True if the promise has not been resolved or rejected yet.
)")
        .def(
            "is_fulfilled",
            [](const atom::async::Promise<py::object>& self) {
                if (self.isCancelled())
                    return false;
                try {
                    auto future = self.getFuture();
                    return future.valid() &&
                           future.wait_for(std::chrono::seconds(0)) ==
                               std::future_status::ready;
                } catch (...) {
                    return false;
                }
            },
            R"(Checks if the promise has been resolved.

Returns:
    bool: True if the promise has been resolved.
)")
        .def(
            "is_rejected",
            [](const atom::async::Promise<py::object>& self) {
                if (self.isCancelled())
                    return true;
                try {
                    auto future = self.getFuture();
                    if (future.valid() &&
                        future.wait_for(std::chrono::seconds(0)) ==
                            std::future_status::ready) {
                        try {
                            future.get();
                            return false;  // 没有异常，不是rejected状态
                        } catch (...) {
                            return true;  // 有异常，是rejected状态
                        }
                    }
                    return false;  // 还没就绪，不是rejected状态
                } catch (...) {
                    return false;
                }
            },
            R"(Checks if the promise has been rejected.

Returns:
    bool: True if the promise has been rejected.
)")
        .def(
            "wait",
            [](atom::async::Promise<py::object>& self,
               unsigned int timeout_ms) {
                auto future = self.getFuture();
                if (timeout_ms == 0) {
                    return future.get();  // 无限等待
                } else {
                    auto status =
                        future.wait_for(std::chrono::milliseconds(timeout_ms));
                    if (status == std::future_status::ready) {
                        return future.get();
                    } else if (status == std::future_status::timeout) {
                        throw std::runtime_error("Promise wait timed out");
                    } else {
                        throw std::runtime_error(
                            "Promise wait failed with unknown status");
                    }
                }
            },
            py::arg("timeout_ms") = 0,
            R"(Waits for the promise to be settled.

Args:
    timeout_ms: Maximum time to wait in milliseconds. 0 means wait indefinitely.

Returns:
    The resolved value if the promise is fulfilled.

Raises:
    Exception: The rejection reason if the promise is rejected.
    TimeoutError: If the timeout is reached before the promise settles.
)")
        .def(
            "then",
            [](atom::async::Promise<py::object>& self,
               py::function on_fulfilled, py::function on_rejected) {
                auto resultPromise =
                    std::make_shared<atom::async::Promise<py::object>>();

                self.onComplete([resultPromise, on_fulfilled,
                                 on_rejected](py::object value) {
                    py::gil_scoped_acquire acquire;
                    try {
                        py::object result = on_fulfilled(value);
                        resultPromise->setValue(result);
                    } catch (const py::error_already_set& e) {
                        try {
                            py::object result = on_rejected(py::str(e.what()));
                            resultPromise->setValue(result);
                        } catch (...) {
                            resultPromise->setException(
                                std::current_exception());
                        }
                    } catch (const std::exception& e) {
                        try {
                            py::object result = on_rejected(py::str(e.what()));
                            resultPromise->setValue(result);
                        } catch (...) {
                            resultPromise->setException(
                                std::current_exception());
                        }
                    }
                });

                return resultPromise;
            },
            py::arg("on_fulfilled"), py::arg("on_rejected") = py::none(),
            R"(Attaches callbacks for the resolution and/or rejection of the Promise.

Args:
    on_fulfilled: The callback to execute when the Promise is resolved.
    on_rejected: Optional callback to execute when the Promise is rejected.

Returns:
    A new Promise that is resolved/rejected with the return value of the called handler.

Examples:
    >>> promise.then(lambda value: print(f"Success: {value}"),
    ...              lambda reason: print(f"Failed: {reason}"))
)")
        .def(
            "catch",
            [](atom::async::Promise<py::object>& self,
               py::function on_rejected) {
                auto resultPromise =
                    std::make_shared<atom::async::Promise<py::object>>();

                self.onComplete([resultPromise, on_rejected](py::object value) {
                    py::gil_scoped_acquire acquire;
                    try {
                        resultPromise->setValue(value);  // 直接传递值
                    } catch (const py::error_already_set& e) {
                        try {
                            py::object result = on_rejected(py::str(e.what()));
                            resultPromise->setValue(result);
                        } catch (...) {
                            resultPromise->setException(
                                std::current_exception());
                        }
                    } catch (const std::exception& e) {
                        try {
                            py::object result = on_rejected(py::str(e.what()));
                            resultPromise->setValue(result);
                        } catch (...) {
                            resultPromise->setException(
                                std::current_exception());
                        }
                    }
                });

                return resultPromise;
            },
            py::arg("on_rejected"),
            R"(Attaches a callback for only the rejection of the Promise.

Args:
    on_rejected: The callback to execute when the Promise is rejected.

Returns:
    A new Promise that is resolved/rejected with the return value of the called handler.

Examples:
    >>> promise.catch(lambda reason: print(f"Failed: {reason}"))
)")
        .def("cancel", &atom::async::Promise<py::object>::cancel,
             R"(Cancels the promise.

Returns:
    bool: True if the promise was successfully cancelled, False otherwise.

Examples:
    >>> promise = Promise()
    >>> success = promise.cancel()
    >>> print(f"Cancelled: {success}")
)")
        .def("is_cancelled", &atom::async::Promise<py::object>::isCancelled,
             R"(Checks if the promise has been cancelled.

Returns:
    bool: True if the promise has been cancelled, False otherwise.

Examples:
    >>> promise = Promise()
    >>> promise.cancel()
    >>> print(promise.is_cancelled())  # True
)")
        .def(
            "on_complete",
            [](atom::async::Promise<py::object>& self, py::function callback) {
                self.onComplete([callback](py::object value) {
                    py::gil_scoped_acquire acquire;
                    callback(value);
                });
            },
            py::arg("callback"),
            R"(Registers a callback to be called when the promise completes.

Args:
    callback: Function to call when the promise resolves with a value.

Examples:
    >>> promise.on_complete(lambda value: print(f"Completed with: {value}"))
)")
        .def(
            "run_async",
            [](atom::async::Promise<py::object>& self, py::function func) {
                self.runAsync([func]() -> py::object {
                    py::gil_scoped_acquire acquire;
                    return func();
                });
            },
            py::arg("func"),
            R"(Runs a function asynchronously and resolves the promise with its result.

Args:
    func: Function to execute asynchronously.

Examples:
    >>> promise = Promise()
    >>> promise.run_async(lambda: "async result")
)");

    // Static Promise methods
    m.def(
        "resolve",
        [](py::object value) {
            auto promise = std::make_shared<atom::async::Promise<py::object>>();
            promise->setValue(value);
            return promise;
        },
        py::arg("value"),
        R"(Creates a Promise that is resolved with the given value.

Args:
    value: The value to resolve the promise with.

Returns:
    A new Promise that is already resolved with the given value.

Examples:
    >>> from atom.async import resolve
    >>> promise = resolve("immediate value")
    >>> promise.is_fulfilled()
    True
)");

    m.def(
        "reject",
        [](py::object reason) {
            auto promise = std::make_shared<atom::async::Promise<py::object>>();
            try {
                throw py::cast<std::string>(reason);
            } catch (...) {
                promise->setException(std::current_exception());
            }
            return promise;
        },
        py::arg("reason"),
        R"(Creates a Promise that is rejected with the given reason.

Args:
    reason: The reason for rejection.

Returns:
    A new Promise that is already rejected with the given reason.

Examples:
    >>> from atom.async import reject
    >>> promise = reject(ValueError("Invalid input"))
    >>> promise.is_rejected()
    True
)");

    m.def(
        "all",
        [](const std::vector<std::shared_ptr<atom::async::Promise<py::object>>>&
               promises) {
            auto allPromise = promiseAll(promises);
            // 将结果转换为py::object封装的Promise
            auto resultPromise =
                std::make_shared<atom::async::Promise<py::object>>();

            allPromise->onComplete(
                [resultPromise](std::vector<py::object> values) {
                    py::gil_scoped_acquire acquire;
                    py::list result;
                    for (const auto& value : values) {
                        result.append(value);
                    }
                    resultPromise->setValue(std::move(result));
                });

            return resultPromise;
        },
        py::arg("promises"),
        R"(Returns a promise that resolves when all the promises in the iterable have resolved.

Args:
    promises: An iterable of promises.

Returns:
    A promise that fulfills with a list of all the resolved values when all promises are resolved,
    or rejects with the reason of the first promise that rejects.

Examples:
    >>> from atom.async import all, resolve
    >>> promise1 = resolve("one")
    >>> promise2 = resolve("two")
    >>> all_promise = all([promise1, promise2])
    >>> all_promise.wait()
    ['one', 'two']
)");

    m.def(
        "race",
        [](const std::vector<std::shared_ptr<atom::async::Promise<py::object>>>&
               promises) { return promiseRace(promises); },
        py::arg("promises"),
        R"(Returns a promise that resolves or rejects as soon as one of the promises resolves or rejects.

Args:
    promises: An iterable of promises.

Returns:
    A promise that adopts the state of the first promise to settle.

Examples:
    >>> import time
    >>> from atom.async import race, Promise
    >>> p1 = Promise()
    >>> p2 = Promise()
    >>> race_promise = race([p1, p2])
    >>> # p2 will resolve first
    >>> def resolve_p1():
    ...     time.sleep(2)
    ...     p1.resolve("p1 done")
    >>> def resolve_p2():
    ...     time.sleep(1)
    ...     p2.resolve("p2 done")
    >>> import threading
    >>> threading.Thread(target=resolve_p1).start()
    >>> threading.Thread(target=resolve_p2).start()
    >>> race_promise.wait()
    'p2 done'
)");

    // Additional utility functions
    m.def(
        "make_ready_promise",
        [](py::object value) {
            return createResolvedPromise<py::object>(value);
        },
        py::arg("value"),
        R"(Creates a promise that is immediately resolved with the given value.

Args:
    value: The value to resolve the promise with.

Returns:
    Promise: A promise that is already resolved.

Examples:
    >>> promise = make_ready_promise("immediate value")
    >>> print(promise.is_fulfilled())  # True
)");

    m.def(
        "make_cancelled_promise",
        []() {
            return createCancelledPromise<py::object>();
        },
        R"(Creates a promise that is immediately cancelled.

Returns:
    Promise: A promise that is already cancelled.

Examples:
    >>> promise = make_cancelled_promise()
    >>> print(promise.is_cancelled())  # True
)");

    m.def(
        "make_promise_from_function",
        [](py::function func) {
            auto promise = std::make_shared<atom::async::Promise<py::object>>();
            promise->runAsync([func]() -> py::object {
                py::gil_scoped_acquire acquire;
                return func();
            });
            return promise;
        },
        py::arg("func"),
        R"(Creates a promise that executes the given function asynchronously.

Args:
    func: Function to execute asynchronously.

Returns:
    Promise: A promise that will be resolved with the function's result.

Examples:
    >>> promise = make_promise_from_function(lambda: "async result")
    >>> result = promise.wait()
    >>> print(result)  # "async result"
)");
}
