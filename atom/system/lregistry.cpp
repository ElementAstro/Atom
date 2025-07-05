/*
 * lregistry.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-17

Description: A self-contained registry manager.

**************************************************/

#include "lregistry.hpp"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <regex>
#include <sstream>

#include <spdlog/spdlog.h>

// Optional JSON library inclusion
#ifdef HAVE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif

// Optional XML library inclusion
#ifdef HAVE_TINYXML2
#include <tinyxml2.h>
#endif

// Optional cryptography library inclusion
#ifdef HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

namespace atom::system {

// Helper for path handling
class PathHelper {
public:
    static std::vector<std::string> splitPath(const std::string& path) {
        std::vector<std::string> components;
        std::istringstream stream(path);
        std::string component;

        while (std::getline(stream, component, '/')) {
            if (!component.empty()) {
                components.push_back(component);
            }
        }

        return components;
    }

    static std::string joinPath(const std::vector<std::string>& components) {
        std::string result;
        for (const auto& component : components) {
            result += "/" + component;
        }
        return result.empty() ? "/" : result;
    }
};

// Registry value with metadata
struct RegistryValue {
    std::string data;
    std::string type;
    std::time_t lastModified;

    RegistryValue() : lastModified(std::time(nullptr)) {}

    RegistryValue(const std::string& d, const std::string& t = "string")
        : data(d), type(t), lastModified(std::time(nullptr)) {}
};

// Registry node for hierarchical structure
struct RegistryNode {
    std::map<std::string, RegistryValue> values;
    std::map<std::string, RegistryNode> children;
    std::time_t created;
    std::time_t lastModified;

    RegistryNode()
        : created(std::time(nullptr)), lastModified(std::time(nullptr)) {}
};

// Transaction data for atomic operations
struct TransactionData {
    RegistryNode originalData;
    bool active = false;
};

class Registry::RegistryImpl {
public:
    RegistryNode rootNode;
    std::string registryFilePath = "registry_data.txt";
    RegistryFormat defaultFormat = RegistryFormat::TEXT;
    bool encryptionEnabled = false;
    std::string encryptionKey;
    bool autoSaveEnabled = true;
    TransactionData transaction;
    std::map<size_t, EventCallback> eventCallbacks;
    size_t nextCallbackId = 1;
    mutable std::recursive_mutex mutex;
    std::string lastError;

    RegistryResult saveRegistryToFile(
        const std::string& filePath = "",
        RegistryFormat format = RegistryFormat::TEXT);

    RegistryResult loadRegistryFromFile(const std::string& filePath,
                                        RegistryFormat format);

    RegistryNode* getNode(const std::string& path,
                          bool createIfMissing = false);
    const RegistryNode* getNode(const std::string& path) const;

    void collectKeyPaths(const RegistryNode& node,
                         const std::string& currentPath,
                         std::vector<std::string>& result) const;

    void notifyEvent(const std::string& eventType, const std::string& keyPath);

    std::string nodeToText(const RegistryNode& node,
                           const std::string& path = "");
    RegistryResult textToNode(const std::string& text, RegistryNode& node);

#ifdef HAVE_NLOHMANN_JSON
    nlohmann::json nodeToJson(const RegistryNode& node);
    RegistryResult jsonToNode(const nlohmann::json& json, RegistryNode& node);
#endif

#ifdef HAVE_TINYXML2
    tinyxml2::XMLElement* nodeToXml(const RegistryNode& node,
                                    tinyxml2::XMLDocument& doc,
                                    const std::string& name);
    RegistryResult xmlToNode(tinyxml2::XMLElement* element, RegistryNode& node);
#endif

#ifdef HAVE_OPENSSL
    std::string encrypt(const std::string& data);
    std::string decrypt(const std::string& encryptedData);
#endif

    bool matchesPattern(const std::string& text,
                        const std::string& pattern) const;
};

Registry::Registry() : pImpl_(std::make_unique<RegistryImpl>()) {
    spdlog::info("Registry constructor called");
}

Registry::~Registry() {
    spdlog::info("Registry destructor called");

    if (pImpl_->autoSaveEnabled) {
        pImpl_->saveRegistryToFile();
    }
}

RegistryResult Registry::initialize(const std::string& filePath,
                                    bool useEncryption) {
    spdlog::info("Initializing registry with file: {}, encryption: {}",
                 filePath, useEncryption);

    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);

    if (!filePath.empty()) {
        pImpl_->registryFilePath = filePath;
    }

    pImpl_->encryptionEnabled = useEncryption;

#ifdef HAVE_OPENSSL
    if (useEncryption) {
        unsigned char key[32];
        if (RAND_bytes(key, sizeof(key)) != 1) {
            pImpl_->lastError = "Failed to generate encryption key";
            spdlog::error("Failed to generate encryption key");
            return RegistryResult::ENCRYPTION_ERROR;
        }

        std::stringstream ss;
        for (int i = 0; i < 32; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<int>(key[i]);
        }
        pImpl_->encryptionKey = ss.str();
    }
#else
    if (useEncryption) {
        spdlog::warn(
            "Encryption requested but OpenSSL support not compiled in");
        pImpl_->lastError =
            "Encryption requested but not supported in this build";
        return RegistryResult::ENCRYPTION_ERROR;
    }
#endif

