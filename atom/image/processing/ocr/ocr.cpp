#include "ocr.hpp"

#ifdef ATOM_IMAGE_HAS_OCR

#include <algorithm>
#include <atomic>
#include <chrono>
#include <execution>
#include <filesystem>
#include <format>
#include <fstream>
#include <future>
#include <iostream>
#include <memory_resource>
#include <mutex>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

#include <opencv2/dnn.hpp>
#include <opencv2/dnn_superres.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/text.hpp>

#include <leptonica/allheaders.h>
#include <tesseract/baseapi.h>

namespace fs = std::filesystem;
namespace views = std::ranges::views;

// Implementation of OCRConfig::fromFile
OCRConfig OCRConfig::fromFile(const std::string& filename) {
    OCRConfig config;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open config file. Using defaults.\n";
        return config;
    }

    // In a real implementation, use a JSON library like nlohmann/json
    // For brevity, I'm omitting the actual JSON parsing code

    return config;
}

// ProgressReporter implementation
ProgressReporter::ProgressReporter(std::string taskName, size_t total)
    : m_taskName(std::move(taskName)),
      m_total(total),
      m_startTime(std::chrono::steady_clock::now()) {}

void ProgressReporter::update(size_t increment) {
    m_current += increment;
    reportProgress();
}

void ProgressReporter::setTotal(size_t total) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_total = total;
}

void ProgressReporter::reportProgress() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto now = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime)
            .count();

    if (m_total > 0) {
        float percentage = static_cast<float>(m_current) * 100.0f / m_total;

        // Calculate ETA
        std::string eta = "N/A";
        if (m_current > 0 && elapsed > 0) {
            float itemsPerSecond = static_cast<float>(m_current) / elapsed;
            if (itemsPerSecond > 0) {
                int etaSeconds = static_cast<int>((m_total - m_current) /
                                                  itemsPerSecond);
                eta = std::format("{}m {}s", etaSeconds / 60,
                                  etaSeconds % 60);
            }
        }

        std::cout << std::format(
            "\r{}: {:.1f}% ({}/{}) - Elapsed: {}s - ETA: {}", m_taskName,
            percentage, m_current, m_total, elapsed, eta);
        std::cout.flush();

        if (m_current >= m_total) {
            std::cout << std::endl;
        }
    }
}

#ifdef ATOM_IMAGE_HAS_OCR
// OCRCache implementation
std::string OCRCache::calculateHash(const cv::Mat& img) const {
    std::vector<uint8_t> buffer;
    cv::imencode(".jpg", img, buffer);

    // Using a simple hash function, in production use a stronger hash like SHA-256
    size_t hash = 0;
    for (const auto& byte : buffer) {
        hash = (hash * 31) + byte;
    }

    return std::to_string(hash);
}

fs::path OCRCache::getCacheFilePath(const std::string& key) const {
    return fs::path(m_cacheDir) / (key + ".txt");
}

OCRCache::OCRCache(const std::string& cacheDir, size_t maxCacheSize)
    : m_cacheDir(cacheDir), m_maxCacheSize(maxCacheSize) {
    // Create cache directory if it doesn't exist
    if (!fs::exists(m_cacheDir)) {
        fs::create_directories(m_cacheDir);
    }
}

std::optional<std::string> OCRCache::get(const cv::Mat& img) {
    std::string key = calculateHash(img);

    std::lock_guard<std::mutex> lock(m_cacheMutex);

    // Check memory cache first
    auto memIter = m_memoryCache.find(key);
    if (memIter != m_memoryCache.end()) {
        return memIter->second;
    }

    // Check file cache
    fs::path cachePath = getCacheFilePath(key);
    if (fs::exists(cachePath)) {
        std::ifstream file(cachePath);
        if (file) {
            std::string content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());

            // Update memory cache
            if (content.size() < 1024 * 10) {  // Only cache small results in memory
                m_memoryCache[key] = content;
            }

            return content;
        }
    }

    return std::nullopt;
}

void OCRCache::store(const cv::Mat& img, const std::string& result) {
    std::string key = calculateHash(img);

    std::lock_guard<std::mutex> lock(m_cacheMutex);

    // Update memory cache
    if (result.size() < 1024 * 10) {  // Only cache small results in memory
        m_memoryCache[key] = result;
    }

    // Update file cache
    fs::path cachePath = getCacheFilePath(key);
    std::ofstream file(cachePath);
    if (file) {
        file << result;
    }

    // Clean cache if needed
    cleanCacheIfNeeded();
}

