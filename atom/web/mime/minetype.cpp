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

#include <spdlog/spdlog.h>
#include "atom/type/json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

class MimeTypes::Impl {
public:
    Impl(std::span<const std::string> knownFiles, bool lenient)
        : lenient_(lenient),
          config_({lenient, true, 1000, false, "application/octet-stream"}),
          isInitialized_(false) {
        initialize(knownFiles);
    }

    Impl(std::span<const std::string> knownFiles, const MimeTypeConfig& config)
        : lenient_(config.lenient), config_(config), isInitialized_(false) {
        initialize(knownFiles);
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
            typeEntries.reserve(jsonData.size() * 2);

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

            addTypesBatch(typeEntries);
            spdlog::info("Loaded {} MIME types from JSON file {}",
                         typeEntries.size(), jsonFile);

        } catch (const json::exception& e) {
            spdlog::error("JSON parsing error for file {}: {}", jsonFile,
                          e.what());
            throw MimeTypeException(std::string("JSON parsing error: ") +
                                    e.what());
        } catch (const std::exception& e) {
            spdlog::error("Failed to read JSON file {}: {}", jsonFile,
                          e.what());
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
                spdlog::info("Loaded {} MIME types from XML file {}",
                             typeEntries.size(), xmlFile);
            }

        } catch (const std::exception& e) {
            spdlog::error("Failed to read XML file {}: {}", xmlFile, e.what());
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
            spdlog::info("Exported {} MIME types to JSON file {}",
                         output.size(), jsonFile);

        } catch (const json::exception& e) {
            spdlog::error("JSON export error for file {}: {}", jsonFile,
                          e.what());
            throw MimeTypeException(std::string("JSON error: ") + e.what());
        } catch (const std::exception& e) {
            spdlog::error("Failed to export to JSON file {}: {}", jsonFile,
                          e.what());
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
            spdlog::info("Exported {} MIME types to XML file {}",
                         reverseMap_.size(), xmlFile);

        } catch (const std::exception& e) {
            spdlog::error("Failed to export to XML file {}: {}", xmlFile,
                          e.what());
            throw MimeTypeException(std::string("Failed to export to XML: ") +
                                    e.what());
        }
    }

    void clearCache() {
        if (config_.useCache) {
            std::unique_lock lock(cacheMutex_);
            cache_.clear();
            spdlog::debug("MIME type cache cleared");
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

        spdlog::debug("MimeTypes configuration updated");
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
        return reverseMap_.contains(mimeType);
    }

    bool hasExtension(const std::string& extension) const {
        if (extension.empty()) {
            return false;
        }

        std::string normalizedExt = normalizeExtension(extension);
        std::shared_lock lock(mutex_);
        return typesMap_.contains(normalizedExt);
    }

    auto guessType(const std::string& url) const
        -> std::pair<std::optional<std::string>, std::optional<std::string>> {
        if (url.empty()) {
            return {std::nullopt, std::nullopt};
        }

        if (config_.useCache) {
            std::shared_lock cacheLock(cacheMutex_);
            if (auto it = cache_.find(url); it != cache_.end()) {
                return {it->second, std::nullopt};
            }
        }

        try {
            fs::path path(url);
            std::string extension = path.extension().string();

            if (!extension.empty()) {
                std::string normalizedExt = normalizeExtension(extension);
                auto result = getMimeType(normalizedExt);

                if (config_.useCache && result.first.has_value()) {
                    std::unique_lock cacheLock(cacheMutex_);
                    if (cache_.size() < config_.cacheSize) {
                        cache_[url] = result.first.value();
                    }
                }

                return result;
            }
        } catch (const std::exception& e) {
            spdlog::warn("Error guessing MIME type for URL {}: {}", url,
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
        if (auto it = reverseMap_.find(mimeType); it != reverseMap_.end()) {
            return it->second;
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
            std::string normalizedExt = normalizeExtension(extension);
            std::unique_lock lock(mutex_);

            typesMap_[normalizedExt] = mimeType;
            reverseMap_[mimeType].push_back(normalizedExt);

            spdlog::trace("Added MIME type mapping: {} -> {}", normalizedExt,
                          mimeType);

        } catch (const std::exception& e) {
            spdlog::error("Failed to add type {} for extension {}: {}",
                          mimeType, extension, e.what());
            throw MimeTypeException(std::string("Failed to add type: ") +
                                    e.what());
        }
    }

    void addTypesBatch(
        std::span<const std::pair<std::string, std::string>> types) {
        try {
            std::unique_lock lock(mutex_);
            size_t addedCount = 0;

            for (const auto& [mimeType, extension] : types) {
                if (mimeType.empty() || extension.empty()) {
                    continue;
                }

                std::string normalizedExt = normalizeExtension(extension);
                typesMap_[normalizedExt] = mimeType;
                reverseMap_[mimeType].push_back(normalizedExt);
                ++addedCount;
            }

            spdlog::debug("Added {} MIME type mappings in batch", addedCount);

        } catch (const std::exception& e) {
            spdlog::error("Failed to add types in batch: {}", e.what());
            throw MimeTypeException(
                std::string("Failed to add types in batch: ") + e.what());
        }
    }

    void listAllTypes() const {
        std::shared_lock lock(mutex_);
        try {
            if (typesMap_.empty()) {
                spdlog::info("No MIME types available");
                return;
            }

            spdlog::info("Listing all MIME types ({} entries):",
                         typesMap_.size());
            for (const auto& [ext, type] : typesMap_) {
                spdlog::info("Extension: {} -> MIME Type: {}", ext, type);
            }
        } catch (const std::exception& e) {
            spdlog::error("Error listing MIME types: {}", e.what());
        }
    }

    template <PathLike T>
    auto guessTypeByContent(const T& filePath) const
        -> std::optional<std::string> {
        std::string path = std::string(filePath);
        if (path.empty()) {
            spdlog::warn("Empty file path provided");
            return std::nullopt;
        }

        try {
            if (!fs::exists(path) || !fs::is_regular_file(path)) {
                spdlog::warn("File does not exist or is not a regular file: {}",
                             path);
                return std::nullopt;
            }

            auto fileSize = fs::file_size(path);
            if (fileSize == 0) {
                spdlog::debug("File is empty: {}", path);
                return "text/plain";
            }

            std::ifstream file(path, std::ios::binary);
            if (!file) {
                spdlog::warn("Could not open file {}", path);
                return std::nullopt;
            }

            constexpr size_t SIGNATURE_SIZE = 32;
            std::array<char, SIGNATURE_SIZE> buffer{};
            file.read(buffer.data(), buffer.size());
            const size_t bytesRead = file.gcount();

            return detectMimeTypeFromSignature(buffer.data(), bytesRead);

        } catch (const std::exception& e) {
            spdlog::error(
                "Error determining MIME type by content for file {}: {}", path,
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
    mutable std::unordered_map<std::string, std::string> cache_;
    bool lenient_;
    MimeTypeConfig config_;
    std::atomic<bool> isInitialized_;

    void initialize(std::span<const std::string> knownFiles) {
        try {
            if (knownFiles.empty()) {
                spdlog::warn("No known MIME type files provided");
            }

            std::vector<std::future<void>> futures;
            futures.reserve(knownFiles.size());

            for (const auto& file : knownFiles) {
                futures.push_back(std::async(std::launch::async, [this, &file] {
                    try {
                        this->readFile(file);
                    } catch (const std::exception& e) {
                        spdlog::error("Failed to read file {}: {}", file,
                                      e.what());
                    }
                }));
            }

            for (auto& future : futures) {
                future.wait();
            }

            isInitialized_ = true;

            if (config_.useCache) {
                cache_.reserve(config_.cacheSize);
            }

            spdlog::debug("MimeTypes initialization completed with {} types",
                          typesMap_.size());

        } catch (const std::exception& e) {
            spdlog::error("Failed to initialize MimeTypes: {}", e.what());
            throw MimeTypeException(
                std::string("Failed to initialize MimeTypes: ") + e.what());
        }
    }

    void readFile(const std::string& file) {
        if (file.empty()) {
            spdlog::warn("Empty file path provided");
            return;
        }

        try {
            if (!fs::exists(file)) {
                spdlog::warn("File does not exist: {}", file);
                return;
            }

            std::ifstream fileStream(file);
            if (!fileStream) {
                spdlog::warn("Could not open file {}", file);
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

            if (!batchEntries.empty()) {
                addTypesBatch(batchEntries);
                spdlog::debug("Loaded {} entries from file {}",
                              batchEntries.size(), file);
            }

        } catch (const std::exception& e) {
            spdlog::error("Failed to read file {}: {}", file, e.what());
        }
    }

    auto getMimeType(const std::string& extension) const
        -> std::pair<std::optional<std::string>, std::optional<std::string>> {
        std::shared_lock lock(mutex_);
        if (auto it = typesMap_.find(extension); it != typesMap_.end()) {
            return {it->second, std::nullopt};
        }

        if (lenient_) {
            return {config_.defaultType, std::nullopt};
        }

        return {std::nullopt, std::nullopt};
    }

    auto normalizeExtension(const std::string& extension) const -> std::string {
        std::string normalized = extension;
        if (normalized[0] != '.') {
            normalized = "." + normalized;
        }

        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return normalized;
    }

    auto detectMimeTypeFromSignature(const char* data, size_t size) const
        -> std::optional<std::string> {
        if (size < 2)
            return std::nullopt;

        static const struct {
            std::string_view signature;
            std::string mimeType;
        } signatures[] = {{"\xFF\xD8\xFF", "image/jpeg"},
                          {"\x89PNG\r\n\x1A\n", "image/png"},
                          {"GIF87a", "image/gif"},
                          {"GIF89a", "image/gif"},
                          {"PK\x03\x04", "application/zip"},
                          {"PK\x05\x06", "application/zip"},
                          {"PK\x07\x08", "application/zip"},
                          {"%PDF", "application/pdf"},
                          {"<!DOCTYPE", "text/html"},
                          {"<html", "text/html"},
                          {"<HTML", "text/html"},
                          {"\x1F\x8B", "application/gzip"},
                          {"BZh", "application/x-bzip2"},
                          {"\x7F"
                           "ELF",
                           "application/x-executable"},
                          {"MZ", "application/x-dosexec"}};

        for (const auto& sig : signatures) {
            if (size >= sig.signature.size() &&
                std::memcmp(data, sig.signature.data(), sig.signature.size()) ==
                    0) {
                return sig.mimeType;
            }
        }

        bool isText =
            std::all_of(data, data + std::min(size, size_t{512}), [](char c) {
                return c >= 32 || c == '\t' || c == '\n' || c == '\r' ||
                       c == '\f';
            });

        return isText ? std::optional<std::string>("text/plain")
                      : std::optional<std::string>("application/octet-stream");
    }
};

MimeTypes::MimeTypes(std::span<const std::string> knownFiles, bool lenient)
    : pImpl(std::make_unique<Impl>(knownFiles, lenient)) {}

MimeTypes::MimeTypes(std::span<const std::string> knownFiles,
                     const MimeTypeConfig& config)
    : pImpl(std::make_unique<Impl>(knownFiles, config)) {}

MimeTypes::~MimeTypes() = default;

void MimeTypes::readJson(const std::string& jsonFile) {
    pImpl->readJson(jsonFile);
}

void MimeTypes::readXml(const std::string& xmlFile) { pImpl->readXml(xmlFile); }

void MimeTypes::exportToJson(const std::string& jsonFile) const {
    pImpl->exportToJson(jsonFile);
}

void MimeTypes::exportToXml(const std::string& xmlFile) const {
    pImpl->exportToXml(xmlFile);
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
    pImpl->addType(mimeType, extension);
}

void MimeTypes::addTypesBatch(
    std::span<const std::pair<std::string, std::string>> types) {
    pImpl->addTypesBatch(types);
}

void MimeTypes::listAllTypes() const { pImpl->listAllTypes(); }

template <PathLike T>
auto MimeTypes::guessTypeByContent(const T& filePath) const
    -> std::optional<std::string> {
    return pImpl->guessTypeByContent(filePath);
}

template std::optional<std::string> MimeTypes::guessTypeByContent<std::string>(
    const std::string& filePath) const;
template std::optional<std::string> MimeTypes::guessTypeByContent<const char*>(
    const char* const& filePath) const;
