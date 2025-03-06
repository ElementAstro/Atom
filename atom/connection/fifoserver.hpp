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
#include <memory>
#include <ranges>
#include <string>
#include <string_view>

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
     * @brief Constructs a new FIFOServer object.
     *
     * @param fifo_path The path to the FIFO pipe.
     * @throws std::invalid_argument If fifo_path is empty.
     * @throws std::runtime_error If FIFO creation fails.
     */
    explicit FIFOServer(std::string_view fifo_path);

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
     * @brief Sends multiple messages from a range
     *
     * @tparam R Range type containing messages
     * @param messages Range of messages to send
     * @return Number of messages successfully queued
     */
    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, std::string>
    size_t sendMessages(R&& messages);

    /**
     * @brief Starts the server.
     *
     * @throws std::runtime_error If server fails to start
     */
    void start();

    /**
     * @brief Stops the server.
     */
    void stop();

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

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace atom::connection

#endif  // ATOM_CONNECTION_FIFOSERVER_HPP