void OCRCache::cleanCacheIfNeeded() {
    // Check total cache size
    size_t totalSize = 0;
    std::vector<std::pair<fs::path, std::filesystem::file_time_type>> files;

    for (const auto& entry : fs::directory_iterator(m_cacheDir)) {
        if (entry.is_regular_file()) {
            totalSize += entry.file_size();
            files.emplace_back(entry.path(), entry.last_write_time());
        }
    }

    // If cache is too large, remove oldest files
    if (totalSize > m_maxCacheSize) {
        // Sort by last write time (oldest first)
        std::sort(files.begin(), files.end(),
                  [](const auto& a, const auto& b) {
                      return a.second < b.second;
                  });

        // Remove oldest files until we're under the limit
        for (const auto& [path, time] : files) {
            if (totalSize <= m_maxCacheSize * 0.8) {
                break;
            }

            totalSize -= fs::file_size(path);
            fs::remove(path);
        }
    }
}

void OCRCache::clear() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_memoryCache.clear();

    for (const auto& entry : fs::directory_iterator(m_cacheDir)) {
        if (entry.is_regular_file()) {
            fs::remove(entry.path());
        }
    }
}

// SpellChecker implementation
SpellChecker::SpellChecker(const std::string& dictionaryPath) {
    if (!dictionaryPath.empty()) {
        loadDictionary(dictionaryPath);
    }
}

void SpellChecker::loadDictionary(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open dictionary file");
    }

    std::string word;
    while (std::getline(file, word)) {
        // Remove trailing newline if present
        if (!word.empty() && word.back() == '\n') {
            word.pop_back();
        }
        // Store word in dictionary with a count of 1
        m_dictionary[word] = 1;
    }
}

void SpellChecker::addWord(const std::string& word) {
    m_dictionary[word]++;
}

bool SpellChecker::isCorrect(const std::string& word) {
    return m_dictionary.count(word) > 0;
}

int SpellChecker::levenshteinDistance(const std::string& s1, const std::string& s2) {
    const std::size_t len1 = s1.size(), len2 = s2.size();
    std::vector<std::vector<int>> d(len1 + 1, std::vector<int>(len2 + 1));

    for (int i = 0; i <= len1; ++i)
        d[i][0] = i;
    for (int j = 0; j <= len2; ++j)
        d[0][j] = j;

    for (int i = 1; i <= len1; ++i) {
        for (int j = 1; j <= len2; ++j) {
            d[i][j] = std::min(
                {d[i - 1][j] + 1, d[i][j - 1] + 1,
                 d[i - 1][j - 1] + (s1[i - 1] == s2[j - 1] ? 0 : 1)});
        }
    }

    return d[len1][len2];
}

std::string SpellChecker::suggest(const std::string& word) {
    if (isCorrect(word)) {
        return word;
    }

    std::string bestMatch = word;
    int minDistance = std::numeric_limits<int>::max();

    // Look for words with edit distance <= 2
    for (const auto& [dictWord, _] : m_dictionary) {
        // Only consider words with similar length
        if (std::abs(static_cast<int>(dictWord.size()) -
                     static_cast<int>(word.size())) > 2) {
            continue;
        }

        int distance = levenshteinDistance(word, dictWord);
        if (distance < minDistance) {
            minDistance = distance;
            bestMatch = dictWord;

            // If distance is 1, it's probably the correct word
            if (distance == 1) {
                break;
            }
        }
    }

    // Only return suggestion if the edit distance is reasonable
    if (minDistance <= 2) {
        return bestMatch;
    } else {
        return word;  // Return original if no good match
    }
}

