#include "minetype.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <unordered_map>

#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

class MimeTypes::Impl {
public:
    Impl(std::span<const std::string> knownFiles, bool lenient)
        : lenient_(lenient),
          config_({lenient, true, 1000, false, "application/octet-stream"}),
          isInitialized_(false) {
        try {
            if (knownFiles.empty()) {
                LOG_F(WARNING, "No known MIME type files provided");
            }

            // Process files concurrently if we have multiple files
            if (knownFiles.size() > 1) {
                std::vector<std::future<void>> futures;
                futures.reserve(knownFiles.size());

                for (const auto& file : knownFiles) {
                    futures.push_back(
                        std::async(std::launch::async, [this, &file] {
                            try {
                                this->readFile(file);
                            } catch (const std::exception& e) {
                                LOG_F(ERROR, "Failed to read file {}: {}", file,
                                      e.what());
                            }
                        }));
                }

                // Wait for all tasks to complete
                for (auto& future : futures) {
                    future.wait();
                }
            } else if (knownFiles.size() == 1) {
                readFile(knownFiles[0]);
            }

            isInitialized_ = true;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to initialize MimeTypes: {}", e.what());
            throw MimeTypeException(
                std::string("Failed to initialize MimeTypes: ") + e.what());
        }

        // Initialize cache
        if (config_.useCache) {
            cache_.reserve(config_.cacheSize);
        }
    }

    Impl(std::span<const std::string> knownFiles, const MimeTypeConfig& config)
        : lenient_(config.lenient), config_(config), isInitialized_(false) {
        try {
            if (knownFiles.empty()) {
                LOG_F(WARNING, "No known MIME type files provided");
            }

            // Process files concurrently if we have multiple files
            if (knownFiles.size() > 1) {
                std::vector<std::future<void>> futures;
                futures.reserve(knownFiles.size());

                for (const auto& file : knownFiles) {
                    futures.push_back(
                        std::async(std::launch::async, [this, &file] {
                            try {
                                this->readFile(file);
                            } catch (const std::exception& e) {
                                LOG_F(ERROR, "Failed to read file {}: {}", file,
                                      e.what());
                            }
                        }));
                }

                // Wait for all tasks to complete
                for (auto& future : futures) {
                    future.wait();
                }
            } else if (knownFiles.size() == 1) {
                readFile(knownFiles[0]);
            }

            isInitialized_ = true;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to initialize MimeTypes: {}", e.what());
            throw MimeTypeException(
                std::string("Failed to initialize MimeTypes: ") + e.what());
        }

        // Initialize cache
        if (config_.useCache) {
            cache_.reserve(config_.cacheSize);
        }
    }

    void readJson(const std::string& jsonFile) {
        if (jsonFile.empty()) {
            throw MimeTypeException("Empty JSON file path provided");
        }

        try {
            std::ifstream file(jsonFile);
            if (!file) {
                throw MimeTypeException("Could not open JSON file " + jsonFile);
            }

            json jsonData;
            file >> jsonData;

            std::vector<std::pair<std::string, std::string>> typeEntries;

            for (const auto& [mimeType, extensions] : jsonData.items()) {
                if (mimeType.empty())
                    continue;

                for (const auto& ext : extensions) {
                    const auto extension = ext.get<std::string>();
                    if (!extension.empty()) {
                        typeEntries.emplace_back(mimeType, extension);
                    }
                }
            }

            // Batch add the types
            addTypesBatch(typeEntries);

        } catch (const json::exception& e) {
            throw MimeTypeException(std::string("JSON parsing error: ") +
                                    e.what());
        } catch (const std::exception& e) {
            throw MimeTypeException(std::string("Failed to read JSON file: ") +
                                    e.what());
        }
    }

