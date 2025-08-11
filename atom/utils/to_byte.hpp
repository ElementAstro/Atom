#ifndef ATOM_UTILS_TO_BYTE_HPP
#define ATOM_UTILS_TO_BYTE_HPP

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <list>
#include <map>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#include "atom/algorithm/rust_numeric.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/endian/conversion.hpp>
#include <boost/exception/all.hpp>
#include <boost/format.hpp>
#endif

// Byte order utilities
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define ATOM_LITTLE_ENDIAN 1
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define ATOM_BIG_ENDIAN 1
#endif

namespace atom::utils {

/**
 * @brief Concept to determine if a type is a numeric type
 */
template <typename T>
concept Number = std::is_arithmetic_v<T>;

/**
 * @brief Concept to determine if a type is an enumeration
 */
template <typename T>
concept Enum = std::is_enum_v<T>;

/**
 * @brief Concept to determine if a type is a string type
 */
template <typename T>
concept StringType = std::is_same_v<std::decay_t<T>, std::string> ||
                     std::is_same_v<std::decay_t<T>, std::string_view>;

/**
 * @brief Concept to determine if a type is a character type
 */
template <typename T>
concept AnyChar = std::is_same_v<std::decay_t<T>, char> ||
                  std::is_same_v<std::decay_t<T>, wchar_t> ||
                  std::is_same_v<std::decay_t<T>, char8_t> ||
                  std::is_same_v<std::decay_t<T>, char16_t> ||
                  std::is_same_v<std::decay_t<T>, char32_t>;

/**
 * @brief Concept to determine if a type is trivially copyable
 */
template <typename T>
concept TriviallyCopyable = std::is_trivially_copyable_v<T>;

/**
 * @brief Concept to determine if a type is a Rust numeric type
 */
template <typename T>
concept RustNumeric = std::is_same_v<T, atom::algorithm::i8> ||
                      std::is_same_v<T, atom::algorithm::i16> ||
                      std::is_same_v<T, atom::algorithm::i32> ||
                      std::is_same_v<T, atom::algorithm::i64> ||
                      std::is_same_v<T, atom::algorithm::u8> ||
                      std::is_same_v<T, atom::algorithm::u16> ||
                      std::is_same_v<T, atom::algorithm::u32> ||
                      std::is_same_v<T, atom::algorithm::u64> ||
                      std::is_same_v<T, atom::algorithm::f32> ||
                      std::is_same_v<T, atom::algorithm::f64> ||
                      std::is_same_v<T, atom::algorithm::isize> ||
                      std::is_same_v<T, atom::algorithm::usize>;

/**
 * @brief Concept to determine if a type is serializable as plain bytes
 */
template <typename T>
concept Serializable = Number<T> || Enum<T> || StringType<T> || AnyChar<T> ||
                       TriviallyCopyable<T> || RustNumeric<T>;

/**
 * @brief Custom exception type with additional information
 *
 * If Boost is enabled, this exception type inherits from Boost's exception
 * facilities to provide enriched error information.
 */
#ifdef ATOM_USE_BOOST
struct SerializationException : virtual std::runtime_error,
                                virtual boost::exception {
    explicit SerializationException(const std::string& message)
        : std::runtime_error(message) {}
};
#else
using SerializationException = std::runtime_error;
#endif

namespace detail {
/**
 * @brief Throws a serialization error with formatted message
 * @param args Arguments to format into error message
 */
template <typename... Args>
[[noreturn]] void throwSerializationError(Args&&... args) {
#ifdef ATOM_USE_BOOST
    throw SerializationException(
        boost::str(boost::format(std::forward<Args>(args)...)));
#else
    std::ostringstream oss;
    (oss << ... << std::forward<Args>(args));
    throw SerializationException(oss.str());
#endif
}

/**
 * @brief Adjusts byte order for cross-platform serialization
 * @tparam T Type to adjust byte order for
 * @param value Value to adjust (modified in place)
 */
template <typename T>
void adjustByteOrder([[maybe_unused]] T& value) {
#ifdef ATOM_USE_BOOST
    boost::endian::conditional_reverse_inplace<boost::endian::order::native,
                                               boost::endian::order::little>(
        value);
#elif defined(ATOM_BIG_ENDIAN)
    if constexpr (std::is_arithmetic_v<T> && sizeof(T) > 1) {
        auto* ptr = reinterpret_cast<uint8_t*>(&value);
        std::reverse(ptr, ptr + sizeof(T));
    }
#endif
}

/**
 * @brief Get underlying value for Rust numeric types
 */
template <typename T>
constexpr auto getUnderlyingValue(const T& value) {
    if constexpr (RustNumeric<T>) {
        if constexpr (std::is_same_v<T, atom::algorithm::i8>) {
            return static_cast<std::int8_t>(value);
        } else if constexpr (std::is_same_v<T, atom::algorithm::i16>) {
            return static_cast<std::int16_t>(value);
        } else if constexpr (std::is_same_v<T, atom::algorithm::i32>) {
            return static_cast<std::int32_t>(value);
        } else if constexpr (std::is_same_v<T, atom::algorithm::i64>) {
            return static_cast<std::int64_t>(value);
        } else if constexpr (std::is_same_v<T, atom::algorithm::u8>) {
            return static_cast<std::uint8_t>(value);
        } else if constexpr (std::is_same_v<T, atom::algorithm::u16>) {
            return static_cast<std::uint16_t>(value);
        } else if constexpr (std::is_same_v<T, atom::algorithm::u32>) {
            return static_cast<std::uint32_t>(value);
        } else if constexpr (std::is_same_v<T, atom::algorithm::u64>) {
            return static_cast<std::uint64_t>(value);
        } else if constexpr (std::is_same_v<T, atom::algorithm::f32>) {
            return static_cast<float>(value);
        } else if constexpr (std::is_same_v<T, atom::algorithm::f64>) {
            return static_cast<double>(value);
        } else if constexpr (std::is_same_v<T, atom::algorithm::isize>) {
            return static_cast<std::ptrdiff_t>(value);
        } else if constexpr (std::is_same_v<T, atom::algorithm::usize>) {
            return static_cast<std::size_t>(value);
        }
    } else {
        return value;
    }
}
}  // namespace detail

/**
 * @brief Serializes a serializable type into a vector of bytes
 *
 * This function converts the given data of a serializable type into a
 * vector of bytes. The size of the vector is equal to the size of the type.
 *
 * @tparam T The type of the data to serialize, must satisfy the Serializable
 * concept
 * @param data The data to serialize
 * @return std::vector<uint8_t> A vector of bytes representing the serialized
 * data
 */
template <Serializable T>
auto serialize(const T& data) -> std::vector<uint8_t> {
    if constexpr (RustNumeric<T>) {
        auto underlying = detail::getUnderlyingValue(data);
        std::vector<uint8_t> bytes(sizeof(underlying));
        detail::adjustByteOrder(underlying);
        std::memcpy(bytes.data(), &underlying, sizeof(underlying));
        return bytes;
    } else {
        std::vector<uint8_t> bytes(sizeof(T));
        T adjusted = data;
        detail::adjustByteOrder(adjusted);
        std::memcpy(bytes.data(), &adjusted, sizeof(T));
        return bytes;
    }
}

/**
 * @brief Serializes a std::string into a vector of bytes
 *
 * This function converts a std::string into a vector of bytes, including
 * the size of the string followed by the string's data.
 *
 * @param str The string to serialize
 * @return std::vector<uint8_t> A vector of bytes representing the serialized
 * string
 */
inline auto serialize(const std::string& str) -> std::vector<uint8_t> {
    const size_t size = str.size();
    std::vector<uint8_t> bytes;
    bytes.reserve(sizeof(size) + size);

    auto sizeBytes = serialize(size);
    bytes.insert(bytes.end(), sizeBytes.begin(), sizeBytes.end());
    bytes.insert(bytes.end(), str.begin(), str.end());

    return bytes;
}

/**
 * @brief Serializes a std::vector into a vector of bytes
 *
 * This function converts a std::vector into a vector of bytes, including
 * the size of the vector followed by the serialized elements.
 *
 * @tparam T The type of elements in the vector
 * @param vec The vector to serialize
 * @return std::vector<uint8_t> A vector of bytes representing the serialized
 * vector
 */
template <typename T>
auto serialize(const std::vector<T>& vec) -> std::vector<uint8_t> {
    std::vector<uint8_t> bytes;
    const size_t size = vec.size();

    auto sizeBytes = serialize(size);
    bytes.insert(bytes.end(), sizeBytes.begin(), sizeBytes.end());

    for (const auto& item : vec) {
        auto itemBytes = serialize(item);
        bytes.insert(bytes.end(), itemBytes.begin(), itemBytes.end());
    }
    return bytes;
}

/**
 * @brief Serializes a std::list into a vector of bytes
 *
 * This function converts a std::list into a vector of bytes, including
 * the size of the list followed by the serialized elements.
 *
 * @tparam T The type of elements in the list
 * @param list The list to serialize
 * @return std::vector<uint8_t> A vector of bytes representing the serialized
 * list
 */
template <typename T>
auto serialize(const std::list<T>& list) -> std::vector<uint8_t> {
    std::vector<uint8_t> bytes;
    const size_t size = list.size();

    auto sizeBytes = serialize(size);
    bytes.insert(bytes.end(), sizeBytes.begin(), sizeBytes.end());

    for (const auto& item : list) {
        auto itemBytes = serialize(item);
        bytes.insert(bytes.end(), itemBytes.begin(), itemBytes.end());
    }
    return bytes;
}

/**
 * @brief Serializes a std::map into a vector of bytes
 *
 * This function converts a std::map into a vector of bytes, including
 * the size of the map followed by the serialized key-value pairs.
 *
 * @tparam Key The type of keys in the map
 * @tparam Value The type of values in the map
 * @param map The map to serialize
 * @return std::vector<uint8_t> A vector of bytes representing the serialized
 * map
 */
template <typename Key, typename Value>
auto serialize(const std::map<Key, Value>& map) -> std::vector<uint8_t> {
    std::vector<uint8_t> bytes;
    const size_t size = map.size();

    auto sizeBytes = serialize(size);
    bytes.insert(bytes.end(), sizeBytes.begin(), sizeBytes.end());

    for (const auto& [key, value] : map) {
        auto keyBytes = serialize(key);
        auto valueBytes = serialize(value);
        bytes.insert(bytes.end(), keyBytes.begin(), keyBytes.end());
        bytes.insert(bytes.end(), valueBytes.begin(), valueBytes.end());
    }
    return bytes;
}

/**
 * @brief Serializes a std::optional into a vector of bytes
 *
 * This function converts a std::optional into a vector of bytes, including
 * a boolean indicating whether the optional has a value, followed by the
 * serialized value if it exists.
 *
 * @tparam T The type of the value contained in the optional
 * @param opt The optional to serialize
 * @return std::vector<uint8_t> A vector of bytes representing the serialized
 * optional
 */
template <typename T>
auto serialize(const std::optional<T>& opt) -> std::vector<uint8_t> {
    std::vector<uint8_t> bytes;
    const bool hasValue = opt.has_value();

    auto hasValueBytes = serialize(hasValue);
    bytes.insert(bytes.end(), hasValueBytes.begin(), hasValueBytes.end());

    if (hasValue) {
        auto valueBytes = serialize(opt.value());
        bytes.insert(bytes.end(), valueBytes.begin(), valueBytes.end());
    }
    return bytes;
}

/**
 * @brief Serializes a std::variant into a vector of bytes
 *
 * This function converts a std::variant into a vector of bytes, including
 * the index of the active alternative, followed by the serialized value.
 *
 * @tparam Ts The types of the alternatives in the variant
 * @param var The variant to serialize
 * @return std::vector<uint8_t> A vector of bytes representing the serialized
 * variant
 */
template <typename... Ts>
auto serialize(const std::variant<Ts...>& var) -> std::vector<uint8_t> {
    std::vector<uint8_t> bytes;
    const size_t index = var.index();

    auto indexBytes = serialize(index);
    bytes.insert(bytes.end(), indexBytes.begin(), indexBytes.end());

    std::visit(
        [&bytes](const auto& value) {
            auto valueBytes = serialize(value);
            bytes.insert(bytes.end(), valueBytes.begin(), valueBytes.end());
        },
        var);
    return bytes;
}

/**
 * @brief Serializes a std::tuple into a vector of bytes
 *
 * This function converts a std::tuple into a vector of bytes by serializing
 * each element.
 *
 * @tparam Ts The types of elements in the tuple
 * @param tup The tuple to serialize
 * @return std::vector<uint8_t> A vector of bytes representing the serialized
 * tuple
 */
template <typename... Ts>
auto serialize(const std::tuple<Ts...>& tup) -> std::vector<uint8_t> {
    std::vector<uint8_t> bytes;
    std::apply(
        [&bytes](const auto&... args) {
            (bytes.insert(bytes.end(), serialize(args).begin(),
                          serialize(args).end()),
             ...);
        },
        tup);
    return bytes;
}

/**
 * @brief Deserializes a type from a span of bytes
 *
 * This function extracts data of a serializable type from the given span of
 * bytes, starting from the specified offset, and advances the offset.
 *
 * @tparam T The type of data to deserialize, must satisfy the Serializable
 * concept
 * @param bytes The span of bytes containing the serialized data
 * @param offset The offset in the span from where to start deserializing
 * @return T The deserialized data
 * @throws SerializationException if the data is too short to contain the
 * expected type
 */
template <Serializable T>
auto deserialize(const std::span<const uint8_t>& bytes, size_t& offset) -> T {
    if constexpr (RustNumeric<T>) {
        using UnderlyingType =
            decltype(detail::getUnderlyingValue(std::declval<T>()));
        if (bytes.size() < offset + sizeof(UnderlyingType)) {
            detail::throwSerializationError(
                "Invalid data: too short to contain the expected type at "
                "offset ",
                offset);
        }
        UnderlyingType data;
        std::memcpy(&data, bytes.data() + offset, sizeof(UnderlyingType));
        offset += sizeof(UnderlyingType);
        detail::adjustByteOrder(data);
        return static_cast<T>(data);
    } else {
        if (bytes.size() < offset + sizeof(T)) {
            detail::throwSerializationError(
                "Invalid data: too short to contain the expected type at "
                "offset ",
                offset);
        }
        T data;
        std::memcpy(&data, bytes.data() + offset, sizeof(T));
        offset += sizeof(T);
        detail::adjustByteOrder(data);
        return data;
    }
}

/**
 * @brief Deserializes a std::string from a span of bytes
 *
 * This function extracts a std::string from the given span of bytes,
 * starting from the specified offset. The string is preceded by its size.
 *
 * @param bytes The span of bytes containing the serialized data
 * @param offset The offset in the span from where to start deserializing
 * @return std::string The deserialized string
 * @throws SerializationException if the size of the string or the data is
 * invalid
 */
inline auto deserializeString(const std::span<const uint8_t>& bytes,
                              size_t& offset) -> std::string {
    auto size = deserialize<size_t>(bytes, offset);
    if (bytes.size() < offset + size) {
        detail::throwSerializationError(
            "Invalid data: size mismatch while deserializing string at offset ",
            offset);
    }
    std::string str(reinterpret_cast<const char*>(bytes.data() + offset), size);
    offset += size;
    return str;
}

/**
 * @brief Deserializes a std::vector from a span of bytes
 *
 * This function extracts a std::vector from the given span of bytes,
 * starting from the specified offset. The vector is preceded by its size.
 *
 * @tparam T The type of elements in the vector
 * @param bytes The span of bytes containing the serialized data
 * @param offset The offset in the span from where to start deserializing
 * @return std::vector<T> The deserialized vector
 * @throws SerializationException if deserialization fails
 */
template <typename T>
auto deserializeVector(const std::span<const uint8_t>& bytes, size_t& offset)
    -> std::vector<T> {
    auto size = deserialize<size_t>(bytes, offset);
    std::vector<T> vec;
    vec.reserve(size);

    for (size_t i = 0; i < size; ++i) {
        if constexpr (StringType<T>) {
            vec.push_back(deserializeString(bytes, offset));
        } else {
            vec.push_back(deserialize<T>(bytes, offset));
        }
    }
    return vec;
}

/**
 * @brief Deserializes a std::list from a span of bytes
 *
 * This function extracts a std::list from the given span of bytes,
 * starting from the specified offset. The list is preceded by its size.
 *
 * @tparam T The type of elements in the list
 * @param bytes The span of bytes containing the serialized data
 * @param offset The offset in the span from where to start deserializing
 * @return std::list<T> The deserialized list
 * @throws SerializationException if deserialization fails
 */
template <typename T>
auto deserializeList(const std::span<const uint8_t>& bytes, size_t& offset)
    -> std::list<T> {
    auto size = deserialize<size_t>(bytes, offset);
    std::list<T> list;

    for (size_t i = 0; i < size; ++i) {
        if constexpr (StringType<T>) {
            list.push_back(deserializeString(bytes, offset));
        } else {
            list.push_back(deserialize<T>(bytes, offset));
        }
    }
    return list;
}

/**
 * @brief Deserializes a std::map from a span of bytes
 *
 * This function extracts a std::map from the given span of bytes,
 * starting from the specified offset. The map is preceded by its size,
 * followed by serialized key-value pairs.
 *
 * @tparam Key The type of keys in the map
 * @tparam Value The type of values in the map
 * @param bytes The span of bytes containing the serialized data
 * @param offset The offset in the span from where to start deserializing
 * @return std::map<Key, Value> The deserialized map
 * @throws SerializationException if deserialization fails
 */
template <typename Key, typename Value>
auto deserializeMap(const std::span<const uint8_t>& bytes, size_t& offset)
    -> std::map<Key, Value> {
    auto size = deserialize<size_t>(bytes, offset);
    std::map<Key, Value> map;

    for (size_t i = 0; i < size; ++i) {
        Key key;
        if constexpr (StringType<Key>) {
            key = deserializeString(bytes, offset);
        } else {
            key = deserialize<Key>(bytes, offset);
        }

        Value value;
        if constexpr (StringType<Value>) {
            value = deserializeString(bytes, offset);
        } else {
            value = deserialize<Value>(bytes, offset);
        }

        map.emplace(std::move(key), std::move(value));
    }
    return map;
}

/**
 * @brief Deserializes a std::optional from a span of bytes
 *
 * This function extracts a std::optional from the given span of bytes,
 * starting from the specified offset. It first reads a boolean indicating
 * whether the optional has a value, followed by the value if it exists.
 *
 * @tparam T The type of the value contained in the optional
 * @param bytes The span of bytes containing the serialized data
 * @param offset The offset in the span from where to start deserializing
 * @return std::optional<T> The deserialized optional
 * @throws SerializationException if deserialization fails
 */
template <typename T>
auto deserializeOptional(const std::span<const uint8_t>& bytes, size_t& offset)
    -> std::optional<T> {
    bool hasValue = deserialize<bool>(bytes, offset);
    if (hasValue) {
        if constexpr (StringType<T>) {
            return deserializeString(bytes, offset);
        } else {
            return deserialize<T>(bytes, offset);
        }
    }
    return std::nullopt;
}

/**
 * @brief Helper function to construct a std::variant from bytes
 *
 * This function constructs a std::variant from the given bytes and index
 * of the active alternative. It uses the provided index sequence to deserialize
 * the value based on the alternative index.
 *
 * @tparam Variant The type of the variant to construct
 * @tparam Is The index sequence for the variant alternatives
 * @param bytes The span of bytes containing the serialized data
 * @param offset The offset in the span from where to start deserializing
 * @param index The index of the active alternative in the variant
 * @param is The index sequence
 * @return Variant The constructed variant
 * @throws SerializationException if deserialization fails
 */
template <typename Variant, std::size_t... Is>
auto constructVariant(const std::span<const uint8_t>& bytes, size_t& offset,
                      size_t index, std::index_sequence<Is...>) -> Variant {
    Variant var;
    bool matched = false;

    ((index == Is ? (var = deserialize<std::variant_alternative_t<Is, Variant>>(
                         bytes, offset),
                     matched = true, void())
                  : void()),
     ...);

    if (!matched) {
        detail::throwSerializationError("Invalid variant index: ", index);
    }
    return var;
}

/**
 * @brief Deserializes a std::variant from a span of bytes
 *
 * This function extracts a std::variant from the given span of bytes,
 * starting from the specified offset. It first reads the index of the active
 * alternative, then deserializes the value based on the alternative index.
 *
 * @tparam Ts The types of the alternatives in the variant
 * @param bytes The span of bytes containing the serialized data
 * @param offset The offset in the span from where to start deserializing
 * @return std::variant<Ts...> The deserialized variant
 * @throws SerializationException if the index of the variant is out of range
 */
template <typename... Ts>
auto deserializeVariant(const std::span<const uint8_t>& bytes, size_t& offset)
    -> std::variant<Ts...> {
    auto index = deserialize<size_t>(bytes, offset);
    if (index >= sizeof...(Ts)) {
        detail::throwSerializationError("Invalid variant index: ", index,
                                        " out of range");
    }
    return constructVariant<std::variant<Ts...>>(
        bytes, offset, index, std::index_sequence_for<Ts...>{});
}

/**
 * @brief Deserializes a std::tuple from a span of bytes
 *
 * This function extracts a std::tuple from the given span of bytes, starting
 * from the specified offset.
 *
 * @tparam Ts The types of elements in the tuple
 * @param bytes The span of bytes containing the serialized data
 * @param offset The offset in the span from where to start deserializing
 * @return std::tuple<Ts...> The deserialized tuple
 * @throws SerializationException if deserialization fails
 */
template <typename... Ts>
auto deserializeTuple(const std::span<const uint8_t>& bytes, size_t& offset)
    -> std::tuple<Ts...> {
    return std::make_tuple(deserialize<Ts>(bytes, offset)...);
}

/**
 * @brief RAII wrapper for file operations
 */
class FileHandle {
    std::fstream file_;

public:
    /**
     * @brief Constructor that opens a file with specified mode
     * @param filename Name of the file to open
     * @param mode File open mode
     * @throws SerializationException if file cannot be opened
     */
    FileHandle(const std::string& filename, std::ios::openmode mode)
        : file_(filename, mode) {
        if (!file_) {
            detail::throwSerializationError("Could not open file: ", filename);
        }
    }

