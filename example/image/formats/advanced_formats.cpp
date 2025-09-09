/*
 * advanced_formats.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file advanced_formats.cpp
 * @brief Advanced image format handling example
 * 
 * This example demonstrates:
 * - Multi-format image support
 * - Format conversion
 * - Metadata preservation
 * - Compression options
 * - Format-specific features
 */

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <filesystem>
#include <fstream>

using namespace std;
namespace fs = std::filesystem;

/**
 * @brief Image format information
 */
struct FormatInfo {
    string name;
    string extension;
    bool supports_compression;
    bool supports_transparency;
    bool supports_animation;
    bool supports_metadata;
    vector<string> color_spaces;
    
    FormatInfo(const string& n, const string& ext, bool comp, bool trans, bool anim, bool meta)
        : name(n), extension(ext), supports_compression(comp), supports_transparency(trans),
          supports_animation(anim), supports_metadata(meta) {}
};

/**
 * @brief Format registry
 */
class FormatRegistry {
private:
    map<string, FormatInfo> formats_;
    
public:
    FormatRegistry() {
        // Register common formats
        registerFormat("JPEG", ".jpg", true, false, false, true);
        registerFormat("PNG", ".png", true, true, false, true);
        registerFormat("TIFF", ".tiff", true, true, false, true);
        registerFormat("BMP", ".bmp", false, false, false, false);
        registerFormat("GIF", ".gif", true, true, true, false);
        registerFormat("WebP", ".webp", true, true, true, true);
        registerFormat("HEIF", ".heif", true, true, false, true);
        registerFormat("AVIF", ".avif", true, true, false, true);
        registerFormat("JXL", ".jxl", true, true, true, true);
        
        // Add color space support
        formats_["JPEG"].color_spaces = {"RGB", "YCbCr", "CMYK", "Grayscale"};
        formats_["PNG"].color_spaces = {"RGB", "RGBA", "Grayscale", "Palette"};
        formats_["TIFF"].color_spaces = {"RGB", "RGBA", "CMYK", "LAB", "Grayscale"};
        formats_["WebP"].color_spaces = {"RGB", "RGBA"};
        formats_["HEIF"].color_spaces = {"RGB", "RGBA", "YUV"};
        formats_["AVIF"].color_spaces = {"RGB", "RGBA", "YUV"};
        formats_["JXL"].color_spaces = {"RGB", "RGBA", "XYB", "Grayscale"};
    }
    
    void registerFormat(const string& name, const string& ext, bool comp, bool trans, bool anim, bool meta) {
        formats_.emplace(name, FormatInfo(name, ext, comp, trans, anim, meta));
    }
    
    const FormatInfo* getFormat(const string& name) const {
        auto it = formats_.find(name);
        return (it != formats_.end()) ? &it->second : nullptr;
    }
    
    vector<string> getSupportedFormats() const {
        vector<string> names;
        for (const auto& pair : formats_) {
            names.push_back(pair.first);
        }
        return names;
    }
    
    void printFormatInfo() const {
        cout << "\n=== Supported Image Formats ===" << endl;
        for (const auto& pair : formats_) {
            const auto& format = pair.second;
            cout << format.name << " (" << format.extension << "):" << endl;
            cout << "  Compression: " << (format.supports_compression ? "Yes" : "No") << endl;
            cout << "  Transparency: " << (format.supports_transparency ? "Yes" : "No") << endl;
            cout << "  Animation: " << (format.supports_animation ? "Yes" : "No") << endl;
            cout << "  Metadata: " << (format.supports_metadata ? "Yes" : "No") << endl;
            cout << "  Color spaces: ";
            for (const auto& cs : format.color_spaces) {
                cout << cs << " ";
            }
            cout << endl << endl;
        }
    }
};

/**
 * @brief Format converter
 */
class FormatConverter {
private:
    FormatRegistry& registry_;
    
public:
    explicit FormatConverter(FormatRegistry& registry) : registry_(registry) {}
    