    spdlog::info("Registry initialized successfully");
    return RegistryResult::SUCCESS;
}

RegistryResult Registry::loadRegistryFromFile(const std::string& filePath,
                                              RegistryFormat format) {
    spdlog::info("Loading registry from file: {}",
                 filePath.empty() ? pImpl_->registryFilePath : filePath);

    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);

    std::string actualFilePath =
        filePath.empty() ? pImpl_->registryFilePath : filePath;
    return pImpl_->loadRegistryFromFile(actualFilePath, format);
}

RegistryResult Registry::createKey(const std::string& keyPath) {
    spdlog::info("Creating key: {}", keyPath);

    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);

    if (keyExists(keyPath)) {
        spdlog::warn("Key already exists: {}", keyPath);
        pImpl_->lastError = "Key already exists: " + keyPath;
        return RegistryResult::ALREADY_EXISTS;
    }

    RegistryNode* node = pImpl_->getNode(keyPath, true);
    if (!node) {
        spdlog::error("Failed to create key: {}", keyPath);
        pImpl_->lastError = "Failed to create key: " + keyPath;
        return RegistryResult::UNKNOWN_ERROR;
    }

    node->lastModified = std::time(nullptr);

    if (pImpl_->autoSaveEnabled) {
        pImpl_->saveRegistryToFile();
    }

    pImpl_->notifyEvent("KeyCreated", keyPath);
    spdlog::debug("Key created successfully: {}", keyPath);
    return RegistryResult::SUCCESS;
}

RegistryResult Registry::deleteKey(const std::string& keyPath) {
    spdlog::info("Deleting key: {}", keyPath);

    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);

    if (keyPath == "/" || keyPath.empty()) {
        spdlog::warn("Cannot delete root key");
        pImpl_->lastError = "Cannot delete root key";
        return RegistryResult::PERMISSION_DENIED;
    }

    auto components = PathHelper::splitPath(keyPath);
    if (components.empty()) {
        spdlog::error("Invalid key path: {}", keyPath);
        pImpl_->lastError = "Invalid key path: " + keyPath;
        return RegistryResult::KEY_NOT_FOUND;
    }

    std::string keyName = components.back();
    components.pop_back();
    std::string parentPath = PathHelper::joinPath(components);

    RegistryNode* parentNode = pImpl_->getNode(parentPath);
    if (!parentNode) {
        spdlog::warn("Parent key not found: {}", parentPath);
        pImpl_->lastError = "Parent key not found: " + parentPath;
        return RegistryResult::KEY_NOT_FOUND;
    }

    auto it = parentNode->children.find(keyName);
    if (it == parentNode->children.end()) {
        spdlog::warn("Key not found: {}", keyPath);
        pImpl_->lastError = "Key not found: " + keyPath;
        return RegistryResult::KEY_NOT_FOUND;
    }

    parentNode->children.erase(it);
    parentNode->lastModified = std::time(nullptr);

    if (pImpl_->autoSaveEnabled) {
        pImpl_->saveRegistryToFile();
    }

    pImpl_->notifyEvent("KeyDeleted", keyPath);
    spdlog::debug("Key deleted successfully: {}", keyPath);
    return RegistryResult::SUCCESS;
}

RegistryResult Registry::setValue(const std::string& keyPath,
                                  const std::string& valueName,
                                  const std::string& data) {
    spdlog::debug("Setting value: {} in key: {}", valueName, keyPath);
    return setTypedValue(keyPath, valueName, data, "string");
}

RegistryResult Registry::setTypedValue(const std::string& keyPath,
                                       const std::string& valueName,
                                       const std::string& data,
                                       const std::string& type) {
    spdlog::debug("Setting typed value: {} of type {} in key: {}", valueName,
                  type, keyPath);

    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);

    RegistryNode* node = pImpl_->getNode(keyPath, true);
    if (!node) {
        spdlog::error("Failed to access key: {}", keyPath);
        pImpl_->lastError = "Failed to access key: " + keyPath;
        return RegistryResult::KEY_NOT_FOUND;
    }

    node->values[valueName] = RegistryValue(data, type);
    node->lastModified = std::time(nullptr);

    if (pImpl_->autoSaveEnabled) {
        pImpl_->saveRegistryToFile();
    }

    pImpl_->notifyEvent("ValueSet", keyPath + "/" + valueName);
    return RegistryResult::SUCCESS;
}

std::optional<std::string> Registry::getValue(const std::string& keyPath,
                                              const std::string& valueName) {
    spdlog::debug("Getting value: {} from key: {}", valueName, keyPath);

    std::string type;
    return getTypedValue(keyPath, valueName, type);
}

std::optional<std::string> Registry::getTypedValue(const std::string& keyPath,
                                                   const std::string& valueName,
                                                   std::string& type) {
    spdlog::debug("Getting typed value: {} from key: {}", valueName, keyPath);

    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);

    const RegistryNode* node = pImpl_->getNode(keyPath);
    if (!node) {
        spdlog::warn("Key not found: {}", keyPath);
        pImpl_->lastError = "Key not found: " + keyPath;
        return std::nullopt;
    }

    auto valueIt = node->values.find(valueName);
    if (valueIt == node->values.end()) {
        spdlog::warn("Value not found: {} in key: {}", valueName, keyPath);
        pImpl_->lastError = "Value not found: " + valueName;
        return std::nullopt;
    }

    type = valueIt->second.type;
    return valueIt->second.data;
}

