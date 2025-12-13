/*
 * serialization.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "serialization.hpp"

#include <algorithm>
#include <fstream>
// #include <sstream> // removed unused include

namespace atom::components {

// JsonSerializer implementation
SerializationResult JsonSerializer::serialize(
    const Component& component, const SerializationOptions& options) {
    const auto startTime = std::chrono::high_resolution_clock::now();
    SerializationResult result;

    try {
        nlohmann::json json = componentToJson(component, options);
        std::string jsonString = json.dump(options.prettyPrint ? 4 : -1);

        result.data.assign(jsonString.begin(), jsonString.end());
        result.originalSize = result.data.size();
        result.compressedSize = result.data.size();  // No compression for now
        result.success = true;

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage =
            "JSON serialization failed: " + std::string(e.what());
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    result.serializationTime =
        std::chrono::duration_cast<std::chrono::microseconds>(endTime -
                                                              startTime);

    return result;
}

DeserializationResult JsonSerializer::deserialize(
    const std::vector<uint8_t>& data, const SerializationOptions& options) {
    const auto startTime = std::chrono::high_resolution_clock::now();
    DeserializationResult result;

    try {
        std::string jsonString(data.begin(), data.end());
        nlohmann::json json = nlohmann::json::parse(jsonString);

        result.component = jsonToComponent(json, options);
        result.success = result.component != nullptr;

        if (json.contains("metadata") && json["metadata"].contains("version")) {
            result.version = json["metadata"]["version"].get<uint32_t>();
        }

        if (json.contains("metadata") &&
            json["metadata"].contains("timestamp")) {
            // Parse timestamp - simplified implementation
            result.timestamp = std::chrono::system_clock::now();
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage =
            "JSON deserialization failed: " + std::string(e.what());
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    result.deserializationTime =
        std::chrono::duration_cast<std::chrono::microseconds>(endTime -
                                                              startTime);

    return result;
}

nlohmann::json JsonSerializer::componentToJson(
    const Component& component, const SerializationOptions& options) {
    nlohmann::json json;

    // Basic component information
    json["name"] = std::string(component.getName());
    json["state"] = static_cast<int>(component.getState());

    // Metadata
    if (options.includeMetadata) {
        json["metadata"]["version"] = options.version;

        if (options.includeTimestamp) {
            auto now = std::chrono::system_clock::now();
            auto timestamp =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch())
                    .count();
            json["metadata"]["timestamp"] = timestamp;
        }

        json["metadata"]["format"] = "JSON";
        json["metadata"]["serializer"] = "JsonSerializer";
    }

    // Performance statistics
    const auto& stats = component.getPerformanceStats();
    json["performance"]["commandCallCount"] = stats.commandCallCount.load();
    json["performance"]["commandErrorCount"] = stats.commandErrorCount.load();
    json["performance"]["eventCount"] = stats.eventCount.load();

    // Variables (simplified - would need actual variable enumeration)
    json["variables"] = nlohmann::json::object();

    // Commands (simplified - would need actual command enumeration)
    json["commands"] = nlohmann::json::array();

    return json;
}

std::shared_ptr<Component> JsonSerializer::jsonToComponent(
    const nlohmann::json& json, const SerializationOptions& /*options*/) {
    if (!json.contains("name")) {
        throw std::runtime_error("Component name not found in JSON data");
    }

    std::string name = json["name"].get<std::string>();
    auto component = std::make_shared<Component>(name);

    // Restore state
    if (json.contains("state")) {
        ComponentState state =
            static_cast<ComponentState>(json["state"].get<int>());
        component->setState(state);
    }

    // Restore variables and commands would be implemented here
    // This is a simplified version

    return component;
}