    /**
     * @brief Convert between formats
     */
    bool convertFormat(const string& input_path, const string& output_path, 
                      const string& target_format, const map<string, string>& options = {}) {
        cout << "Converting " << input_path << " to " << target_format << " format..." << endl;
        
        // Get source format from extension
        string source_ext = fs::path(input_path).extension().string();
        string source_format = detectFormatFromExtension(source_ext);
        
        if (source_format.empty()) {
            cout << "  Error: Unknown source format for extension " << source_ext << endl;
            return false;
        }
        
        const FormatInfo* source_info = registry_.getFormat(source_format);
        const FormatInfo* target_info = registry_.getFormat(target_format);
        
        if (!source_info || !target_info) {
            cout << "  Error: Unsupported format" << endl;
            return false;
        }
        
        cout << "  Source: " << source_info->name << " -> Target: " << target_info->name << endl;
        
        // Check compatibility
        checkCompatibility(*source_info, *target_info);
        
        // Apply conversion options
        applyConversionOptions(*target_info, options);
        
        // Simulate conversion
        cout << "  Conversion completed successfully" << endl;
        return true;
    }
    
    /**
     * @brief Batch convert multiple files
     */
    void batchConvert(const vector<string>& input_paths, const string& target_format, 
                     const string& output_dir, const map<string, string>& options = {}) {
        cout << "\nBatch converting " << input_paths.size() << " files to " << target_format << "..." << endl;
        
        int successful = 0;
        int failed = 0;
        
        for (const auto& input_path : input_paths) {
            string filename = fs::path(input_path).stem().string();
            const FormatInfo* target_info = registry_.getFormat(target_format);
            string output_path = output_dir + "/" + filename + target_info->extension;
            
            if (convertFormat(input_path, output_path, target_format, options)) {
                successful++;
            } else {
                failed++;
            }
        }
        
        cout << "Batch conversion completed: " << successful << " successful, " << failed << " failed" << endl;
    }
    
private:
    string detectFormatFromExtension(const string& ext) {
        for (const auto& pair : registry_.getSupportedFormats()) {
            const FormatInfo* info = registry_.getFormat(pair);
            if (info && info->extension == ext) {
                return pair;
            }
        }
        return "";
    }
    
    void checkCompatibility(const FormatInfo& source, const FormatInfo& target) {
        cout << "  Checking compatibility..." << endl;
        
        if (source.supports_transparency && !target.supports_transparency) {
            cout << "    Warning: Target format doesn't support transparency" << endl;
        }
        
        if (source.supports_animation && !target.supports_animation) {
            cout << "    Warning: Target format doesn't support animation" << endl;
        }
        
        if (source.supports_metadata && !target.supports_metadata) {
            cout << "    Warning: Metadata may be lost in conversion" << endl;
        }
        
        // Check color space compatibility
        bool color_space_compatible = false;
        for (const auto& source_cs : source.color_spaces) {
            for (const auto& target_cs : target.color_spaces) {
                if (source_cs == target_cs) {
                    color_space_compatible = true;
                    break;
                }
            }
            if (color_space_compatible) break;
        }
        
        if (!color_space_compatible) {
            cout << "    Warning: Color space conversion may be required" << endl;
        }
    }
    
    void applyConversionOptions(const FormatInfo& target, const map<string, string>& options) {
        cout << "  Applying conversion options..." << endl;
        
        for (const auto& option : options) {
            cout << "    " << option.first << ": " << option.second << endl;
            
            if (option.first == "quality" && target.supports_compression) {
                int quality = stoi(option.second);
                if (quality < 1 || quality > 100) {
                    cout << "      Warning: Quality should be between 1-100" << endl;
                }
            }
            
            if (option.first == "compression" && !target.supports_compression) {
                cout << "      Warning: Target format doesn't support compression" << endl;
            }
            
            if (option.first == "preserve_transparency" && !target.supports_transparency) {
                cout << "      Warning: Target format doesn't support transparency" << endl;
            }
        }
    }
};

/**
 * @brief Format analyzer
 */