RegistryResult Registry::deleteValue(const std::string& keyPath,
                                     const std::string& valueName) {
    spdlog::info("Deleting value: {} from key: {}", valueName, keyPath);

    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);

    RegistryNode* node = pImpl_->getNode(keyPath);
    if (!node) {
        spdlog::warn("Key not found: {}", keyPath);
        pImpl_->lastError = "Key not found: " + keyPath;
        return RegistryResult::KEY_NOT_FOUND;
    }

    auto valueIt = node->values.find(valueName);
    if (valueIt == node->values.end()) {
        spdlog::warn("Value not found: {} in key: {}", valueName, keyPath);
        pImpl_->lastError = "Value not found: " + valueName;
        return RegistryResult::VALUE_NOT_FOUND;
    }

    node->values.erase(valueIt);
    node->lastModified = std::time(nullptr);

    if (pImpl_->autoSaveEnabled) {
        pImpl_->saveRegistryToFile();
    }

    pImpl_->notifyEvent("ValueDeleted", keyPath + "/" + valueName);
    spdlog::debug("Value deleted successfully: {} from key: {}", valueName,
                  keyPath);
    return RegistryResult::SUCCESS;
}

RegistryResult Registry::backupRegistryData(const std::string& backupPath) {
    spdlog::info("Backing up registry data to: {}",
                 backupPath.empty() ? "auto-generated path" : backupPath);

    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);

    std::string actualBackupPath = backupPath;
    if (actualBackupPath.empty()) {
        std::time_t currentTime = std::time(nullptr);
        std::stringstream ss;
        ss << "registry_backup_" << currentTime << ".dat";
        actualBackupPath = ss.str();
    }

    RegistryResult result =
        pImpl_->saveRegistryToFile(actualBackupPath, pImpl_->defaultFormat);

    if (result == RegistryResult::SUCCESS) {
        spdlog::info("Registry data backed up successfully to: {}",
                     actualBackupPath);
    } else {
        spdlog::error("Failed to backup registry data: {}", pImpl_->lastError);
    }

    return result;
}

RegistryResult Registry::restoreRegistryData(const std::string& backupFile) {
    spdlog::info("Restoring registry data from: {}", backupFile);

    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);

    if (!std::filesystem::exists(backupFile)) {
        spdlog::error("Backup file does not exist: {}", backupFile);
        pImpl_->lastError = "Backup file does not exist: " + backupFile;
        return RegistryResult::FILE_ERROR;
    }

    RegistryFormat format = pImpl_->defaultFormat;
    std::string extension =
        std::filesystem::path(backupFile).extension().string();
    if (extension == ".json")
        format = RegistryFormat::JSON;
    else if (extension == ".xml")
        format = RegistryFormat::XML;
    else if (extension == ".bin")
        format = RegistryFormat::BINARY;

    RegistryResult result = pImpl_->loadRegistryFromFile(backupFile, format);

    if (result == RegistryResult::SUCCESS) {
        spdlog::info("Registry data restored successfully from: {}",
                     backupFile);

        if (pImpl_->autoSaveEnabled) {
            pImpl_->saveRegistryToFile();
        }

        pImpl_->notifyEvent("RegistryRestored", backupFile);
    } else {
        spdlog::error("Failed to restore registry data: {}", pImpl_->lastError);
    }

    return result;
}

bool Registry::keyExists(const std::string& keyPath) const {
    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);
    return pImpl_->getNode(keyPath) != nullptr;
}

bool Registry::valueExists(const std::string& keyPath,
                           const std::string& valueName) const {
    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);

    const RegistryNode* node = pImpl_->getNode(keyPath);
    return node && (node->values.find(valueName) != node->values.end());
}

std::vector<std::string> Registry::getValueNames(
    const std::string& keyPath) const {
    spdlog::debug("Getting value names for key: {}", keyPath);

    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);

    std::vector<std::string> valueNames;
    const RegistryNode* node = pImpl_->getNode(keyPath);
    if (node) {
        for (const auto& pair : node->values) {
            valueNames.push_back(pair.first);
        }
    }

    return valueNames;
}

std::vector<std::string> Registry::getAllKeys() const {
    spdlog::debug("Getting all registry keys");

    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);

    std::vector<std::string> keyPaths;
    pImpl_->collectKeyPaths(pImpl_->rootNode, "", keyPaths);
    return keyPaths;
}

std::optional<RegistryValueInfo> Registry::getValueInfo(
    const std::string& keyPath, const std::string& valueName) const {
    spdlog::debug("Getting value info for: {} in key: {}", valueName, keyPath);

    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);

    const RegistryNode* node = pImpl_->getNode(keyPath);
    if (!node) {
        spdlog::warn("Key not found: {}", keyPath);
        pImpl_->lastError = "Key not found: " + keyPath;
        return std::nullopt;
    }

    auto valueIt = node->values.find(valueName);
    if (valueIt == node->values.end()) {
        spdlog::warn("Value not found: {} in key: {}", valueName, keyPath);
        pImpl_->lastError = "Value not found: " + valueName;
        return std::nullopt;
    }

    RegistryValueInfo info;
    info.name = valueName;
    info.type = valueIt->second.type;
    info.lastModified = valueIt->second.lastModified;
    info.size = valueIt->second.data.size();

    return info;
}