// BinarySerializer implementation
SerializationResult BinarySerializer::serialize(
    const Component& component, const SerializationOptions& options) {
    const auto startTime = std::chrono::high_resolution_clock::now();
    SerializationResult result;

    try {
        std::vector<uint8_t> buffer;

        // Create header
        BinaryHeader header;
        header.version = options.version;
        header.timestamp =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();

        // Serialize component data (simplified)
        std::string name(component.getName());
        ComponentState state = component.getState();

        // Calculate data size
        header.dataSize =
            sizeof(uint32_t) + name.size() + sizeof(ComponentState);

        // Write header
        writeHeader(buffer, header);

        // Write component data
        uint32_t nameSize = static_cast<uint32_t>(name.size());
        buffer.insert(
            buffer.end(), reinterpret_cast<const uint8_t*>(&nameSize),
            reinterpret_cast<const uint8_t*>(&nameSize) + sizeof(nameSize));

        buffer.insert(buffer.end(), name.begin(), name.end());

        buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&state),
                      reinterpret_cast<const uint8_t*>(&state) + sizeof(state));

        // Calculate and update checksum
        header.checksum = calculateChecksum(buffer);
        writeHeader(buffer, header);  // Update header with checksum

        result.data = std::move(buffer);
        result.originalSize = result.data.size();
        result.compressedSize = result.data.size();
        result.success = true;

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage =
            "Binary serialization failed: " + std::string(e.what());
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    result.serializationTime =
        std::chrono::duration_cast<std::chrono::microseconds>(endTime -
                                                              startTime);

    return result;
}

DeserializationResult BinarySerializer::deserialize(
    const std::vector<uint8_t>& data, const SerializationOptions& /*options*/) {
    const auto startTime = std::chrono::high_resolution_clock::now();
    DeserializationResult result;

    try {
        if (data.size() < sizeof(BinaryHeader)) {
            throw std::runtime_error("Invalid binary data: too small");
        }

        size_t offset = 0;
        BinaryHeader header = readHeader(data, offset);

        if (header.magic != 0x41544F4D) {
            throw std::runtime_error("Invalid binary data: wrong magic number");
        }

        // Verify checksum (simplified)
        uint32_t calculatedChecksum = calculateChecksum(data);
        if (calculatedChecksum != header.checksum) {
            throw std::runtime_error("Invalid binary data: checksum mismatch");
        }

        // Read component data
        if (offset + sizeof(uint32_t) > data.size()) {
            throw std::runtime_error("Invalid binary data: truncated");
        }

        uint32_t nameSize;
        std::memcpy(&nameSize, &data[offset], sizeof(nameSize));
        offset += sizeof(nameSize);

        if (offset + nameSize + sizeof(ComponentState) > data.size()) {
            throw std::runtime_error("Invalid binary data: truncated");
        }

        std::string name(reinterpret_cast<const char*>(&data[offset]),
                         nameSize);
        offset += nameSize;

        ComponentState state;
        std::memcpy(&state, &data[offset], sizeof(state));

        // Create component
        auto component = std::make_shared<Component>(name);
        component->setState(state);

        result.component = component;
        result.version = header.version;
        result.timestamp = std::chrono::system_clock::time_point{
            std::chrono::milliseconds{header.timestamp}};
        result.success = true;

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage =
            "Binary deserialization failed: " + std::string(e.what());
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    result.deserializationTime =
        std::chrono::duration_cast<std::chrono::microseconds>(endTime -
                                                              startTime);

    return result;
}

void BinarySerializer::writeHeader(std::vector<uint8_t>& buffer,
                                   const BinaryHeader& header) {
    if (buffer.size() < sizeof(BinaryHeader)) {
        buffer.resize(sizeof(BinaryHeader));
    }

    std::memcpy(buffer.data(), &header, sizeof(BinaryHeader));
}

BinarySerializer::BinaryHeader BinarySerializer::readHeader(
    const std::vector<uint8_t>& buffer, size_t& offset) {
    BinaryHeader header;
    std::memcpy(&header, buffer.data() + offset, sizeof(BinaryHeader));
    offset += sizeof(BinaryHeader);
    return header;
}

uint32_t BinarySerializer::calculateChecksum(const std::vector<uint8_t>& data) {
    // Simple CRC32-like checksum (simplified implementation)
    uint32_t checksum = 0;
    for (size_t i = sizeof(BinaryHeader); i < data.size(); ++i) {
        checksum = (checksum << 1) ^ data[i];
    }
    return checksum;
}

// SerializationManager implementation
SerializationManager& SerializationManager::instance() {
    static SerializationManager instance;
    return instance;
}

SerializationManager::SerializationManager() {
    // Register default serializers
    registerSerializer(std::make_unique<JsonSerializer>());
    registerSerializer(std::make_unique<BinarySerializer>());
}

void SerializationManager::registerSerializer(
    std::unique_ptr<ISerializer> serializer) {
    serializers_.push_back(std::move(serializer));
}

void SerializationManager::registerSerializer(
    SerializationFormat /*format*/, std::unique_ptr<ISerializer> serializer) {
    // The format parameter is informational; the serializer reports its own
    // supported formats via supportsFormat()
    serializers_.push_back(std::move(serializer));
}

bool SerializationManager::hasSerializer(SerializationFormat format) const {
    for (const auto& serializer : serializers_) {
        if (serializer->supportsFormat(format)) {
            return true;
        }
    }
    return false;
}

SerializationResult SerializationManager::serialize(
    const Component& component, const SerializationOptions& options) {
    statistics_.totalSerializations++;

    ISerializer* serializer = getSerializer(options.format);
    if (!serializer) {
        SerializationResult result;
        result.success = false;
        result.errorMessage =
            "No serializer available for the specified format";
        return result;
    }

    SerializationResult result = serializer->serialize(component, options);

    if (result.success) {
        statistics_.successfulSerializations++;
        statistics_.totalBytesWritten += result.data.size();
    }

    statistics_.totalSerializationTime += result.serializationTime;
    statistics_.formatUsage[options.format]++;

    return result;
}

DeserializationResult SerializationManager::deserialize(
    const std::vector<uint8_t>& data, const SerializationOptions& options) {
    statistics_.totalDeserializations++;

    ISerializer* serializer = getSerializer(options.format);
    if (!serializer) {
        DeserializationResult result;
        result.success = false;
        result.errorMessage =
            "No serializer available for the specified format";
        return result;
    }

    DeserializationResult result = serializer->deserialize(data, options);

    if (result.success) {
        statistics_.successfulDeserializations++;
        statistics_.totalBytesRead += data.size();
    }

    statistics_.totalDeserializationTime += result.deserializationTime;

    return result;
}

ISerializer* SerializationManager::getSerializer(SerializationFormat format) {
    for (auto& serializer : serializers_) {
        if (serializer->supportsFormat(format)) {
            return serializer.get();
        }
    }
    return nullptr;
}

bool SerializationManager::serializeToFile(
    const Component& component, const std::string& filename,
    const SerializationOptions& options) {
    SerializationResult result = serialize(component, options);
    if (!result.success) {
        return false;
    }

    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(result.data.data()),
               result.data.size());
    return file.good();
}

