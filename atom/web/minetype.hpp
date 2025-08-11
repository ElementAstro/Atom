#ifndef ATOM_WEB_MIMETYPE_HPP
#define ATOM_WEB_MIMETYPE_HPP

#include <concepts>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

/**
 * @brief Exception thrown when MIME type operations fail.
 */
class MimeTypeException : public std::runtime_error {
public:
    explicit MimeTypeException(const std::string& msg)
        : std::runtime_error(msg) {}
};

/**
 * @brief Concept for valid path types
 */
template <typename T>
concept PathLike = requires(T a) {
    { std::string(a) } -> std::convertible_to<std::string>;
};

/**
 * @brief Configuration options for MimeTypes class.
 */
struct MimeTypeConfig {
    bool lenient = false;     ///< Whether to be lenient in MIME type detection.
    bool useCache = true;     ///< Whether to use caching for frequent lookups.
    size_t cacheSize = 1000;  ///< Maximum number of entries in the cache.
    bool enableDeepScanning =
        false;  ///< Whether to enable deep content scanning.
    std::string defaultType =
        "application/octet-stream";  ///< Default MIME type when unknown.
};

/**
 * @class MimeTypes
 * @brief A class for handling MIME types and file extensions.
 */
class MimeTypes {
public:
    /**
     * @brief Constructs a MimeTypes object.
     * @param knownFiles A vector of known file paths.
     * @param lenient A flag indicating whether to be lenient in MIME type
     * detection.
     * @throws MimeTypeException if initialization fails.
     */
    explicit MimeTypes(std::span<const std::string> knownFiles,
                       bool lenient = false);

    /**
     * @brief Constructs a MimeTypes object with custom configuration.
     * @param knownFiles A vector of known file paths.
     * @param config Configuration options for MIME type handling.
     * @throws MimeTypeException if initialization fails.
     */
    explicit MimeTypes(std::span<const std::string> knownFiles,
                       const MimeTypeConfig& config);

    /**
     * @brief Destructor.
     */
    ~MimeTypes();

    /**
     * @brief Reads MIME types from a JSON file.
     * @param jsonFile The path to the JSON file.
     * @throws MimeTypeException if reading the JSON file fails.
     */
    void readJson(const std::string& jsonFile);

    /**
     * @brief Reads MIME types from an XML file.
     * @param xmlFile The path to the XML file.
     * @throws MimeTypeException if reading the XML file fails.
     */
    void readXml(const std::string& xmlFile);

    /**
     * @brief Guesses the MIME type and charset of a URL.
     * @param url The URL to guess the MIME type for.
     * @return A pair containing the guessed MIME type and charset, if
     * available.
     */
    std::pair<std::optional<std::string>, std::optional<std::string>> guessType(
        const std::string& url) const;

    /**
     * @brief Guesses all possible file extensions for a given MIME type.
     * @param mimeType The MIME type to guess extensions for.
     * @return A vector of possible file extensions.
     */
    std::vector<std::string> guessAllExtensions(
        const std::string& mimeType) const;

    /**
     * @brief Guesses the file extension for a given MIME type.
     * @param mimeType The MIME type to guess the extension for.
     * @return The guessed file extension, if available.
     */
    std::optional<std::string> guessExtension(
        const std::string& mimeType) const;

    /**
     * @brief Adds a new MIME type and file extension pair.
     * @param mimeType The MIME type to add.
     * @param extension The file extension to associate with the MIME type.
     * @throws MimeTypeException if the input is invalid.
     */
    void addType(const std::string& mimeType, const std::string& extension);

    /**
     * @brief Adds multiple MIME type and file extension pairs in batch.
     * @param types Vector of pairs containing MIME type and extension.
     */
    void addTypesBatch(
        std::span<const std::pair<std::string, std::string>> types);

    /**
     * @brief Lists all known MIME types and their associated file extensions.
     */
    void listAllTypes() const;

    /**
     * @brief Guesses the MIME type of a file based on its content.
     * @tparam T A type convertible to string path
     * @param filePath The path to the file.
     * @return The guessed MIME type, if available.
     * @throws MimeTypeException if the file cannot be accessed.
     */
    template <PathLike T>
    std::optional<std::string> guessTypeByContent(const T& filePath) const;

    /**
     * @brief Exports all MIME types to a JSON file.
     * @param jsonFile The path to the output JSON file.
     * @throws MimeTypeException if exporting fails.
     */
    void exportToJson(const std::string& jsonFile) const;

    /**
     * @brief Exports all MIME types to an XML file.
     * @param xmlFile The path to the output XML file.
     * @throws MimeTypeException if exporting fails.
     */
    void exportToXml(const std::string& xmlFile) const;

    /**
     * @brief Clears the internal cache to free memory.
     */
    void clearCache();

    /**
     * @brief Updates the configuration settings.
     * @param config New configuration options.
     */
    void updateConfig(const MimeTypeConfig& config);

    /**
     * @brief Gets the current configuration.
     * @return Current configuration settings.
     */
    MimeTypeConfig getConfig() const;

    /**
     * @brief Checks if a MIME type is registered.
     * @param mimeType The MIME type to check.
     * @return True if the MIME type is registered, false otherwise.
     */
    bool hasMimeType(const std::string& mimeType) const;

    /**
     * @brief Checks if a file extension is registered.
     * @param extension The file extension to check.
     * @return True if the extension is registered, false otherwise.
     */
    bool hasExtension(const std::string& extension) const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

#endif  // ATOM_WEB_MIMETYPE_HPP