size_t Registry::registerEventCallback(EventCallback callback) {
    spdlog::debug("Registering event callback");

    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);

    size_t callbackId = pImpl_->nextCallbackId++;
    pImpl_->eventCallbacks[callbackId] = callback;

    spdlog::debug("Event callback registered with ID: {}", callbackId);
    return callbackId;
}

bool Registry::unregisterEventCallback(size_t callbackId) {
    spdlog::debug("Unregistering event callback ID: {}", callbackId);

    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);

    auto it = pImpl_->eventCallbacks.find(callbackId);
    if (it != pImpl_->eventCallbacks.end()) {
        pImpl_->eventCallbacks.erase(it);
        spdlog::debug("Event callback unregistered: {}", callbackId);
        return true;
    }

    spdlog::warn("Event callback not found: {}", callbackId);
    return false;
}

bool Registry::beginTransaction() {
    spdlog::info("Beginning transaction");

    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);

    if (pImpl_->transaction.active) {
        spdlog::warn("Transaction already active");
        pImpl_->lastError = "Transaction already active";
        return false;
    }

    pImpl_->transaction.originalData = pImpl_->rootNode;
    pImpl_->transaction.active = true;

    spdlog::debug("Transaction begun successfully");
    return true;
}

RegistryResult Registry::commitTransaction() {
    spdlog::info("Committing transaction");

    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);

    if (!pImpl_->transaction.active) {
        spdlog::warn("No active transaction to commit");
        pImpl_->lastError = "No active transaction to commit";
        return RegistryResult::UNKNOWN_ERROR;
    }

    pImpl_->transaction.active = false;

    if (pImpl_->autoSaveEnabled) {
        pImpl_->saveRegistryToFile();
    }

    pImpl_->notifyEvent("TransactionCommitted", "");
    spdlog::debug("Transaction committed successfully");
    return RegistryResult::SUCCESS;
}

RegistryResult Registry::rollbackTransaction() {
    spdlog::info("Rolling back transaction");

    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);

    if (!pImpl_->transaction.active) {
        spdlog::warn("No active transaction to rollback");
        pImpl_->lastError = "No active transaction to rollback";
        return RegistryResult::UNKNOWN_ERROR;
    }

    pImpl_->rootNode = pImpl_->transaction.originalData;
    pImpl_->transaction.active = false;

    pImpl_->notifyEvent("TransactionRolledBack", "");
    spdlog::debug("Transaction rolled back successfully");
    return RegistryResult::SUCCESS;
}

RegistryResult Registry::exportRegistry(const std::string& filePath,
                                        RegistryFormat format) {
    spdlog::info("Exporting registry to: {}", filePath);

    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);
    return pImpl_->saveRegistryToFile(filePath, format);
}

RegistryResult Registry::importRegistry(const std::string& filePath,
                                        RegistryFormat format,
                                        bool mergeExisting) {
    spdlog::info("Importing registry from: {}, merge: {}", filePath,
                 mergeExisting);

    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);

    if (!mergeExisting) {
        pImpl_->rootNode = RegistryNode();
    }

    RegistryResult result = pImpl_->loadRegistryFromFile(filePath, format);

    if (result == RegistryResult::SUCCESS) {
        if (pImpl_->autoSaveEnabled) {
            pImpl_->saveRegistryToFile();
        }

        pImpl_->notifyEvent("RegistryImported", filePath);
        spdlog::info("Registry imported successfully from: {}", filePath);
    }

    return result;
}

std::vector<std::string> Registry::searchKeys(
    const std::string& pattern) const {
    spdlog::debug("Searching keys with pattern: {}", pattern);

    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);

    std::vector<std::string> allKeys;
    pImpl_->collectKeyPaths(pImpl_->rootNode, "", allKeys);

    std::vector<std::string> matchingKeys;
    for (const auto& key : allKeys) {
        if (pImpl_->matchesPattern(key, pattern)) {
            matchingKeys.push_back(key);
        }
    }

    spdlog::debug("Found {} matching keys", matchingKeys.size());
    return matchingKeys;
}

std::vector<std::pair<std::string, std::string>> Registry::searchValues(
    const std::string& valuePattern) const {
    spdlog::debug("Searching values with pattern: {}", valuePattern);

    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);

    std::vector<std::pair<std::string, std::string>> results;
    std::vector<std::string> allKeys;
    pImpl_->collectKeyPaths(pImpl_->rootNode, "", allKeys);

    for (const auto& keyPath : allKeys) {
        const RegistryNode* node = pImpl_->getNode(keyPath);
        if (!node)
            continue;

        for (const auto& value : node->values) {
            if (pImpl_->matchesPattern(value.second.data, valuePattern)) {
                results.emplace_back(keyPath + "/" + value.first,
                                     value.second.data);
            }
        }
    }

    spdlog::debug("Found {} matching values", results.size());
    return results;
}

void Registry::setAutoSave(bool enable) {
    spdlog::debug("Setting auto-save to: {}", enable);

    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);
    pImpl_->autoSaveEnabled = enable;

    if (enable && pImpl_->transaction.active) {
        spdlog::warn("Auto-save enabled while transaction is active");
    }
}

std::string Registry::getLastError() const {
    std::lock_guard<std::recursive_mutex> lock(pImpl_->mutex);
    return pImpl_->lastError;
}

