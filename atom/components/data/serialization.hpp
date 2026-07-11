/*
 * serialization.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Component Serialization/Deserialization System
Provides comprehensive serialization support for components with
JSON, binary, and custom format support, including versioning
and schema validation.

**************************************************/

#ifndef ATOM_COMPONENT_SERIALIZATION_HPP
#define ATOM_COMPONENT_SERIALIZATION_HPP

#include <any>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "../core/component.hpp"
#include "atom/type/expected.hpp"
#include "atom/type/json.hpp"

namespace atom::components {

/**
 * @brief Serialization format types
 */
enum class SerializationFormat : uint8_t {
    JSON,
    Binary,
    XML,
    MessagePack,
    Custom
};

/**
 * @brief Serialization options
 */
struct SerializationOptions {
    SerializationFormat format = SerializationFormat::JSON;
    bool includeMetadata = true;
    bool includeTimestamp = true;
    bool includeVersion = true;
    bool includeVariables = true;
    bool includeCommands = false;
    bool prettyPrint = false;
    bool compressData = false;
    bool encryptData = false;
    bool enableCompression = false;  // Alias for compressData
    bool enableEncryption = false;   // Alias for encryptData
    std::string encryptionKey;
    uint32_t version = 1;
    std::unordered_map<std::string, std::any> customOptions;
};

/**
 * @brief Serialization error categories
 */
enum class SerializationErrorCode : uint8_t {
    NoSerializer,       ///< No serializer registered for the requested format
    SerializeFailed,    ///< The serializer threw while encoding
    DeserializeFailed,  ///< The serializer threw while decoding
    InvalidData,        ///< Input data was malformed or truncated
    ChecksumMismatch,   ///< Binary checksum did not match
    FileError           ///< File could not be opened/read/written
};

/**
 * @brief Structured serialization error
 */
struct SerializationError {
    SerializationErrorCode code;
    std::string message;
};

/**
 * @brief Successful serialization payload (data + metadata)
 */
struct SerializationResult {
    std::vector<uint8_t> data;
    size_t originalSize = 0;
    size_t compressedSize = 0;
    std::chrono::microseconds serializationTime{0};
};

/**
 * @brief Successful deserialization payload (component + metadata)
 */
struct DeserializationResult {
    std::shared_ptr<Component> component;
    uint32_t version = 0;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::microseconds deserializationTime{0};
};

/// Result of a serialize call: payload on success, structured error on failure.
using SerializationOutcome =
    atom::type::expected<SerializationResult, SerializationError>;
/// Result of a deserialize call: payload on success, structured error on failure.
using DeserializationOutcome =
    atom::type::expected<DeserializationResult, SerializationError>;

/**
 * @brief Type trait for serializable types
 */
template <typename T>
struct is_serializable {
    template <typename U>
    static auto test(int)
        -> decltype(std::declval<U>().serialize(),
                    std::declval<U>().deserialize(
                        std::declval<const std::vector<uint8_t>&>()),
                    std::true_type{});

    template <typename>
    static std::false_type test(...);

    static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T>
inline constexpr bool is_serializable_v = is_serializable<T>::value;

/**
 * @brief Serializer interface for custom serialization implementations
 */
class ISerializer {
public:
    virtual ~ISerializer() = default;

    virtual SerializationOutcome serialize(
        const Component& component, const SerializationOptions& options) = 0;

    virtual DeserializationOutcome deserialize(
        const std::vector<uint8_t>& data,
        const SerializationOptions& options) = 0;

    virtual bool supportsFormat(SerializationFormat format) const = 0;
    virtual std::string getFormatName() const = 0;
};

/**
 * @brief JSON serializer implementation
 */
class JsonSerializer : public ISerializer {
public:
    SerializationOutcome serialize(const Component& component,
                                   const SerializationOptions& options) override;

    DeserializationOutcome deserialize(
        const std::vector<uint8_t>& data,
        const SerializationOptions& options) override;

    bool supportsFormat(SerializationFormat format) const override {
        return format == SerializationFormat::JSON;
    }

    std::string getFormatName() const override { return "JSON"; }

private:
    nlohmann::json componentToJson(const Component& component,
                                   const SerializationOptions& options);

    std::shared_ptr<Component> jsonToComponent(
        const nlohmann::json& json, const SerializationOptions& options);
};

/**
 * @brief Binary serializer implementation
 */
class BinarySerializer : public ISerializer {
public:
    SerializationOutcome serialize(const Component& component,
                                   const SerializationOptions& options) override;

    DeserializationOutcome deserialize(
        const std::vector<uint8_t>& data,
        const SerializationOptions& options) override;

    bool supportsFormat(SerializationFormat format) const override {
        return format == SerializationFormat::Binary;
    }

    std::string getFormatName() const override { return "Binary"; }

