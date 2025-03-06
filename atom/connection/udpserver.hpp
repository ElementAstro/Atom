#ifndef ATOM_CONNECTION_UDP_HPP
#define ATOM_CONNECTION_UDP_HPP

#include <concepts>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include "atom/type/expected.hpp"

namespace atom::connection {

// Error codes for UDP operations
enum class UdpError {
    SocketCreationFailed,
    BindFailed,
    NetworkInitFailed,
    SendFailed,
    NotRunning,
    InvalidAddress,
    InvalidPort
};

/**
 * @concept MessageHandlerCallable
 * @brief Concept that defines the requirements for a message handler function.
 */
template <typename T>
concept MessageHandlerCallable =
    requires(T t, const std::string& msg, const std::string& ip, int port) {
        { t(msg, ip, port) } -> std::same_as<void>;
    };

/**
 * @class UdpSocketHub
 * @brief Represents a hub for managing UDP sockets and message handling.
 */
class UdpSocketHub {
public:
    /**
     * @brief Type definition for message handler function.
     * @param message The message received.
     * @param ip The IP address of the sender.
     * @param port The port of the sender.
     */
    using MessageHandler =
        std::function<void(const std::string&, const std::string&, int)>;

    /**
     * @brief Constructor.
     */
    UdpSocketHub();

    /**
     * @brief Destructor.
     */
    ~UdpSocketHub();

    UdpSocketHub(const UdpSocketHub&) = delete;
    UdpSocketHub& operator=(const UdpSocketHub&) = delete;
    UdpSocketHub(UdpSocketHub&&) noexcept = default;
    UdpSocketHub& operator=(UdpSocketHub&&) noexcept = default;

    /**
     * @brief Starts the UDP socket hub and binds it to the specified port.
     * @param port The port on which the UDP socket hub will listen for incoming
     * messages.
     * @return type::expected with void on success or UdpError on failure
     */
    [[nodiscard]] type::expected<void, UdpError> start(
        std::uint16_t port) noexcept;

    /**
     * @brief Stops the UDP socket hub.
     */
    void stop() noexcept;

    /**
     * @brief Checks if the UDP socket hub is currently running.
     * @return True if the UDP socket hub is running, false otherwise.
     */
    [[nodiscard]] bool isRunning() const noexcept;

    /**
     * @brief Adds a message handler function to the UDP socket hub.
     * @param handler The message handler function to add.
     */
    template <MessageHandlerCallable T>
    void addMessageHandler(T&& handler) {
        addMessageHandlerImpl(MessageHandler(std::forward<T>(handler)));
    }

    /**
     * @brief Removes a message handler function from the UDP socket hub.
     * @param handler The message handler function to remove.
     */
    template <MessageHandlerCallable T>
    void removeMessageHandler(T&& handler) {
        removeMessageHandlerImpl(MessageHandler(std::forward<T>(handler)));
    }

    /**
     * @brief Sends a message to the specified IP address and port.
     * @param message The message to send.
     * @param ip The IP address of the recipient.
     * @param port The port of the recipient.
     * @return type::expected with void on success or UdpError on failure
     */
    [[nodiscard]] type::expected<void, UdpError> sendTo(
        std::string_view message, std::string_view ip,
        std::uint16_t port) noexcept;

    /**
     * @brief Set the maximum buffer size for receiving messages
     * @param size The new buffer size in bytes
     */
    void setBufferSize(std::size_t size) noexcept;

private:
    void addMessageHandlerImpl(MessageHandler handler);
    void removeMessageHandlerImpl(MessageHandler handler);

    class Impl; /**< Forward declaration of the implementation class. */
    std::unique_ptr<Impl> impl_; /**< Pointer to the implementation object. */
};

}  // namespace atom::connection

#endif