// Implementation of RegistryImpl methods

RegistryResult Registry::RegistryImpl::saveRegistryToFile(
    const std::string& filePath, RegistryFormat format) {
    std::string actualFilePath = filePath.empty() ? registryFilePath : filePath;
    spdlog::debug("Saving registry to file: {}", actualFilePath);

    try {
        std::ofstream file(actualFilePath, std::ios::binary);
        if (!file.is_open()) {
            lastError = "Unable to open file for writing: " + actualFilePath;
            spdlog::error("Failed to open file for writing: {}",
                          actualFilePath);
            return RegistryResult::FILE_ERROR;
        }

        switch (format) {
            case RegistryFormat::TEXT: {
                std::string content = nodeToText(rootNode);

#ifdef HAVE_OPENSSL
                if (encryptionEnabled) {
                    content = encrypt(content);
                }
#endif

                file << content;
                break;
            }

#ifdef HAVE_NLOHMANN_JSON
            case RegistryFormat::JSON: {
                nlohmann::json jsonData = nodeToJson(rootNode);
                std::string content = jsonData.dump(4);

#ifdef HAVE_OPENSSL
                if (encryptionEnabled) {
                    content = encrypt(content);
                }
#endif

                file << content;
                break;
            }
#endif

#ifdef HAVE_TINYXML2
            case RegistryFormat::XML: {
                tinyxml2::XMLDocument doc;
                tinyxml2::XMLElement* rootElement =
                    nodeToXml(rootNode, doc, "Registry");
                doc.InsertFirstChild(rootElement);

                tinyxml2::XMLPrinter printer;
                doc.Print(&printer);
                std::string content = printer.CStr();

#ifdef HAVE_OPENSSL
                if (encryptionEnabled) {
                    content = encrypt(content);
                }
#endif

                file << content;
                break;
            }
#endif

            case RegistryFormat::BINARY:
                lastError = "Binary format not fully implemented";
                spdlog::error("Binary format not implemented");
                return RegistryResult::INVALID_FORMAT;

            default:
                lastError = "Unsupported registry format";
                spdlog::error("Unsupported registry format");
                return RegistryResult::INVALID_FORMAT;
        }

        file.close();
        spdlog::debug("Registry saved successfully to: {}", actualFilePath);
        return RegistryResult::SUCCESS;
    } catch (const std::exception& e) {
        lastError = "Exception while saving registry: " + std::string(e.what());
        spdlog::error("Exception while saving registry: {}", e.what());
        return RegistryResult::UNKNOWN_ERROR;
    }
}

RegistryResult Registry::RegistryImpl::loadRegistryFromFile(
    const std::string& filePath, RegistryFormat format) {
    spdlog::debug("Loading registry from file: {}", filePath);

    try {
        if (!std::filesystem::exists(filePath)) {
            lastError = "File does not exist: " + filePath;
            spdlog::warn("File does not exist: {}", filePath);
            return RegistryResult::FILE_ERROR;
        }

        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            lastError = "Unable to open file for reading: " + filePath;
            spdlog::error("Failed to open file for reading: {}", filePath);
            return RegistryResult::FILE_ERROR;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        file.close();

#ifdef HAVE_OPENSSL
        if (encryptionEnabled) {
            try {
                content = decrypt(content);
            } catch (const std::exception& e) {
                lastError = "Decryption error: " + std::string(e.what());
                spdlog::error("Decryption error: {}", e.what());
                return RegistryResult::ENCRYPTION_ERROR;
            }
        }
#endif

        switch (format) {
            case RegistryFormat::TEXT: {
                RegistryNode tempNode;
                RegistryResult result = textToNode(content, tempNode);
                if (result == RegistryResult::SUCCESS) {
                    rootNode = std::move(tempNode);
                }
                return result;
            }

#ifdef HAVE_NLOHMANN_JSON
            case RegistryFormat::JSON: {
                try {
                    nlohmann::json jsonData = nlohmann::json::parse(content);
                    RegistryNode tempNode;
                    RegistryResult result = jsonToNode(jsonData, tempNode);
                    if (result == RegistryResult::SUCCESS) {
                        rootNode = std::move(tempNode);
                    }
                    return result;
                } catch (const nlohmann::json::exception& e) {
                    lastError = "JSON parsing error: " + std::string(e.what());
                    spdlog::error("JSON parsing error: {}", e.what());
                    return RegistryResult::INVALID_FORMAT;
                }
            }
#endif

#ifdef HAVE_TINYXML2
            case RegistryFormat::XML: {
                tinyxml2::XMLDocument doc;
                if (doc.Parse(content.c_str()) != tinyxml2::XML_SUCCESS) {
                    lastError =
                        "XML parsing error: " + std::string(doc.ErrorStr());
                    spdlog::error("XML parsing error: {}", doc.ErrorStr());
                    return RegistryResult::INVALID_FORMAT;
                }

                tinyxml2::XMLElement* rootElement =
                    doc.FirstChildElement("Registry");
                if (!rootElement) {
                    lastError =
                        "Invalid XML structure: missing Registry root element";
                    spdlog::error(
                        "Invalid XML structure: missing Registry root element");
                    return RegistryResult::INVALID_FORMAT;
                }

                RegistryNode tempNode;
                RegistryResult result = xmlToNode(rootElement, tempNode);
                if (result == RegistryResult::SUCCESS) {
                    rootNode = std::move(tempNode);
                }
                return result;
            }
#endif

            case RegistryFormat::BINARY:
                lastError = "Binary format not fully implemented";
                spdlog::error("Binary format not implemented");
                return RegistryResult::INVALID_FORMAT;

            default:
                lastError = "Unsupported registry format";
                spdlog::error("Unsupported registry format");
                return RegistryResult::INVALID_FORMAT;
        }
    } catch (const std::exception& e) {
        lastError =
            "Exception while loading registry: " + std::string(e.what());
        spdlog::error("Exception while loading registry: {}", e.what());
        return RegistryResult::UNKNOWN_ERROR;
    }
}

