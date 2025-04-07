#include "atom/async/message_queue.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Helper to create an io_context wrapper that can be managed by Python
class PyIOContext {
public:
    PyIOContext() : io_context_(), work_(asio::make_work_guard(io_context_)) {
        thread_ = std::thread([this]() { io_context_.run(); });
    }

    ~PyIOContext() {
        work_.reset();
        io_context_.stop();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    asio::io_context& get_io_context() { return io_context_; }

private:
    asio::io_context io_context_;
    asio::executor_work_guard<asio::io_context::executor_type> work_;
    std::thread thread_;
};

// Template to declare MessageQueue for different types
template <typename T>
void declare_message_queue(py::module& m, const std::string& type_name) {
    using MQ = atom::async::MessageQueue<T>;
    using FilterFunc = std::function<bool(const T&)>;
    using CallbackFunc = std::function<void(const T&)>;

    std::string class_name = "MessageQueue" + type_name;

    py::class_<MQ>(
        m, class_name.c_str(),
        R"(A message queue that allows subscribers to receive messages.

This class provides a priority-based message queue with filtering, timeouts,
and support for both synchronous and asynchronous message processing.

Args:
    io_context: The asio::io_context to use for asynchronous operations

Examples:
    >>> from atom.async.message_queue import PyIOContext, MessageQueueString
    >>> 
    >>> # Create an IO context for async operations
    >>> io_context = PyIOContext()
    >>> 
    >>> # Create a message queue
    >>> queue = MessageQueueString(io_context)
    >>> 
    >>> # Define a message handler
    >>> def message_handler(message):
    >>>     print(f"Received: {message}")
    >>> 
    >>> # Subscribe to messages
    >>> queue.subscribe(message_handler, "my_subscriber")
    >>> 
    >>> # Start processing messages
    >>> queue.start_processing()
    >>> 
    >>> # Publish messages
    >>> queue.publish("Hello, world!")
    >>> queue.publish("Another message", 10)  # Higher priority
)")
        .def(py::init<asio::io_context&>(), py::arg("io_context"),
             "Creates a new MessageQueue that uses the given IO context")

        .def(
            "subscribe",
            [](MQ& mq, py::function callback, std::string_view subscriberName,
               int priority, std::optional<py::function> filter,
               std::chrono::milliseconds timeout) {
                // Create C++ callback from Python function
                CallbackFunc cpp_callback = [callback](const T& message) {
                    py::gil_scoped_acquire acquire;
                    try {
                        callback(message);
                    } catch (py::error_already_set& e) {
                        // 使用pybind11推荐的方式处理Python异常
                        PyErr_Clear();
                        // 以下是非阻塞错误报告，与PyErr_WriteUnraisable类似但更安全
                        py::print(
                            py::str(
                                "Unhandled exception in message handler: {}"),
                            py::str(e.what()),
                            py::arg("file") =
                                py::module::import("sys").attr("stderr"));
                    }
                };

                FilterFunc cpp_filter = nullptr;
                if (filter) {
                    cpp_filter = [filter](const T& message) -> bool {
                        py::gil_scoped_acquire acquire;
                        try {
                            return filter.value()(message)
                                .template cast<bool>();
                        } catch (py::error_already_set& e) {
                            // 使用pybind11推荐的方式处理Python异常
                            PyErr_Clear();
                            // 以下是非阻塞错误报告，与PyErr_WriteUnraisable类似但更安全
                            py::print(
                                py::str("Unhandled exception in message "
                                        "filter: {}"),
                                py::str(e.what()),
                                py::arg("file") =
                                    py::module::import("sys").attr("stderr"));
                            return false;
                        }
                    };
                }

                mq.subscribe(cpp_callback, subscriberName, priority, cpp_filter,
                             timeout);
            },
            py::arg("callback"), py::arg("subscriber_name"),
            py::arg("priority") = 0, py::arg("filter") = py::none(),
            py::arg("timeout") = std::chrono::milliseconds::zero(),
            R"(Subscribe to messages with a callback and optional filter and timeout.

Args:
    callback: The callback function to be called when a new message is received
    subscriber_name: The name of the subscriber
    priority: The priority of the subscriber (higher priority receives messages first)
    filter: Optional filter function to only receive messages that match the criteria
    timeout: The maximum time allowed for the subscriber to process a message

Examples:
    >>> # Basic subscription
    >>> queue.subscribe(lambda msg: print(msg), "basic_subscriber")
    >>> 
    >>> # Priority subscription
    >>> queue.subscribe(lambda msg: print(f"High priority: {msg}"), 
    >>>                "high_priority", 10)
    >>> 
    >>> # With filter
    >>> queue.subscribe(lambda msg: print(f"Filtered: {msg}"), 
    >>>                "filtered", 0, 
    >>>                lambda msg: "important" in msg)
    >>> 
    >>> # With timeout
    >>> queue.subscribe(lambda msg: process_message(msg), 
    >>>                "timeout_protected", 0, None, 
    >>>                timeout=5000)  # 5 seconds
)")

        .def(
            "unsubscribe",
            [](MQ& mq, py::function callback) {
                CallbackFunc cpp_callback = [callback](const T&) {
                    // We only need the function address for comparison
                };
                return mq.unsubscribe(cpp_callback);
            },
            py::arg("callback"),
            R"(Unsubscribe from messages using the given callback.

Args:
    callback: The callback function used during subscription

Returns:
    True if subscriber was found and removed, False otherwise

Examples:
    >>> # Define a handler
    >>> def my_handler(msg):
    >>>     print(msg)
    >>> 
    >>> # Subscribe
    >>> queue.subscribe(my_handler, "my_subscriber")
    >>> 
    >>> # Later, unsubscribe
    >>> queue.unsubscribe(my_handler)
)")

        .def("publish", py::overload_cast<const T&, int>(&MQ::publish),
             py::arg("message"), py::arg("priority") = 0,
             R"(Publish a message to the queue.

Args:
    message: The message to publish
    priority: The priority of the message (higher priority messages are handled first)

Examples:
    >>> # Publish a basic message
    >>> queue.publish("Hello, world!")
    >>> 
    >>> # Publish a high-priority message
    >>> queue.publish("Urgent message", 10)
)")

        .def("start_processing", &MQ::startProcessing,
             R"(Start processing messages in the queue.

This method starts a background thread that processes messages as they are published.

Examples:
    >>> queue.start_processing()
)")

        .def("stop_processing", &MQ::stopProcessing,
             R"(Stop processing messages in the queue.

This method stops the background thread that processes messages.

Examples:
    >>> queue.stop_processing()
)")

        .def("get_message_count", &MQ::getMessageCount,
             R"(Get the number of messages currently in the queue.

Returns:
    The number of messages in the queue

Examples:
    >>> count = queue.get_message_count()
    >>> print(f"Messages in queue: {count}")
)")

        .def(
            "get_subscriber_count", &MQ::getSubscriberCount,
            R"(Get the number of subscribers currently subscribed to the queue.

Returns:
    The number of subscribers

Examples:
    >>> count = queue.get_subscriber_count()
    >>> print(f"Number of subscribers: {count}")
)")

        .def(
            "cancel_messages",
            [](MQ& mq, py::function cancel_condition) {
                auto cpp_condition =
                    [cancel_condition](const T& message) -> bool {
                    py::gil_scoped_acquire acquire;
                    try {
                        return cancel_condition(message).template cast<bool>();
                    } catch (py::error_already_set& e) {
                        // 使用pybind11推荐的方式处理Python异常
                        PyErr_Clear();
                        // 以下是非阻塞错误报告，与PyErr_WriteUnraisable类似但更安全
                        py::print(py::str("Unhandled exception in message "
                                          "cancellation: {}"),
                                  py::str(e.what()),
                                  py::arg("file") =
                                      py::module::import("sys").attr("stderr"));
                        return false;
                    }
                };
                return mq.cancelMessages(cpp_condition);
            },
            py::arg("cancel_condition"),
            R"(Cancel specific messages that meet a given condition.

Args:
    cancel_condition: A function that returns True for messages that should be canceled

Returns:
    The number of messages that were canceled

Examples:
    >>> # Cancel all messages containing "error"
    >>> count = queue.cancel_messages(lambda msg: "error" in msg)
    >>> print(f"Canceled {count} messages")
)")

        .def("clear_all_messages", &MQ::clearAllMessages,
             R"(Clear all pending messages in the queue.

Returns:
    The number of messages that were cleared

Examples:
    >>> count = queue.clear_all_messages()
    >>> print(f"Cleared {count} messages from the queue")
)")

        // Define awaitable for use in async/await Python code
        .def(
            "await_message",
            [](MQ& mq, std::optional<py::function> filter) -> py::object {
                // 使用Python的asyncio.Future代替C++协程
                py::object asyncio = py::module::import("asyncio");
                py::object future = asyncio.attr("Future")();

                // 创建C++过滤器
                FilterFunc cpp_filter = nullptr;
                if (filter) {
                    cpp_filter = [filter](const T& message) -> bool {
                        py::gil_scoped_acquire acquire;
                        try {
                            return filter.value()(message)
                                .template cast<bool>();
                        } catch (py::error_already_set& e) {
                            // 使用pybind11推荐的方式处理Python异常
                            PyErr_Clear();
                            // 以下是非阻塞错误报告，与PyErr_WriteUnraisable类似但更安全
                            py::print(
                                py::str(
                                    "Unhandled exception in await filter: {}"),
                                py::str(e.what()),
                                py::arg("file") =
                                    py::module::import("sys").attr("stderr"));
                            return false;
                        }
                    };
                }

                // 创建一次性订阅者
                mq.subscribe(
                    [future](const T& message) {
                        py::gil_scoped_acquire acquire;
                        try {
                            if (!future.attr("done")().cast<bool>()) {
                                future.attr("set_result")(message);
                            }
                        } catch (py::error_already_set& e) {
                            // 使用pybind11推荐的方式处理Python异常
                            PyErr_Clear();
                            // 以下是非阻塞错误报告，与PyErr_WriteUnraisable类似但更安全
                            py::print(
                                py::str("Unhandled exception in awaitable "
                                        "handler: {}"),
                                py::str(e.what()),
                                py::arg("file") =
                                    py::module::import("sys").attr("stderr"));
                        }
                    },
                    "awaitable_subscriber", 0, cpp_filter,
                    std::chrono::milliseconds::zero());

                return future;
            },
            py::arg("filter") = py::none(),
            R"(Create an awaitable for use in async Python code.

Args:
    filter: Optional filter function to only receive messages that match the criteria

Returns:
    An awaitable object for use with Python's await keyword

Examples:
    >>> async def process_messages():
    >>>     # Wait for a message
    >>>     msg = await queue.await_message()
    >>>     print(f"Received: {msg}")
    >>>     
    >>>     # Wait for a filtered message
    >>>     important_msg = await queue.await_message(
    >>>         lambda m: "important" in m)
    >>>     print(f"Important message: {important_msg}")
)")

        // Python-specific methods
        .def("__len__", &MQ::getMessageCount)
        .def("__bool__", [](const MQ& mq) { return mq.getMessageCount() > 0; });
}

PYBIND11_MODULE(message_queue, m) {
    m.doc() = R"pbdoc(
        Asynchronous Message Queue with Priority
        ---------------------------------------

        This module provides a priority-based message queue with filtering, timeouts,
        and support for both synchronous and asynchronous message processing.
        
        The message queue allows components to communicate through a publish-subscribe
        pattern with priority handling, ensuring that high-priority messages are 
        processed before lower-priority ones.
        
        Features:
          - Priority-based message processing
          - Message filtering
          - Subscriber timeout handling
          - Asynchronous message processing
          - Python async/await support
          - Cancellation of pending messages
          
        Example:
            >>> from atom.async.message_queue import PyIOContext, MessageQueueString
            >>> 
            >>> # Create an IO context for async operations
            >>> io_context = PyIOContext()
            >>> 
            >>> # Create a message queue
            >>> queue = MessageQueueString(io_context)
            >>> 
            >>> # Define a message handler
            >>> def message_handler(message):
            >>>     print(f"Received: {message}")
            >>> 
            >>> # Subscribe to messages
            >>> queue.subscribe(message_handler, "my_subscriber")
            >>> 
            >>> # Start processing messages
            >>> queue.start_processing()
            >>> 
            >>> # Publish messages
            >>> queue.publish("Hello, world!")
            >>> queue.publish("Another message", 10)  # Higher priority
            >>> 
            >>> # Using async/await
            >>> async def process_messages():
            >>>     message = await queue.await_message()
            >>>     print(f"Async received: {message}")
    )pbdoc";