class FormatAnalyzer {
public:
    /**
     * @brief Analyze format characteristics
     */
    static void analyzeFormat(const string& format_name, FormatRegistry& registry) {
        cout << "\nAnalyzing " << format_name << " format characteristics..." << endl;
        
        const FormatInfo* info = registry.getFormat(format_name);
        if (!info) {
            cout << "  Error: Unknown format" << endl;
            return;
        }
        
        cout << "  Format: " << info->name << endl;
        cout << "  Extension: " << info->extension << endl;
        
        // Analyze capabilities
        cout << "  Capabilities:" << endl;
        cout << "    Compression: " << (info->supports_compression ? "Supported" : "Not supported") << endl;
        cout << "    Transparency: " << (info->supports_transparency ? "Supported" : "Not supported") << endl;
        cout << "    Animation: " << (info->supports_animation ? "Supported" : "Not supported") << endl;
        cout << "    Metadata: " << (info->supports_metadata ? "Supported" : "Not supported") << endl;
        
        // Analyze use cases
        cout << "  Recommended use cases:" << endl;
        if (info->supports_compression && !info->supports_transparency) {
            cout << "    - Photography (lossy compression)" << endl;
        }
        if (info->supports_transparency) {
            cout << "    - Graphics with transparency" << endl;
            cout << "    - Web graphics" << endl;
        }
        if (info->supports_animation) {
            cout << "    - Animated graphics" << endl;
        }
        if (info->supports_metadata) {
            cout << "    - Professional photography" << endl;
            cout << "    - Archival storage" << endl;
        }
        
        // Performance characteristics (simulated)
        cout << "  Performance characteristics:" << endl;
        cout << "    Encoding speed: " << getEncodingSpeed(*info) << endl;
        cout << "    Decoding speed: " << getDecodingSpeed(*info) << endl;
        cout << "    Compression ratio: " << getCompressionRatio(*info) << endl;
    }
    
private:
    static string getEncodingSpeed(const FormatInfo& info) {
        if (info.name == "BMP") return "Very Fast";
        if (info.name == "JPEG") return "Fast";
        if (info.name == "PNG") return "Medium";
        if (info.name == "TIFF") return "Medium";
        if (info.name == "WebP") return "Medium";
        if (info.name == "HEIF") return "Slow";
        if (info.name == "AVIF") return "Very Slow";
        if (info.name == "JXL") return "Slow";
        return "Unknown";
    }
    
    static string getDecodingSpeed(const FormatInfo& info) {
        if (info.name == "BMP") return "Very Fast";
        if (info.name == "JPEG") return "Very Fast";
        if (info.name == "PNG") return "Fast";
        if (info.name == "TIFF") return "Fast";
        if (info.name == "WebP") return "Fast";
        if (info.name == "HEIF") return "Medium";
        if (info.name == "AVIF") return "Medium";
        if (info.name == "JXL") return "Fast";
        return "Unknown";
    }
    
    static string getCompressionRatio(const FormatInfo& info) {
        if (!info.supports_compression) return "None";
        if (info.name == "JPEG") return "High (lossy)";
        if (info.name == "PNG") return "Medium (lossless)";
        if (info.name == "WebP") return "High (lossy/lossless)";
        if (info.name == "HEIF") return "Very High (lossy)";
        if (info.name == "AVIF") return "Very High (lossy)";
        if (info.name == "JXL") return "Excellent (lossy/lossless)";
        return "Variable";
    }
};

/**
 * @brief Demonstrate advanced format handling
 */
void demonstrateAdvancedFormats() {
    cout << "=== Advanced Image Format Handling Demo ===" << endl;
    
    // 1. Initialize format registry
    FormatRegistry registry;
    registry.printFormatInfo();
    
    // 2. Format analysis
    cout << "\n=== Format Analysis ===" << endl;
    FormatAnalyzer::analyzeFormat("JPEG", registry);
    FormatAnalyzer::analyzeFormat("PNG", registry);
    FormatAnalyzer::analyzeFormat("WebP", registry);
    FormatAnalyzer::analyzeFormat("AVIF", registry);
    
    // 3. Format conversion
    cout << "\n=== Format Conversion ===" << endl;
    FormatConverter converter(registry);
    
    // Single file conversion
    map<string, string> jpeg_options = {
        {"quality", "85"},
        {"progressive", "true"},
        {"optimize", "true"}
    };
    converter.convertFormat("sample.png", "sample.jpg", "JPEG", jpeg_options);
    
    map<string, string> webp_options = {
        {"quality", "90"},
        {"lossless", "false"},
        {"preserve_transparency", "true"}
    };
    converter.convertFormat("sample.png", "sample.webp", "WebP", webp_options);
    
    // Batch conversion
    vector<string> batch_files = {"image1.jpg", "image2.png", "image3.bmp"};
    map<string, string> avif_options = {
        {"quality", "75"},
        {"speed", "6"}
    };
    converter.batchConvert(batch_files, "AVIF", "output", avif_options);
}

/**
 * @brief Main function
 */
int main() {
    try {
        cout << "Advanced Image Format Handling Example" << endl;
        cout << "=====================================" << endl;
        
        demonstrateAdvancedFormats();
        
        cout << "\nAdvanced format handling demonstration completed!" << endl;
        return 0;
        
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}
