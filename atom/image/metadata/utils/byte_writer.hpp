#ifndef ATOM_IMAGE_METADATA_BYTE_WRITER_HPP
#define ATOM_IMAGE_METADATA_BYTE_WRITER_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "../types/exif_types.hpp"
#include "byte_reader.hpp"

namespace atom::image::metadata {

/**
 * @brief Exception for buffer write errors
 */
class BufferWriteException : public std::runtime_error {
public:
    explicit BufferWriteException(const std::string& message)
        : std::runtime_error("Buffer write error: " + message) {}
};

/**
 * @brief Binary data writer with endianness support
 *
 * Provides methods to write various data types to a byte buffer
 * with automatic endianness handling.
 */
class ByteWriter {
public:
    /**
     * @brief Construct with initial capacity
     */
    explicit ByteWriter(size_t initialCapacity = 1024,
                        ByteOrder order = ByteOrder::BIG_ENDIAN)
        : byteOrder_(order) {
        buffer_.reserve(initialCapacity);
    }

    /**
     * @brief Construct from existing buffer
     */
    explicit ByteWriter(std::vector<std::byte> buffer,
                        ByteOrder order = ByteOrder::BIG_ENDIAN)
        : buffer_(std::move(buffer)), byteOrder_(order) {}

    // Non-copyable but movable
    ByteWriter(const ByteWriter&) = default;
    ByteWriter& operator=(const ByteWriter&) = default;
    ByteWriter(ByteWriter&&) = default;
    ByteWriter& operator=(ByteWriter&&) = default;

    /**
     * @brief Set byte order
     */
    void setByteOrder(ByteOrder order) noexcept { byteOrder_ = order; }

    /**
     * @brief Get current byte order
     */
    [[nodiscard]] ByteOrder getByteOrder() const noexcept { return byteOrder_; }

    /**
     * @brief Check if using little endian
     */
    [[nodiscard]] bool isLittleEndian() const noexcept {
        return byteOrder_ == ByteOrder::LITTLE_ENDIAN;
    }

    /**
     * @brief Get current write position (size)
     */
    [[nodiscard]] size_t position() const noexcept { return buffer_.size(); }

    /**
     * @brief Get buffer size
     */
    [[nodiscard]] size_t size() const noexcept { return buffer_.size(); }

    /**
     * @brief Check if buffer is empty
     */
    [[nodiscard]] bool empty() const noexcept { return buffer_.empty(); }

    /**
     * @brief Reserve capacity
     */
    void reserve(size_t capacity) { buffer_.reserve(capacity); }

    /**
     * @brief Clear buffer
     */
    void clear() noexcept { buffer_.clear(); }

    /**
     * @brief Get const reference to buffer
     */
    [[nodiscard]] const std::vector<std::byte>& buffer() const noexcept {
        return buffer_;
    }

    /**
     * @brief Get mutable reference to buffer
     */
    [[nodiscard]] std::vector<std::byte>& buffer() noexcept { return buffer_; }

    /**
     * @brief Move buffer out
     */
    [[nodiscard]] std::vector<std::byte> takeBuffer() noexcept {
        return std::move(buffer_);
    }

    /**
     * @brief Get data pointer
     */
    [[nodiscard]] const std::byte* data() const noexcept {
        return buffer_.data();
    }

    // ========== Write methods (append to end) ==========

    /**
     * @brief Write single byte
     */
    void writeUint8(uint8_t value) {
        buffer_.push_back(static_cast<std::byte>(value));
    }

    /**
     * @brief Write signed byte
     */
    void writeInt8(int8_t value) { writeUint8(static_cast<uint8_t>(value)); }

    /**
     * @brief Write 16-bit unsigned integer
     */
    void writeUint16(uint16_t value) {
        if (byteOrder_ == ByteOrder::LITTLE_ENDIAN) {
            buffer_.push_back(static_cast<std::byte>(value & 0xFF));
            buffer_.push_back(static_cast<std::byte>((value >> 8) & 0xFF));
        } else {
            buffer_.push_back(static_cast<std::byte>((value >> 8) & 0xFF));
            buffer_.push_back(static_cast<std::byte>(value & 0xFF));
        }
    }

    /**
     * @brief Write 16-bit signed integer
     */
    void writeInt16(int16_t value) {
        writeUint16(static_cast<uint16_t>(value));
    }

