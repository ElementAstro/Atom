#include "atom/extra/asio/mqtt/client.hpp"
#include "atom/extra/asio/mqtt/types.hpp"

#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(mqtt, m) {
    m.doc() = R"(MQTT client module for the atom package.

This module provides a modern C++20 MQTT client implementation with support for
both MQTT 3.1.1 and 5.0 protocols, SSL/TLS connections, automatic reconnection,
QoS management, and asynchronous operations.

Examples:
    >>> from atom.extra.asio import mqtt
    >>>
    >>> # Create MQTT client
    >>> client = mqtt.Client()
    >>>
    >>> # Set up connection options
    >>> options = mqtt.ConnectionOptions()
    >>> options.client_id = "python_client"
    >>> options.username = "user"
    >>> options.password = "pass"
    >>>
    >>> # Connect to broker
    >>> client.async_connect("localhost", 1883, options)
    >>>
    >>> # Publish message
    >>> client.async_publish("test/topic", "Hello MQTT!", mqtt.QoS.AT_LEAST_ONCE)
    >>>
    >>> # Subscribe to topic
    >>> subscription = mqtt.Subscription()
    >>> subscription.topic_filter = "test/+"
    >>> subscription.qos = mqtt.QoS.AT_LEAST_ONCE
    >>> client.async_subscribe([subscription])
)";

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

    // ProtocolVersion enum
    py::enum_<mqtt::ProtocolVersion>(m, "ProtocolVersion",
                                     R"(MQTT protocol version enumeration.

Specifies which MQTT protocol version to use for connections.)")
        .value("V3_1_1", mqtt::ProtocolVersion::V3_1_1, "MQTT version 3.1.1")
        .value("V5_0", mqtt::ProtocolVersion::V5_0, "MQTT version 5.0")
        .export_values();

    // QoS enum
    py::enum_<mqtt::QoS>(m, "QoS",
                         R"(Quality of Service levels for MQTT message delivery.

Defines the delivery guarantees for MQTT messages.)")
        .value("AT_MOST_ONCE", mqtt::QoS::AT_MOST_ONCE,
               "Fire and forget delivery (QoS 0)")
        .value("AT_LEAST_ONCE", mqtt::QoS::AT_LEAST_ONCE,
               "Acknowledged delivery (QoS 1)")
        .value("EXACTLY_ONCE", mqtt::QoS::EXACTLY_ONCE,
               "Assured delivery (QoS 2)")
        .export_values();

    // ErrorCode enum
    py::enum_<mqtt::ErrorCode>(m, "ErrorCode",
                               R"(MQTT error and return codes.

Represents standard MQTT error codes and MQTT 5.0 specific codes.)")
        .value("SUCCESS", mqtt::ErrorCode::SUCCESS, "Operation successful")
        .value("CONNECTION_REFUSED_PROTOCOL",
               mqtt::ErrorCode::CONNECTION_REFUSED_PROTOCOL,
               "Connection refused - unacceptable protocol version")
        .value("CONNECTION_REFUSED_IDENTIFIER",
               mqtt::ErrorCode::CONNECTION_REFUSED_IDENTIFIER,
               "Connection refused - identifier rejected")
        .value("CONNECTION_REFUSED_SERVER_UNAVAILABLE",
               mqtt::ErrorCode::CONNECTION_REFUSED_SERVER_UNAVAILABLE,
               "Connection refused - server unavailable")
        .value("CONNECTION_REFUSED_BAD_CREDENTIALS",
               mqtt::ErrorCode::CONNECTION_REFUSED_BAD_CREDENTIALS,
               "Connection refused - bad user name or password")
        .value("CONNECTION_REFUSED_NOT_AUTHORIZED",
               mqtt::ErrorCode::CONNECTION_REFUSED_NOT_AUTHORIZED,
               "Connection refused - not authorized")
        .value("UNSPECIFIED_ERROR", mqtt::ErrorCode::UNSPECIFIED_ERROR,
               "Unspecified error")
        .value("MALFORMED_PACKET", mqtt::ErrorCode::MALFORMED_PACKET,
               "Malformed packet")
        .value("PROTOCOL_ERROR", mqtt::ErrorCode::PROTOCOL_ERROR,
               "Protocol error")
        .value("IMPLEMENTATION_SPECIFIC",
               mqtt::ErrorCode::IMPLEMENTATION_SPECIFIC,
               "Implementation specific error")
        .value("UNSUPPORTED_PROTOCOL_VERSION",
               mqtt::ErrorCode::UNSUPPORTED_PROTOCOL_VERSION,
               "Unsupported protocol version")
        .value("CLIENT_IDENTIFIER_NOT_VALID",
               mqtt::ErrorCode::CLIENT_IDENTIFIER_NOT_VALID,
               "Client identifier not valid")
        .value("BAD_USER_NAME_OR_PASSWORD",
               mqtt::ErrorCode::BAD_USER_NAME_OR_PASSWORD,
               "Bad user name or password")
        .value("NOT_AUTHORIZED", mqtt::ErrorCode::NOT_AUTHORIZED,
               "Not authorized")
        .value("SERVER_UNAVAILABLE", mqtt::ErrorCode::SERVER_UNAVAILABLE,
               "Server unavailable")
        .value("SERVER_BUSY", mqtt::ErrorCode::SERVER_BUSY, "Server busy")
        .value("BANNED", mqtt::ErrorCode::BANNED, "Banned")
        .value("BAD_AUTHENTICATION_METHOD",
               mqtt::ErrorCode::BAD_AUTHENTICATION_METHOD,
               "Bad authentication method")
        .value("TOPIC_FILTER_INVALID", mqtt::ErrorCode::TOPIC_FILTER_INVALID,
               "Topic filter invalid")
        .value("TOPIC_NAME_INVALID", mqtt::ErrorCode::TOPIC_NAME_INVALID,
               "Topic name invalid")
        .value("PACKET_IDENTIFIER_IN_USE",
               mqtt::ErrorCode::PACKET_IDENTIFIER_IN_USE,
               "Packet identifier in use")
        .value("PACKET_IDENTIFIER_NOT_FOUND",
               mqtt::ErrorCode::PACKET_IDENTIFIER_NOT_FOUND,
               "Packet identifier not found")
        .value("RECEIVE_MAXIMUM_EXCEEDED",
               mqtt::ErrorCode::RECEIVE_MAXIMUM_EXCEEDED,
               "Receive maximum exceeded")
        .value("TOPIC_ALIAS_INVALID", mqtt::ErrorCode::TOPIC_ALIAS_INVALID,
               "Topic alias invalid")
        .value("PACKET_TOO_LARGE", mqtt::ErrorCode::PACKET_TOO_LARGE,
               "Packet too large")
        .value("MESSAGE_RATE_TOO_HIGH", mqtt::ErrorCode::MESSAGE_RATE_TOO_HIGH,
               "Message rate too high")
        .value("QUOTA_EXCEEDED", mqtt::ErrorCode::QUOTA_EXCEEDED,
               "Quota exceeded")
        .value("ADMINISTRATIVE_ACTION", mqtt::ErrorCode::ADMINISTRATIVE_ACTION,
               "Administrative action")
        .value("PAYLOAD_FORMAT_INVALID",
               mqtt::ErrorCode::PAYLOAD_FORMAT_INVALID,
               "Payload format invalid")
        .export_values();

    // ConnectionOptions struct
    py::class_<mqtt::ConnectionOptions>(
        m, "ConnectionOptions",
        R"(Configuration options for establishing an MQTT connection.

Contains all parameters required to connect to an MQTT broker, including
authentication, session, will message, protocol version, and TLS settings.

Examples:
    >>> options = mqtt.ConnectionOptions()
    >>> options.client_id = "my_client"
    >>> options.username = "user"
    >>> options.password = "pass"
    >>> options.keep_alive = 60  # seconds
    >>> options.clean_session = True
    >>> options.use_tls = True
)")
        .def(py::init<>(), "Create default connection options")
        .def_readwrite("client_id", &mqtt::ConnectionOptions::client_id,
                       "Unique client identifier")
        .def_readwrite("username", &mqtt::ConnectionOptions::username,
                       "Username for authentication")
        .def_readwrite("password", &mqtt::ConnectionOptions::password,
                       "Password for authentication")
        .def_readwrite("keep_alive", &mqtt::ConnectionOptions::keep_alive,
                       "Keep-alive interval in seconds")
        .def_readwrite("clean_session", &mqtt::ConnectionOptions::clean_session,
                       "Whether to start a clean session")
        .def_readwrite("will_topic", &mqtt::ConnectionOptions::will_topic,
                       "Topic for the will message")
        .def_readwrite("will_payload", &mqtt::ConnectionOptions::will_payload,
                       "Payload for the will message")
        .def_readwrite("will_qos", &mqtt::ConnectionOptions::will_qos,
                       "QoS for the will message")
        .def_readwrite("will_retain", &mqtt::ConnectionOptions::will_retain,
                       "Retain flag for the will message")
        .def_readwrite("version", &mqtt::ConnectionOptions::version,
                       "MQTT protocol version")
        .def_readwrite("use_tls", &mqtt::ConnectionOptions::use_tls,
                       "Enable TLS/SSL")
        .def_readwrite("ca_cert_file", &mqtt::ConnectionOptions::ca_cert_file,
                       "Path to CA certificate file")
        .def_readwrite("cert_file", &mqtt::ConnectionOptions::cert_file,
                       "Path to client certificate file")
        .def_readwrite("private_key_file",
                       &mqtt::ConnectionOptions::private_key_file,
                       "Path to private key file")
        .def_readwrite("verify_certificate",
                       &mqtt::ConnectionOptions::verify_certificate,
                       "Whether to verify server certificate");

    // Message struct
    py::class_<mqtt::Message>(m, "Message",
                              R"(Represents an MQTT message.

Contains topic, payload, QoS, retain flag, packet ID, and MQTT 5.0 properties.

Examples:
    >>> message = mqtt.Message()
    >>> message.topic = "sensors/temperature"
    >>> message.payload = b"23.5"
    >>> message.qos = mqtt.QoS.AT_LEAST_ONCE
    >>> message.retain = False
)")
        .def(py::init<>(), "Create an empty message")
        .def_readwrite("topic", &mqtt::Message::topic, "Topic name")
        .def_readwrite("payload", &mqtt::Message::payload,
                       "Message payload as bytes")
        .def_readwrite("qos", &mqtt::Message::qos, "Quality of Service level")
        .def_readwrite("retain", &mqtt::Message::retain, "Retain flag")
        .def_readwrite("packet_id", &mqtt::Message::packet_id,
                       "Packet identifier")
        .def_readwrite("message_expiry_interval",
                       &mqtt::Message::message_expiry_interval,
                       "Message expiry interval (MQTT 5.0)")
        .def_readwrite("response_topic", &mqtt::Message::response_topic,
                       "Response topic (MQTT 5.0)")
        .def_readwrite("correlation_data", &mqtt::Message::correlation_data,
                       "Correlation data (MQTT 5.0)")
        .def_readwrite("content_type", &mqtt::Message::content_type,
                       "Content type (MQTT 5.0)");

    // Subscription struct
    py::class_<mqtt::Subscription>(
        m, "Subscription",
        R"(Represents a subscription to an MQTT topic filter.

Contains topic filter, QoS, and MQTT 5.0 subscription options.

Examples:
    >>> subscription = mqtt.Subscription()
    >>> subscription.topic_filter = "sensors/+/temperature"
    >>> subscription.qos = mqtt.QoS.AT_LEAST_ONCE
    >>> subscription.no_local = False
)")
        .def(py::init<>(), "Create an empty subscription")
        .def_readwrite("topic_filter", &mqtt::Subscription::topic_filter,
                       "Topic filter to subscribe to")
        .def_readwrite("qos", &mqtt::Subscription::qos, "Requested QoS level")
        .def_readwrite("no_local", &mqtt::Subscription::no_local,
                       "Do not receive own publications (MQTT 5.0)")
        .def_readwrite("retain_as_published",
                       &mqtt::Subscription::retain_as_published,
                       "Retain as published flag (MQTT 5.0)")
        .def_readwrite("retain_handling", &mqtt::Subscription::retain_handling,
                       "Retain handling option (MQTT 5.0)");

    // ClientStats struct
    py::class_<mqtt::ClientStats>(m, "ClientStats",
                                  R"(Statistics for an MQTT client session.

Tracks message and byte counts, connection time, and reconnect attempts.)")
        .def(py::init<>(), "Create empty statistics")
        .def_readwrite("messages_sent", &mqtt::ClientStats::messages_sent,
                       "Number of messages sent")
        .def_readwrite("messages_received",
                       &mqtt::ClientStats::messages_received,
                       "Number of messages received")
        .def_readwrite("bytes_sent", &mqtt::ClientStats::bytes_sent,
                       "Number of bytes sent")
        .def_readwrite("bytes_received", &mqtt::ClientStats::bytes_received,
                       "Number of bytes received")
        .def_readwrite("connected_since", &mqtt::ClientStats::connected_since,
                       "Connection start time")
        .def_readwrite("reconnect_count", &mqtt::ClientStats::reconnect_count,
                       "Number of reconnection attempts");

    // Client class binding
    py::class_<mqtt::Client>(m, "Client",
                             R"(Modern MQTT Client with C++20 Features.

The Client class provides a full-featured, thread-safe, and asynchronous MQTT
client implementation using modern C++20 features and ASIO for networking. It
supports secure (TLS) and plain connections, automatic reconnection, QoS
management, event handlers, statistics, and advanced configuration.

Key features:
- Asynchronous connect, publish, subscribe, and unsubscribe operations
- Support for both plain TCP and SSL/TLS transports
- Automatic reconnection with exponential backoff
- Keep-alive and ping management
- QoS 0/1/2 message tracking and retransmission
- User-defined event handlers for messages, connection, and disconnection
- Thread-safe statistics and monitoring
- Customizable client ID and connection options

Examples:
    >>> client = mqtt.Client()
    >>>
    >>> # Set message handler
    >>> def on_message(message):
    ...     print(f"Received: {message.topic} = {message.payload}")
    >>> client.set_message_handler(on_message)
    >>>
    >>> # Connect to broker
    >>> options = mqtt.ConnectionOptions()
    >>> options.client_id = "python_client"
    >>> client.async_connect("localhost", 1883, options)
    >>>
    >>> # Publish message
    >>> client.async_publish("test/topic", b"Hello MQTT!")
)")
        .def(py::init<bool>(), py::arg("auto_start_io") = true,
             R"(Construct a new MQTT Client.

Args:
    auto_start_io: If true, automatically starts the IO thread.
)")
        .def(
            "async_connect",
            [](mqtt::Client& self, const std::string& host, uint16_t port,
               const mqtt::ConnectionOptions& options) {
                self.async_connect(host, port, options, nullptr);
            },
            py::arg("host"), py::arg("port"), py::arg("options"),
            R"(Asynchronously connect to the MQTT broker.

Args:
    host: Broker hostname or IP address.
    port: Broker port.
    options: Connection options (client ID, credentials, etc).
    callback: Optional callback invoked on connection result.

Examples:
    >>> def on_connect(error):
    ...     if error == mqtt.ErrorCode.SUCCESS:
    ...         print("Connected successfully")
    ...     else:
    ...         print(f"Connection failed: {error}")
    >>>
    >>> client.async_connect("localhost", 1883, options, on_connect)
)")
        .def("disconnect", &mqtt::Client::disconnect,
             py::arg("reason") = mqtt::ErrorCode::SUCCESS,
             R"(Disconnect from the MQTT broker.

Args:
    reason: Error code for disconnect (default: SUCCESS).
)")
        .def("is_connected", &mqtt::Client::is_connected,
             R"(Check if the client is currently connected.

Returns:
    True if connected to the broker.
)")
        .def("get_state", &mqtt::Client::get_state,
             R"(Get the current connection state.

Returns:
    ConnectionState enum value.
)")
        .def(
            "async_publish",
            [](mqtt::Client& self, const std::string& topic,
               const py::bytes& payload, mqtt::QoS qos, bool retain) {
                std::string payload_str = payload;
                self.async_publish(
                    topic,
                    std::span<const uint8_t>(
                        reinterpret_cast<const uint8_t*>(payload_str.data()),
                        payload_str.size()),
                    qos, retain, nullptr);
            },
            py::arg("topic"), py::arg("payload"),
            py::arg("qos") = mqtt::QoS::AT_MOST_ONCE, py::arg("retain") = false,
            R"(Asynchronously publish a message to a topic.

Args:
    topic: Topic string.
    payload: Message payload as bytes.
    qos: Quality of Service level (default: AT_MOST_ONCE).
    retain: Retain flag (default: False).
    callback: Optional callback invoked on publish result.

Examples:
    >>> def on_publish(error):
    ...     if error == mqtt.ErrorCode.SUCCESS:
    ...         print("Message published successfully")
    >>>
    >>> client.async_publish("sensors/temp", b"23.5",
    ...                     mqtt.QoS.AT_LEAST_ONCE, False, on_publish)
)")
        .def(
            "async_subscribe",
            [](mqtt::Client& self,
               const std::vector<mqtt::Subscription>& subscriptions) {
                self.async_subscribe(subscriptions, nullptr);
            },
            py::arg("subscriptions"),
            R"(Asynchronously subscribe to one or more topics.

Args:
    subscriptions: List of Subscription objects.
    callback: Optional callback invoked on subscribe result.

Examples:
    >>> def on_subscribe(error, granted_qos):
    ...     if error == mqtt.ErrorCode.SUCCESS:
    ...         print(f"Subscribed with QoS: {granted_qos}")
    >>>
    >>> subs = [mqtt.Subscription()]
    >>> subs[0].topic_filter = "sensors/+"
    >>> subs[0].qos = mqtt.QoS.AT_LEAST_ONCE
    >>> client.async_subscribe(subs, on_subscribe)
)")
        .def(
            "async_unsubscribe",
            [](mqtt::Client& self,
               const std::vector<std::string>& topic_filters) {
                self.async_unsubscribe(topic_filters, nullptr);
            },
            py::arg("topic_filters"),
            R"(Asynchronously unsubscribe from one or more topics.

Args:
    topic_filters: List of topic filter strings.
    callback: Optional callback invoked on unsubscribe result.

Examples:
    >>> def on_unsubscribe(error):
    ...     if error == mqtt.ErrorCode.SUCCESS:
    ...         print("Unsubscribed successfully")
    >>>
    >>> client.async_unsubscribe(["sensors/+"], on_unsubscribe)
)")

        .def("get_stats", &mqtt::Client::get_stats,
             R"(Get a snapshot of the current client statistics.

Returns:
    ClientStats structure with message and byte counts.
)")
        .def("set_auto_reconnect", &mqtt::Client::set_auto_reconnect,
             py::arg("enabled"),
             R"(Enable or disable automatic reconnection.

Args:
    enabled: Whether to automatically reconnect on connection loss.
)")
        .def("get_auto_reconnect", &mqtt::Client::get_auto_reconnect,
             R"(Get whether automatic reconnection is enabled.

Returns:
    True if automatic reconnection is enabled.
)")
        .def("reset_stats", &mqtt::Client::reset_stats,
             R"(Reset the client statistics.)")
        .def("run", &mqtt::Client::run,
             R"(Run the IO context in the current thread.)")
        .def("stop", &mqtt::Client::stop,
             R"(Stop the IO context and all asynchronous operations.)");
}
