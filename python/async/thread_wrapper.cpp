#include "atom/async/thread_wrapper.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>
#include <pybind11/stl.h>

#include <thread>
#include <future>
#include <chrono>
#include <optional>
#include <variant>

namespace py = pybind11;

// Helper function for thread pool creation
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency()) 
        : running_(true), thread_count_(num_threads) {
        
        workers_.reserve(num_threads);
        
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    
                    {
                        std::unique_lock lock(queue_mutex_);
                        condition_.wait(lock, [this] { 
                            return !running_ || !tasks_.empty(); 
                        });
                        
                        if (!running_ && tasks_.empty()) {
                            return;
                        }
                        
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    
                    task();
                }
            });
        }
    }
    
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using return_type = std::invoke_result_t<F, Args...>;
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> result = task->get_future();
        
        {
            std::unique_lock lock(queue_mutex_);
            
            if (!running_) {
                throw std::runtime_error("ThreadPool is stopped");
            }
            
            tasks_.emplace([task]() { (*task)(); });
        }
        
        condition_.notify_one();
        return result;
    }
    
    ~ThreadPool() {
        {
            std::unique_lock lock(queue_mutex_);
            running_ = false;
        }
        
        condition_.notify_all();
        
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
    
    size_t thread_count() const {
        return thread_count_;
    }
    
    size_t pending_tasks() const {
        std::unique_lock lock(queue_mutex_);
        return tasks_.size();
    }

private:
    bool running_;
    size_t thread_count_;
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
};

// Helper for task cancellation
struct TaskCancellation {
    std::stop_source source;
    
    TaskCancellation() : source(std::stop_source()) {}
    
    bool request_stop() {
        return source.request_stop();
    }
    
    bool stop_requested() const {
        return source.stop_requested();
    }
    
    std::stop_token get_token() const {
        return source.get_token();
    }
};

// Helper for async task with cancellation support
template<typename R>
class AsyncTask {
public:
    AsyncTask(std::function<R(std::stop_token)> func) 
        : cancellation_(std::make_shared<TaskCancellation>()),
          future_(std::async(std::launch::async, [func, cancellation = cancellation_]() -> R {
              return func(cancellation->get_token());
          })) {}
          
    bool cancel() {
        return cancellation_->request_stop();
    }
    
    bool is_cancelled() const {
        return cancellation_->stop_requested();
    }
    
    std::variant<R, std::exception_ptr> get_result(std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) {
        if (timeout.count() > 0) {
            auto status = future_.wait_for(timeout);
            if (status != std::future_status::ready) {
                throw std::runtime_error("Task not completed within timeout");
            }
        }
        
        try {
            return future_.get();
        } catch (...) {
            return std::current_exception();
        }
    }
    
    template<typename Rep, typename Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& timeout_duration) {
        return future_.wait_for(timeout_duration) == std::future_status::ready;
    }
    
    bool is_ready() {
        return future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }
    
private:
    std::shared_ptr<TaskCancellation> cancellation_;
    std::future<R> future_;
};