    /**
     * @brief Write 32-bit unsigned integer
     */
    void writeUint32(uint32_t value) {
        if (byteOrder_ == ByteOrder::LITTLE_ENDIAN) {
            buffer_.push_back(static_cast<std::byte>(value & 0xFF));
            buffer_.push_back(static_cast<std::byte>((value >> 8) & 0xFF));
            buffer_.push_back(static_cast<std::byte>((value >> 16) & 0xFF));
            buffer_.push_back(static_cast<std::byte>((value >> 24) & 0xFF));
        } else {
            buffer_.push_back(static_cast<std::byte>((value >> 24) & 0xFF));
            buffer_.push_back(static_cast<std::byte>((value >> 16) & 0xFF));
            buffer_.push_back(static_cast<std::byte>((value >> 8) & 0xFF));
            buffer_.push_back(static_cast<std::byte>(value & 0xFF));
        }
    }

    /**
     * @brief Write 32-bit signed integer
     */
    void writeInt32(int32_t value) {
        writeUint32(static_cast<uint32_t>(value));
    }

    /**
     * @brief Write 64-bit unsigned integer
     */
    void writeUint64(uint64_t value) {
        if (byteOrder_ == ByteOrder::LITTLE_ENDIAN) {
            for (int i = 0; i < 8; ++i) {
                buffer_.push_back(
                    static_cast<std::byte>((value >> (i * 8)) & 0xFF));
            }
        } else {
            for (int i = 7; i >= 0; --i) {
                buffer_.push_back(
                    static_cast<std::byte>((value >> (i * 8)) & 0xFF));
            }
        }
    }

    /**
     * @brief Write 64-bit signed integer
     */
    void writeInt64(int64_t value) {
        writeUint64(static_cast<uint64_t>(value));
    }

    /**
     * @brief Write 32-bit float
     */
    void writeFloat(float value) {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(float));
        writeUint32(bits);
    }

    /**
     * @brief Write 64-bit double
     */
    void writeDouble(double value) {
        uint64_t bits;
        std::memcpy(&bits, &value, sizeof(double));
        writeUint64(bits);
    }

    /**
     * @brief Write rational number
     */
    void writeRational(const Rational& r) {
        writeUint32(r.numerator);
        writeUint32(r.denominator);
    }

    /**
     * @brief Write rational from double
     */
    void writeRational(double value, uint32_t precision = 10000) {
        writeRational(Rational::fromDouble(value, precision));
    }

    /**
     * @brief Write signed rational number
     */
    void writeSRational(const SRational& r) {
        writeInt32(r.numerator);
        writeInt32(r.denominator);
    }

    /**
     * @brief Write string (without null terminator)
     */
    void writeString(const std::string& str) {
        for (char c : str) {
            buffer_.push_back(static_cast<std::byte>(c));
        }
    }

    /**
     * @brief Write null-terminated string
     */
    void writeNullTerminatedString(const std::string& str) {
        writeString(str);
        buffer_.push_back(std::byte{0});
    }

    /**
     * @brief Write fixed-length string (padded with nulls if shorter)
     */
    void writeFixedString(const std::string& str, size_t length) {
        size_t writeLen = std::min(str.size(), length);
        for (size_t i = 0; i < writeLen; ++i) {
            buffer_.push_back(static_cast<std::byte>(str[i]));
        }
        for (size_t i = writeLen; i < length; ++i) {
            buffer_.push_back(std::byte{0});
        }
    }