std::string SpellChecker::correctText(const std::string& text) {
    std::stringstream ss(text);
    std::string word;
    std::stringstream result;

    while (ss >> word) {
        // Remove punctuation for checking
        std::string cleanWord = word;
        cleanWord.erase(
            std::remove_if(cleanWord.begin(), cleanWord.end(),
                           [](unsigned char c) { return std::ispunct(c); }),
            cleanWord.end());

        if (!cleanWord.empty()) {
            std::string corrected = suggest(cleanWord);

            // If the word had punctuation, preserve it
            if (cleanWord != word) {
                for (size_t i = 0, j = 0; i < word.size(); ++i) {
                    if (std::ispunct(word[i])) {
                        result << word[i];
                    } else if (j < corrected.size()) {
                        result << corrected[j++];
                    }
                }
            } else {
                result << corrected;
            }
        } else {
            result << word;  // Preserve original if it's just punctuation
        }

        result << " ";
    }

    return result.str();
}

// EnhancedOCRProcessor implementation
EnhancedOCRProcessor::EnhancedOCRProcessor(const OCRConfig& config)
    : m_config(config) {
    // Initialize cache if enabled
    if (m_config.cacheResults) {
        m_cache = std::make_unique<OCRCache>(m_config.cache.cacheDir,
                                             m_config.cache.maxCacheSize);
    }

    // Initialize spell checker if enabled
    if (m_config.enableSpellCheck) {
        m_spellChecker = std::make_unique<SpellChecker>("dict/english.txt");
    }

    // Initialize Tesseract
    if (m_tessApi.Init(nullptr, m_config.language.c_str()) != 0) {
        throw std::runtime_error("Could not initialize Tesseract OCR engine");
    }

    // Set page segmentation mode to automatic
    m_tessApi.SetPageSegMode(tesseract::PSM_AUTO);
}

EnhancedOCRProcessor::~EnhancedOCRProcessor() {
    m_tessApi.End();
}

bool EnhancedOCRProcessor::detectLanguage(const cv::Mat& image, std::string& detectedLanguage) {
    // In a real implementation, this would use a language detection model
    // For simplicity, we'll assume English
    detectedLanguage = "eng";
    return true;
}

cv::Mat EnhancedOCRProcessor::applySuperResolution(const cv::Mat& image) {
    if (!m_superRes) {
        m_superRes = std::make_unique<cv::dnn_superres::DnnSuperResImpl>();
        try {
            m_superRes->readModel(m_config.superResolution.modelPath);
            m_superRes->setModel(m_config.superResolution.modelName,
                                 m_config.superResolution.scale);
        } catch (const cv::Exception& e) {
            return image;
        }
    }

    cv::Mat result;
    try {
        m_superRes->upsample(image, result);
        return result;
    } catch (const cv::Exception& e) {
        return image;
    }
}

cv::Mat EnhancedOCRProcessor::deskew(const cv::Mat& image) {
    cv::Mat gray;
    if (image.channels() > 1) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image.clone();
    }

    // Threshold the image
    cv::Mat binary;
    cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    // Find all contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    // Find the largest contour
    double maxArea = 0;
    int maxAreaIdx = -1;
    for (size_t i = 0; i < contours.size(); i++) {
        double area = cv::contourArea(contours[i]);
        if (area > maxArea) {
            maxArea = area;
            maxAreaIdx = static_cast<int>(i);
        }
    }

    if (maxAreaIdx < 0) {
        return image;
    }

    // Find minimum area rectangle
    cv::RotatedRect minRect = cv::minAreaRect(contours[maxAreaIdx]);

    // Determine angle
    float angle = minRect.angle;

    // Adjust angle
    if (angle < -45)
        angle += 90;

    // Rotate the image to deskew it
    cv::Point2f center(image.cols / 2.0f, image.rows / 2.0f);
    cv::Mat rotationMatrix = cv::getRotationMatrix2D(center, angle, 1.0);
    cv::Mat rotated;
    cv::warpAffine(image, rotated, rotationMatrix, image.size(),
                   cv::INTER_CUBIC, cv::BORDER_REPLICATE);

    return rotated;
}

