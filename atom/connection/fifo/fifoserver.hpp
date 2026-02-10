/*
 * fifoserver.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: FIFO Server

*************************************************/

#ifndef ATOM_CONNECTION_FIFOSERVER_HPP
#define ATOM_CONNECTION_FIFOSERVER_HPP

#include <concepts>
#include <future>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>

#include "fifo_common.hpp"

namespace atom::connection {

/**
 * @brief Concept for types that can be converted to messages
 */
template <typename T>
concept Messageable = std::convertible_to<T, std::string> || requires(T t) {
    { std::to_string(t) } -> std::convertible_to<std::string>;
};

/**
 * @brief A class representing a server for handling FIFO messages.
 */
class FIFOServer {
public:
    /**
     * @brief Constructs a new FIFOServer object with default configuration.
     *
     * @param fifo_path The path to the FIFO pipe.
     * @throws std::invalid_argument If fifo_path is empty.
     * @throws std::runtime_error If FIFO creation fails.
     */
    explicit FIFOServer(std::string_view fifo_path);

    /**
     * @brief Constructs a new FIFOServer object with custom configuration.
     *
     * @param fifo_path The path to the FIFO pipe.
     * @param config Custom server configuration.
     * @throws std::invalid_argument If fifo_path is empty.
     * @throws std::runtime_error If FIFO creation fails.
     */
    FIFOServer(std::string_view fifo_path, const ServerConfig& config);

    /**
     * @brief Destroys the FIFOServer object.
     */
    ~FIFOServer();

    // Disable copy operations
    FIFOServer(const FIFOServer&) = delete;
    FIFOServer& operator=(const FIFOServer&) = delete;

    // Enable move operations
    FIFOServer(FIFOServer&&) noexcept;
    FIFOServer& operator=(FIFOServer&&) noexcept;

    /**
     * @brief Sends a message through the FIFO pipe.
     *
     * @param message The message to be sent.
     * @return True if message was queued successfully, false otherwise.
     */
    bool sendMessage(std::string message);

    /**
     * @brief Sends a message with specified priority.
     *
     * @param message The message to be sent.
     * @param priority The priority level for the message.
     * @return True if message was queued successfully, false otherwise.
     */
    bool sendMessage(std::string message, MessagePriority priority);

    /**
     * @brief Sends a message of any type that can be converted to string.
     *
     * @tparam T Type that satisfies the Messageable concept
     * @param message Message to be sent
     * @return True if message was queued successfully, false otherwise.
     */
    template <Messageable T>
    bool sendMessage(const T& message) {
        if constexpr (std::convertible_to<T, std::string>) {
            return sendMessage(std::string(message));
        } else {
            return sendMessage(std::to_string(message));
        }
    }

    /**
     * @brief Sends a message of any type with specified priority.
     *
     * @tparam T Type that satisfies the Messageable concept
     * @param message Message to be sent
     * @param priority The priority level for the message
     * @return True if message was queued successfully, false otherwise.
     */
    template <Messageable T>
    bool sendMessage(const T& message, MessagePriority priority) {
        if constexpr (std::convertible_to<T, std::string>) {
            return sendMessage(std::string(message), priority);
        } else {
            return sendMessage(std::to_string(message), priority);
        }
    }

    /**
     * @brief Sends a message asynchronously.
     *
     * @param message The message to be sent.
     * @return A future that will contain the result of the send operation.
     */
    std::future<bool> sendMessageAsync(std::string message);

    /**
     * @brief Sends a message asynchronously with the specified priority.
     *
     * @param message The message to be sent.
     * @param priority The priority level for the message.
     * @return A future that will contain the result of the send operation.
     */
    std::future<bool> sendMessageAsync(std::string message,
                                       MessagePriority priority);

    /**
     * @brief Sends multiple messages from a range
     *
     * @tparam R Range type containing messages
     * @param messages Range of messages to send
     * @return Number of messages successfully queued
     */
    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, std::string>
    size_t sendMessages(R&& messages) {
        size_t count = 0;
        for (auto&& msg : messages) {
            if (sendMessage(std::string(msg))) {
                ++count;
            }
        }
        return count;
    }

    /**
     * @brief Sends multiple messages with the same priority
     *
     * @tparam R Range type containing messages
     * @param messages Range of messages to send
     * @param priority Priority level for all messages
     * @return Number of messages successfully queued
     */
    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, std::string>
    size_t sendMessages(R&& messages, MessagePriority priority) {
        size_t count = 0;
        for (auto&& msg : messages) {
            if (sendMessage(std::string(msg), priority)) {
                ++count;
            }
        }
        return count;
    }

    /**
     * @brief Registers a callback for message delivery status
     *
     * @param callback Function to call when a message delivery status changes
     * @return A unique identifier for the callback registration
     */
    int registerMessageCallback(MessageCallback callback);

    /**
     * @brief Unregisters a previously registered message callback
     *
     * @param id The identifier returned by registerMessageCallback
     * @return True if callback was successfully unregistered
     */
    bool unregisterMessageCallback(int id);

    /**
     * @brief Registers a callback for server status changes
     *
     * @param callback Function to call when server status changes
     * @return A unique identifier for the callback registration
     */
    int registerStatusCallback(StatusCallback callback);

    /**
     * @brief Unregisters a previously registered status callback
     *
     * @param id The identifier returned by registerStatusCallback
     * @return True if callback was successfully unregistered
     */
    bool unregisterStatusCallback(int id);

    /**
     * @brief Starts the server.
     *
     * @throws std::runtime_error If server fails to start
     */
    void start();

    /**
     * @brief Stops the server.
     *
     * @param flush_queue If true, processes remaining messages before stopping
     */
    void stop(bool flush_queue = true);

    /**
     * @brief Clears all pending messages from the queue.
     *
     * @return Number of messages cleared
     */
    size_t clearQueue();

    /**
     * @brief Checks if the server is running.
     *
     * @return True if the server is running, false otherwise.
     */
    [[nodiscard]] bool isRunning() const;

    /**
     * @brief Gets the path of the FIFO pipe.
     *
     * @return The FIFO path as a string
     */
    [[nodiscard]] std::string getFifoPath() const;

    /**
     * @brief Gets the current configuration.
     *
     * @return The current server configuration
     */
    [[nodiscard]] ServerConfig getConfig() const;

    /**
     * @brief Updates the server configuration.
     *
     * @param config New configuration settings
     * @return True if configuration was updated successfully
     */
    bool updateConfig(const ServerConfig& config);

    /**
     * @brief Gets current server statistics.
     *
     * @return Statistics about server operation
     */
    [[nodiscard]] ServerStats getStatistics() const;

    /**
     * @brief Resets server statistics.
     */
    void resetStatistics();

    /**
     * @brief Sets the log level for the server.
     *
     * @param level New log level
     */
    void setLogLevel(LogLevel level);

    /**
     * @brief Gets the current number of messages in the queue.
     *
     * @return Current queue size
     */
    [[nodiscard]] size_t getQueueSize() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace atom::connection

#endif  // ATOM_CONNECTION_FIFOSERVER_HPP