    /**
     * @brief Write raw bytes
     */
    void writeBytes(const uint8_t* data, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            buffer_.push_back(static_cast<std::byte>(data[i]));
        }
    }

    /**
     * @brief Write raw bytes from vector
     */
    void writeBytes(const std::vector<uint8_t>& data) {
        for (uint8_t b : data) {
            buffer_.push_back(static_cast<std::byte>(b));
        }
    }

    /**
     * @brief Write std::byte vector
     */
    void writeBytes(const std::vector<std::byte>& data) {
        buffer_.insert(buffer_.end(), data.begin(), data.end());
    }

    /**
     * @brief Write padding bytes
     */
    void writePadding(size_t count, std::byte value = std::byte{0}) {
        for (size_t i = 0; i < count; ++i) {
            buffer_.push_back(value);
        }
    }

    // ========== Write at specific offset ==========

    /**
     * @brief Write uint16 at specific offset
     */
    void writeUint16At(size_t offset, uint16_t value) {
        ensureSize(offset + 2);
        if (byteOrder_ == ByteOrder::LITTLE_ENDIAN) {
            buffer_[offset] = static_cast<std::byte>(value & 0xFF);
            buffer_[offset + 1] = static_cast<std::byte>((value >> 8) & 0xFF);
        } else {
            buffer_[offset] = static_cast<std::byte>((value >> 8) & 0xFF);
            buffer_[offset + 1] = static_cast<std::byte>(value & 0xFF);
        }
    }

    /**
     * @brief Write uint32 at specific offset
     */
    void writeUint32At(size_t offset, uint32_t value) {
        ensureSize(offset + 4);
        if (byteOrder_ == ByteOrder::LITTLE_ENDIAN) {
            buffer_[offset] = static_cast<std::byte>(value & 0xFF);
            buffer_[offset + 1] = static_cast<std::byte>((value >> 8) & 0xFF);
            buffer_[offset + 2] = static_cast<std::byte>((value >> 16) & 0xFF);
            buffer_[offset + 3] = static_cast<std::byte>((value >> 24) & 0xFF);
        } else {
            buffer_[offset] = static_cast<std::byte>((value >> 24) & 0xFF);
            buffer_[offset + 1] = static_cast<std::byte>((value >> 16) & 0xFF);
            buffer_[offset + 2] = static_cast<std::byte>((value >> 8) & 0xFF);
            buffer_[offset + 3] = static_cast<std::byte>(value & 0xFF);
        }
    }

    /**
     * @brief Write bytes at specific offset
     */
    void writeBytesAt(size_t offset, const uint8_t* data, size_t count) {
        ensureSize(offset + count);
        for (size_t i = 0; i < count; ++i) {
            buffer_[offset + i] = static_cast<std::byte>(data[i]);
        }
    }

    // ========== Alignment helpers ==========

    /**
     * @brief Align to word boundary (2 bytes)
     */
    void alignToWord() {
        if (buffer_.size() % 2 != 0) {
            buffer_.push_back(std::byte{0});
        }
    }

    /**
     * @brief Align to dword boundary (4 bytes)
     */
    void alignToDword() {
        while (buffer_.size() % 4 != 0) {
            buffer_.push_back(std::byte{0});
        }
    }

    /**
     * @brief Align to specific boundary
     */
    void alignTo(size_t alignment) {
        while (buffer_.size() % alignment != 0) {
            buffer_.push_back(std::byte{0});
        }
    }

    // ========== Static utility functions ==========

    /**
     * @brief Write uint16 big-endian to pointer
     */
    static void writeUint16Be(std::byte* data, uint16_t value) noexcept {
        data[0] = static_cast<std::byte>((value >> 8) & 0xFF);
        data[1] = static_cast<std::byte>(value & 0xFF);
    }

    /**
     * @brief Write uint16 little-endian to pointer
     */
    static void writeUint16Le(std::byte* data, uint16_t value) noexcept {
        data[0] = static_cast<std::byte>(value & 0xFF);
        data[1] = static_cast<std::byte>((value >> 8) & 0xFF);
    }

    /**
     * @brief Write uint32 big-endian to pointer
     */
    static void writeUint32Be(std::byte* data, uint32_t value) noexcept {
        data[0] = static_cast<std::byte>((value >> 24) & 0xFF);
        data[1] = static_cast<std::byte>((value >> 16) & 0xFF);
        data[2] = static_cast<std::byte>((value >> 8) & 0xFF);
        data[3] = static_cast<std::byte>(value & 0xFF);
    }

    /**
     * @brief Write uint32 little-endian to pointer
     */
    static void writeUint32Le(std::byte* data, uint32_t value) noexcept {
        data[0] = static_cast<std::byte>(value & 0xFF);
        data[1] = static_cast<std::byte>((value >> 8) & 0xFF);
        data[2] = static_cast<std::byte>((value >> 16) & 0xFF);
        data[3] = static_cast<std::byte>((value >> 24) & 0xFF);
    }

private:
    std::vector<std::byte> buffer_;
    ByteOrder byteOrder_;

    void ensureSize(size_t size) {
        if (buffer_.size() < size) {
            buffer_.resize(size, std::byte{0});
        }
    }
};

}  // namespace atom::image::metadata

#endif  // ATOM_IMAGE_METADATA_BYTE_WRITER_HPP
