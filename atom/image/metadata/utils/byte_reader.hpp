#ifndef ATOM_IMAGE_METADATA_BYTE_READER_HPP
#define ATOM_IMAGE_METADATA_BYTE_READER_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include "../../exceptions.hpp"
#include "../types/exif_types.hpp"

namespace atom::image::metadata {

/**
 * @brief Byte order (endianness)
 */
enum class ByteOrder {
    LITTLE_ENDIAN,  ///< Intel byte order (least significant byte first)
    BIG_ENDIAN      ///< Motorola byte order (most significant byte first)
};

// Use unified BufferReadException from exceptions.hpp
using BufferReadException = atom::image::BufferReadException;

/**
 * @brief Safe binary data reader with bounds checking
 *
 * Provides methods to read various data types from a byte buffer
 * with automatic endianness handling and bounds checking.
 */
class ByteReader {
public:
    /**
     * @brief Construct from raw pointer and size
     */
    ByteReader(const std::byte* data, size_t size,
               ByteOrder order = ByteOrder::BIG_ENDIAN)
        : data_(data), size_(size), position_(0), byteOrder_(order) {}

    /**
     * @brief Construct from vector
     */
    explicit ByteReader(const std::vector<std::byte>& data,
                        ByteOrder order = ByteOrder::BIG_ENDIAN)
        : data_(data.data()),
          size_(data.size()),
          position_(0),
          byteOrder_(order) {}

    /**
     * @brief Construct from uint8_t pointer
     */
    ByteReader(const uint8_t* data, size_t size,
               ByteOrder order = ByteOrder::BIG_ENDIAN)
        : data_(reinterpret_cast<const std::byte*>(data)),
          size_(size),
          position_(0),
          byteOrder_(order) {}

    /**
     * @brief Construct from char pointer
     */
    ByteReader(const char* data, size_t size,
               ByteOrder order = ByteOrder::BIG_ENDIAN)
        : data_(reinterpret_cast<const std::byte*>(data)),
          size_(size),
          position_(0),
          byteOrder_(order) {}

    // Non-copyable but movable
    ByteReader(const ByteReader&) = default;
    ByteReader& operator=(const ByteReader&) = default;
    ByteReader(ByteReader&&) = default;
    ByteReader& operator=(ByteReader&&) = default;

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
     * @brief Get current read position
     */
    [[nodiscard]] size_t position() const noexcept { return position_; }

    /**
     * @brief Get total buffer size
     */
    [[nodiscard]] size_t size() const noexcept { return size_; }

    /**
     * @brief Get remaining bytes
     */
    [[nodiscard]] size_t remaining() const noexcept {
        return position_ < size_ ? size_ - position_ : 0;
    }

    /**
     * @brief Check if end of buffer reached
     */
    [[nodiscard]] bool eof() const noexcept { return position_ >= size_; }

    /**
     * @brief Seek to absolute position
     */
    void seek(size_t pos) {
        if (pos > size_) {
            throw BufferReadException(
                "Seek position out of bounds: " + std::to_string(pos) + " > " +
                std::to_string(size_));
        }
        position_ = pos;
    }

    /**
     * @brief Skip bytes from current position
     */
    void skip(size_t count) { seek(position_ + count); }

    /**
     * @brief Reset to beginning
     */
    void reset() noexcept { position_ = 0; }

    /**
     * @brief Check if reading n bytes is safe
     */
    [[nodiscard]] bool canRead(size_t n) const noexcept {
        return position_ + n <= size_;
    }

    /**
     * @brief Get pointer to current position
     */
    [[nodiscard]] const std::byte* current() const noexcept {
        return data_ + position_;
    }

    /**
     * @brief Get pointer to data at specific offset
     */
    [[nodiscard]] const std::byte* at(size_t offset) const {
        if (offset >= size_) {
            throw BufferReadException("Offset out of bounds");
        }
        return data_ + offset;
    }

    // ========== Read methods (advance position) ==========

    /**
     * @brief Read single byte
     */
    [[nodiscard]] uint8_t readUint8() {
        checkBounds(1);
        return std::to_integer<uint8_t>(data_[position_++]);
    }

    /**
     * @brief Read signed byte
     */
    [[nodiscard]] int8_t readInt8() { return static_cast<int8_t>(readUint8()); }

    /**
     * @brief Read 16-bit unsigned integer
     */
    [[nodiscard]] uint16_t readUint16() {
        checkBounds(2);
        uint16_t value = peekUint16At(position_);
        position_ += 2;
        return value;
    }

    /**
     * @brief Read 16-bit signed integer
     */
    [[nodiscard]] int16_t readInt16() {
        return static_cast<int16_t>(readUint16());
    }

    /**
     * @brief Read 32-bit unsigned integer
     */
    [[nodiscard]] uint32_t readUint32() {
        checkBounds(4);
        uint32_t value = peekUint32At(position_);
        position_ += 4;
        return value;
    }

