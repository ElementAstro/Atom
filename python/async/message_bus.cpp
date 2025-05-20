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

    // Add version information
    m.attr("__version__") = "1.0.0";
    m.attr("MAX_HISTORY_SIZE") = atom::async::MessageBus::K_MAX_HISTORY_SIZE;
    m.attr("MAX_SUBSCRIBERS_PER_MESSAGE") =
        atom::async::MessageBus::K_MAX_SUBSCRIBERS_PER_MESSAGE;
}