    // Binary format header
    struct BinaryHeader {
        uint32_t magic = 0x41544F4D;  // "ATOM"
        uint32_t version = 1;
        uint32_t dataSize = 0;
        uint32_t checksum = 0;
        uint64_t timestamp = 0;
        uint32_t flags = 0;  // Compression, encryption, etc.
        uint32_t reserved = 0;
    };

private:
    void writeHeader(std::vector<uint8_t>& buffer, const BinaryHeader& header);
    BinaryHeader readHeader(const std::vector<uint8_t>& buffer, size_t& offset);
    uint32_t calculateChecksum(const std::vector<uint8_t>& data);
};

/**
 * @brief Component serialization manager
 */
class SerializationManager {
public:
    /**
     * @brief Gets the singleton instance
     * @return Reference to the serialization manager
     */
    static SerializationManager& instance();

    /**
     * @brief Registers a custom serializer
     * @param serializer Serializer implementation
     */
    void registerSerializer(std::unique_ptr<ISerializer> serializer);

    /**
     * @brief Registers a custom serializer for a specific format
     * @param format The format to register the serializer for
     * @param serializer Serializer implementation
     */
    void registerSerializer(SerializationFormat format,
                            std::unique_ptr<ISerializer> serializer);

    /**
     * @brief Checks if a serializer is registered for a format
     * @param format The format to check
     * @return True if a serializer is registered for the format
     */
    bool hasSerializer(SerializationFormat format) const;

    /**
     * @brief Serializes a component
     * @param component Component to serialize
     * @param options Serialization options
     * @return Serialization result
     */
    SerializationOutcome serialize(const Component& component,
                                   const SerializationOptions& options = {});

    /**
     * @brief Deserializes component data
     * @param data Serialized data
     * @param options Deserialization options
     * @return Deserialization result
     */
    DeserializationOutcome deserialize(const std::vector<uint8_t>& data,
                                       const SerializationOptions& options = {});

    /**
     * @brief Serializes a component to file
     * @param component Component to serialize
     * @param filename Output filename
     * @param options Serialization options
     * @return True if successful
     */
    bool serializeToFile(const Component& component,
                         const std::string& filename,
                         const SerializationOptions& options = {});

    /**
     * @brief Deserializes a component from file
     * @param filename Input filename
     * @param options Deserialization options
     * @return Deserialization result
     */
    DeserializationOutcome deserializeFromFile(
        const std::string& filename, const SerializationOptions& options = {});

    /**
     * @brief Gets available serialization formats
     * @return Vector of supported formats
     */
    std::vector<SerializationFormat> getSupportedFormats() const;

    /**
     * @brief Validates serialized data
     * @param data Serialized data
     * @param format Expected format
     * @return True if data is valid
     */
    bool validateData(const std::vector<uint8_t>& data,
                      SerializationFormat format) const;

    /**
     * @brief Gets serialization statistics
     * @return Serialization statistics
     */
    struct Statistics {
        uint64_t totalSerializations = 0;
        uint64_t totalDeserializations = 0;
        uint64_t successfulSerializations = 0;
        uint64_t successfulDeserializations = 0;
        std::chrono::microseconds totalSerializationTime{0};
        std::chrono::microseconds totalDeserializationTime{0};
        size_t totalBytesWritten = 0;
        size_t totalBytesRead = 0;
        std::unordered_map<SerializationFormat, uint64_t> formatUsage;
    };

    const Statistics& getStatistics() const { return statistics_; }
    void resetStatistics() { statistics_ = Statistics{}; }

private:
    SerializationManager();
    ~SerializationManager() = default;

    SerializationManager(const SerializationManager&) = delete;
    SerializationManager& operator=(const SerializationManager&) = delete;

    ISerializer* getSerializer(SerializationFormat format);

    std::vector<std::unique_ptr<ISerializer>> serializers_;
    Statistics statistics_;
};

/**
 * @brief Serialization helper macros and functions
 */
#define ATOM_SERIALIZABLE(ClassName)                     \
    friend class atom::components::SerializationManager; \
    virtual std::vector<uint8_t> serialize() const;      \
    virtual bool deserialize(const std::vector<uint8_t>& data);

/**
 * @brief Schema validator for component serialization
 */
class SchemaValidator {
public:
    struct Schema {
        std::string name;
        uint32_t version;
        std::unordered_map<std::string, std::string> requiredFields;
        std::unordered_map<std::string, std::string> optionalFields;
        std::function<bool(const nlohmann::json&)> customValidator;
    };

    /**
     * @brief Registers a schema for a component type
     * @param componentType Component type name
     * @param schema Schema definition
     */
    void registerSchema(const std::string& componentType, const Schema& schema);

    /**
     * @brief Validates component data against schema
     * @param componentType Component type name
     * @param data Component data (JSON format)
     * @return True if valid
     */
    bool validate(const std::string& componentType,
                  const nlohmann::json& data) const;

    /**
     * @brief Gets schema for a component type
     * @param componentType Component type name
     * @return Optional schema
     */
    std::optional<Schema> getSchema(const std::string& componentType) const;

private:
    std::unordered_map<std::string, Schema> schemas_;
};

}  // namespace atom::components

#endif  // ATOM_COMPONENT_SERIALIZATION_HPP
