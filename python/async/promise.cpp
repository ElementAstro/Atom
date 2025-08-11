#include "atom/async/promise.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// 用于创建一个已解决的Promise
template <typename T>
auto createResolvedPromise(const T& value) {
    auto promise = std::make_shared<atom::async::Promise<T>>();
    promise->setValue(value);
    return promise;
}

// 用于创建一个已拒绝的Promise
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

PYBIND11_MODULE(promise, m) {
    m.doc() =
        "Promise implementation module for asynchronous operations in the atom "
        "package";

    // 注册异常转换
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
                    throw py::cast<std::string>(e.what());
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
                    throw py::cast<std::string>(reason);
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
                        if (!py::isinstance<py::none>(on_fulfilled)) {
                            py::object result = on_fulfilled(value);
                            resultPromise->setValue(result);
                        } else {
                            resultPromise->setValue(value);
                        }
                    } catch (const py::error_already_set& e) {
                        try {
                            if (!py::isinstance<py::none>(on_rejected)) {
                                py::object result =
                                    on_rejected(py::str(e.what()));
                                resultPromise->setValue(result);
                            } else {
                                throw;
                            }
                        } catch (...) {
                            resultPromise->setException(
                                std::current_exception());
                        }
                    } catch (const std::exception& e) {
                        try {
                            if (!py::isinstance<py::none>(on_rejected)) {
                                py::object result =
                                    on_rejected(py::str(e.what()));
                                resultPromise->setValue(result);
                            } else {
                                resultPromise->setException(
                                    std::current_exception());
                            }
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
)");

    // 静态Promise方法
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
}