DeserializationResult SerializationManager::deserializeFromFile(
    const std::string& filename, const SerializationOptions& options) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        DeserializationResult result;
        result.success = false;
        result.errorMessage = "Failed to open file: " + filename;
        return result;
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());

    return deserialize(data, options);
}

std::vector<SerializationFormat> SerializationManager::getSupportedFormats()
    const {
    std::vector<SerializationFormat> formats;

    for (const auto& serializer : serializers_) {
        if (serializer->supportsFormat(SerializationFormat::JSON)) {
            formats.push_back(SerializationFormat::JSON);
        }
        if (serializer->supportsFormat(SerializationFormat::Binary)) {
            formats.push_back(SerializationFormat::Binary);
        }
        // Add other formats as needed
    }

    // Remove duplicates
    std::sort(formats.begin(), formats.end());
    formats.erase(std::unique(formats.begin(), formats.end()), formats.end());

    return formats;
}

bool SerializationManager::validateData(const std::vector<uint8_t>& data,
                                        SerializationFormat format) const {
    if (data.empty())
        return false;

    switch (format) {
        case SerializationFormat::JSON: {
            try {
                std::string jsonString(data.begin(), data.end());
                auto parsed = nlohmann::json::parse(jsonString);
                (void)parsed;  // explicitly ignore parsed JSON
                return true;
            } catch (...) {
                return false;
            }
        }
        case SerializationFormat::Binary: {
            if (data.size() < sizeof(BinarySerializer::BinaryHeader)) {
                return false;
            }

            BinarySerializer::BinaryHeader header;
            std::memcpy(&header, data.data(), sizeof(header));
            return header.magic == 0x41544F4D;
        }
        default:
            return false;
    }
}

}  // namespace atom::components
