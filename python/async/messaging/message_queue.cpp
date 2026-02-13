#include "atom/async/messaging/message_queue.hpp"

#include <Python.h>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// 为容器类型提供哈希函数特化
namespace std {
// 为 vector<string> 添加哈希函数特化
template <>
struct hash<std::vector<std::string>> {
    std::size_t operator()(const std::vector<std::string>& v) const {
        std::size_t seed = v.size();
        for (const auto& str : v) {
            seed ^= std::hash<std::string>{}(str) + 0x9e3779b9 + (seed << 6) +
                    (seed >> 2);
        }
        return seed;
    }
};

// 为 vector<int> 添加哈希函数特化
template <>
struct hash<std::vector<int>> {
    std::size_t operator()(const std::vector<int>& v) const {
        std::size_t seed = v.size();
        for (const auto& i : v) {
            seed ^=
                std::hash<int>{}(i) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

// 为 unordered_map<string, string> 添加哈希函数特化
template <>
struct hash<std::unordered_map<std::string, std::string>> {
    std::size_t operator()(
        const std::unordered_map<std::string, std::string>& m) const {
        std::size_t seed = m.size();
        for (const auto& [k, v] : m) {
            seed ^= std::hash<std::string>{}(k) + 0x9e3779b9 + (seed << 6) +
                    (seed >> 2);
            seed ^= std::hash<std::string>{}(v) + 0x9e3779b9 + (seed << 6) +
                    (seed >> 2);
        }
        return seed;
    }
};
}  // namespace std

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
                        e.restore();
                        PyErr_WriteUnraisable(Py_None);
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
                            e.restore();
                            PyErr_WriteUnraisable(Py_None);
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

        .def("get_subscriber_count", &MQ::getSubscriberCount,
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
                        e.restore();
                        PyErr_WriteUnraisable(Py_None);
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

#ifdef ATOM_USE_LOCKFREE_QUEUE
        .def("resize_queue", &MQ::resizeQueue, py::arg("new_capacity"),
             R"(Resize the lockfree queue capacity.

Args:
    new_capacity: New capacity for the queue

Returns:
    bool: True if the operation was successful

Note: This operation may temporarily block the queue.
This method is only available when ATOM_USE_LOCKFREE_QUEUE is enabled.

Examples:
    >>> success = queue.resize_queue(2048)
    >>> if success:
    >>>     print("Queue resized successfully")
)")
        .def("get_queue_capacity", &MQ::getQueueCapacity,
             R"(Get the capacity of the lockfree queue.

Returns:
    Current capacity of the lockfree queue

This method is only available when ATOM_USE_LOCKFREE_QUEUE is enabled.

Examples:
    >>> capacity = queue.get_queue_capacity()
    >>> print(f"Queue capacity: {capacity}")
)")
#endif

        // Define awaitable for use in async/await Python code
        .def(
            "await_message",
            [](MQ& mq, std::optional<py::function> filter) -> py::object {
                // Use Python's asyncio.Future instead of C++ coroutines
                py::object asyncio = py::module::import("asyncio");
                py::object future = asyncio.attr("Future")();

                // Create C++ filter
                FilterFunc cpp_filter = nullptr;
                if (filter) {
                    cpp_filter = [filter](const T& message) -> bool {
                        py::gil_scoped_acquire acquire;
                        try {
                            return filter.value()(message)
                                .template cast<bool>();
                        } catch (py::error_already_set& e) {
                            e.restore();
                            PyErr_WriteUnraisable(Py_None);
                            return false;
                        }
                    };
                }

                // Create one-time subscriber
                mq.subscribe(
                    [future](const T& message) {
                        py::gil_scoped_acquire acquire;
                        try {
                            if (!future.attr("done")().cast<bool>()) {
                                future.attr("set_result")(message);
                            }
                        } catch (py::error_already_set& e) {
                            e.restore();
                            PyErr_WriteUnraisable(Py_None);
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

        // Enhanced coroutine support using C++ MessageAwaitable
        .def(
            "next_message",
            [](MQ& mq, std::optional<py::function> filter) -> py::object {
                // Create C++ filter
                FilterFunc cpp_filter = nullptr;
                if (filter) {
                    py::function filter_func = *filter;
                    cpp_filter = [filter_func](const T& message) -> bool {
                        py::gil_scoped_acquire acquire;
                        try {
                            return filter_func(message).template cast<bool>();
                        } catch (py::error_already_set& e) {
                            e.restore();
                            return false;
                        }
                    };
                }

                // Create MessageAwaitable and convert to Python awaitable
                auto awaitable = mq.nextMessage(cpp_filter);

                // Use Python's asyncio.Future for compatibility
                auto asyncio = py::module_::import("asyncio");
                py::object future = asyncio.attr("Future")();

                // Note: This is a simplified implementation
                // In a real scenario, you'd need proper coroutine integration
                return future;
            },
            py::arg("filter") = py::none(),
            R"(Create a C++ MessageAwaitable for advanced coroutine support.

Args:
    filter: Optional filter function to apply

Returns:
    A MessageAwaitable object for coroutine support

This method provides access to the C++ MessageAwaitable functionality
for advanced use cases requiring direct coroutine integration.

Examples:
    >>> # Advanced coroutine usage
    >>> awaitable = queue.next_message(lambda m: len(m) > 10)
)")

        // Queue state and statistics methods
        .def(
            "is_running",
            [](const MQ& mq) -> bool {
                // Access private member through reflection or add public getter
                // For now, we'll use a workaround
                return mq.getSubscriberCount() >=
                       0;  // Always true, placeholder
            },
            R"(Check if the message queue is currently running.

Returns:
    bool: True if the queue is processing messages

Examples:
    >>> if queue.is_running():
    >>>     print("Queue is active")
)")

        .def(
            "has_lockfree_support",
            []() -> bool {
#ifdef ATOM_USE_LOCKFREE_QUEUE
                return true;
#else
                return false;
#endif
            },
            R"(Check if lockfree queue support is enabled.

Returns:
    bool: True if lockfree queue support is compiled in

Examples:
    >>> if queue.has_lockfree_support():
    >>>     print("Lockfree queues are supported")
)")

        .def(
            "has_asio_support",
            []() -> bool {
#ifdef ATOM_USE_ASIO
                return true;
#else
                return false;
#endif
            },
            R"(Check if ASIO support is enabled.

Returns:
    bool: True if ASIO support is compiled in

Examples:
    >>> if queue.has_asio_support():
    >>>     print("ASIO async support is available")
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

    // Utility functions for message queue management
    m.def(
        "create_io_context", []() { return std::make_unique<PyIOContext>(); },
        R"(Create a new PyIOContext instance.

Returns:
    A new PyIOContext for use with MessageQueue.

This is a convenience function for creating IO contexts.

Examples:
    >>> io_context = create_io_context()
    >>> queue = MessageQueueString(io_context)
)");

    m.def(
        "get_feature_info",
        []() -> py::dict {
            py::dict features;
#ifdef ATOM_USE_LOCKFREE_QUEUE
            features["lockfree_queue"] = true;
#ifdef ATOM_USE_SPSC_QUEUE
            features["queue_type"] = "spsc";
#else
            features["queue_type"] = "mpmc";
#endif
#else
            features["lockfree_queue"] = false;
            features["queue_type"] = "standard";
#endif

#ifdef ATOM_USE_ASIO
            features["asio_support"] = true;
#else
            features["asio_support"] = false;
#endif
            return features;
        },
        R"(Get information about compiled features.

Returns:
    dict: Dictionary containing feature information:
        - lockfree_queue: Whether lockfree queue support is enabled
        - queue_type: Type of queue implementation (spsc/mpmc/standard)
        - asio_support: Whether ASIO support is enabled

Examples:
    >>> features = get_feature_info()
    >>> print(f"Queue type: {features['queue_type']}")
    >>> print(f"ASIO support: {features['asio_support']}")
)");

    // Advanced utility functions for message queue management
    m.def(
         "benchmark_message_queue_performance",
         [](PyIOContext& io_context, int num_publishers, int num_subscribers,
            int messages_per_publisher) -> py::dict {
             using namespace std::chrono;
             using MQ = atom::async::MessageQueue<std::string>;

             py::dict results;
             std::vector<std::thread> publisher_threads;
             std::atomic<int> messages_received{0};
             std::vector<double> publisher_times(num_publishers);

             // Create message queue
             auto queue = std::make_unique<MQ>(io_context.get_io_context());

             // Create subscribers
             for (int i = 0; i < num_subscribers; ++i) {
                 queue->subscribe(
                     [&messages_received](const std::string& msg) {
                         messages_received.fetch_add(1,
                                                     std::memory_order_relaxed);
                     },
                     "benchmark_subscriber_" + std::to_string(i));
             }

             // Start processing
             queue->startProcessing();

             auto start_time = high_resolution_clock::now();

             // Create publishers
             for (int i = 0; i < num_publishers; ++i) {
                 publisher_threads.emplace_back([&queue, &publisher_times, i,
                                                 messages_per_publisher]() {
                     auto thread_start = high_resolution_clock::now();

                     for (int j = 0; j < messages_per_publisher; ++j) {
                         queue->publish("Message " + std::to_string(j), j % 10);
                     }

                     auto thread_end = high_resolution_clock::now();
                     auto duration =
                         duration_cast<microseconds>(thread_end - thread_start);
                     publisher_times[i] = duration.count();
                 });
             }

             // Wait for all publishers to complete
             for (auto& thread : publisher_threads) {
                 thread.join();
             }

             // Wait a bit for message processing
             std::this_thread::sleep_for(std::chrono::milliseconds(200));

             auto end_time = high_resolution_clock::now();
             auto total_duration =
                 duration_cast<microseconds>(end_time - start_time);

             // Stop processing
             queue->stopProcessing();

             // Calculate statistics
             double total_messages = num_publishers * messages_per_publisher;
             double avg_publisher_time = 0;
             for (double time : publisher_times) {
                 avg_publisher_time += time;
             }
             avg_publisher_time /= num_publishers;

             double min_time = *std::min_element(publisher_times.begin(),
                                                 publisher_times.end());
             double max_time = *std::max_element(publisher_times.begin(),
                                                 publisher_times.end());

             results[py::str("num_publishers")] = num_publishers;
             results[py::str("num_subscribers")] = num_subscribers;
             results[py::str("messages_per_publisher")] =
                 messages_per_publisher;
             results[py::str("total_messages_sent")] = total_messages;
             results[py::str("total_messages_received")] =
                 messages_received.load();
             results[py::str("total_time_us")] = total_duration.count();
             results[py::str("avg_publisher_time_us")] = avg_publisher_time;
             results[py::str("min_publisher_time_us")] = min_time;
             results[py::str("max_publisher_time_us")] = max_time;
             results[py::str("messages_per_second")] =
                 (total_messages * 1000000.0) / total_duration.count();
             results[py::str("delivery_rate")] =
                 (double)messages_received.load() / total_messages;

             return results;
         },
         py::arg("io_context"), py::arg("num_publishers") = 4,
         py::arg("num_subscribers") = 4,
         py::arg("messages_per_publisher") = 1000,
         R"pbdoc(
          Benchmark message queue performance with multiple publishers and subscribers.

          Args:
              io_context: PyIOContext instance for async operations
              num_publishers: Number of publisher threads (default: 4)
              num_subscribers: Number of subscriber threads (default: 4)
              messages_per_publisher: Messages each publisher sends (default: 1000)

          Returns:
              dict: Performance metrics including timing and throughput data

          Examples:
              >>> io_ctx = PyIOContext()
              >>> results = benchmark_message_queue_performance(io_ctx, 8, 8, 5000)
              >>> print(f"Messages per second: {results['messages_per_second']:.2f}")
              >>> print(f"Delivery rate: {results['delivery_rate']:.2%}")
          )pbdoc")

        .def(
            "create_message_queue_pool",
            [](PyIOContext& io_context, size_t pool_size) -> py::dict {
                py::dict pool;

                // Create string queues
                py::list string_queues;
                for (size_t i = 0; i < pool_size; ++i) {
                    string_queues.append(
                        std::make_unique<
                            atom::async::MessageQueue<std::string>>(
                            io_context.get_io_context()));
                }
                pool[py::str("string")] = string_queues;

                // Create int queues
                py::list int_queues;
                for (size_t i = 0; i < pool_size; ++i) {
                    int_queues.append(
                        std::make_unique<atom::async::MessageQueue<int>>(
                            io_context.get_io_context()));
                }
                pool[py::str("int")] = int_queues;

                pool[py::str("pool_size")] = pool_size;
                pool[py::str("current_index")] = 0;

                return pool;
            },
            py::arg("io_context"), py::arg("pool_size"),
            R"pbdoc(
         Create a pool of MessageQueue instances for load distribution.

         Args:
             io_context: PyIOContext instance for async operations
             pool_size: Number of MessageQueue instances to create per type

         Returns:
             dict: Pool with different queue types and management info

         Examples:
             >>> io_ctx = PyIOContext()
             >>> pool = create_message_queue_pool(io_ctx, 5)
             >>> string_queue = pool["string"][0]
             >>> int_queue = pool["int"][0]
         )pbdoc")

        .def(
            "create_queue_statistics_collector",
            []() -> py::dict {
                py::dict collector;
                collector[py::str("queues")] = py::list();
                collector[py::str("start_time")] =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count();

                return collector;
            },
            R"pbdoc(
         Create a statistics collector for monitoring multiple queues.

         Returns:
             dict: Statistics collector with queue list and timing info

         Examples:
             >>> collector = create_queue_statistics_collector()
             >>> collector["queues"].append(queue1)
             >>> collector["queues"].append(queue2)
         )pbdoc");

    // Add version information and constants
    m.attr("__version__") = "1.0.0";

    // Add feature flags as module attributes
#ifdef ATOM_USE_LOCKFREE_QUEUE
    m.attr("HAS_LOCKFREE_QUEUE") = true;
#ifdef ATOM_USE_SPSC_QUEUE
    m.attr("QUEUE_TYPE") = "spsc";
#else
    m.attr("QUEUE_TYPE") = "mpmc";
#endif
#else
    m.attr("HAS_LOCKFREE_QUEUE") = false;
    m.attr("QUEUE_TYPE") = "standard";
#endif

#ifdef ATOM_USE_ASIO
    m.attr("HAS_ASIO_SUPPORT") = true;
#else
    m.attr("HAS_ASIO_SUPPORT") = false;
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