RegistryNode* Registry::RegistryImpl::getNode(const std::string& path,
                                              bool createIfMissing) {
    if (path.empty() || path == "/") {
        return &rootNode;
    }

    std::vector<std::string> components = PathHelper::splitPath(path);
    RegistryNode* currentNode = &rootNode;

    for (const auto& component : components) {
        auto it = currentNode->children.find(component);

        if (it == currentNode->children.end()) {
            if (createIfMissing) {
                currentNode->children[component] = RegistryNode();
                currentNode = &currentNode->children[component];
            } else {
                return nullptr;
            }
        } else {
            currentNode = &it->second;
        }
    }

    return currentNode;
}

const RegistryNode* Registry::RegistryImpl::getNode(
    const std::string& path) const {
    if (path.empty() || path == "/") {
        return &rootNode;
    }

    std::vector<std::string> components = PathHelper::splitPath(path);
    const RegistryNode* currentNode = &rootNode;

    for (const auto& component : components) {
        auto it = currentNode->children.find(component);

        if (it == currentNode->children.end()) {
            return nullptr;
        } else {
            currentNode = &it->second;
        }
    }

    return currentNode;
}

void Registry::RegistryImpl::collectKeyPaths(
    const RegistryNode& node, const std::string& currentPath,
    std::vector<std::string>& result) const {
    if (!currentPath.empty()) {
        result.push_back(currentPath);
    }

    for (const auto& child : node.children) {
        std::string childPath = currentPath.empty()
                                    ? "/" + child.first
                                    : currentPath + "/" + child.first;
        collectKeyPaths(child.second, childPath, result);
    }
}

void Registry::RegistryImpl::notifyEvent(const std::string& eventType,
                                         const std::string& keyPath) {
    spdlog::debug("Event triggered: {} for key: {}", eventType, keyPath);

    for (const auto& callback : eventCallbacks) {
        try {
            callback.second(eventType, keyPath);
        } catch (const std::exception& e) {
            spdlog::error("Exception in event callback: {}", e.what());
        }
    }
}

std::string Registry::RegistryImpl::nodeToText(const RegistryNode& node,
                                               const std::string& path) {
    std::stringstream ss;

    // Write values for this node
    if (!path.empty()) {
        ss << "[" << path << "]" << std::endl;

        for (const auto& value : node.values) {
            ss << value.first << "=" << value.second.type << ":"
               << value.second.data << std::endl;
        }

        ss << std::endl;
    }

    // Recursively write child nodes
    for (const auto& child : node.children) {
        std::string childPath =
            path.empty() ? child.first : path + "/" + child.first;
        ss << nodeToText(child.second, childPath);
    }

    return ss.str();
}

RegistryResult Registry::RegistryImpl::textToNode(const std::string& text,
                                                  RegistryNode& node) {
    std::istringstream ss(text);
    std::string line;
    std::string currentPath;
    RegistryNode* currentNode = &node;

    while (std::getline(ss, line)) {
        // Skip empty lines
        if (line.empty())
            continue;

        // Check if this is a section header
        if (line[0] == '[' && line[line.length() - 1] == ']') {
            // Extract the path
            currentPath = line.substr(1, line.length() - 2);

            // Navigate to or create the node
            currentNode = getNode(currentPath, true);
            if (!currentNode) {
                lastError = "Failed to create node for path: " + currentPath;
                spdlog::error("Failed to create node for path: {}",
                              currentPath);
                return RegistryResult::UNKNOWN_ERROR;
            }
        }
        // Check if this is a value assignment
        else if (line.find('=') != std::string::npos) {
            size_t equalsPos = line.find('=');
            std::string valueName = line.substr(0, equalsPos);
            std::string valueData = line.substr(equalsPos + 1);

            // Check if there's a type specifier
            std::string valueType = "string";
            size_t colonPos = valueData.find(':');
            if (colonPos != std::string::npos) {
                valueType = valueData.substr(0, colonPos);
                valueData = valueData.substr(colonPos + 1);
            }

            // Set the value
            currentNode->values[valueName] =
                RegistryValue(valueData, valueType);
        }
    }

    return RegistryResult::SUCCESS;
}

#ifdef HAVE_NLOHMANN_JSON
nlohmann::json Registry::RegistryImpl::nodeToJson(const RegistryNode& node) {
    nlohmann::json result = nlohmann::json::object();

    // Add metadata
    result["created"] = node.created;
    result["lastModified"] = node.lastModified;

    // Add values
    nlohmann::json values = nlohmann::json::object();
    for (const auto& value : node.values) {
        nlohmann::json valueObj = nlohmann::json::object();
        valueObj["data"] = value.second.data;
        valueObj["type"] = value.second.type;
        valueObj["lastModified"] = value.second.lastModified;

        values[value.first] = valueObj;
    }
    result["values"] = values;

    // Add children
    nlohmann::json children = nlohmann::json::object();
    for (const auto& child : node.children) {
        children[child.first] = nodeToJson(child.second);
    }
    result["children"] = children;

    return result;
}

