/*
 * serialization_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-25

Description: Serialization System Example
Demonstrates multi-format serialization, versioning, schema validation,
and comprehensive serialization features with the component system.

**************************************************/

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "atom/components/component.hpp"
#include "atom/components/registry.hpp"
#include "atom/components/serialization.hpp"

using namespace atom::components;

/**
 * @brief Component with rich data for serialization testing
 */
class SerializableComponent : public Component {
public:
    explicit SerializableComponent(const std::string& name) : Component(name) {
        std::cout << "SerializableComponent '" << name << "' created"
                  << std::endl;

        // Add various types of data for serialization
        addVariable<int>("player_id", 12345);
        addVariable<std::string>("player_name", "SerializationHero");
        addVariable<double>("experience_points", 1250.75);
        addVariable<bool>("is_premium", true);
        addVariable<std::vector<int>>("inventory_ids",
                                      {101, 102, 103, 201, 202});
        addVariable<std::vector<std::string>>(
            "achievements", {"first_kill", "level_10", "explorer"});

        // Nested data as JSON strings (for complex structures)
        addVariable<std::string>("player_stats", R"({
            "strength": 15,
            "agility": 12,
            "intelligence": 18,
            "vitality": 10
        })");

        addVariable<std::string>("game_settings", R"({
            "graphics_quality": "high",
            "sound_volume": 0.8,
            "music_volume": 0.6,
            "controls": {
                "move_forward": "W",
                "move_backward": "S",
                "move_left": "A",
                "move_right": "D"
            }
        })");

        // Metadata
        addVariable<std::string>("version", "1.2.3");
        addVariable<std::string>("created_date", "2024-12-25");
        addVariable<int>("save_count", 0);

        // Commands for data manipulation
        def("incrementSaveCount", [this]() -> int {
            auto count = getVariable<int>("save_count");
            if (count) {
                int newCount = count->get() + 1;
                setValue("save_count", newCount);
                return newCount;
            }
            return 0;
        });

        def("addAchievement", [this](const std::string& achievement) {
            auto achievements =
                getVariable<std::vector<std::string>>("achievements");
            if (achievements) {
                auto list = achievements->get();
                list.push_back(achievement);
                setValue("achievements", list);
                std::cout << "  [" << getName()
                          << "] Added achievement: " << achievement
                          << std::endl;
            }
        });

        def("addInventoryItem", [this](int itemId) {
            auto inventory = getVariable<std::vector<int>>("inventory_ids");
            if (inventory) {
                auto items = inventory->get();
                items.push_back(itemId);
                setValue("inventory_ids", items);
                std::cout << "  [" << getName()
                          << "] Added item to inventory: " << itemId
                          << std::endl;
            }
        });

        def("getPlayerInfo", [this]() -> std::string {
            auto name = getVariable<std::string>("player_name");
            auto id = getVariable<int>("player_id");
            auto exp = getVariable<double>("experience_points");
            auto premium = getVariable<bool>("is_premium");

            return "Player: " + (name ? name->get() : "Unknown") +
                   " (ID: " + std::to_string(id ? id->get() : 0) + ")" +
                   ", Experience: " + std::to_string(exp ? exp->get() : 0.0) +
                   ", Premium: " + (premium && premium->get() ? "Yes" : "No");
        });
    }
};

