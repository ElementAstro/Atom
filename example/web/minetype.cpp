#include "atom/web/minetype.hpp"

#include <iostream>
#include <optional>
#include <vector>

int main() {
    // Create a MimeTypes object with known files and lenient flag
    std::vector<std::string> knownFiles = {"file1.txt", "file2.html"};
    MimeTypes mimeTypes(knownFiles, true);

    // Read MIME types from a JSON file
    mimeTypes.readJson("mime_types.json");

    // Guess the MIME type and charset of a URL
    std::pair<std::optional<std::string>, std::optional<std::string>>
        typeAndCharset = mimeTypes.guessType("http://example.com/file.txt");
    if (typeAndCharset.first) {
        std::cout << "Guessed MIME type: " << *typeAndCharset.first
                  << std::endl;
    }
    if (typeAndCharset.second) {
        std::cout << "Guessed charset: " << *typeAndCharset.second << std::endl;
    }

    // Guess all possible file extensions for a given MIME type
    std::vector<std::string> extensions =
        mimeTypes.guessAllExtensions("text/html");
    std::cout << "Possible extensions for text/html: ";
    for (const auto& ext : extensions) {
        std::cout << ext << " ";
    }
    std::cout << std::endl;

    // Guess the file extension for a given MIME type
    std::optional<std::string> extension =
        mimeTypes.guessExtension("image/png");
    if (extension) {
        std::cout << "Guessed extension for image/png: " << *extension
                  << std::endl;
    }

    // Add a new MIME type and file extension pair
    mimeTypes.addType("application/example", ".ex");

    // List all known MIME types and their associated file extensions
    mimeTypes.listAllTypes();

    // Guess the MIME type of a file based on its content
    std::optional<std::string> contentType =
        mimeTypes.guessTypeByContent("example.txt");
    if (contentType) {
        std::cout << "Guessed MIME type by content: " << *contentType
                  << std::endl;
    }

    return 0;
}