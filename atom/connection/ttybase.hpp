#ifndef ATOM_CONNECTION_TTYBASE_HPP
#define ATOM_CONNECTION_TTYBASE_HPP

#include <cstdint>
#include <future>
#include <memory>
#include <span>
#include <string>
#include <string_view>

/**
 * @class TTYBase
 * @brief Provides a base class for handling TTY (teletypewriter) connections.
 *
 * This class serves as an interface for reading from and writing to TTY
 * devices, handling various responses and errors associated with the
 * communication. It employs the PIMPL design pattern to hide implementation
 * details and reduce compilation dependencies.
 */
class TTYBase {
public:
    /**
     * @enum TTYResponse
     * @brief Enumerates the possible responses for TTY operations.
     */
    enum class TTYResponse {
        OK = 0,           ///< Operation completed successfully.
        ReadError = -1,   ///< An error occurred while reading from the TTY.
        WriteError = -2,  ///< An error occurred while writing to the TTY.
        SelectError =
            -3,        ///< An error occurred while selecting the TTY device.
        Timeout = -4,  ///< The operation timed out.
        PortFailure = -5,  ///< Failed to connect to the TTY port.
        ParamError = -6,   ///< Invalid parameter provided to the function.
        Errno = -7,        ///< An error occurred as indicated by errno.
        Overflow = -8      ///< A buffer overflow occurred during the operation.
    };

    /**
     * @brief Constructs a TTYBase instance with the specified driver name.
     *
     * @param driverName The name of the TTY driver to use.
     */
    explicit TTYBase(std::string_view driverName);

    /**
     * @brief Destructor for TTYBase.
     *
     * Cleans up resources associated with the TTY connection.
     */
    virtual ~TTYBase();

    // Delete copy operations to prevent accidental copying
    TTYBase(const TTYBase&) = delete;
    TTYBase& operator=(const TTYBase&) = delete;

    // Allow move operations
    TTYBase(TTYBase&& other) noexcept;
    TTYBase& operator=(TTYBase&& other) noexcept;

    /**
     * @brief Safely reads data from the TTY device using a std::span.
     *
     * @param buffer The buffer to store the read data.
     * @param timeout The timeout for the read operation in seconds.
     * @param nbytesRead A reference to store the actual number of bytes read.
     * @return TTYResponse indicating the result of the read operation.
     * @throws std::system_error If a critical system error occurs
     */
    TTYResponse read(std::span<uint8_t> buffer, uint8_t timeout,
                     uint32_t& nbytesRead);

    /**
     * @brief Reads a section of data from the TTY until a stop byte is
     * encountered, with buffer safety.
     *
     * @param buffer The buffer to store the read data.
     * @param stopByte The byte value at which to stop reading.
     * @param timeout The timeout for the read operation in seconds.
     * @param nbytesRead A reference to store the actual number of bytes read.
     * @return TTYResponse indicating the result of the read operation.
     * @throws std::system_error If a critical system error occurs
     */
    TTYResponse readSection(std::span<uint8_t> buffer, uint8_t stopByte,
                            uint8_t timeout, uint32_t& nbytesRead);

    /**
     * @brief Safely writes data to the TTY device using a std::span.
     *
     * @param buffer The data to write.
     * @param nbytesWritten A reference to store the actual number of bytes
     * written.
     * @return TTYResponse indicating the result of the write operation.
     * @throws std::system_error If a critical system error occurs
     */
    TTYResponse write(std::span<const uint8_t> buffer, uint32_t& nbytesWritten);

    /**
     * @brief Writes a string to the TTY device with safety checks.
     *
     * @param string The string to write to the TTY.
     * @param nbytesWritten A reference to store the actual number of bytes
     * written.
     * @return TTYResponse indicating the result of the write operation.
     */
    TTYResponse writeString(std::string_view string, uint32_t& nbytesWritten);

    /**
     * @brief Asynchronously reads data from the TTY device.
     *
     * @param buffer The buffer to store the read data.
     * @param timeout The timeout for the read operation in seconds.
     * @return std::future<std::pair<TTYResponse, uint32_t>> A future containing
     * the response and the number of bytes read.
     */
    std::future<std::pair<TTYResponse, uint32_t>> readAsync(
        std::span<uint8_t> buffer, uint8_t timeout);

    /**
     * @brief Asynchronously writes data to the TTY device.
     *
     * @param buffer The data to write.
     * @return std::future<std::pair<TTYResponse, uint32_t>> A future containing
     * the response and the number of bytes written.
     */
    std::future<std::pair<TTYResponse, uint32_t>> writeAsync(
        std::span<const uint8_t> buffer);

    /**
     * @brief Connects to the specified TTY device with enhanced validation.
     *
     * @param device The device name or path to connect to.
     * @param bitRate The baud rate for the connection.
     * @param wordSize The data size in bits per character.
     * @param parity The parity mode (e.g., none, odd, even).
     * @param stopBits The number of stop bits used in the communication.
     * @return TTYResponse indicating the result of the connection attempt.
     * @throws std::invalid_argument For invalid parameters
     * @throws std::system_error For system-level errors
     */
    TTYResponse connect(std::string_view device, uint32_t bitRate,
                        uint8_t wordSize, uint8_t parity, uint8_t stopBits);

    /**
     * @brief Disconnects from the TTY device, performing appropriate cleanup.
     *
     * @return TTYResponse indicating the result of the disconnection.
     */
    TTYResponse disconnect() noexcept;

    /**
     * @brief Enables or disables debugging information.
     *
     * @param enabled True to enable debugging, false to disable.
     */
    void setDebug(bool enabled) noexcept;

    /**
     * @brief Gets the error message corresponding to a given TTYResponse code.
     *
     * @param code The TTYResponse code for which to retrieve the error message.
     * @return A string containing the error message.
     */
    [[nodiscard]]
    std::string getErrorMessage(TTYResponse code) const noexcept;

    /**
     * @brief Gets the file descriptor of the TTY port.
     *
     * @return The integer file descriptor of the TTY port.
     */
    [[nodiscard]]
    int getPortFD() const noexcept;

    /**
     * @brief Checks if the TTY port is connected.
     *
     * @return True if connected, false otherwise.
     */
    [[nodiscard]]
    bool isConnected() const noexcept;

private:
    // Forward declaration of the private implementation class
    class Impl;

    // Unique pointer to the implementation
    std::unique_ptr<Impl> m_pImpl;
};

// Concept to check if a type is byte-like (usable in TTY operations)
template <typename T>
concept ByteLike = std::is_trivially_copyable_v<T> && sizeof(T) == 1;

// Helper to create a byte span from a container of byte-like elements
template <typename Container>
    requires std::ranges::range<Container> &&
             ByteLike<std::ranges::range_value_t<Container>>
auto makeByteSpan(Container& container) {
    using value_type = std::ranges::range_value_t<Container>;
    return std::span<uint8_t>(
        reinterpret_cast<uint8_t*>(std::ranges::data(container)),
        std::ranges::size(container) * sizeof(value_type));
}

#endif  // ATOM_CONNECTION_TTYBASE_HPP