    void readXml(const std::string& xmlFile) {
        if (xmlFile.empty()) {
            throw MimeTypeException("Empty XML file path provided");
        }

        try {
            std::ifstream file(xmlFile);
            if (!file) {
                throw MimeTypeException("Could not open XML file " + xmlFile);
            }

            std::string line;
            std::vector<std::pair<std::string, std::string>> typeEntries;

            std::string currentMimeType;
            while (std::getline(file, line)) {
                auto mimeTypePos = line.find("<mime-type type=\"");
                if (mimeTypePos != std::string::npos) {
                    size_t start = mimeTypePos + 17;
                    size_t end = line.find("\"", start);
                    if (end != std::string::npos) {
                        currentMimeType = line.substr(start, end - start);
                    }
                }

                auto globPos = line.find("<glob pattern=\"*.");
                if (!currentMimeType.empty() && globPos != std::string::npos) {
                    size_t start = globPos + 15;
                    size_t end = line.find("\"", start);
                    if (end != std::string::npos) {
                        std::string extension = line.substr(start, end - start);
                        typeEntries.emplace_back(currentMimeType, extension);
                    }
                }

                if (line.find("</mime-type>") != std::string::npos) {
                    currentMimeType.clear();
                }
            }

            if (!typeEntries.empty()) {
                addTypesBatch(typeEntries);
                LOG_F(INFO, "Loaded {} MIME types from XML file",
                      typeEntries.size());
            }

        } catch (const std::exception& e) {
            throw MimeTypeException(std::string("Failed to read XML file: ") +
                                    e.what());
        }
    }

    void exportToJson(const std::string& jsonFile) const {
        if (jsonFile.empty()) {
            throw MimeTypeException("Empty JSON file path provided");
        }

        try {
            json output;
            std::shared_lock lock(mutex_);

            for (const auto& [mimeType, extensions] : reverseMap_) {
                json extArray = json::array();
                for (const auto& ext : extensions) {
                    extArray.push_back(ext.substr(1));
                }
                output[mimeType] = extArray;
            }

            std::ofstream file(jsonFile);
            if (!file) {
                throw MimeTypeException("Could not open file for writing: " +
                                        jsonFile);
            }

            file << output.dump(4);
            LOG_F(INFO, "Exported {} MIME types to JSON file {}", output.size(),
                  jsonFile);
        } catch (const json::exception& e) {
            throw MimeTypeException(std::string("JSON error: ") + e.what());
        } catch (const std::exception& e) {
            throw MimeTypeException(std::string("Failed to export to JSON: ") +
                                    e.what());
        }
    }

    void exportToXml(const std::string& xmlFile) const {
        if (xmlFile.empty()) {
            throw MimeTypeException("Empty XML file path provided");
        }

        try {
            std::ofstream file(xmlFile);
            if (!file) {
                throw MimeTypeException("Could not open file for writing: " +
                                        xmlFile);
            }

            file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
            file << "<mime-info "
                    "xmlns=\"http://www.freedesktop.org/standards/"
                    "shared-mime-info\">\n";

            std::shared_lock lock(mutex_);
            for (const auto& [mimeType, extensions] : reverseMap_) {
                file << "  <mime-type type=\"" << mimeType << "\">\n";
                for (const auto& ext : extensions) {
                    file << "    <glob pattern=\"*" << ext << "\"/>\n";
                }
                file << "  </mime-type>\n";
            }

            file << "</mime-info>\n";
            LOG_F(INFO, "Exported {} MIME types to XML file {}",
                  reverseMap_.size(), xmlFile);
        } catch (const std::exception& e) {
            throw MimeTypeException(std::string("Failed to export to XML: ") +
                                    e.what());
        }
    }

    void clearCache() {
        if (config_.useCache) {
            std::unique_lock lock(cacheMutex_);
            cache_.clear();
            LOG_F(INFO, "MIME type cache cleared");
        }
    }

    void updateConfig(const MimeTypeConfig& config) {
        std::unique_lock lock(configMutex_);
        bool cacheChanged = (config_.useCache != config.useCache ||
                             config_.cacheSize != config.cacheSize);

        config_ = config;
        lenient_ = config.lenient;

        if (cacheChanged) {
            std::unique_lock cacheLock(cacheMutex_);
            if (config_.useCache) {
                cache_.reserve(config_.cacheSize);
            } else {
                cache_.clear();
            }
        }

        LOG_F(INFO, "MimeTypes configuration updated");
    }

