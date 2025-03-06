#ifndef ATOM_CONNECTION_SOCKETHUB_HPP
#define ATOM_CONNECTION_SOCKETHUB_HPP

#include <concepts>
#include <functional>
#include <memory>
#include <string_view>

namespace atom::connection {

class SocketHubImpl;

// Define concepts for message handlers
template <typename T>
concept MessageHandler = requires(T h, std::string_view msg) {
    { h(msg) } -> std::same_as<void>;
};

/**
 * @class SocketHub
 * @brief Manages socket connections with improved C++20 features
 */
class SocketHub {
public:
    /**
     * @brief Constructs a SocketHub instance.
     * @throws std::bad_alloc If memory allocation fails
     */
    SocketHub();

    /**
     * @brief Destroys the SocketHub instance.
     */
    ~SocketHub() noexcept;

    // Prevent copying
    SocketHub(const SocketHub&) = delete;
    SocketHub& operator=(const SocketHub&) = delete;

    // Allow moving
    SocketHub(SocketHub&&) noexcept;
    SocketHub& operator=(SocketHub&&) noexcept;

    /**
     * @brief Starts the socket service.
     * @param port The port number (valid range: 1-65535)
     * @throws std::invalid_argument If port is invalid
     * @throws std::runtime_error If socket creation fails
     */
    void start(int port);

    /**
     * @brief Stops the socket service.
     * @throws std::runtime_error If cleanup operations fail
     */
    void stop() noexcept;

    /**
     * @brief Adds a message handler.
     * @param handler A function to handle incoming messages
     * @throws std::invalid_argument If handler is invalid
     */
    template <MessageHandler H>
    void addHandler(H&& handler) {
        addHandlerImpl(std::forward<H>(handler));
    }

    /**
     * @brief Checks if the socket service is running.
     * @return True if running, false otherwise
     */
    [[nodiscard]] auto isRunning() const noexcept -> bool;

private:
    void addHandlerImpl(std::function<void(std::string_view)> handler);
    std::unique_ptr<SocketHubImpl> impl_;
};

}  // namespace atom::connection

#endif  // ATOM_CONNECTION_SOCKETHUB_HPP