    /**
     * @brief Read 32-bit signed integer
     */
    [[nodiscard]] int32_t readInt32() {
        return static_cast<int32_t>(readUint32());
    }

    /**
     * @brief Read 64-bit unsigned integer
     */
    [[nodiscard]] uint64_t readUint64() {
        checkBounds(8);
        uint64_t value = peekUint64At(position_);
        position_ += 8;
        return value;
    }

    /**
     * @brief Read 64-bit signed integer
     */
    [[nodiscard]] int64_t readInt64() {
        return static_cast<int64_t>(readUint64());
    }

    /**
     * @brief Read 32-bit float
     */
    [[nodiscard]] float readFloat() {
        uint32_t bits = readUint32();
        float value;
        std::memcpy(&value, &bits, sizeof(float));
        return value;
    }

    /**
     * @brief Read 64-bit double
     */
    [[nodiscard]] double readDouble() {
        uint64_t bits = readUint64();
        double value;
        std::memcpy(&value, &bits, sizeof(double));
        return value;
    }

    /**
     * @brief Read rational number (two 32-bit unsigned integers)
     */
    [[nodiscard]] Rational readRational() {
        Rational r;
        r.numerator = readUint32();
        r.denominator = readUint32();
        return r;
    }

    /**
     * @brief Read signed rational number (two 32-bit signed integers)
     */
    [[nodiscard]] SRational readSRational() {
        SRational r;
        r.numerator = readInt32();
        r.denominator = readInt32();
        return r;
    }

    /**
     * @brief Read fixed-length string
     */
    [[nodiscard]] std::string readString(size_t length) {
        checkBounds(length);
        std::string result(reinterpret_cast<const char*>(data_ + position_),
                           length);
        position_ += length;
        // Remove trailing nulls
        size_t end = result.find('\0');
        if (end != std::string::npos) {
            result.resize(end);
        }
        return result;
    }

    /**
     * @brief Read null-terminated string
     */
    [[nodiscard]] std::string readNullTerminatedString(size_t maxLength = 0) {
        std::string result;
        size_t count = 0;
        while (position_ < size_ && (maxLength == 0 || count < maxLength)) {
            uint8_t ch = std::to_integer<uint8_t>(data_[position_++]);
            if (ch == 0)
                break;
            result += static_cast<char>(ch);
            ++count;
        }
        return result;
    }

    /**
     * @brief Read bytes into vector
     */
    [[nodiscard]] std::vector<uint8_t> readBytes(size_t count) {
        checkBounds(count);
        std::vector<uint8_t> result(count);
        for (size_t i = 0; i < count; ++i) {
            result[i] = std::to_integer<uint8_t>(data_[position_++]);
        }
        return result;
    }

    /**
     * @brief Read bytes into std::byte vector
     */
    [[nodiscard]] std::vector<std::byte> readByteVector(size_t count) {
        checkBounds(count);
        std::vector<std::byte> result(data_ + position_,
                                      data_ + position_ + count);
        position_ += count;
        return result;
    }

    // ========== Peek methods (don't advance position) ==========

    /**
     * @brief Peek uint8 at current position
     */
    [[nodiscard]] uint8_t peekUint8() const {
        checkBounds(1);
        return std::to_integer<uint8_t>(data_[position_]);
    }

    /**
     * @brief Peek uint16 at current position
     */
    [[nodiscard]] uint16_t peekUint16() const {
        return peekUint16At(position_);
    }

    /**
     * @brief Peek uint32 at current position
     */
    [[nodiscard]] uint32_t peekUint32() const {
        return peekUint32At(position_);
    }

    /**
     * @brief Peek uint16 at specific offset
     */
    [[nodiscard]] uint16_t peekUint16At(size_t offset) const {
        if (offset + 2 > size_) {
            throw BufferReadException("Peek out of bounds");
        }
        const std::byte* p = data_ + offset;
        if (byteOrder_ == ByteOrder::LITTLE_ENDIAN) {
            return static_cast<uint16_t>(std::to_integer<int>(p[0])) |
                   (static_cast<uint16_t>(std::to_integer<int>(p[1])) << 8);
        } else {
            return (static_cast<uint16_t>(std::to_integer<int>(p[0])) << 8) |
                   static_cast<uint16_t>(std::to_integer<int>(p[1]));
        }
    }

    /**
     * @brief Peek uint32 at specific offset
     */
    [[nodiscard]] uint32_t peekUint32At(size_t offset) const {
        if (offset + 4 > size_) {
            throw BufferReadException("Peek out of bounds");
        }
        const std::byte* p = data_ + offset;
        if (byteOrder_ == ByteOrder::LITTLE_ENDIAN) {
            return std::to_integer<uint32_t>(p[0]) |
                   (std::to_integer<uint32_t>(p[1]) << 8) |
                   (std::to_integer<uint32_t>(p[2]) << 16) |
                   (std::to_integer<uint32_t>(p[3]) << 24);
        } else {
            return (std::to_integer<uint32_t>(p[0]) << 24) |
                   (std::to_integer<uint32_t>(p[1]) << 16) |
                   (std::to_integer<uint32_t>(p[2]) << 8) |
                   std::to_integer<uint32_t>(p[3]);
        }
    }