    MimeTypeConfig getConfig() const {
        std::shared_lock lock(configMutex_);
        return config_;
    }

    bool hasMimeType(const std::string& mimeType) const {
        if (mimeType.empty()) {
            return false;
        }
        std::shared_lock lock(mutex_);
        return reverseMap_.find(mimeType) != reverseMap_.end();
    }

    bool hasExtension(const std::string& extension) const {
        if (extension.empty()) {
            return false;
        }

        std::string normalizedExt = extension;
        if (normalizedExt[0] != '.') {
            normalizedExt = "." + normalizedExt;
        }

        std::transform(normalizedExt.begin(), normalizedExt.end(),
                       normalizedExt.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        std::shared_lock lock(mutex_);
        return typesMap_.find(normalizedExt) != typesMap_.end();
    }

    auto guessType(const std::string& url) const
        -> std::pair<std::optional<std::string>, std::optional<std::string>> {
        if (url.empty()) {
            return {std::nullopt, std::nullopt};
        }

        try {
            fs::path path(url);
            std::string extension = path.extension().string();

            if (!extension.empty() && extension[0] == '.') {
                std::transform(extension.begin(), extension.end(),
                               extension.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                return getMimeType(extension);
            }
        } catch (const std::exception& e) {
            LOG_F(WARNING, "Error guessing MIME type for URL {}: {}", url,
                  e.what());
        }

        return {std::nullopt, std::nullopt};
    }

    auto guessAllExtensions(const std::string& mimeType) const
        -> std::vector<std::string> {
        if (mimeType.empty()) {
            return {};
        }

        std::shared_lock lock(mutex_);
        auto iter = reverseMap_.find(mimeType);
        if (iter != reverseMap_.end()) {
            return iter->second;
        }
        return {};
    }

    auto guessExtension(const std::string& mimeType) const
        -> std::optional<std::string> {
        if (mimeType.empty()) {
            return std::nullopt;
        }

        auto extensions = guessAllExtensions(mimeType);
        return extensions.empty() ? std::nullopt
                                  : std::make_optional(extensions[0]);
    }

    void addType(const std::string& mimeType, const std::string& extension) {
        if (mimeType.empty() || extension.empty()) {
            throw MimeTypeException(
                "MIME type and extension must not be empty");
        }

        try {
            std::string normalizedExt = extension;
            if (normalizedExt[0] != '.') {
                normalizedExt = "." + normalizedExt;
            }

            std::transform(normalizedExt.begin(), normalizedExt.end(),
                           normalizedExt.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            std::unique_lock lock(mutex_);
            typesMap_[normalizedExt] = mimeType;
            reverseMap_[mimeType].push_back(normalizedExt);
        } catch (const std::exception& e) {
            throw MimeTypeException(std::string("Failed to add type: ") +
                                    e.what());
        }
    }

    void addTypesBatch(
        std::span<const std::pair<std::string, std::string>> types) {
        try {
            std::unique_lock lock(mutex_);
            for (const auto& [mimeType, extension] : types) {
                if (mimeType.empty() || extension.empty()) {
                    continue;
                }

                std::string normalizedExt = extension;
                if (normalizedExt[0] != '.') {
                    normalizedExt = "." + normalizedExt;
                }

                std::transform(normalizedExt.begin(), normalizedExt.end(),
                               normalizedExt.begin(),
                               [](unsigned char c) { return std::tolower(c); });

                typesMap_[normalizedExt] = mimeType;
                reverseMap_[mimeType].push_back(normalizedExt);
            }
        } catch (const std::exception& e) {
            throw MimeTypeException(
                std::string("Failed to add types in batch: ") + e.what());
        }
    }

    void listAllTypes() const {
        std::shared_lock lock(mutex_);
        try {
            if (typesMap_.empty()) {
                LOG_F(INFO, "No MIME types available");
                return;
            }

            LOG_F(INFO,
                  "Listing all MIME types ({} entries):", typesMap_.size());
            for (const auto& [ext, type] : typesMap_) {
                LOG_F(INFO, "Extension: {} -> MIME Type: {}", ext, type);
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error listing MIME types: {}", e.what());
        }
    }

    template <PathLike T>
    auto guessTypeByContent(const T& filePath) const
        -> std::optional<std::string> {
        std::string path = std::string(filePath);
        if (path.empty()) {
            LOG_F(WARNING, "Empty file path provided");
            return std::nullopt;
        }

        try {
            if (!fs::exists(path) || !fs::is_regular_file(path)) {
                LOG_F(WARNING,
                      "File does not exist or is not a regular file: {}", path);
                return std::nullopt;
            }

            auto fileSize = fs::file_size(path);
            if (fileSize == 0) {
                LOG_F(INFO, "File is empty: {}", path);
                return "text/plain";
            }

            std::ifstream file(path, std::ios::binary);
            if (!file) {
                LOG_F(WARNING, "Could not open file {}", path);
                return std::nullopt;
            }

            constexpr size_t signatureSize = 16;
            std::array<char, signatureSize> buffer{};
            file.read(buffer.data(), buffer.size());
            const size_t bytesRead = file.gcount();

            if (bytesRead >= 2 && buffer[0] == '\xFF' && buffer[1] == '\xD8') {
                return "image/jpeg";
            }
            if (bytesRead >= 4 && buffer[0] == '\x89' && buffer[1] == 'P' &&
                buffer[2] == 'N' && buffer[3] == 'G') {
                return "image/png";
            }
            if (bytesRead >= 3 && buffer[0] == 'G' && buffer[1] == 'I' &&
                buffer[2] == 'F') {
                return "image/gif";
            }
            if (bytesRead >= 2 && buffer[0] == 'P' && buffer[1] == 'K') {
                return "application/zip";
            }
            if (bytesRead >= 4 && buffer[0] == '%' && buffer[1] == 'P' &&
                buffer[2] == 'D' && buffer[3] == 'F') {
                return "application/pdf";
            }
            if (bytesRead >= 4 && buffer[0] == '<' &&
                (buffer[1] == '?' || buffer[1] == '!' || buffer[1] == 'h')) {
                return "text/html";
            }

            bool isText = true;
            for (size_t i = 0; i < bytesRead && i < 512; i++) {
                char c = buffer[i];
                if (c == 0 || (c < 32 && c != '\t' && c != '\n' && c != '\r' &&
                               c != '\f')) {
                    isText = false;
                    break;
                }
            }

            if (isText) {
                return "text/plain";
            }

            return "application/octet-stream";

        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error determining MIME type by content: {}",
                  e.what());
            return std::nullopt;
        }
    }

private:
    mutable std::shared_mutex mutex_;
    mutable std::shared_mutex cacheMutex_;
    mutable std::shared_mutex configMutex_;
    std::unordered_map<std::string, std::string> typesMap_;
    std::unordered_map<std::string, std::vector<std::string>> reverseMap_;
    std::unordered_map<std::string, std::string> cache_;
    bool lenient_;
    MimeTypeConfig config_;
    std::atomic<bool> isInitialized_;

    void readFile(const std::string& file) {
        if (file.empty()) {
            LOG_F(WARNING, "Empty file path provided");
            return;
        }

        try {
            if (!fs::exists(file)) {
                LOG_F(WARNING, "File does not exist: {}", file);
                return;
            }

            std::ifstream fileStream(file);
            if (!fileStream) {
                LOG_F(WARNING, "Could not open file {}", file);
                return;
            }

            std::string line;
            std::vector<std::pair<std::string, std::string>> batchEntries;

            while (std::getline(fileStream, line)) {
                if (line.empty() || line[0] == '#') {
                    continue;
                }

                std::istringstream iss(line);
                std::string mimeType;
                if (iss >> mimeType) {
                    std::string ext;
                    while (iss >> ext) {
                        batchEntries.emplace_back(mimeType, ext);
                    }
                }
            }

            addTypesBatch(batchEntries);

        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to read file {}: {}", file, e.what());
        }
    }

    auto getMimeType(const std::string& extension) const
        -> std::pair<std::optional<std::string>, std::optional<std::string>> {
        std::shared_lock lock(mutex_);
        auto iter = typesMap_.find(extension);
        if (iter != typesMap_.end()) {
            return {iter->second, std::nullopt};
        }
        if (lenient_) {
            return {"application/octet-stream", std::nullopt};
        }
        return {std::nullopt, std::nullopt};
    }
};

MimeTypes::MimeTypes(std::span<const std::string> knownFiles, bool lenient)
    : pImpl(std::make_unique<Impl>(knownFiles, lenient)) {}

MimeTypes::MimeTypes(std::span<const std::string> knownFiles,
                     const MimeTypeConfig& config)
    : pImpl(std::make_unique<Impl>(knownFiles, config)) {}

MimeTypes::~MimeTypes() = default;

void MimeTypes::readJson(const std::string& jsonFile) {
    try {
        pImpl->readJson(jsonFile);
    } catch (const MimeTypeException& e) {
        LOG_F(ERROR, "Failed to read JSON: {}", e.what());
        throw;
    }
}

void MimeTypes::readXml(const std::string& xmlFile) {
    try {
        pImpl->readXml(xmlFile);
    } catch (const MimeTypeException& e) {
        LOG_F(ERROR, "Failed to read XML: {}", e.what());
        throw;
    }
}

void MimeTypes::exportToJson(const std::string& jsonFile) const {
    try {
        pImpl->exportToJson(jsonFile);
    } catch (const MimeTypeException& e) {
        LOG_F(ERROR, "Failed to export to JSON: {}", e.what());
        throw;
    }
}

void MimeTypes::exportToXml(const std::string& xmlFile) const {
    try {
        pImpl->exportToXml(xmlFile);
    } catch (const MimeTypeException& e) {
        LOG_F(ERROR, "Failed to export to XML: {}", e.what());
        throw;
    }
}

void MimeTypes::clearCache() { pImpl->clearCache(); }

void MimeTypes::updateConfig(const MimeTypeConfig& config) {
    pImpl->updateConfig(config);
}

MimeTypeConfig MimeTypes::getConfig() const { return pImpl->getConfig(); }

bool MimeTypes::hasMimeType(const std::string& mimeType) const {
    return pImpl->hasMimeType(mimeType);
}

bool MimeTypes::hasExtension(const std::string& extension) const {
    return pImpl->hasExtension(extension);
}

auto MimeTypes::guessType(const std::string& url) const
    -> std::pair<std::optional<std::string>, std::optional<std::string>> {
    return pImpl->guessType(url);
}

auto MimeTypes::guessAllExtensions(const std::string& mimeType) const
    -> std::vector<std::string> {
    return pImpl->guessAllExtensions(mimeType);
}

auto MimeTypes::guessExtension(const std::string& mimeType) const
    -> std::optional<std::string> {
    return pImpl->guessExtension(mimeType);
}

void MimeTypes::addType(const std::string& mimeType,
                        const std::string& extension) {
    try {
        pImpl->addType(mimeType, extension);
    } catch (const MimeTypeException& e) {
        LOG_F(ERROR, "Failed to add type: {}", e.what());
        throw;
    }
}

void MimeTypes::addTypesBatch(
    std::span<const std::pair<std::string, std::string>> types) {
    try {
        pImpl->addTypesBatch(types);
    } catch (const MimeTypeException& e) {
        LOG_F(ERROR, "Failed to add types in batch: {}", e.what());
        throw;
    }
}

void MimeTypes::listAllTypes() const { pImpl->listAllTypes(); }

template <PathLike T>
auto MimeTypes::guessTypeByContent(const T& filePath) const
    -> std::optional<std::string> {
    try {
        return pImpl->guessTypeByContent(filePath);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error in guessTypeByContent: {}", e.what());
        return std::nullopt;
    }
}

template std::optional<std::string> MimeTypes::guessTypeByContent<std::string>(
    const std::string& filePath) const;
template std::optional<std::string> MimeTypes::guessTypeByContent<const char*>(
    const char* const& filePath) const;