void demonstrateBasicSerialization() {
    std::cout << "\n=== Basic Serialization Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto& serializer = SerializationManager::instance();

    std::cout << "\n1. Creating component with data..." << std::endl;
    auto component =
        registry.createComponent<SerializableComponent>("PlayerData");

    // Add some dynamic data
    component->executeCommand("addAchievement", {"serialization_master"});
    component->executeCommand("addInventoryItem", {"999"});
    component->executeCommand("incrementSaveCount", {});

    std::cout << "Component data: "
              << component->executeCommand("getPlayerInfo", {}) << std::endl;

    std::cout << "\n2. Testing JSON serialization..." << std::endl;

    SerializationOptions jsonOptions;
    jsonOptions.format = SerializationFormat::JSON;
    jsonOptions.includeMetadata = true;
    jsonOptions.includeTimestamp = true;
    jsonOptions.includeVersion = true;
    jsonOptions.version = 1;

    auto jsonResult = serializer.serialize(*component, jsonOptions);
    if (jsonResult.success) {
        std::cout << "JSON serialization successful!" << std::endl;
        std::cout << "Original size: " << jsonResult.originalSize << " bytes"
                  << std::endl;
        std::cout << "Serialized size: " << jsonResult.data.size() << " bytes"
                  << std::endl;
        std::cout << "Serialization time: "
                  << jsonResult.serializationTime.count() << " μs" << std::endl;

        // Convert to string for display
        std::string jsonString(jsonResult.data.begin(), jsonResult.data.end());
        std::cout << "JSON data (first 200 chars): "
                  << jsonString.substr(0, 200) << "..." << std::endl;
    } else {
        std::cout << "JSON serialization failed: " << jsonResult.errorMessage
                  << std::endl;
    }

    std::cout << "\n3. Testing binary serialization..." << std::endl;

    SerializationOptions binaryOptions;
    binaryOptions.format = SerializationFormat::Binary;
    binaryOptions.includeMetadata = true;
    binaryOptions.compressData = true;
    binaryOptions.version = 1;

    auto binaryResult = serializer.serialize(*component, binaryOptions);
    if (binaryResult.success) {
        std::cout << "Binary serialization successful!" << std::endl;
        std::cout << "Original size: " << binaryResult.originalSize << " bytes"
                  << std::endl;
        std::cout << "Compressed size: " << binaryResult.compressedSize
                  << " bytes" << std::endl;
        std::cout << "Compression ratio: "
                  << (100.0 * binaryResult.compressedSize /
                      binaryResult.originalSize)
                  << "%" << std::endl;
        std::cout << "Serialization time: "
                  << binaryResult.serializationTime.count() << " μs"
                  << std::endl;
    } else {
        std::cout << "Binary serialization failed: "
                  << binaryResult.errorMessage << std::endl;
    }
}

void demonstrateFileSerialization() {
    std::cout << "\n=== File Serialization Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto& serializer = SerializationManager::instance();

    auto component = registry.getComponent("PlayerData");
    if (!component) {
        std::cout << "Component not found" << std::endl;
        return;
    }

    std::cout << "\n4. Serializing to files..." << std::endl;

    // Create output directory
    std::filesystem::create_directories("serialization_output");

    // JSON file
    SerializationOptions jsonOptions;
    jsonOptions.format = SerializationFormat::JSON;
    jsonOptions.includeMetadata = true;
    jsonOptions.includeTimestamp = true;

    bool jsonSuccess = serializer.serializeToFile(
        *component, "serialization_output/player_data.json", jsonOptions);
    if (jsonSuccess) {
        std::cout << "JSON file saved: serialization_output/player_data.json"
                  << std::endl;
    } else {
        std::cout << "Failed to save JSON file" << std::endl;
    }

    // Binary file
    SerializationOptions binaryOptions;
    binaryOptions.format = SerializationFormat::Binary;
    binaryOptions.compressData = true;
    binaryOptions.includeMetadata = true;

    bool binarySuccess = serializer.serializeToFile(
        *component, "serialization_output/player_data.bin", binaryOptions);
    if (binarySuccess) {
        std::cout << "Binary file saved: serialization_output/player_data.bin"
                  << std::endl;
    } else {
        std::cout << "Failed to save binary file" << std::endl;
    }

    // XML file (if supported)
    SerializationOptions xmlOptions;
    xmlOptions.format = SerializationFormat::XML;
    xmlOptions.includeMetadata = true;

    bool xmlSuccess = serializer.serializeToFile(
        *component, "serialization_output/player_data.xml", xmlOptions);
    if (xmlSuccess) {
        std::cout << "XML file saved: serialization_output/player_data.xml"
                  << std::endl;
    } else {
        std::cout << "XML serialization not supported or failed" << std::endl;
    }

    // List created files
    std::cout << "\nCreated files:" << std::endl;
    for (const auto& entry :
         std::filesystem::directory_iterator("serialization_output")) {
        auto size = std::filesystem::file_size(entry);
        std::cout << "  " << entry.path().filename().string() << " (" << size
                  << " bytes)" << std::endl;
    }
}