    /**
     * @brief Peek uint64 at specific offset
     */
    [[nodiscard]] uint64_t peekUint64At(size_t offset) const {
        if (offset + 8 > size_) {
            throw BufferReadException("Peek out of bounds");
        }
        const std::byte* p = data_ + offset;
        if (byteOrder_ == ByteOrder::LITTLE_ENDIAN) {
            return std::to_integer<uint64_t>(p[0]) |
                   (std::to_integer<uint64_t>(p[1]) << 8) |
                   (std::to_integer<uint64_t>(p[2]) << 16) |
                   (std::to_integer<uint64_t>(p[3]) << 24) |
                   (std::to_integer<uint64_t>(p[4]) << 32) |
                   (std::to_integer<uint64_t>(p[5]) << 40) |
                   (std::to_integer<uint64_t>(p[6]) << 48) |
                   (std::to_integer<uint64_t>(p[7]) << 56);
        } else {
            return (std::to_integer<uint64_t>(p[0]) << 56) |
                   (std::to_integer<uint64_t>(p[1]) << 48) |
                   (std::to_integer<uint64_t>(p[2]) << 40) |
                   (std::to_integer<uint64_t>(p[3]) << 32) |
                   (std::to_integer<uint64_t>(p[4]) << 24) |
                   (std::to_integer<uint64_t>(p[5]) << 16) |
                   (std::to_integer<uint64_t>(p[6]) << 8) |
                   std::to_integer<uint64_t>(p[7]);
        }
    }

    /**
     * @brief Peek rational at specific offset
     */
    [[nodiscard]] Rational peekRationalAt(size_t offset) const {
        Rational r;
        r.numerator = peekUint32At(offset);
        r.denominator = peekUint32At(offset + 4);
        return r;
    }

    /**
     * @brief Peek string at specific offset
     */
    [[nodiscard]] std::string peekStringAt(size_t offset, size_t length) const {
        if (offset + length > size_) {
            throw BufferReadException("Peek string out of bounds");
        }
        std::string result(reinterpret_cast<const char*>(data_ + offset),
                           length);
        size_t end = result.find('\0');
        if (end != std::string::npos) {
            result.resize(end);
        }
        return result;
    }

    // ========== Static utility functions ==========

    /**
     * @brief Read uint16 big-endian from pointer
     */
    static uint16_t readUint16Be(const std::byte* data) noexcept {
        return (static_cast<uint16_t>(std::to_integer<int>(data[0])) << 8) |
               static_cast<uint16_t>(std::to_integer<int>(data[1]));
    }

    /**
     * @brief Read uint16 little-endian from pointer
     */
    static uint16_t readUint16Le(const std::byte* data) noexcept {
        return static_cast<uint16_t>(std::to_integer<int>(data[0])) |
               (static_cast<uint16_t>(std::to_integer<int>(data[1])) << 8);
    }

    /**
     * @brief Read uint32 big-endian from pointer
     */
    static uint32_t readUint32Be(const std::byte* data) noexcept {
        return (std::to_integer<uint32_t>(data[0]) << 24) |
               (std::to_integer<uint32_t>(data[1]) << 16) |
               (std::to_integer<uint32_t>(data[2]) << 8) |
               std::to_integer<uint32_t>(data[3]);
    }

    /**
     * @brief Read uint32 little-endian from pointer
     */
    static uint32_t readUint32Le(const std::byte* data) noexcept {
        return std::to_integer<uint32_t>(data[0]) |
               (std::to_integer<uint32_t>(data[1]) << 8) |
               (std::to_integer<uint32_t>(data[2]) << 16) |
               (std::to_integer<uint32_t>(data[3]) << 24);
    }

    /**
     * @brief Create a sub-reader from current position
     */
    [[nodiscard]] ByteReader subReader(size_t length) const {
        if (position_ + length > size_) {
            throw BufferReadException("Sub-reader exceeds buffer bounds");
        }
        return ByteReader(data_ + position_, length, byteOrder_);
    }

    /**
     * @brief Create a sub-reader from specific offset
     */
    [[nodiscard]] ByteReader subReaderAt(size_t offset, size_t length) const {
        if (offset + length > size_) {
            throw BufferReadException("Sub-reader exceeds buffer bounds");
        }
        return ByteReader(data_ + offset, length, byteOrder_);
    }

private:
    const std::byte* data_;
    size_t size_;
    size_t position_;
    ByteOrder byteOrder_;

    void checkBounds(size_t n) const {
        if (position_ + n > size_) {
            throw BufferReadException("Read beyond buffer bounds: position=" +
                                      std::to_string(position_) +
                                      ", read=" + std::to_string(n) +
                                      ", size=" + std::to_string(size_));
        }
    }
};

}  // namespace atom::image::metadata

#endif  // ATOM_IMAGE_METADATA_BYTE_READER_HPP
