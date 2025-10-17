// message_bus_bindings.cpp
#include "atom/async/message_bus.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Helper template for creating Python message handlers for different types
template <typename MessageType>
py::function create_message_handler(py::function py_handler) {
    return py::cpp_function([py_handler](const MessageType& message) {
        py::gil_scoped_acquire acquire;
        try {
            py_handler(message);
        } catch (py::error_already_set& e) {
            // Pass the Python handler function as context to
            // PyErr_WriteUnraisable
            PyErr_WriteUnraisable(py_handler.ptr());
        }
    });
}

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

// Template to declare message type subscribers
template <typename MessageType>
void declare_message_type(py::module& m, const std::string& type_name) {
    // Create a typed wrapper for the subscribe method
    m.def(("subscribe_" + type_name).c_str(),
          [](std::shared_ptr<atom::async::MessageBus> bus,
             std::string_view name, py::function handler, bool async, bool once,
             std::optional<py::function> filter) {
              // Create filter function if provided
              std::function<bool(const MessageType&)> cpp_filter;
              if (filter) {
                  // Capture the Python filter function by value for the lambda
                  py::function py_filter_func = filter.value();
                  cpp_filter = [py_filter_func](const MessageType& msg) {
                      py::gil_scoped_acquire acquire;
                      try {
                          return py_filter_func(msg).template cast<bool>();
                      } catch (py::error_already_set& e) {
                          // Pass the Python filter function as context
                          PyErr_WriteUnraisable(py_filter_func.ptr());
                          return false;
                      }
                  };
              } else {
                  cpp_filter = [](const MessageType&) { return true; };
              }

              // Create and register the handler
              // Capture the Python handler function by value for the lambda
              py::function py_handler_func = handler;
              auto cpp_handler = [py_handler_func](const MessageType& msg) {
                  py::gil_scoped_acquire acquire;
                  try {
                      py_handler_func(msg);
                  } catch (py::error_already_set& e) {
                      // Pass the Python handler function as context
                      PyErr_WriteUnraisable(py_handler_func.ptr());
                  }
              };

              return bus->subscribe<MessageType>(name, cpp_handler, async, once,
                                                 cpp_filter);
          },
          py::arg("bus"), py::arg("name"), py::arg("handler"),
          py::arg("async") = true, py::arg("once") = false,
          py::arg("filter") = py::none(),
          R"(Subscribe to a specific message type.

Args:
    bus: The MessageBus instance
    name: The name of the message or namespace (supports hierarchical routing)
    handler: The function to call when a message is received
    async: Whether to call the handler asynchronously (default: True)
    once: Whether to unsubscribe after the first message (default: False)
    filter: Optional function to filter messages (default: None)

Returns:
    A token that can be used to unsubscribe

Examples:
    >>> def handler(message):
    >>>     print(f"Received: {message}")
    >>>
    >>> # Subscribe to string messages
    >>> token = subscribe_string(bus, "notifications.system", handler)
    >>>
    >>> # Unsubscribe later
    >>> unsubscribe_string(bus, token)
)");

    // Create a typed wrapper for the unsubscribe method
    m.def(("unsubscribe_" + type_name).c_str(),
          [](std::shared_ptr<atom::async::MessageBus> bus,
             atom::async::MessageBus::Token token) {
              bus->unsubscribe<MessageType>(token);
          },
          py::arg("bus"), py::arg("token"),
          R"(Unsubscribe from a specific message type.

Args:
    bus: The MessageBus instance
    token: The subscription token returned from subscribe

Examples:
    >>> # Unsubscribe using the token
    >>> unsubscribe_string(bus, token)
)");

    // Create a typed wrapper for the unsubscribeAll method
    m.def(("unsubscribe_all_" + type_name).c_str(),
          [](std::shared_ptr<atom::async::MessageBus> bus,
             std::string_view name) { bus->unsubscribeAll<MessageType>(name); },
          py::arg("bus"), py::arg("name"),
          R"(Unsubscribe all handlers for a specific message name or namespace.

Args:
    bus: The MessageBus instance
    name: The name of the message or namespace

Examples:
    >>> # Unsubscribe all handlers for a namespace
    >>> unsubscribe_all_string(bus, "notifications")
)");

    // Create a typed wrapper for the publish method
    m.def(("publish_" + type_name).c_str(),
          [](std::shared_ptr<atom::async::MessageBus> bus,
             std::string_view name, const MessageType& message,
             std::optional<std::chrono::milliseconds> delay = std::nullopt) {
              bus->publish<MessageType>(name, message, delay);
          },
          py::arg("bus"), py::arg("name"), py::arg("message"),
          py::arg("delay") = py::none(),
          R"(Publish a message of a specific type.

Args:
    bus: The MessageBus instance
    name: The name of the message
    message: The message to publish
    delay: Optional delay before publishing the message (in milliseconds)

Examples:
    >>> # Publish a string message
    >>> publish_string(bus, "notifications.system", "System is shutting down")
    >>>
    >>> # Publish with delay
    >>> publish_string(bus, "notifications.system", "Delayed message", 5000)
)");

    // Create a typed wrapper for the publishGlobal method
    m.def(("publish_global_" + type_name).c_str(),
          [](std::shared_ptr<atom::async::MessageBus> bus,
             const MessageType& message) {
              bus->publishGlobal<MessageType>(message);
          },
          py::arg("bus"), py::arg("message"),
          R"(Publish a message to all subscribers of a specific type.

Args:
    bus: The MessageBus instance
    message: The message to publish

Examples:
    >>> # Publish to all string subscribers
    >>> publish_global_string(bus, "Global announcement")
)");

    // Create a typed wrapper for the getSubscriberCount method
    m.def(("get_subscriber_count_" + type_name).c_str(),
          [](std::shared_ptr<atom::async::MessageBus> bus,
             std::string_view name) {
              return bus->getSubscriberCount<MessageType>(name);
          },
          py::arg("bus"), py::arg("name"),
          R"(Get the number of subscribers for a specific message or namespace.

Args:
    bus: The MessageBus instance
    name: The name of the message or namespace

Returns:
    The number of subscribers

Examples:
    >>> # Check how many subscribers are listening
    >>> count = get_subscriber_count_string(bus, "notifications")
)");

    // Create a typed wrapper for the hasSubscriber method
    m.def(
        ("has_subscriber_" + type_name).c_str(),
        [](std::shared_ptr<atom::async::MessageBus> bus,
           std::string_view name) {
            return bus->hasSubscriber<MessageType>(name);
        },
        py::arg("bus"), py::arg("name"),
        R"(Check if there are any subscribers for a specific message or namespace.

Args:
    bus: The MessageBus instance
    name: The name of the message or namespace

Returns:
    True if there are subscribers, False otherwise

Examples:
    >>> # Check if anyone is listening
    >>> if has_subscriber_string(bus, "notifications.error"):
    >>>     publish_string(bus, "notifications.error", "An error occurred")
)");

    // Create a typed wrapper for the getMessageHistory method
    m.def(
        ("get_message_history_" + type_name).c_str(),
        [](std::shared_ptr<atom::async::MessageBus> bus, std::string_view name,
           std::size_t count = atom::async::MessageBus::K_MAX_HISTORY_SIZE) {
            return bus->getMessageHistory<MessageType>(name, count);
        },
        py::arg("bus"), py::arg("name"),
        py::arg("count") = atom::async::MessageBus::K_MAX_HISTORY_SIZE,
        R"(Get the message history for a specific message name.

Args:
    bus: The MessageBus instance
    name: The name of the message
    count: Maximum number of messages to return (default: 100)

Returns:
    A list of messages

Examples:
    >>> # Get the last 5 notification messages
    >>> messages = get_message_history_string(bus, "notifications", 5)
    >>> for msg in messages:
    >>>     print(msg)
)");
}