PYBIND11_MODULE(thread_wrapper, m) {
    m.doc() = "Thread wrapper implementation module for the atom package";

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

    // Thread class binding
    py::class_<atom::async::Thread>(
        m, "Thread",
        R"(A wrapper class for managing a C++20 jthread with enhanced functionality.

This class provides a convenient interface for managing threads, allowing for
starting, stopping, and joining threads easily.

Examples:
    >>> from atom.async import Thread
    >>> def worker(stop_token, name):
    ...     import time
    ...     print(f"Worker {name} started")
    ...     while not stop_token.stop_requested():
    ...         time.sleep(0.1)
    ...     print(f"Worker {name} stopped")
    >>> thread = Thread()
    >>> thread.start(worker, "thread1")
    >>> # Let it run for a bit
    >>> thread.request_stop()
    >>> thread.join()
)")
        .def(py::init<>(), "Constructs a new Thread object.")
        .def("start", 
             [](atom::async::Thread& self, py::function func, py::args args) {
                 self.start([func, args](std::stop_token st) {
                     // First try with stop_token
                     try {
                         if (py::hasattr(func, "__call__")) {
                             py::object py_stop_token = py::cast(st);
                             func(py_stop_token, *args);
                             return;
                         }
                     } catch (const py::error_already_set&) {
                         // If that fails, try without stop_token
                         PyErr_Clear();
                     }
                     
                     // Call without stop_token
                     func(*args);
                 });
             },
             py::arg("func"),
             R"(Starts a new thread with the specified callable object and arguments.

Args:
    func: The callable object to execute in the new thread.
    *args: The arguments to pass to the callable object.

Raises:
    RuntimeError: If the thread cannot be started.

Examples:
    >>> def task(name):
    ...     print(f"Hello from {name}")
    >>> thread = Thread()
    >>> thread.start(task, "worker thread")
)")
        .def("start_with_result",
             [](atom::async::Thread& self, py::function func, py::args args) {
                 return self.startWithResult<py::object>(
                     [func, args]() -> py::object {
                         return func(*args);
                     });
             },
             py::arg("func"),
             R"(Starts a thread with a function that returns a value.

Args:
    func: The callable object to execute in the new thread.
    *args: The arguments to pass to the callable object.

Returns:
    A future that will contain the result.

Raises:
    RuntimeError: If the thread cannot be started.

Examples:
    >>> def compute(x, y):
    ...     return x + y
    >>> thread = Thread()
    >>> future = thread.start_with_result(compute, 5, 7)
    >>> result = future.result()  # Will be 12
)")
        .def("request_stop", &atom::async::Thread::requestStop,
             R"(Requests the thread to stop execution.

Examples:
    >>> thread.request_stop()  # Signal the thread to stop
)")
        .def("join", &atom::async::Thread::join,
             R"(Waits for the thread to finish execution.

Raises:
    RuntimeError: If joining the thread throws an exception.

Examples:
    >>> thread.join()  # Wait for thread to complete
)")
        .def("try_join_for", 
             [](atom::async::Thread& self, std::chrono::milliseconds timeout) {
                 return self.tryJoinFor(timeout);
             },
             py::arg("timeout"),
             R"(Tries to join the thread with a timeout.

Args:
    timeout: The maximum time to wait in milliseconds.

Returns:
    True if joined successfully, False if timed out.

Examples:
    >>> from atom.async import milliseconds
    >>> thread.try_join_for(milliseconds(500))  # Try to join with 500ms timeout
)")
        .def("running", &atom::async::Thread::running,
             R"(Checks if the thread is currently running.

Returns:
    True if the thread is running, False otherwise.

Examples:
    >>> if thread.running():
    ...     print("Thread is still active")
)")
        .def("get_id", 
             [](const atom::async::Thread& self) {
                 return py::cast(self.getId()).attr("__str__")();
             },
             R"(Gets the ID of the thread.

Returns:
    The ID of the thread as a string.

Examples:
    >>> thread_id = thread.get_id()
    >>> print(f"Thread ID: {thread_id}")
)")
        .def("should_stop", &atom::async::Thread::shouldStop,
             R"(Checks if the thread should stop.

Returns:
    True if the thread should stop, False otherwise.

Examples:
    >>> if thread.should_stop():
    ...     print("Thread has been requested to stop")
)");

    // TaskCancellation class binding
    py::class_<TaskCancellation, std::shared_ptr<TaskCancellation>>(
        m, "TaskCancellation",
        R"(Provides cancellation support for asynchronous tasks.

This class allows requesting cancellation of tasks and checking cancellation status.

Examples:
    >>> from atom.async import TaskCancellation
    >>> cancellation = TaskCancellation()
    >>> # Pass to a task that supports cancellation
    >>> cancellation.request_stop()  # Request cancellation
)")
        .def(py::init<>(), "Constructs a new TaskCancellation object.")
        .def("request_stop", &TaskCancellation::request_stop,
             R"(Requests cancellation.

Returns:
    True if this call made the stop request, false if it was already requested.
)")
        .def("stop_requested", &TaskCancellation::stop_requested,
             R"(Checks if cancellation has been requested.

Returns:
    True if cancellation has been requested, False otherwise.
)");

    // StopToken binding (simplified for Python use)
    py::class_<std::stop_token>(
        m, "StopToken",
        R"(A token that can be used to check if cancellation has been requested.

This token is passed to cancellable tasks to check for cancellation requests.

Examples:
    >>> def cancellable_task(stop_token, args):
    ...     while not stop_token.stop_requested():
    ...         # Do work
    ...         pass
)")
        .def("stop_requested", &std::stop_token::stop_requested,
             R"(Checks if cancellation has been requested.

Returns:
    True if cancellation has been requested, False otherwise.
)");

    // AsyncTask template instantiation for Python object
    py::class_<AsyncTask<py::object>, std::shared_ptr<AsyncTask<py::object>>>(
        m, "AsyncTask",
        R"(An asynchronous task with cancellation support.

This class wraps a function execution in a separate thread, allowing
cancellation and result retrieval.

Examples:
    >>> from atom.async import create_async_task
    >>> def long_task(stop_token):
    ...     import time
    ...     for i in range(10):
    ...         if stop_token.stop_requested():
    ...             return "Cancelled"
    ...         time.sleep(0.1)
    ...     return "Completed"
    >>> task = create_async_task(long_task)
    >>> # Later...
    >>> task.cancel()  # Request cancellation
    >>> result = task.get_result()  # Will be "Cancelled"
)")
        .def("cancel", &AsyncTask<py::object>::cancel,
             R"(Requests cancellation of the task.

Returns:
    True if this call made the stop request, false if it was already requested.
)")
        .def("is_cancelled", &AsyncTask<py::object>::is_cancelled,
             R"(Checks if the task has been cancelled.

Returns:
    True if cancellation has been requested, False otherwise.
)")
        .def("get_result", 
             [](AsyncTask<py::object>& self, std::chrono::milliseconds timeout) {
                 try {
                     auto result = self.get_result(timeout);
                     if (std::holds_alternative<py::object>(result)) {
                         return std::get<py::object>(result);
                     } else {
                         std::rethrow_exception(std::get<std::exception_ptr>(result));
                         return py::object();  // Unreachable
                     }
                 } catch (const std::exception& e) {
                     throw py::error_already_set();
                 }
             },
             py::arg("timeout") = std::chrono::milliseconds(0),
             R"(Gets the result of the task.

Args:
    timeout: Maximum time to wait for the result in milliseconds. 
             0 means wait indefinitely.

Returns:
    The result of the task.

Raises:
    RuntimeError: If the task has not completed within the timeout.
    Exception: Any exception thrown by the task.
)")
        .def("wait_for", 
             [](AsyncTask<py::object>& self, std::chrono::milliseconds timeout) {
                 return self.wait_for(timeout);
             },
             py::arg("timeout"),
             R"(Waits for the task to complete up to the specified timeout.

Args:
    timeout: Maximum time to wait in milliseconds.

Returns:
    True if the task completed, False if it timed out.
)")
        .def("is_ready", &AsyncTask<py::object>::is_ready,
             R"(Checks if the task has completed.

Returns:
    True if the task has completed, False otherwise.
)");

    // ThreadPool class binding
    py::class_<ThreadPool, std::shared_ptr<ThreadPool>>(
        m, "ThreadPool",
        R"(A thread pool for executing tasks in parallel.

This class manages a pool of worker threads and provides an interface for
scheduling tasks to be executed asynchronously.

Args:
    num_threads: Number of worker threads to create. Default is the number of
                 hardware threads available.

Examples:
    >>> from atom.async import ThreadPool
    >>> pool = ThreadPool(4)  # Create a pool with 4 worker threads
    >>> def task(x, y):
    ...     return x + y
    >>> future = pool.enqueue(task, 5, 7)
    >>> result = future.result()  # Will be 12
)")
        .def(py::init<size_t>(), py::arg("num_threads") = std::thread::hardware_concurrency(),
             "Constructs a new ThreadPool with the specified number of threads.")
        .def("enqueue", 
             [](ThreadPool& self, py::function func, py::args args) {
                 return self.enqueue([func, args]() {
                     try {
                         return func(*args);
                     } catch (const py::error_already_set& e) {
                         // Convert Python exception to C++ exception
                         throw std::runtime_error(e.what());
                     }
                 });
             },
             py::arg("func"),
             R"(Schedules a task for execution in the thread pool.

Args:
    func: The callable object to execute.
    *args: The arguments to pass to the callable object.

Returns:
    A future that will contain the result of the task.

Raises:
    RuntimeError: If the thread pool has been stopped.

Examples:
    >>> def compute(x, y):
    ...     return x * y
    >>> future = pool.enqueue(compute, 6, 7)
    >>> result = future.result()  # Will be 42
)")
        .def("thread_count", &ThreadPool::thread_count,
             R"(Gets the number of worker threads in the pool.

Returns:
    The number of worker threads.
)")
        .def("pending_tasks", &ThreadPool::pending_tasks,
             R"(Gets the number of pending tasks in the queue.

Returns:
    The number of pending tasks.
)");

    // Future API for Python use
    py::class_<std::shared_future<py::object>>(
        m, "Future",
        R"(A future representing the result of an asynchronous operation.

This class provides methods to check the status of an asynchronous operation
and retrieve its result when available.

Examples:
    >>> future = thread.start_with_result(lambda: 42)
    >>> result = future.result()  # Will be 42
)")
        .def("result", 
             [](std::shared_future<py::object>& self) {
                 try {
                     return self.get();
                 } catch (const std::exception& e) {
                     throw std::runtime_error(std::string("Task failed: ") + e.what());
                 }
             },
             R"(Gets the result of the asynchronous operation.

Returns:
    The result of the operation.

Raises:
    RuntimeError: If the operation failed with an exception.
)")
        .def("wait", 
             [](std::shared_future<py::object>& self) {
                 self.wait();
             },
             R"(Waits for the operation to complete.

This method blocks until the operation completes.
)")
        .def("wait_for", 
             [](std::shared_future<py::object>& self, std::chrono::milliseconds timeout) {
                 return self.wait_for(timeout) == std::future_status::ready;
             },
             py::arg("timeout"),
             R"(Waits for the operation to complete up to the specified timeout.

Args:
    timeout: Maximum time to wait in milliseconds.

Returns:
    True if the operation completed, False if it timed out.
)")
        .def("is_ready", 
             [](std::shared_future<py::object>& self) {
                 return self.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
             },
             R"(Checks if the operation has completed.

Returns:
    True if the operation has completed, False otherwise.
)");

    // Task class binding (simplified version)
    py::class_<atom::async::Task<py::object>>(
        m, "Task", 
        R"(A simple C++20 coroutine task wrapper.

This class represents an asynchronous task using C++20 coroutines.
)")
        .def(py::init<>([](py::function func) {
            // Simple implementation for demonstration
            auto task = atom::async::Task<py::object>();
            std::thread([func]() { 
                try {
                    func();
                } catch (...) {
                    // Exception handling
                }
            }).detach();
            return task;
        }), py::arg("func"), "Creates a new Task from a function.");

    // Factory functions
    m.def("create_thread", 
          []() { return std::make_unique<atom::async::Thread>(); },
          R"(Creates a new Thread object.

Returns:
    A new Thread instance.

Examples:
    >>> from atom.async import create_thread
    >>> thread = create_thread()
)");

    m.def("create_thread_pool", 
          [](size_t num_threads) { 
              return std::make_shared<ThreadPool>(num_threads); 
          },
          py::arg("num_threads") = std::thread::hardware_concurrency(),
          R"(Creates a new ThreadPool object.

Args:
    num_threads: Number of worker threads to create. Default is the number of
                 hardware threads available.

Returns:
    A new ThreadPool instance.

Examples:
    >>> from atom.async import create_thread_pool
    >>> pool = create_thread_pool(8)  # 8 worker threads
)");

    m.def("create_async_task",
          [](py::function func) {
              return std::make_shared<AsyncTask<py::object>>(
                  [func](std::stop_token st) -> py::object {
                      try {
                          // Try with stop token
                          return func(py::cast(st));
                      } catch (const py::error_already_set&) {
                          // If that fails, try without stop token
                          PyErr_Clear();
                          return func();
                      }
                  });
          },
          py::arg("func"),
          R"(Creates a new AsyncTask with cancellation support.

Args:
    func: Function to execute asynchronously. Can take a stop_token parameter.

Returns:
    A new AsyncTask instance.

Examples:
    >>> from atom.async import create_async_task
    >>> def task(stop_token=None):
    ...     while not (stop_token and stop_token.stop_requested()):
    ...         # Do work
    ...         pass
    ...     return "Done"
    >>> async_task = create_async_task(task)
)");

    m.def("current_thread_id", 
          []() {
              return py::cast(std::this_thread::get_id()).attr("__str__")();
          },
          R"(Gets the ID of the current thread.

Returns:
    The ID of the current thread as a string.

Examples:
    >>> from atom.async import current_thread_id
    >>> print(f"Current thread ID: {current_thread_id()}")
)");

    m.def("sleep", 
          [](std::chrono::milliseconds duration) {
              std::this_thread::sleep_for(duration);
          },
          py::arg("duration"),
          R"(Suspends the current thread for the specified duration.

Args:
    duration: Time to sleep in milliseconds.

Examples:
    >>> from atom.async import sleep, milliseconds
    >>> sleep(milliseconds(500))  # Sleep for 500ms
)");

    m.def("yield_now", 
          []() {
              std::this_thread::yield();
          },
          R"(Suggests that the implementation reschedules execution of threads.

This function is used to improve performance by avoiding thread busy-waiting.

Examples:
    >>> from atom.async import yield_now
    >>> yield_now()  # Yield to other threads
)");

    // Helper functions for time durations
    m.def("milliseconds", 
          [](long long ms) { return std::chrono::milliseconds(ms); },
          py::arg("ms"),
          R"(Creates a milliseconds duration.

Args:
    ms: Number of milliseconds.

Returns:
    A duration representing the specified number of milliseconds.

Examples:
    >>> from atom.async import milliseconds
    >>> duration = milliseconds(500)  # 500ms
)");

    m.def("seconds", 
          [](long long s) { return std::chrono::seconds(s); },
          py::arg("s"),
          R"(Creates a seconds duration.

Args:
    s: Number of seconds.

Returns:
    A duration representing the specified number of seconds.

Examples:
    >>> from atom.async import seconds
    >>> duration = seconds(2)  # 2 seconds
)");

    m.def("minutes", 
          [](long long m) { return std::chrono::minutes(m); },
          py::arg("m"),
          R"(Creates a minutes duration.

Args:
    m: Number of minutes.

Returns:
    A duration representing the specified number of minutes.

Examples:
    >>> from atom.async import minutes
    >>> duration = minutes(5)  # 5 minutes
)");

    m.def("hardware_concurrency", 
          []() { return std::thread::hardware_concurrency(); },
          R"(Gets the number of concurrent threads supported by the implementation.

Returns:
    The number of concurrent threads supported, or 0 if the value is not well defined.

Examples:
    >>> from atom.async import hardware_concurrency
    >>> num_threads = hardware_concurrency()
    >>> print(f"System supports {num_threads} concurrent threads")
)");

    // Run in background convenience function
    m.def("run_in_background",
          [](py::function func, py::args args) {
              auto thread = std::make_shared<atom::async::Thread>();
              thread->start([func, args]() {
                  try {
                      func(*args);
                  } catch (const py::error_already_set& e) {
                      // Log Python exception
                      PyErr_WriteUnraisable(func.ptr());
                  } catch (const std::exception& e) {
                      // Log C++ exception
                  }
              });
              return thread;
          },
          py::arg("func"),
          R"(Runs a function in a background thread.

Args:
    func: The function to run.
    *args: Arguments to pass to the function.

Returns:
    A Thread object that can be used to manage the background thread.

Examples:
    >>> from atom.async import run_in_background
    >>> def background_task(name):
    ...     print(f"Running task: {name}")
    >>> thread = run_in_background(background_task, "background process")
)");
}