void demonstrateDeserialization() {
    std::cout << "\n=== Deserialization Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto& serializer = SerializationManager::instance();

    std::cout << "\n5. Deserializing from files..." << std::endl;

    // Test JSON deserialization
    std::cout << "\n--- JSON Deserialization ---" << std::endl;
    auto jsonResult =
        serializer.deserializeFromFile("serialization_output/player_data.json");
    if (jsonResult.success && jsonResult.component) {
        std::cout << "JSON deserialization successful!" << std::endl;
        std::cout << "Version: " << jsonResult.version << std::endl;
        std::cout << "Deserialization time: "
                  << jsonResult.deserializationTime.count() << " μs"
                  << std::endl;

        // Test the deserialized component
        auto info = jsonResult.component->executeCommand("getPlayerInfo", {});
        std::cout << "Deserialized component info: " << info << std::endl;

        // Register the deserialized component
        registry.addComponent("DeserializedFromJSON", jsonResult.component);
    } else {
        std::cout << "JSON deserialization failed: " << jsonResult.errorMessage
                  << std::endl;
    }

    // Test binary deserialization
    std::cout << "\n--- Binary Deserialization ---" << std::endl;
    auto binaryResult =
        serializer.deserializeFromFile("serialization_output/player_data.bin");
    if (binaryResult.success && binaryResult.component) {
        std::cout << "Binary deserialization successful!" << std::endl;
        std::cout << "Version: " << binaryResult.version << std::endl;
        std::cout << "Deserialization time: "
                  << binaryResult.deserializationTime.count() << " μs"
                  << std::endl;

        auto info = binaryResult.component->executeCommand("getPlayerInfo", {});
        std::cout << "Deserialized component info: " << info << std::endl;

        registry.addComponent("DeserializedFromBinary", binaryResult.component);
    } else {
        std::cout << "Binary deserialization failed: "
                  << binaryResult.errorMessage << std::endl;
    }
}

void demonstrateVersioning() {
    std::cout << "\n=== Versioning Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto& serializer = SerializationManager::instance();

    std::cout << "\n6. Testing version compatibility..." << std::endl;

    // Create a component with version 1 data
    auto componentV1 =
        registry.createComponent<SerializableComponent>("PlayerDataV1");

    SerializationOptions v1Options;
    v1Options.format = SerializationFormat::JSON;
    v1Options.includeVersion = true;
    v1Options.version = 1;

    bool v1Success = serializer.serializeToFile(
        *componentV1, "serialization_output/player_data_v1.json", v1Options);
    if (v1Success) {
        std::cout << "Version 1 data saved" << std::endl;
    }

    // Simulate version 2 with additional fields
    componentV1->addVariable<std::string>("new_feature", "version_2_feature");
    componentV1->addVariable<int>("skill_points", 25);

    SerializationOptions v2Options;
    v2Options.format = SerializationFormat::JSON;
    v2Options.includeVersion = true;
    v2Options.version = 2;

    bool v2Success = serializer.serializeToFile(
        *componentV1, "serialization_output/player_data_v2.json", v2Options);
    if (v2Success) {
        std::cout << "Version 2 data saved" << std::endl;
    }

    // Test loading different versions
    std::cout << "\n--- Loading Version 1 ---" << std::endl;
    auto v1Result = serializer.deserializeFromFile(
        "serialization_output/player_data_v1.json");
    if (v1Result.success) {
        std::cout << "Loaded version: " << v1Result.version << std::endl;
    }

    std::cout << "\n--- Loading Version 2 ---" << std::endl;
    auto v2Result = serializer.deserializeFromFile(
        "serialization_output/player_data_v2.json");
    if (v2Result.success) {
        std::cout << "Loaded version: " << v2Result.version << std::endl;
    }
}

void demonstrateCustomSerialization() {
    std::cout << "\n=== Custom Serialization Demo ===" << std::endl;

    auto& serializer = SerializationManager::instance();

    std::cout << "\n7. Testing custom serialization formats..." << std::endl;

    // Register custom serializer
    serializer.registerCustomSerializer(
        "CUSTOM", [](const Component& component) -> std::vector<uint8_t> {
            std::string customData = "CUSTOM_FORMAT|";
            customData += component.getName() + "|";
            customData += std::to_string(component.getVariableCount()) + "|";
            customData += "END";

            return std::vector<uint8_t>(customData.begin(), customData.end());
        });

    // Register custom deserializer
    serializer.registerCustomDeserializer(
        "CUSTOM",
        [](const std::vector<uint8_t>& data) -> std::shared_ptr<Component> {
            std::string customData(data.begin(), data.end());

            // Parse custom format
            if (customData.find("CUSTOM_FORMAT|") == 0) {
                size_t pos1 = customData.find('|', 14);
                size_t pos2 = customData.find('|', pos1 + 1);

                if (pos1 != std::string::npos && pos2 != std::string::npos) {
                    std::string name = customData.substr(14, pos1 - 14);
                    std::string countStr =
                        customData.substr(pos1 + 1, pos2 - pos1 - 1);

                    auto component = std::make_shared<SerializableComponent>(
                        name + "_Custom");
                    component->addVariable<std::string>("custom_loaded",
                                                        "true");
                    component->addVariable<int>("original_var_count",
                                                std::stoi(countStr));

                    return component;
                }
            }

            return nullptr;
        });

    // Test custom serialization
    auto& registry = Registry::instance();
    auto component = registry.getComponent("PlayerData");
    if (component) {
        SerializationOptions customOptions;
        customOptions.format = SerializationFormat::Custom;
        customOptions.customOptions["format_name"] = std::string("CUSTOM");

        auto customResult = serializer.serialize(*component, customOptions);
        if (customResult.success) {
            std::cout << "Custom serialization successful!" << std::endl;
            std::string customString(customResult.data.begin(),
                                     customResult.data.end());
            std::cout << "Custom data: " << customString << std::endl;

            // Test custom deserialization
            auto deserializedResult =
                serializer.deserialize(customResult.data, customOptions);
            if (deserializedResult.success && deserializedResult.component) {
                std::cout << "Custom deserialization successful!" << std::endl;
                std::cout << "Deserialized component: "
                          << deserializedResult.component->getName()
                          << std::endl;

                auto customLoaded =
                    deserializedResult.component->getVariable<std::string>(
                        "custom_loaded");
                if (customLoaded) {
                    std::cout << "Custom loaded flag: " << customLoaded->get()
                              << std::endl;
                }
            }
        } else {
            std::cout << "Custom serialization failed: "
                      << customResult.errorMessage << std::endl;
        }
    }
}