RegistryResult Registry::RegistryImpl::jsonToNode(const nlohmann::json& json,
                                                  RegistryNode& node) {
    try {
        // Load metadata
        if (json.contains("created") && json["created"].is_number_unsigned()) {
            node.created = json["created"].get<std::time_t>();
        }

        if (json.contains("lastModified") &&
            json["lastModified"].is_number_unsigned()) {
            node.lastModified = json["lastModified"].get<std::time_t>();
        }

        // Load values
        if (json.contains("values") && json["values"].is_object()) {
            for (auto it = json["values"].begin(); it != json["values"].end();
                 ++it) {
                const std::string& valueName = it.key();
                const nlohmann::json& valueObj = it.value();

                RegistryValue value;

                if (valueObj.contains("data") && valueObj["data"].is_string()) {
                    value.data = valueObj["data"].get<std::string>();
                }

                if (valueObj.contains("type") && valueObj["type"].is_string()) {
                    value.type = valueObj["type"].get<std::string>();
                }

                if (valueObj.contains("lastModified") &&
                    valueObj["lastModified"].is_number_unsigned()) {
                    value.lastModified =
                        valueObj["lastModified"].get<std::time_t>();
                }

                node.values[valueName] = value;
            }
        }

        // Load children
        if (json.contains("children") && json["children"].is_object()) {
            for (auto it = json["children"].begin();
                 it != json["children"].end(); ++it) {
                const std::string& childName = it.key();
                const nlohmann::json& childJson = it.value();

                RegistryNode childNode;
                RegistryResult result = jsonToNode(childJson, childNode);

                if (result != RegistryResult::SUCCESS) {
                    return result;
                }

                node.children[childName] = std::move(childNode);
            }
        }

        return RegistryResult::SUCCESS;
    } catch (const nlohmann::json::exception& e) {
        lastError = "JSON parsing error: " + std::string(e.what());
        spdlog::error("JSON parsing error: {}", e.what());
        return RegistryResult::INVALID_FORMAT;
    }
}
#endif

#ifdef HAVE_TINYXML2
tinyxml2::XMLElement* Registry::RegistryImpl::nodeToXml(
    const RegistryNode& node, tinyxml2::XMLDocument& doc,
    const std::string& name) {
    tinyxml2::XMLElement* element = doc.NewElement(name.c_str());

    // Add metadata
    element->SetAttribute("created", static_cast<unsigned int>(node.created));
    element->SetAttribute("lastModified",
                          static_cast<unsigned int>(node.lastModified));

    // Add values
    tinyxml2::XMLElement* valuesElement = doc.NewElement("Values");
    for (const auto& value : node.values) {
        tinyxml2::XMLElement* valueElement = doc.NewElement("Value");
        valueElement->SetAttribute("name", value.first.c_str());
        valueElement->SetAttribute("type", value.second.type.c_str());
        valueElement->SetAttribute(
            "lastModified",
            static_cast<unsigned int>(value.second.lastModified));
        valueElement->SetText(value.second.data.c_str());

        valuesElement->InsertEndChild(valueElement);
    }
    element->InsertEndChild(valuesElement);

    // Add children
    tinyxml2::XMLElement* childrenElement = doc.NewElement("Children");
    for (const auto& child : node.children) {
        tinyxml2::XMLElement* childElement =
            nodeToXml(child.second, doc, "Node");
        childElement->SetAttribute("name", child.first.c_str());
        childrenElement->InsertEndChild(childElement);
    }
    element->InsertEndChild(childrenElement);

    return element;
}

RegistryResult Registry::RegistryImpl::xmlToNode(tinyxml2::XMLElement* element,
                                                 RegistryNode& node) {
    try {
        // Load metadata
        if (element->Attribute("created")) {
            node.created =
                static_cast<std::time_t>(element->UnsignedAttribute("created"));
        }

        if (element->Attribute("lastModified")) {
            node.lastModified = static_cast<std::time_t>(
                element->UnsignedAttribute("lastModified"));
        }

        // Load values
        tinyxml2::XMLElement* valuesElement =
            element->FirstChildElement("Values");
        if (valuesElement) {
            tinyxml2::XMLElement* valueElement =
                valuesElement->FirstChildElement("Value");
            while (valueElement) {
                const char* nameAttr = valueElement->Attribute("name");
                const char* typeAttr = valueElement->Attribute("type");
                const char* dataText = valueElement->GetText();

                if (nameAttr && dataText) {
                    std::string name = nameAttr;
                    std::string type = typeAttr ? typeAttr : "string";
                    std::string data = dataText;

                    RegistryValue value(data, type);

                    if (valueElement->Attribute("lastModified")) {
                        value.lastModified = static_cast<std::time_t>(
                            valueElement->UnsignedAttribute("lastModified"));
                    }

                    node.values[name] = value;
                }

                valueElement = valueElement->NextSiblingElement("Value");
            }
        }

        // Load children
        tinyxml2::XMLElement* childrenElement =
            element->FirstChildElement("Children");
        if (childrenElement) {
            tinyxml2::XMLElement* childElement =
                childrenElement->FirstChildElement("Node");
            while (childElement) {
                const char* nameAttr = childElement->Attribute("name");

                if (nameAttr) {
                    std::string name = nameAttr;
                    RegistryNode childNode;

                    RegistryResult result = xmlToNode(childElement, childNode);
                    if (result != RegistryResult::SUCCESS) {
                        return result;
                    }

                    node.children[name] = std::move(childNode);
                }

                childElement = childElement->NextSiblingElement("Node");
            }
        }

        return RegistryResult::SUCCESS;
    } catch (const std::exception& e) {
        lastError = "XML parsing error: " + std::string(e.what());
        spdlog::error("XML parsing error: {}", e.what());
        return RegistryResult::INVALID_FORMAT;
    }
}
#endif

