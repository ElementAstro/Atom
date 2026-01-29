#include "ocr_config.hpp"

#include <fstream>
#include <iostream>
#include <string>

// Feature detection for JSON support
#ifndef ATOM_OCR_HAS_JSON
#if __has_include(<nlohmann/json.hpp>)
#define ATOM_OCR_HAS_JSON 1
#else
#define ATOM_OCR_HAS_JSON 0
#endif
#endif

#if ATOM_OCR_HAS_JSON
#include <nlohmann/json.hpp>
#endif

namespace atom::image::ocr {

OCRConfig OCRConfig::fromFile(const std::string& filename) {
    OCRConfig config;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open config file '" << filename
                  << "'. Using defaults.\n";
        return config;
    }

#if ATOM_OCR_HAS_JSON
    try {
        nlohmann::json j;
        file >> j;

        // Parse top-level settings
        if (j.contains("language"))
            config.language = j["language"].get<std::string>();
        if (j.contains("enableDeskew"))
            config.enableDeskew = j["enableDeskew"].get<bool>();
        if (j.contains("enablePerspectiveCorrection"))
            config.enablePerspectiveCorrection =
                j["enablePerspectiveCorrection"].get<bool>();
        if (j.contains("enableNoiseRemoval"))
            config.enableNoiseRemoval = j["enableNoiseRemoval"].get<bool>();
        if (j.contains("enableTextDetection"))
            config.enableTextDetection = j["enableTextDetection"].get<bool>();
        if (j.contains("enableSpellCheck"))
            config.enableSpellCheck = j["enableSpellCheck"].get<bool>();
        if (j.contains("enableSuperResolution"))
            config.enableSuperResolution =
                j["enableSuperResolution"].get<bool>();
        if (j.contains("cacheResults"))
            config.cacheResults = j["cacheResults"].get<bool>();
        if (j.contains("maxThreads"))
            config.maxThreads = j["maxThreads"].get<size_t>();

        // Parse preprocessing settings
        if (j.contains("preprocessing")) {
            auto& pp = j["preprocessing"];
            if (pp.contains("applyGaussianBlur"))
                config.preprocessing.applyGaussianBlur =
                    pp["applyGaussianBlur"].get<bool>();
            if (pp.contains("gaussianKernelSize"))
                config.preprocessing.gaussianKernelSize =
                    pp["gaussianKernelSize"].get<int>();
            if (pp.contains("applyThreshold"))
                config.preprocessing.applyThreshold =
                    pp["applyThreshold"].get<bool>();
            if (pp.contains("useAdaptiveThreshold"))
                config.preprocessing.useAdaptiveThreshold =
                    pp["useAdaptiveThreshold"].get<bool>();
            if (pp.contains("blockSize"))
                config.preprocessing.blockSize = pp["blockSize"].get<int>();
            if (pp.contains("constantC"))
                config.preprocessing.constantC = pp["constantC"].get<double>();
            if (pp.contains("medianBlurSize"))
                config.preprocessing.medianBlurSize =
                    pp["medianBlurSize"].get<int>();
            if (pp.contains("applyClahe"))
                config.preprocessing.applyClahe = pp["applyClahe"].get<bool>();
            if (pp.contains("clipLimit"))
                config.preprocessing.clipLimit = pp["clipLimit"].get<double>();
            if (pp.contains("binarizationMethod"))
                config.preprocessing.binarizationMethod =
                    pp["binarizationMethod"].get<int>();
        }

        // Parse super resolution settings
        if (j.contains("superResolution")) {
            auto& sr = j["superResolution"];
            if (sr.contains("modelPath"))
                config.superResolution.modelPath =
                    sr["modelPath"].get<std::string>();
            if (sr.contains("modelName"))
                config.superResolution.modelName =
                    sr["modelName"].get<std::string>();
            if (sr.contains("scale"))
                config.superResolution.scale = sr["scale"].get<int>();
        }

        // Parse text detection settings
        if (j.contains("textDetection")) {
            auto& td = j["textDetection"];
            if (td.contains("confThreshold"))
                config.textDetection.confThreshold =
                    td["confThreshold"].get<float>();
            if (td.contains("nmsThreshold"))
                config.textDetection.nmsThreshold =
                    td["nmsThreshold"].get<float>();
            if (td.contains("detectionSize"))
                config.textDetection.detectionSize =
                    td["detectionSize"].get<int>();
            if (td.contains("modelPath"))
                config.textDetection.modelPath =
                    td["modelPath"].get<std::string>();
        }

        // Parse cache settings
        if (j.contains("cache")) {
            auto& cache = j["cache"];
            if (cache.contains("maxCacheSize"))
                config.cache.maxCacheSize = cache["maxCacheSize"].get<size_t>();
            if (cache.contains("cacheDir"))
                config.cache.cacheDir = cache["cacheDir"].get<std::string>();
        }

    } catch (const nlohmann::json::exception& e) {
        std::cerr << "Error parsing config file: " << e.what()
                  << ". Using defaults.\n";
    }
#else
    // Simple key-value parsing without JSON library
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#' || line[0] == '/')
            continue;

        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);

            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t\""));
            key.erase(key.find_last_not_of(" \t\",") + 1);
            value.erase(0, value.find_first_not_of(" \t\""));
            value.erase(value.find_last_not_of(" \t\",") + 1);

            if (key == "language")
                config.language = value;
            else if (key == "enableDeskew")
                config.enableDeskew = (value == "true");
            else if (key == "enableSpellCheck")
                config.enableSpellCheck = (value == "true");
            else if (key == "enableSuperResolution")
                config.enableSuperResolution = (value == "true");
            else if (key == "cacheResults")
                config.cacheResults = (value == "true");
        }
    }
    std::cerr << "Note: Full JSON config parsing requires nlohmann-json.\n";
#endif

    return config;
}

}  // namespace atom::image::ocr