void demonstratePerformanceAnalysis() {
    std::cout << "\n=== Performance Analysis Demo ===" << std::endl;

    auto& registry = Registry::instance();
    auto& serializer = SerializationManager::instance();

    std::cout << "\n8. Analyzing serialization performance..." << std::endl;

    // Create multiple components for performance testing
    std::vector<std::shared_ptr<SerializableComponent>> components;
    for (int i = 0; i < 10; ++i) {
        auto comp = registry.createComponent<SerializableComponent>(
            "PerfTest_" + std::to_string(i));

        // Add some data to make serialization more realistic
        for (int j = 0; j < 10; ++j) {
            comp->executeCommand("addAchievement",
                                 {"achievement_" + std::to_string(j)});
            comp->executeCommand("addInventoryItem",
                                 {std::to_string(1000 + j)});
        }

        components.push_back(comp);
    }

    // Test different formats
    std::vector<SerializationFormat> formats = {SerializationFormat::JSON,
                                                SerializationFormat::Binary};

    for (auto format : formats) {
        std::cout << "\n--- Testing ";
        switch (format) {
            case SerializationFormat::JSON:
                std::cout << "JSON";
                break;
            case SerializationFormat::Binary:
                std::cout << "Binary";
                break;
            default:
                std::cout << "Unknown";
                break;
        }
        std::cout << " format ---" << std::endl;

        SerializationOptions options;
        options.format = format;
        options.includeMetadata = true;
        options.compressData = (format == SerializationFormat::Binary);

        auto start = std::chrono::high_resolution_clock::now();

        size_t totalSize = 0;
        size_t totalCompressedSize = 0;
        int successCount = 0;

        for (const auto& comp : components) {
            auto result = serializer.serialize(*comp, options);
            if (result.success) {
                successCount++;
                totalSize += result.originalSize;
                totalCompressedSize += result.data.size();
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "  Components serialized: " << successCount << "/"
                  << components.size() << std::endl;
        std::cout << "  Total original size: " << totalSize << " bytes"
                  << std::endl;
        std::cout << "  Total serialized size: " << totalCompressedSize
                  << " bytes" << std::endl;
        if (totalSize > 0) {
            std::cout << "  Compression ratio: "
                      << (100.0 * totalCompressedSize / totalSize) << "%"
                      << std::endl;
        }
        std::cout << "  Total time: " << duration.count() << " μs" << std::endl;
        std::cout << "  Average per component: "
                  << (duration.count() / components.size()) << " μs"
                  << std::endl;
    }
}

void cleanup() {
    std::cout << "\n=== Cleanup ===" << std::endl;

    // Remove test files
    try {
        if (std::filesystem::exists("serialization_output")) {
            std::filesystem::remove_all("serialization_output");
            std::cout << "Cleaned up serialization_output directory"
                      << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "Cleanup error: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "=== Atom Component Serialization Examples ===" << std::endl;

    try {
        demonstrateBasicSerialization();
        demonstrateFileSerialization();
        demonstrateDeserialization();
        demonstrateVersioning();
        demonstrateCustomSerialization();
        demonstratePerformanceAnalysis();

        cleanup();

        std::cout
            << "\n=== All Serialization Examples Completed Successfully! ==="
            << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error in serialization examples: " << e.what()
                  << std::endl;
        cleanup();
        return 1;
    }

    return 0;
}