PYBIND11_MODULE(message_bus, m) {
    m.doc() = R"pbdoc(
        Asynchronous Message Bus with Hierarchical Routing
        -------------------------------------------------

        This module provides an event-driven communication system with
        hierarchical routing, filtering, and asynchronous message handling.

        The message bus allows components to communicate without direct coupling,
        enabling a modular, extensible architecture.

        Features:
          - Hierarchical message routing with namespace support
          - Type-safe message passing
          - Asynchronous message handling
          - Message filtering
          - Message history tracking
          - Delayed message publishing

        Example:
            >>> from atom.async.message_bus import PyIOContext, MessageBus
            >>> from atom.async.message_bus import publish_string, subscribe_string
            >>>
            >>> # Create an IO context for async operations
            >>> io_context = PyIOContext()
            >>>
            >>> # Create a message bus
            >>> bus = MessageBus.create_shared(io_context)
            >>>
            >>> # Define a message handler
            >>> def message_handler(message):
            >>>     print(f"Received: {message}")
            >>>
            >>> # Subscribe to a message type
            >>> token = subscribe_string(bus, "system.notifications", message_handler)
            >>>
            >>> # Publish a message
            >>> publish_string(bus, "system.notifications", "Hello from the message bus!")
            >>>
            >>> # Publish with delay (5 seconds)
            >>> publish_string(bus, "system.notifications", "Delayed message", 5000)
    )pbdoc";

    // Register exception translations
    py::register_exception<atom::async::MessageBusException>(
        m, "MessageBusException", PyExc_RuntimeError);

    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::async::MessageBusException& e) {
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
        for the message bus.

        The IO context is automatically started on creation and stopped
        when the object is garbage collected.

        Examples:
            >>> io_context = PyIOContext()
            >>> bus = MessageBus.create_shared(io_context)
    )")
        .def(py::init<>(), "Creates an IO context with a background thread");

    // Define the MessageBus class
    py::class_<atom::async::MessageBus,
               std::shared_ptr<atom::async::MessageBus>>(m, "MessageBus", R"(
        A message bus for asynchronous event-driven communication.

        The MessageBus provides a way for components to communicate without
        direct coupling, using a publish-subscribe pattern with hierarchical routing.

        Examples:
            >>> io_context = PyIOContext()
            >>> bus = MessageBus.create_shared(io_context)
    )")
        .def_static(
            "create_shared",
            [](PyIOContext& py_io_context) {
                return atom::async::MessageBus::createShared(
                    py_io_context.get_io_context());
            },
            py::arg("io_context"), "Creates a shared instance of MessageBus")
        .def("clear_all_subscribers",
             &atom::async::MessageBus::clearAllSubscribers,
             "Clears all subscribers from the message bus")
        .def("get_active_namespaces",
             &atom::async::MessageBus::getActiveNamespaces,
             R"(Gets the list of active namespaces.

Returns:
    A list of active namespace names

Examples:
    >>> namespaces = bus.get_active_namespaces()
    >>> for ns in namespaces:
    >>>     print(f"Active namespace: {ns}")
)")
        .def("is_active", &atom::async::MessageBus::isActive,
             R"(Checks if the message bus is currently active.

Returns:
    bool: True if the message bus is active and processing messages.

This is particularly useful when using lock-free queues to check
if the background processing is running.
)")
        .def("get_statistics",
             [](const atom::async::MessageBus& self) -> py::dict {
                 auto stats = self.getStatistics();
                 py::dict result;
                 result["subscriber_count"] = stats.subscriberCount;
                 result["type_count"] = stats.typeCount;
                 result["namespace_count"] = stats.namespaceCount;
                 result["history_total_messages"] = stats.historyTotalMessages;
#ifdef ATOM_USE_LOCKFREE_QUEUE
                 result["pending_queue_size_approx"] = stats.pendingQueueSizeApprox;
#endif
                 return result;
             },
             R"(Gets comprehensive statistics about the message bus.

Returns:
    dict: Dictionary containing:
        - subscriber_count: Total number of active subscribers
        - type_count: Number of different message types registered
        - namespace_count: Number of active namespaces
        - history_total_messages: Total messages stored in history
        - pending_queue_size_approx: Approximate pending queue size (if lock-free)

Examples:
    >>> stats = bus.get_statistics()
    >>> print(f"Active subscribers: {stats['subscriber_count']}")
    >>> print(f"Message types: {stats['type_count']}")
)")
#ifdef ATOM_USE_LOCKFREE_QUEUE
        .def("start_message_processing", &atom::async::MessageBus::startMessageProcessing,
             R"(Starts the background message processing for lock-free queues.

This method is only available when ATOM_USE_LOCKFREE_QUEUE is enabled.
It starts a background thread or async task to process queued messages.

Note: This is automatically called when the message bus is created,
but can be used to restart processing if it was stopped.
)")
        .def("stop_message_processing", &atom::async::MessageBus::stopMessageProcessing,
             R"(Stops the background message processing for lock-free queues.

This method is only available when ATOM_USE_LOCKFREE_QUEUE is enabled.
It gracefully stops the background processing thread or async task.

Warning: After calling this, messages will queue up but not be delivered
until processing is restarted.
)")
#endif
        .def_static("create_shared",
             [](PyIOContext& py_io_context) {
                 return std::make_shared<atom::async::MessageBus>(
                     py_io_context.get_io_context());
             },
             py::arg("io_context"),
             R"(Creates a shared instance of MessageBus.

Args:
    io_context: PyIOContext instance for async operations.

Returns:
    A shared pointer to a MessageBus instance.

Examples:
    >>> io_context = PyIOContext()
    >>> bus = MessageBus.create_shared(io_context)
)");

    // Register typed message handlers for common types
    declare_message_type<std::string>(m, "string");
    declare_message_type<int>(m, "int");
    declare_message_type<double>(m, "double");
    declare_message_type<bool>(m, "bool");
    declare_message_type<std::vector<std::string>>(m, "string_list");
    declare_message_type<std::vector<int>>(m, "int_list");
    declare_message_type<std::vector<double>>(m, "double_list");
    declare_message_type<std::unordered_map<std::string, std::string>>(
        m, "string_dict");

    // Utility functions for message bus management
    m.def("create_message_bus",
          [](PyIOContext& py_io_context) {
              return std::make_shared<atom::async::MessageBus>(
                  py_io_context.get_io_context());
          },
          py::arg("io_context"),
          R"(Creates a new MessageBus instance.

Args:
    io_context: PyIOContext instance for async operations.

Returns:
    A shared pointer to a MessageBus instance.

This is a convenience function equivalent to MessageBus.create_shared().
)");

    m.def("create_io_context",
          []() {
              return std::make_unique<PyIOContext>();
          },
          R"(Creates a new PyIOContext instance.

Returns:
    A new PyIOContext for use with MessageBus.

This is a convenience function for creating IO contexts.
)");

    // Enhanced message type registration with Python object support
    declare_message_type<py::object>(m, "object");
    declare_message_type<py::dict>(m, "dict");
    declare_message_type<py::list>(m, "list");
    declare_message_type<py::tuple>(m, "tuple");

    // Utility functions for working with message patterns
    m.def("is_namespace_match",
          [](const std::string& message_name, const std::string& namespace_pattern) -> bool {
              // Simple namespace matching: "system.notifications" matches "system"
              return message_name.rfind(namespace_pattern + ".", 0) == 0;
          },
          py::arg("message_name"), py::arg("namespace_pattern"),
          R"(Checks if a message name matches a namespace pattern.

Args:
    message_name: The full message name (e.g., "system.notifications.error")
    namespace_pattern: The namespace pattern (e.g., "system.notifications")

Returns:
    bool: True if the message name starts with the namespace pattern.

Examples:
    >>> is_namespace_match("system.notifications.error", "system.notifications")
    True
    >>> is_namespace_match("user.login", "system")
    False
)");

    m.def("extract_namespace",
          [](const std::string& message_name) -> std::string {
              auto pos = message_name.find('.');
              if (pos != std::string::npos) {
                  return message_name.substr(0, pos);
              }
              return message_name;
          },
          py::arg("message_name"),
          R"(Extracts the namespace from a message name.

Args:
    message_name: The full message name (e.g., "system.notifications.error")

Returns:
    str: The namespace part (e.g., "system")

Examples:
    >>> extract_namespace("system.notifications.error")
    "system"
    >>> extract_namespace("simple_message")
    "simple_message"
)");

    m.def("validate_message_name",
          [](const std::string& message_name) -> bool {
              if (message_name.empty()) return false;
              if (message_name.front() == '.' || message_name.back() == '.') return false;
              if (message_name.find("..") != std::string::npos) return false;
              return true;
          },
          py::arg("message_name"),
          R"(Validates a message name according to naming conventions.

Args:
    message_name: The message name to validate

Returns:
    bool: True if the message name is valid

A valid message name:
    - Cannot be empty
    - Cannot start or end with '.'
    - Cannot contain consecutive dots '..'

Examples:
    >>> validate_message_name("system.notifications")
    True
    >>> validate_message_name(".invalid")
    False
    >>> validate_message_name("invalid..name")
    False
)");

    // Advanced utility functions for message bus management
    m.def("benchmark_message_bus_performance",
          [](std::shared_ptr<atom::async::MessageBus> bus, int num_publishers,
             int num_subscribers, int messages_per_publisher) -> py::dict {
              using namespace std::chrono;

              py::dict results;
              std::vector<std::thread> publisher_threads;
              std::vector<std::thread> subscriber_threads;
              std::atomic<int> messages_received{0};
              std::vector<double> publisher_times(num_publishers);

              // Create subscribers
              std::vector<atom::async::MessageBus::Token> tokens;
              for (int i = 0; i < num_subscribers; ++i) {
                  auto token = bus->subscribe<std::string>(
                      "benchmark.test",
                      [&messages_received](const std::string& msg) {
                          messages_received.fetch_add(1, std::memory_order_relaxed);
                      });
                  tokens.push_back(token);
              }

              auto start_time = high_resolution_clock::now();

              // Create publishers
              for (int i = 0; i < num_publishers; ++i) {
                  publisher_threads.emplace_back([bus, &publisher_times, i, messages_per_publisher]() {
                      auto thread_start = high_resolution_clock::now();

                      for (int j = 0; j < messages_per_publisher; ++j) {
                          bus->publish<std::string>("benchmark.test",
                                                   "Message " + std::to_string(j));
                      }

                      auto thread_end = high_resolution_clock::now();
                      auto duration = duration_cast<microseconds>(thread_end - thread_start);
                      publisher_times[i] = duration.count();
                  });
              }

              // Wait for all publishers to complete
              for (auto& thread : publisher_threads) {
                  thread.join();
              }

              // Wait a bit for message processing
              std::this_thread::sleep_for(std::chrono::milliseconds(100));

              auto end_time = high_resolution_clock::now();
              auto total_duration = duration_cast<microseconds>(end_time - start_time);

              // Cleanup subscribers
              for (auto token : tokens) {
                  bus->unsubscribe<std::string>(token);
              }

              // Calculate statistics
              double total_messages = num_publishers * messages_per_publisher;
              double avg_publisher_time = 0;
              for (double time : publisher_times) {
                  avg_publisher_time += time;
              }
              avg_publisher_time /= num_publishers;

              double min_time = *std::min_element(publisher_times.begin(), publisher_times.end());
              double max_time = *std::max_element(publisher_times.begin(), publisher_times.end());

              results[py::str("num_publishers")] = num_publishers;
              results[py::str("num_subscribers")] = num_subscribers;
              results[py::str("messages_per_publisher")] = messages_per_publisher;
              results[py::str("total_messages_sent")] = total_messages;
              results[py::str("total_messages_received")] = messages_received.load();
              results[py::str("total_time_us")] = total_duration.count();
              results[py::str("avg_publisher_time_us")] = avg_publisher_time;
              results[py::str("min_publisher_time_us")] = min_time;
              results[py::str("max_publisher_time_us")] = max_time;
              results[py::str("messages_per_second")] = (total_messages * 1000000.0) / total_duration.count();
              results[py::str("delivery_rate")] = (double)messages_received.load() / total_messages;

              return results;
          },
          py::arg("bus"), py::arg("num_publishers") = 4, py::arg("num_subscribers") = 4,
          py::arg("messages_per_publisher") = 1000,
          R"pbdoc(
          Benchmark message bus performance with multiple publishers and subscribers.

          Args:
              bus: MessageBus instance to benchmark
              num_publishers: Number of publisher threads (default: 4)
              num_subscribers: Number of subscriber threads (default: 4)
              messages_per_publisher: Messages each publisher sends (default: 1000)

          Returns:
              dict: Performance metrics including timing and throughput data

          Examples:
              >>> results = benchmark_message_bus_performance(bus, 8, 8, 5000)
              >>> print(f"Messages per second: {results['messages_per_second']:.2f}")
              >>> print(f"Delivery rate: {results['delivery_rate']:.2%}")
          )pbdoc")

    .def("create_message_bus_pool",
         [](PyIOContext& io_context, size_t pool_size) -> py::list {
             py::list pool;
             for (size_t i = 0; i < pool_size; ++i) {
                 pool.append(std::make_shared<atom::async::MessageBus>(
                     io_context.get_io_context()));
             }
             return pool;
         },
         py::arg("io_context"), py::arg("pool_size"),
         R"pbdoc(
         Create a pool of MessageBus instances for load distribution.

         Args:
             io_context: PyIOContext instance for async operations
             pool_size: Number of MessageBus instances to create

         Returns:
             list: List of MessageBus instances

         Examples:
             >>> io_ctx = PyIOContext()
             >>> bus_pool = create_message_bus_pool(io_ctx, 5)
             >>> # Use different buses for different services
             >>> service1_bus = bus_pool[0]
             >>> service2_bus = bus_pool[1]
         )pbdoc")

    .def("create_message_router",
         [](const std::vector<std::shared_ptr<atom::async::MessageBus>>& buses) -> py::dict {
             py::dict router;
             router[py::str("buses")] = py::cast(buses);
             router[py::str("current_index")] = 0;
             router[py::str("round_robin")] = py::cpp_function(
                 [buses](py::dict& router_dict) -> std::shared_ptr<atom::async::MessageBus> {
                     int current = router_dict[py::str("current_index")].cast<int>();
                     router_dict[py::str("current_index")] = (current + 1) % buses.size();
                     return buses[current];
                 });

             return router;
         },
         py::arg("buses"),
         R"pbdoc(
         Create a message router for load balancing across multiple buses.

         Args:
             buses: List of MessageBus instances to route between

         Returns:
             dict: Router with round-robin selection function

         Examples:
             >>> buses = [bus1, bus2, bus3]
             >>> router = create_message_router(buses)
             >>> selected_bus = router["round_robin"](router)
         )pbdoc");

    // Add version information and constants
    m.attr("__version__") = "1.0.0";
    m.attr("MAX_HISTORY_SIZE") = atom::async::MessageBus::K_MAX_HISTORY_SIZE;
    m.attr("MAX_SUBSCRIBERS_PER_MESSAGE") =
        atom::async::MessageBus::K_MAX_SUBSCRIBERS_PER_MESSAGE;

    // Add feature flags
#ifdef ATOM_USE_ASIO
    m.attr("HAS_ASIO_SUPPORT") = true;
#else
    m.attr("HAS_ASIO_SUPPORT") = false;
#endif

#ifdef ATOM_USE_LOCKFREE_QUEUE
    m.attr("HAS_LOCKFREE_QUEUE") = true;
#else
    m.attr("HAS_LOCKFREE_QUEUE") = false;
#endif

#ifdef ATOM_COROUTINE_SUPPORT
    m.attr("HAS_COROUTINE_SUPPORT") = true;
#else
    m.attr("HAS_COROUTINE_SUPPORT") = false;
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
