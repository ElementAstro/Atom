#include "atom/algorithm/fnmatch.hpp"

#include <iostream>
#include <vector>

int main() {
    // Example usage of fnmatch
    {
        std::string pattern = "*.txt";
        std::string filename1 = "document.txt";
        std::string filename2 = "image.png";

        bool match1 = atom::algorithm::fnmatch(pattern, filename1);
        bool match2 = atom::algorithm::fnmatch(pattern, filename2);

        std::cout << "Pattern: " << pattern << std::endl;
        std::cout << "Filename: " << filename1
                  << " -> Match: " << std::boolalpha << match1 << std::endl;
        std::cout << "Filename: " << filename2
                  << " -> Match: " << std::boolalpha << match2 << std::endl;
    }

    // Example usage of filter with a single pattern
    {
        std::vector<std::string> filenames = {"document.txt", "image.png",
                                              "notes.txt", "photo.jpg"};
        std::string pattern = "*.txt";

        bool anyMatch = atom::algorithm::filter(filenames, pattern);

        std::cout << "\nPattern: " << pattern << std::endl;
        std::cout << "Any match: " << std::boolalpha << anyMatch << std::endl;
    }

    // Example usage of filter with multiple patterns
    {
        std::vector<std::string> filenames = {"document.txt", "image.png",
                                              "notes.txt", "photo.jpg"};
        std::vector<std::string> patterns = {"*.txt", "*.jpg"};

        std::vector<std::string> matchedFiles =
            atom::algorithm::filter(filenames, patterns);

        std::cout << "\nPatterns: ";
        for (const auto& pat : patterns) {
            std::cout << pat << " ";
        }
        std::cout << std::endl;

        std::cout << "Matched files: ";
        for (const auto& file : matchedFiles) {
            std::cout << file << " ";
        }
        std::cout << std::endl;
    }

    // Example usage of translate
    {
        std::string pattern = "*.txt";
        std::string regex;

        bool success = atom::algorithm::translate(pattern, regex);

        std::cout << "\nPattern: " << pattern << std::endl;
        std::cout << "Translated regex: " << regex << std::endl;
        std::cout << "Translation successful: " << std::boolalpha << success
                  << std::endl;
    }

    return 0;
}