    // Register exception translations
    py::register_exception<atom::async::MessageQueueException>(
        m, "MessageQueueException", PyExc_RuntimeError);
    py::register_exception<atom::async::SubscriberException>(
        m, "SubscriberException", PyExc_RuntimeError);
    py::register_exception<atom::async::TimeoutException>(m, "TimeoutException",
                                                          PyExc_TimeoutError);

    // Register general exception translator - 修复了异常层次结构问题
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::async::TimeoutException& e) {
            PyErr_SetString(PyExc_TimeoutError, e.what());
        } catch (const atom::async::SubscriberException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const atom::async::MessageQueueException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Define the IO context wrapper
    py::class_<PyIOContext>(m, "PyIOContext", R"(
        Python-friendly wrapper for asio::io_context.
        
        This class manages a thread that processes asynchronous operations
        for the message queue.
        
        The IO context is automatically started on creation and stopped
        when the object is garbage collected.
        
        Examples:
            >>> io_context = PyIOContext()
            >>> queue = MessageQueueString(io_context)
    )")
        .def(py::init<>(), "Creates an IO context with a background thread");

    // Register message queues for common types
    declare_message_queue<std::string>(m, "String");
    declare_message_queue<int>(m, "Int");
    declare_message_queue<double>(m, "Double");
    declare_message_queue<bool>(m, "Bool");
    declare_message_queue<std::vector<std::string>>(m, "StringList");
    declare_message_queue<std::vector<int>>(m, "IntList");
    declare_message_queue<std::unordered_map<std::string, std::string>>(
        m, "StringDict");

    // Add version information
    m.attr("__version__") = "1.0.0";
}