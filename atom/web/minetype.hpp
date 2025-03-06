#ifndef MIMETYPES_H
#define MIMETYPES_H

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

// Concept for valid path types
template <typename T>
concept PathLike = requires(T a) {
    { std::string(a) } -> std::convertible_to<std::string>;
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

private:
    class Impl;  ///< Forward declaration of the implementation class.
    std::unique_ptr<Impl> pImpl;  ///< Pointer to the implementation.
};

#endif  // MIMETYPES_H