std::vector<cv::Rect> EnhancedOCRProcessor::detectTextRegions(const cv::Mat& image) {
    std::vector<cv::Rect> textBoxes;

    try {
        // Check if we need to load the model
        if (m_textDetector.empty()) {
            m_textDetector = cv::dnn::readNet(m_config.textDetection.modelPath);
            if (m_textDetector.empty()) {
                return textBoxes;
            }
        }

        // Get image dimensions
        float height = static_cast<float>(image.rows);
        float width = static_cast<float>(image.cols);

        // Create a blob from the image
        int detectionSize = m_config.textDetection.detectionSize;
        cv::Mat blob = cv::dnn::blobFromImage(
            image, 1.0, cv::Size(detectionSize, detectionSize),
            cv::Scalar(123.68, 116.78, 103.94), true, false);

        // Set the blob as input and get output layer names
        m_textDetector.setInput(blob);
        std::vector<std::string> outNames = {
            "feature_fusion/Conv_7/Sigmoid", "feature_fusion/concat_3"};

        // Forward pass
        std::vector<cv::Mat> outputBlobs;
        m_textDetector.forward(outputBlobs, outNames);

        // Get scores and geometry
        cv::Mat scores = outputBlobs[0];
        cv::Mat geometry = outputBlobs[1];

        // Decode predictions
        std::vector<cv::RotatedRect> detections;
        std::vector<float> confidences;

        for (int y = 0; y < scores.size[2]; ++y) {
            float* scoresData = scores.ptr<float>(0, 0, y);
            float* x0Data = geometry.ptr<float>(0, 0, y);
            float* x1Data = geometry.ptr<float>(0, 1, y);
            float* x2Data = geometry.ptr<float>(0, 2, y);
            float* x3Data = geometry.ptr<float>(0, 3, y);
            float* anglesData = geometry.ptr<float>(0, 4, y);

            for (int x = 0; x < scores.size[3]; ++x) {
                float score = scoresData[x];

                // Filter weak detections
                if (score < m_config.textDetection.confThreshold) {
                    continue;
                }

                // Compute rotated bounding box
                float offsetX = x * 4.0f;
                float offsetY = y * 4.0f;
                float angle = anglesData[x];
                float cos = std::cos(angle);
                float sin = std::sin(angle);

                float h = x0Data[x] + x2Data[x];
                float w = x1Data[x] + x3Data[x];

                cv::Point2f offset(
                    offsetX + (cos * x1Data[x] + sin * x2Data[x]),
                    offsetY - (sin * x1Data[x] - cos * x2Data[x]));
                cv::Point2f p1 = cv::Point2f(-sin * h, -cos * h) + offset;
                cv::Point2f p3 = cv::Point2f(-cos * w, sin * w) + offset;
                cv::RotatedRect r(
                    0.5f * (p1 + p3), cv::Size2f(w, h),
                    -angle * 180.0f / static_cast<float>(CV_PI));

                detections.push_back(r);
                confidences.push_back(score);
            }
        }

        // Apply non-maximum suppression
        std::vector<int> indices;
        cv::dnn::NMSBoxes(detections, confidences,
                          m_config.textDetection.confThreshold,
                          m_config.textDetection.nmsThreshold, indices);

        // Scale boxes back to original size
        float rW = width / static_cast<float>(detectionSize);
        float rH = height / static_cast<float>(detectionSize);

        for (size_t i = 0; i < indices.size(); ++i) {
            cv::RotatedRect& box = detections[indices[i]];
            cv::Point2f vertices[4];
            box.points(vertices);

            // Convert rotated rect to axis-aligned bounding box and scale
            float minX = std::numeric_limits<float>::max();
            float maxX = 0;
            float minY = std::numeric_limits<float>::max();
            float maxY = 0;

            for (int j = 0; j < 4; ++j) {
                vertices[j].x *= rW;
                vertices[j].y *= rH;

                minX = std::min(minX, vertices[j].x);
                maxX = std::max(maxX, vertices[j].x);
                minY = std::min(minY, vertices[j].y);
                maxY = std::max(maxY, vertices[j].y);
            }

            // Create axis-aligned bounding box with margins
            int margin = 10;
            cv::Rect rect(
                std::max(0, static_cast<int>(minX) - margin),
                std::max(0, static_cast<int>(minY) - margin),
                std::min(static_cast<int>(width) - 1,
                         static_cast<int>(maxX - minX) + 2 * margin),
                std::min(static_cast<int>(height) - 1,
                         static_cast<int>(maxY - minY) + 2 * margin));

            textBoxes.push_back(rect);
        }

    } catch (const cv::Exception& e) {
        // Error in text detection
    }

    return textBoxes;
}

#endif // ATOM_IMAGE_HAS_OCR