#ifdef HAVE_OPENSSL
std::string Registry::RegistryImpl::encrypt(const std::string& data) {
    if (!encryptionEnabled || encryptionKey.empty()) {
        spdlog::warn("Encryption requested but not enabled or missing key");
        return data;
    }

    try {
        std::vector<unsigned char> key(16);
        for (size_t i = 0; i < 16; i++) {
            std::string byteString = encryptionKey.substr(i * 2, 2);
            key[i] =
                static_cast<unsigned char>(std::stoi(byteString, nullptr, 16));
        }

        std::vector<unsigned char> iv(16);
        if (RAND_bytes(iv.data(), static_cast<int>(iv.size())) != 1) {
            throw std::runtime_error("Failed to generate random IV");
        }

        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            throw std::runtime_error("Failed to create cipher context");
        }

        if (EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr, key.data(),
                               iv.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed to initialize encryption");
        }

        std::vector<unsigned char> ciphertext(data.size() +
                                              EVP_MAX_BLOCK_LENGTH);
        int ciphertextLen = 0;

        if (EVP_EncryptUpdate(
                ctx, ciphertext.data(), &ciphertextLen,
                reinterpret_cast<const unsigned char*>(data.data()),
                static_cast<int>(data.size())) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed during encryption update");
        }

        int finalLen = 0;
        if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + ciphertextLen,
                                &finalLen) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed during encryption finalization");
        }

        ciphertextLen += finalLen;
        ciphertext.resize(ciphertextLen);

        EVP_CIPHER_CTX_free(ctx);

        std::vector<unsigned char> combined;
        combined.insert(combined.end(), iv.begin(), iv.end());
        combined.insert(combined.end(), ciphertext.begin(), ciphertext.end());

        std::stringstream ss;
        for (unsigned char byte : combined) {
            ss << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<int>(byte);
        }

        return ss.str();
    } catch (const std::exception& e) {
        spdlog::error("Encryption error: {}", e.what());
        throw;
    }
}

std::string Registry::RegistryImpl::decrypt(const std::string& encryptedData) {
    if (!encryptionEnabled || encryptionKey.empty()) {
        spdlog::warn("Decryption requested but not enabled or missing key");
        return encryptedData;
    }

    try {
        std::vector<unsigned char> key(16);
        for (size_t i = 0; i < 16; i++) {
            std::string byteString = encryptionKey.substr(i * 2, 2);
            key[i] =
                static_cast<unsigned char>(std::stoi(byteString, nullptr, 16));
        }

        std::vector<unsigned char> combined;
        for (size_t i = 0; i < encryptedData.length(); i += 2) {
            std::string byteString = encryptedData.substr(i, 2);
            combined.push_back(
                static_cast<unsigned char>(std::stoi(byteString, nullptr, 16)));
        }

        if (combined.size() < 16) {
            throw std::runtime_error("Invalid encrypted data: too short");
        }

        std::vector<unsigned char> iv(combined.begin(), combined.begin() + 16);
        std::vector<unsigned char> ciphertext(combined.begin() + 16,
                                              combined.end());

        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            throw std::runtime_error("Failed to create cipher context");
        }

        if (EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr, key.data(),
                               iv.data()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed to initialize decryption");
        }

        std::vector<unsigned char> plaintext(ciphertext.size());
        int plaintextLen = 0;

        if (EVP_DecryptUpdate(ctx, plaintext.data(), &plaintextLen,
                              ciphertext.data(),
                              static_cast<int>(ciphertext.size())) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed during decryption update");
        }

        int finalLen = 0;
        if (EVP_DecryptFinal_ex(ctx, plaintext.data() + plaintextLen,
                                &finalLen) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            throw std::runtime_error("Failed during decryption finalization");
        }

        plaintextLen += finalLen;
        plaintext.resize(plaintextLen);

        EVP_CIPHER_CTX_free(ctx);

        return std::string(reinterpret_cast<char*>(plaintext.data()),
                           plaintext.size());
    } catch (const std::exception& e) {
        spdlog::error("Decryption error: {}", e.what());
        throw;
    }
}
#endif

bool Registry::RegistryImpl::matchesPattern(const std::string& text,
                                            const std::string& pattern) const {
    try {
        std::regex regex(pattern);
        return std::regex_search(text, regex);
    } catch (const std::regex_error&) {
        return text.find(pattern) != std::string::npos;
    }
}

}  // namespace atom::system
