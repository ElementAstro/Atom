/*
 * minetype_comprehensive_example.cpp
 *
 * Copyright (C) 2025 Developers <example.com>
 *
 * A comprehensive example demonstrating advanced features of the Atom MimeTypes class
 */

#include "atom/web/minetype.hpp"
#include "atom/log/loguru.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <map>

void demonstrateBasicMimeTypeDetection() {
    std::cout << "\n=== Basic MIME Type Detection ===\n";
    
    try {
        // Create MimeTypes instance
        MimeTypes mimeTypes({}, true);  // Empty known files, lenient mode
        
        // Test various file extensions
        std::vector<std::string> testFiles = {
            "document.pdf",
            "image.jpg",
            "image.jpeg",
            "image.png",
            "image.gif",
            "video.mp4",
            "video.avi",
            "audio.mp3",
            "audio.wav",
            "text.txt",
            "data.json",
            "style.css",
            "script.js",
            "page.html",
            "archive.zip",
            "archive.tar.gz",
            "executable.exe",
            "unknown.xyz"
        };
        
        std::cout << "MIME type detection by file extension:\n";
        std::cout << "File                | MIME Type                    | Charset\n";
        std::cout << "--------------------|------------------------------|----------\n";
        
        for (const auto& filename : testFiles) {
            auto [mimeType, charset] = mimeTypes.guessType(filename);
            
            std::string mimeStr = mimeType ? *mimeType : "unknown";
            std::string charsetStr = charset ? *charset : "none";
            
            std::cout << std::left << std::setw(19) << filename << " | "
                      << std::setw(28) << mimeStr << " | "
                      << charsetStr << "\n";
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error in basic MIME type detection: " << e.what() << "\n";
    }
}

void demonstrateURLMimeTypeDetection() {
    std::cout << "\n=== URL MIME Type Detection ===\n";
    
    try {
        MimeTypes mimeTypes({}, true);
        
        // Test various URLs
        std::vector<std::string> testUrls = {
            "https://example.com/document.pdf",
            "https://api.example.com/data.json",
            "https://cdn.example.com/image.png?v=123",
            "https://example.com/page.html#section",
            "https://example.com/style.css?timestamp=456",
            "https://example.com/script.js",
            "https://example.com/video.mp4",
            "https://example.com/archive.zip",
            "https://example.com/no-extension",
            "https://example.com/file.unknown"
        };
        
        std::cout << "MIME type detection from URLs:\n";
        std::cout << "URL                                    | MIME Type\n";
        std::cout << "---------------------------------------|---------------------------\n";
        
        for (const auto& url : testUrls) {
            auto [mimeType, charset] = mimeTypes.guessType(url);
            
            std::string mimeStr = mimeType ? *mimeType : "unknown";
            std::string displayUrl = url.length() > 38 ? url.substr(0, 35) + "..." : url;
            
            std::cout << std::left << std::setw(38) << displayUrl << " | " << mimeStr << "\n";
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error in URL MIME type detection: " << e.what() << "\n";
    }
}

void demonstrateExtensionGuessing() {
    std::cout << "\n=== Extension Guessing ===\n";
    
    try {
        MimeTypes mimeTypes({}, true);
        
        // Test guessing extensions from MIME types
        std::vector<std::string> mimeTypesList = {
            "text/html",
            "text/plain",
            "application/json",
            "application/pdf",
            "image/jpeg",
            "image/png",
            "image/gif",
            "video/mp4",
            "audio/mpeg",
            "application/zip",
            "application/javascript",
            "text/css",
            "application/xml",
            "application/octet-stream"
        };
        
        std::cout << "Extension guessing from MIME types:\n";
        std::cout << "MIME Type                    | Primary Ext | All Extensions\n";
        std::cout << "-----------------------------|-------------|------------------\n";
        
        for (const auto& mimeType : mimeTypesList) {
            auto primaryExt = mimeTypes.guessExtension(mimeType);
            auto allExts = mimeTypes.guessAllExtensions(mimeType);
            
            std::string primaryStr = primaryExt ? *primaryExt : "none";
            
            std::string allExtsStr;
            for (size_t i = 0; i < allExts.size(); ++i) {
                if (i > 0) allExtsStr += ", ";
                allExtsStr += allExts[i];
                if (allExtsStr.length() > 15) {
                    allExtsStr += "...";
                    break;
                }
            }
            if (allExts.empty()) allExtsStr = "none";
            
            std::cout << std::left << std::setw(28) << mimeType << " | "
                      << std::setw(11) << primaryStr << " | "
                      << allExtsStr << "\n";
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error in extension guessing: " << e.what() << "\n";
    }
}

void demonstrateContentBasedDetection() {
    std::cout << "\n=== Content-Based Detection ===\n";
    
    try {
        MimeTypes mimeTypes({}, true);
        
        // Create test files with different content types
        std::filesystem::create_directories("test_files");
        
        // Create HTML file
        {
            std::ofstream htmlFile("test_files/test.html");
            htmlFile << "<!DOCTYPE html>\n<html><head><title>Test</title></head><body><h1>Hello World</h1></body></html>\n";
        }
        
        // Create JSON file
        {
            std::ofstream jsonFile("test_files/test.json");
            jsonFile << "{\n  \"name\": \"test\",\n  \"value\": 123,\n  \"active\": true\n}\n";
        }
        
        // Create XML file
        {
            std::ofstream xmlFile("test_files/test.xml");
            xmlFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<root><item>test</item></root>\n";
        }
        
        // Create plain text file
        {
            std::ofstream txtFile("test_files/test.txt");
            txtFile << "This is a plain text file.\nIt contains multiple lines.\n";
        }
        
        // Create binary file (fake image header)
        {
            std::ofstream binFile("test_files/test.bin", std::ios::binary);
            // Write PNG signature
            unsigned char pngHeader[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
            binFile.write(reinterpret_cast<char*>(pngHeader), sizeof(pngHeader));
            binFile << "fake png data...";
        }
        
        std::vector<std::string> testFiles = {
            "test_files/test.html",
            "test_files/test.json",
            "test_files/test.xml",
            "test_files/test.txt",
            "test_files/test.bin"
        };
        
        std::cout << "Content-based MIME type detection:\n";
        std::cout << "File                | Extension-based      | Content-based\n";
        std::cout << "--------------------|----------------------|------------------\n";
        
        for (const auto& filename : testFiles) {
            // Extension-based detection
            auto [extMime, extCharset] = mimeTypes.guessType(filename);
            std::string extMimeStr = extMime ? *extMime : "unknown";
            
            // Content-based detection
            auto contentMime = mimeTypes.guessTypeByContent(filename);
            std::string contentMimeStr = contentMime ? *contentMime : "unknown";
            
            std::string displayName = std::filesystem::path(filename).filename().string();
            
            std::cout << std::left << std::setw(19) << displayName << " | "
                      << std::setw(20) << extMimeStr << " | "
                      << contentMimeStr << "\n";
        }
        
        // Cleanup
        std::filesystem::remove_all("test_files");
        
    } catch (const std::exception& e) {
        std::cerr << "Error in content-based detection: " << e.what() << "\n";
    }
}

void demonstrateCustomMimeTypes() {
    std::cout << "\n=== Custom MIME Types ===\n";
    
    try {
        MimeTypes mimeTypes({}, true);
        
        // Add custom MIME types
        std::cout << "Adding custom MIME types:\n";
        
        std::vector<std::pair<std::string, std::string>> customTypes = {
            {"application/x-custom-format", ".custom"},
            {"application/x-proprietary", ".prop"},
            {"text/x-special", ".special"},
            {"application/x-atom-config", ".atomcfg"}
        };
        
        for (const auto& [mimeType, extension] : customTypes) {
            mimeTypes.addType(mimeType, extension);
            std::cout << "  Added: " << mimeType << " -> " << extension << "\n";
        }
        
        // Test the custom types
        std::cout << "\nTesting custom MIME types:\n";
        std::vector<std::string> customFiles = {
            "data.custom",
            "config.prop",
            "document.special",
            "settings.atomcfg",
            "unknown.xyz"  // This should still be unknown
        };
        
        for (const auto& filename : customFiles) {
            auto [mimeType, charset] = mimeTypes.guessType(filename);
            std::string mimeStr = mimeType ? *mimeType : "unknown";
            
            std::cout << "  " << filename << " -> " << mimeStr << "\n";
        }
        
        // Test reverse lookup
        std::cout << "\nReverse lookup (MIME type to extension):\n";
        for (const auto& [mimeType, expectedExt] : customTypes) {
            auto extension = mimeTypes.guessExtension(mimeType);
            std::string extStr = extension ? *extension : "none";
            std::cout << "  " << mimeType << " -> " << extStr << "\n";
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error in custom MIME types: " << e.what() << "\n";
    }
}

void demonstrateKnownFilesFeature() {
    std::cout << "\n=== Known Files Feature ===\n";
    
    try {
        // Create some test files
        std::filesystem::create_directories("known_files");
        
        std::vector<std::string> knownFiles = {
            "known_files/document.pdf",
            "known_files/image.jpg",
            "known_files/data.json"
        };
        
        // Create the files
        for (const auto& filename : knownFiles) {
            std::ofstream file(filename);
            file << "test content for " << filename << "\n";
        }
        
        // Create MimeTypes with known files
        MimeTypes mimeTypesWithKnown(knownFiles, false);  // Non-lenient mode
        MimeTypes mimeTypesLenient({}, true);  // Lenient mode for comparison
        
        std::cout << "Testing known files vs unknown files:\n";
        std::cout << "File                | Known Files Mode | Lenient Mode\n";
        std::cout << "--------------------|------------------|------------------\n";
        
        std::vector<std::string> testFiles = {
            "known_files/document.pdf",  // Known
            "known_files/image.jpg",     // Known
            "known_files/data.json",     // Known
            "unknown_files/test.txt",    // Unknown
            "unknown_files/image.png"    // Unknown
        };
        
        for (const auto& filename : testFiles) {
            auto [knownMime, knownCharset] = mimeTypesWithKnown.guessType(filename);
            auto [lenientMime, lenientCharset] = mimeTypesLenient.guessType(filename);
            
            std::string knownStr = knownMime ? *knownMime : "unknown";
            std::string lenientStr = lenientMime ? *lenientMime : "unknown";
            
            std::string displayName = std::filesystem::path(filename).filename().string();
            
            std::cout << std::left << std::setw(19) << displayName << " | "
                      << std::setw(16) << knownStr << " | "
                      << lenientStr << "\n";
        }
        
        // Cleanup
        std::filesystem::remove_all("known_files");
        
    } catch (const std::exception& e) {
        std::cerr << "Error in known files feature: " << e.what() << "\n";
    }
}

void demonstrateAllTypesListing() {
    std::cout << "\n=== All Types Listing ===\n";
    
    try {
        MimeTypes mimeTypes({}, true);
        
        // Add a few custom types for demonstration
        mimeTypes.addType("application/x-demo", ".demo");
        mimeTypes.addType("text/x-example", ".example");
        
        std::cout << "Listing all known MIME types (first 20):\n";
        
        // Note: The actual listAllTypes() method might print to stdout
        // For this example, we'll demonstrate the concept
        std::cout << "Calling mimeTypes.listAllTypes()...\n";
        mimeTypes.listAllTypes();
        
        std::cout << "\nNote: The complete list has been printed above.\n";
        std::cout << "This includes both built-in and custom MIME types.\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error in all types listing: " << e.what() << "\n";
    }
}

int main(int argc, char** argv) {
    // Initialize logging
    loguru::init(argc, argv);
    loguru::add_file("minetype_comprehensive_example.log", loguru::Append, loguru::Verbosity_MAX);
    
    std::cout << "============================================\n";
    std::cout << "    ATOM MIME TYPES COMPREHENSIVE DEMO     \n";
    std::cout << "============================================\n";
    
    try {
        demonstrateBasicMimeTypeDetection();
        demonstrateURLMimeTypeDetection();
        demonstrateExtensionGuessing();
        demonstrateContentBasedDetection();
        demonstrateCustomMimeTypes();
        demonstrateKnownFilesFeature();
        demonstrateAllTypesListing();
        
        std::cout << "\n============================================\n";
        std::cout << "   MIME TYPES COMPREHENSIVE DEMO COMPLETED \n";
        std::cout << "============================================\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