    /**
     * @brief Destructor that ensures file is properly closed
     */
    ~FileHandle() {
        if (file_.is_open()) {
            file_.close();
        }
    }

    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
    FileHandle(FileHandle&&) = default;
    FileHandle& operator=(FileHandle&&) = default;

    /**
     * @brief Seeks to a position in the file
     * @param pos Position to seek to
     * @param dir Direction for seeking
     */
    void seekg(std::streampos pos, std::ios::seekdir dir = std::ios::beg) {
        file_.seekg(pos, dir);
    }

    /**
     * @brief Gets current position in file
     * @return Current file position
     */
    std::streampos tellg() { return file_.tellg(); }

    /**
     * @brief Reads data from file
     * @param s Buffer to read into
     * @param count Number of bytes to read
     * @return true if read was successful
     */
    bool read(char* s, std::streamsize count) {
        return static_cast<bool>(file_.read(s, count));
    }

    /**
     * @brief Writes data to file
     * @param data Vector of bytes to write
     * @throws SerializationException if write fails
     */
    void write(const std::vector<uint8_t>& data) {
        file_.write(reinterpret_cast<const char*>(data.data()),
                    static_cast<std::streamsize>(data.size()));
        if (!file_) {
            detail::throwSerializationError("Failed to write data to file");
        }
    }
};

/**
 * @brief Saves serialized data to a file
 *
 * This function writes the given vector of bytes to a file. If the file cannot
 * be opened for writing, it throws a runtime error.
 *
 * @param data The vector of bytes to save
 * @param filename The name of the file to write to
 * @throws SerializationException if the file cannot be opened for writing
 */
inline void saveToFile(const std::vector<uint8_t>& data,
                       const std::string& filename) {
    FileHandle file(filename, std::ios::binary | std::ios::out);
    file.write(data);
}

/**
 * @brief Loads serialized data from a file
 *
 * This function reads the contents of a file into a vector of bytes. If the
 * file cannot be opened for reading, it throws a runtime error.
 *
 * @param filename The name of the file to read from
 * @return std::vector<uint8_t> A vector of bytes representing the loaded data
 * @throws SerializationException if the file cannot be opened for reading
 */
inline auto loadFromFile(const std::string& filename) -> std::vector<uint8_t> {
    FileHandle file(filename, std::ios::binary | std::ios::in);
    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    if (size < 0) {
        detail::throwSerializationError("Failed to determine size of file: ",
                                        filename);
    }
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        detail::throwSerializationError("Failed to read data from file: ",
                                        filename);
    }
    return data;
}

}  // namespace atom::utils

#endif  // ATOM_UTILS_TO_BYTE_HPP
