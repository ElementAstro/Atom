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

// Configuration options that can be loaded from a JSON file
struct OCRConfig {
    std::string language = "eng";
    bool enableDeskew = true;
    bool enablePerspectiveCorrection = true;
    bool enableNoiseRemoval = true;
    bool enableTextDetection = true;
    bool enableSpellCheck = false;
    bool enableSuperResolution = false;
    bool cacheResults = true;
    size_t maxThreads = std::thread::hardware_concurrency();

    // Preprocessing parameters
    struct PreprocessingParams {
        bool applyGaussianBlur = true;
        int gaussianKernelSize = 3;
        bool applyThreshold = true;
        bool useAdaptiveThreshold = true;
        int blockSize = 11;
        double constantC = 2;
        int medianBlurSize = 3;
        bool applyClahe = false;
        double clipLimit = 2.0;
        int binarizationMethod = 0;  // 0: Otsu, 1: Adaptive, 2: Sauvola
    } preprocessing;

    // Super resolution parameters
    struct SuperResolutionParams {
        std::string modelPath = "models/ESPCN_x4.pb";
        std::string modelName = "espcn";
        int scale = 4;
    } superResolution;

    // Text detection parameters
    struct TextDetectionParams {
        float confThreshold = 0.5f;
        float nmsThreshold = 0.4f;
        int detectionSize = 320;
        std::string modelPath = "models/east_text_detection.pb";
    } textDetection;

    // Cache parameters
    struct CacheParams {
        size_t maxCacheSize = 100 * 1024 * 1024;  // 100 MB
        std::string cacheDir = ".ocr_cache";
    } cache;

    // Load from JSON file
    static OCRConfig fromFile(const std::string& filename) {
        OCRConfig config;
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr
                << "Warning: Could not open config file. Using defaults.\n";
            return config;
        }

        // In a real implementation, use a JSON library like nlohmann/json
        // For brevity, I'm omitting the actual JSON parsing code

        return config;
    }
};

// Progress reporting class
class ProgressReporter {
private:
    std::string m_taskName;
    size_t m_total;
    std::atomic<size_t> m_current{0};
    std::chrono::time_point<std::chrono::steady_clock> m_startTime;
    mutable std::mutex m_mutex;

public:
    ProgressReporter(std::string taskName, size_t total)
        : m_taskName(std::move(taskName)),
          m_total(total),
          m_startTime(std::chrono::steady_clock::now()) {}

    void update(size_t increment = 1) {
        m_current += increment;
        reportProgress();
    }

    void setTotal(size_t total) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_total = total;
    }

    void reportProgress() const {
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
};

// Result caching system
class OCRCache {
private:
    std::unordered_map<std::string, std::string> m_memoryCache;
    std::string m_cacheDir;
    size_t m_maxCacheSize;
    std::mutex m_cacheMutex;

    // Calculate hash of image for caching
    std::string calculateHash(const cv::Mat& img) const {
        std::vector<uint8_t> buffer;
        cv::imencode(".jpg", img, buffer);

        // Using a simple hash function, in production use a stronger hash like
        // SHA-256
        size_t hash = 0;
        for (const auto& byte : buffer) {
            hash = (hash * 31) + byte;
        }

        return std::to_string(hash);
    }

    // Get cache file path
    fs::path getCacheFilePath(const std::string& key) const {
        return fs::path(m_cacheDir) / (key + ".txt");
    }

public:
    OCRCache(const std::string& cacheDir, size_t maxCacheSize)
        : m_cacheDir(cacheDir), m_maxCacheSize(maxCacheSize) {
        // Create cache directory if it doesn't exist
        if (!fs::exists(m_cacheDir)) {
            fs::create_directories(m_cacheDir);
        }
    }

    // Try to get cached result
    std::optional<std::string> get(const cv::Mat& img) {
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
                if (content.size() <
                    1024 * 10) {  // Only cache small results in memory
                    m_memoryCache[key] = content;
                }

                return content;
            }
        }

        return std::nullopt;
    }

    // Store result in cache
    void store(const cv::Mat& img, const std::string& result) {
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

    // Clean old cache entries if cache size exceeds the limit
    void cleanCacheIfNeeded() {
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

    // Clear the cache
    void clear() {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_memoryCache.clear();

        for (const auto& entry : fs::directory_iterator(m_cacheDir)) {
            if (entry.is_regular_file()) {
                fs::remove(entry.path());
            }
        }
    }
};

// Logger class
class Logger {
public:
    enum class Level { DEBUG, INFO, WARNING, ERROR };

private:
    Level m_level;
    std::ofstream m_file;
    std::mutex m_mutex;
    bool m_consoleOutput;

public:
    Logger(Level level = Level::INFO, const std::string& logFile = "",
           bool consoleOutput = true)
        : m_level(level), m_consoleOutput(consoleOutput) {
        if (!logFile.empty()) {
            m_file.open(logFile, std::ios_base::app);
        }
    }

    ~Logger() {
        if (m_file.is_open()) {
            m_file.close();
        }
    }

    void setLevel(Level level) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_level = level;
    }

    template <typename... Args>
    void log(Level level, std::format_string<Args...> fmt, Args&&... args) {
        if (level < m_level) {
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);

        std::string levelStr;
        switch (level) {
            case Level::DEBUG:
                levelStr = "DEBUG";
                break;
            case Level::INFO:
                levelStr = "INFO";
                break;
            case Level::WARNING:
                levelStr = "WARNING";
                break;
            case Level::ERROR:
                levelStr = "ERROR";
                break;
        }

        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        auto* tm_info = std::localtime(&now_time_t);

        char timeBuffer[26];
        strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", tm_info);

        std::string message = std::format(fmt, std::forward<Args>(args)...);
        std::string logEntry =
            std::format("[{}] [{}] {}", timeBuffer, levelStr, message);

        if (m_consoleOutput) {
            std::cout << logEntry << std::endl;
        }

        if (m_file.is_open()) {
            m_file << logEntry << std::endl;
            m_file.flush();
        }
    }

    // Convenience methods
    template <typename... Args>
    void debug(std::format_string<Args...> fmt, Args&&... args) {
        log(Level::DEBUG, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void info(std::format_string<Args...> fmt, Args&&... args) {
        log(Level::INFO, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void warning(std::format_string<Args...> fmt, Args&&... args) {
        log(Level::WARNING, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(std::format_string<Args...> fmt, Args&&... args) {
        log(Level::ERROR, fmt, std::forward<Args>(args)...);
    }
};

// Spell checker class
class SpellChecker {
private:
    std::unordered_map<std::string, int> m_dictionary;

    // Calculate edit distance between two words
    int levenshteinDistance(const std::string& s1, const std::string& s2) {
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

public:
    SpellChecker(const std::string& dictionaryPath = "") {
        if (!dictionaryPath.empty()) {
            loadDictionary(dictionaryPath);
        }
    }

    void loadDictionary(const std::string& filePath) {
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

    void addWord(const std::string& word) { m_dictionary[word]++; }

    bool isCorrect(const std::string& word) {
        return m_dictionary.count(word) > 0;
    }

    std::string suggest(const std::string& word) {
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

    std::string correctText(const std::string& text) {
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
};

// Enhanced OCR processor with additional features
class EnhancedOCRProcessor {
private:
    tesseract::TessBaseAPI m_tessApi;
    OCRConfig m_config;
    std::unique_ptr<OCRCache> m_cache;
    std::unique_ptr<Logger> m_logger;
    std::unique_ptr<SpellChecker> m_spellChecker;
    std::unique_ptr<cv::dnn_superres::DnnSuperResImpl> m_superRes;

    // For text detection
    cv::dnn::Net m_textDetector;

    // Language detection model (placeholder for a real implementation)
    bool detectLanguage(const cv::Mat& image, std::string& detectedLanguage) {
        // In a real implementation, this would use a language detection model
        // For simplicity, we'll assume English
        detectedLanguage = "eng";
        return true;
    }

    // Apply super resolution to enhance image quality
    cv::Mat applySuperResolution(const cv::Mat& image) {
        if (!m_superRes) {
            m_superRes = std::make_unique<cv::dnn_superres::DnnSuperResImpl>();
            try {
                m_superRes->readModel(m_config.superResolution.modelPath);
                m_superRes->setModel(m_config.superResolution.modelName,
                                     m_config.superResolution.scale);
            } catch (const cv::Exception& e) {
                m_logger->error("Failed to load super resolution model: {}",
                                e.what());
                return image;
            }
        }

        cv::Mat result;
        try {
            m_superRes->upsample(image, result);
            m_logger->debug("Applied super resolution, new size: {}x{}",
                            result.cols, result.rows);
            return result;
        } catch (const cv::Exception& e) {
            m_logger->error("Super resolution failed: {}", e.what());
            return image;
        }
    }

    // Perform deskewing to correct rotated text
    cv::Mat deskew(const cv::Mat& image) {
        cv::Mat gray;
        if (image.channels() > 1) {
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = image.clone();
        }

        // Threshold the image
        cv::Mat binary;
        cv::threshold(gray, binary, 0, 255,
                      cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

        // Find all contours
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(binary, contours, cv::RETR_LIST,
                         cv::CHAIN_APPROX_SIMPLE);

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
            m_logger->debug("No significant contours found for deskewing");
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

        m_logger->debug("Image deskewed by {} degrees", angle);

        return rotated;
    }

    // Detect text regions in the image
    std::vector<cv::Rect> detectTextRegions(const cv::Mat& image) {
        std::vector<cv::Rect> textBoxes;

        try {
            // Check if we need to load the model
            if (m_textDetector.empty()) {
                m_textDetector =
                    cv::dnn::readNet(m_config.textDetection.modelPath);
                if (m_textDetector.empty()) {
                    m_logger->error("Failed to load text detection model");
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

            m_logger->info("Detected {} text regions", textBoxes.size());

        } catch (const cv::Exception& e) {
            m_logger->error("Error in text detection: {}", e.what());
        }

        return textBoxes;
    }

    // Apply perspective correction to the image
    cv::Mat applyPerspectiveCorrection(const cv::Mat& image) {
        cv::Mat gray;
        if (image.channels() > 1) {
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = image.clone();
        }

        // Blur and threshold
        cv::Mat blurred;
        cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);
        cv::Mat thresh;
        cv::threshold(blurred, thresh, 0, 255,
                      cv::THRESH_BINARY_INV + cv::THRESH_OTSU);

        // Find contours
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(thresh, contours, cv::RETR_EXTERNAL,
                         cv::CHAIN_APPROX_SIMPLE);

        // Find the largest contour which should be the document
        double maxArea = 0;
        std::vector<cv::Point> docContour;
        for (const auto& contour : contours) {
            double area = cv::contourArea(contour);
            if (area > maxArea) {
                maxArea = area;
                docContour = contour;
            }
        }

        if (docContour.empty()) {
            m_logger->warning(
                "No document contour found for perspective correction");
            return image;
        }

        // Approximate the contour to get the corners
        cv::Mat hull;
        cv::convexHull(docContour, hull);
        std::vector<cv::Point> approx;
        cv::approxPolyDP(hull, approx, cv::arcLength(hull, true) * 0.02, true);

        // If we don't have exactly 4 corners, try to find them
        if (approx.size() != 4) {
            m_logger->warning(
                "Document does not have exactly 4 corners (found {}), using "
                "bounding rect",
                approx.size());
            cv::Rect boundRect = cv::boundingRect(docContour);
            approx = {cv::Point(boundRect.x, boundRect.y),
                      cv::Point(boundRect.x + boundRect.width, boundRect.y),
                      cv::Point(boundRect.x + boundRect.width,
                                boundRect.y + boundRect.height),
                      cv::Point(boundRect.x, boundRect.y + boundRect.height)};
        }

        // Sort corners: top-left, top-right, bottom-right, bottom-left
        std::sort(approx.begin(), approx.end(),
                  [](const cv::Point& a, const cv::Point& b) {
                      return a.x + a.y < b.x + b.y;
                  });

        if (approx[1].y > approx[2].y) {
            std::swap(approx[1], approx[2]);
        }

        // Convert to floating point
        std::vector<cv::Point2f> src = {
            cv::Point2f(static_cast<float>(approx[0].x),
                        static_cast<float>(approx[0].y)),
            cv::Point2f(static_cast<float>(approx[1].x),
                        static_cast<float>(approx[1].y)),
            cv::Point2f(static_cast<float>(approx[2].x),
                        static_cast<float>(approx[2].y)),
            cv::Point2f(static_cast<float>(approx[3].x),
                        static_cast<float>(approx[3].y))};

        // Compute width and height of the destination image
        float width =
            std::max(cv::norm(src[1] - src[0]), cv::norm(src[2] - src[3]));

        float height =
            std::max(cv::norm(src[3] - src[0]), cv::norm(src[2] - src[1]));

        // Define the destination image corners
        std::vector<cv::Point2f> dst = {
            cv::Point2f(0, 0), cv::Point2f(width - 1, 0),
            cv::Point2f(width - 1, height - 1), cv::Point2f(0, height - 1)};

        // Calculate perspective transform and apply it
        cv::Mat perspectiveMatrix = cv::getPerspectiveTransform(src, dst);
        cv::Mat warped;
        cv::warpPerspective(image, warped, perspectiveMatrix,
                            cv::Size(width, height));

        m_logger->debug("Applied perspective correction");

        return warped;
    }

    // Apply noise removal techniques
    cv::Mat removeNoise(const cv::Mat& image) {
        cv::Mat result;

        // Apply median blur
        cv::medianBlur(image, result, m_config.preprocessing.medianBlurSize);

        // Apply bilateral filter to preserve edges
        cv::bilateralFilter(result, result, 9, 75, 75);

        m_logger->debug("Applied noise removal");

        return result;
    }

    // Adaptive binarization using Sauvola's method
    cv::Mat sauvolaBinarization(const cv::Mat& grayImage, int windowSize = 21,
                                double k = 0.34) {
        cv::Mat mean, stddev, result;

        // Calculate local mean
        cv::boxFilter(grayImage, mean, CV_32F, cv::Size(windowSize, windowSize),
                      cv::Point(-1, -1), true, cv::BORDER_REFLECT);

        // Calculate local standard deviation
        cv::Mat meanSquare, graySquare;
        grayImage.convertTo(graySquare, CV_32F);
        graySquare = graySquare.mul(graySquare);

        cv::boxFilter(graySquare, meanSquare, CV_32F,
                      cv::Size(windowSize, windowSize), cv::Point(-1, -1), true,
                      cv::BORDER_REFLECT);

        cv::sqrt(meanSquare - mean.mul(mean), stddev);

        // Calculate threshold
        cv::Mat threshold = mean.mul(1.0 + k * ((stddev / 128.0) - 1.0));

        // Apply threshold
        cv::Mat binary;
        cv::compare(grayImage, threshold, binary, cv::CMP_GT);

        binary.convertTo(result, CV_8U, 255);

        return result;
    }

    // Enhanced preprocessing with various techniques
    cv::Mat enhancedPreprocess(const cv::Mat& inputImage) {
        cv::Mat processedImage;

        // Convert to grayscale if the image has multiple channels
        if (inputImage.channels() > 1) {
            cv::cvtColor(inputImage, processedImage, cv::COLOR_BGR2GRAY);
        } else {
            processedImage = inputImage.clone();
        }

        // Apply super resolution if enabled
        if (m_config.enableSuperResolution) {
            processedImage = applySuperResolution(processedImage);
        }

        // Apply perspective correction if enabled
        if (m_config.enablePerspectiveCorrection) {
            processedImage = applyPerspectiveCorrection(processedImage);
        }

        // Apply deskew if enabled
        if (m_config.enableDeskew) {
            processedImage = deskew(processedImage);
        }

        // Apply noise removal if enabled
        if (m_config.enableNoiseRemoval) {
            processedImage = removeNoise(processedImage);
        }

        // Apply CLAHE (Contrast Limited Adaptive Histogram Equalization) if
        // enabled
        if (m_config.preprocessing.applyClahe) {
            cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
            clahe->setClipLimit(m_config.preprocessing.clipLimit);
            clahe->apply(processedImage, processedImage);
        }

        // Apply Gaussian blur to reduce noise
        if (m_config.preprocessing.applyGaussianBlur) {
            cv::GaussianBlur(
                processedImage, processedImage,
                cv::Size(m_config.preprocessing.gaussianKernelSize,
                         m_config.preprocessing.gaussianKernelSize),
                0);
        }

        // Apply thresholding
        if (m_config.preprocessing.applyThreshold) {
            switch (m_config.preprocessing.binarizationMethod) {
                case 0:  // Otsu
                    cv::threshold(processedImage, processedImage, 0, 255,
                                  cv::THRESH_BINARY | cv::THRESH_OTSU);
                    break;
                case 1:  // Adaptive
                    cv::adaptiveThreshold(processedImage, processedImage, 255,
                                          cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                                          cv::THRESH_BINARY,
                                          m_config.preprocessing.blockSize,
                                          m_config.preprocessing.constantC);
                    break;
                case 2:  // Sauvola
                    processedImage = sauvolaBinarization(processedImage);
                    break;
            }
        }

        return processedImage;
    }

    // Calculate confidence score of OCR result
    float calculateConfidence(const tesseract::TessBaseAPI& api) {
        float confidence = api.MeanTextConf();
        return confidence / 100.0f;  // Normalize to [0, 1]
    }

    // Extract structured data from OCR results
    std::unordered_map<std::string, std::string> extractStructuredData(
        const std::string& text) {
        std::unordered_map<std::string, std::string> data;

        // Example patterns for various structured data
        // In a real application, these would be more sophisticated

        // Email
        std::regex emailRegex(
            R"(\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b)");
        std::smatch emailMatch;
        if (std::regex_search(text, emailMatch, emailRegex)) {
            data["email"] = emailMatch.str();
        }

        // Phone number (simple pattern)
        std::regex phoneRegex(
            R"(\b(\+\d{1,3}[ -]?)?$?\d{3}$?[ -]?\d{3}[ -]?\d{4}\b)");
        std::smatch phoneMatch;
        if (std::regex_search(text, phoneMatch, phoneRegex)) {
            data["phone"] = phoneMatch.str();
        }

        // Date (various formats)
        std::regex dateRegex(
            R"(\b(0[1-9]|[12][0-9]|3[01])[-/.](0[1-9]|1[012])[-/.](19|20)\d\d\b)");
        std::smatch dateMatch;
        if (std::regex_search(text, dateMatch, dateRegex)) {
            data["date"] = dateMatch.str();
        }

        // Address (simplified)
        std::regex addressRegex(
            R"(\b\d+\s+([A-Za-z]+\s+){1,5}(Street|St|Avenue|Ave|Road|Rd|Boulevard|Blvd)\.?\b)");
        std::smatch addressMatch;
        if (std::regex_search(text, addressMatch, addressRegex)) {
            data["address"] = addressMatch.str();
        }

        return data;
    }

    // Process text regions separately and combine results
    std::string processTextRegions(const cv::Mat& image) {
        if (!m_config.enableTextDetection) {
            return {};
        }

        std::vector<cv::Rect> textRegions = detectTextRegions(image);
        if (textRegions.empty()) {
            return {};
        }

        std::vector<std::string> regionTexts;
        regionTexts.reserve(textRegions.size());

        for (const auto& region : textRegions) {
            cv::Mat regionImage = image(region);
            cv::Mat processedRegion = enhancedPreprocess(regionImage);

            m_tessApi.SetImage(processedRegion.data, processedRegion.cols,
                               processedRegion.rows, processedRegion.channels(),
                               processedRegion.step);

            char* outText = m_tessApi.GetUTF8Text();
            if (outText && strlen(outText) > 0) {
                regionTexts.push_back(std::string(outText));
            }

            if (outText)
                tesseract::TessDeleteText(outText);
        }

        // Combine region texts
        return std::accumulate(regionTexts.begin(), regionTexts.end(),
                               std::string(),
                               [](const std::string& a, const std::string& b) {
                                   return a + (a.empty() ? "" : "\n") + b;
                               });
    }

    // A struct to hold OCR result with confidence and metadata
    struct OCRResult {
        std::string text;
        float confidence;
        std::unordered_map<std::string, std::string> structuredData;
        std::string language;

        OCRResult() : confidence(0.0f) {}

        OCRResult(std::string t, float conf,
                  const std::unordered_map<std::string, std::string>& data = {},
                  std::string lang = "eng")
            : text(std::move(t)),
              confidence(conf),
              structuredData(data),
              language(std::move(lang)) {}
    };

public:
    EnhancedOCRProcessor(const OCRConfig& config = OCRConfig())
        : m_config(config) {
        // Initialize logger
        m_logger =
            std::make_unique<Logger>(Logger::Level::INFO, "ocr_processing.log");

        // Initialize cache if enabled
        if (m_config.cacheResults) {
            m_cache = std::make_unique<OCRCache>(m_config.cache.cacheDir,
                                                 m_config.cache.maxCacheSize);
        }

        // Initialize spell checker if enabled
        if (m_config.enableSpellCheck) {
            m_spellChecker = std::make_unique<SpellChecker>("dict/english.txt");
        }

        m_logger->info("Initializing OCR with language: {}", m_config.language);

        // Initialize Tesseract
        if (m_tessApi.Init(nullptr, m_config.language.c_str()) != 0) {
            throw std::runtime_error(
                "Could not initialize Tesseract OCR engine");
        }

        // Set page segmentation mode to automatic
        m_tessApi.SetPageSegMode(tesseract::PSM_AUTO);

        m_logger->info("OCR engine initialized successfully");
    }

    ~EnhancedOCRProcessor() {
        m_tessApi.End();
        m_logger->info("OCR engine shutdown");
    }

    // Process single image and return enhanced result
    OCRResult processImage(const cv::Mat& image) {
        auto start = std::chrono::high_resolution_clock::now();

        // Try to get result from cache first
        if (m_cache) {
            auto cachedResult = m_cache->get(image);
            if (cachedResult) {
                m_logger->debug("Using cached OCR result");
                return OCRResult(
                    *cachedResult,
                    1.0f);  // Assuming cached results are high confidence
            }
        }

        // Detect language if not specified
        std::string detectedLanguage;
        if (detectLanguage(image, detectedLanguage)) {
            if (detectedLanguage != m_config.language) {
                m_logger->info("Detected language: {}, switching from {}",
                               detectedLanguage, m_config.language);

                // Reinitialize Tesseract with the detected language
                m_tessApi.End();
                if (m_tessApi.Init(nullptr, detectedLanguage.c_str()) != 0) {
                    m_logger->error(
                        "Failed to switch language to {}, falling back to {}",
                        detectedLanguage, m_config.language);
                    // Re-initialize with original language
                    m_tessApi.Init(nullptr, m_config.language.c_str());
                }
            }
        }

        // Process the image
        cv::Mat processedImage = enhancedPreprocess(image);

        // Set image data
        m_tessApi.SetImage(processedImage.data, processedImage.cols,
                           processedImage.rows, processedImage.channels(),
                           processedImage.step);

        // Run OCR
        char* outText = m_tessApi.GetUTF8Text();
        if (!outText) {
            m_logger->error("OCR failed to produce text");

            // Try processing text regions separately
            std::string regionText = processTextRegions(image);
            if (!regionText.empty()) {
                m_logger->info(
                    "Recovered text by processing regions separately");
                return OCRResult(regionText, 0.5f);
            }

            return OCRResult();
        }

        std::string result(outText);
        tesseract::TessDeleteText(outText);

        // Get confidence
        float confidence = calculateConfidence(m_tessApi);

        // Apply spell checking if enabled
        if (m_config.enableSpellCheck && m_spellChecker) {
            result = m_spellChecker->correctText(result);
            m_logger->debug("Applied spell checking");
        }

        // Extract structured data
        auto structuredData = extractStructuredData(result);

        // Cache the result if caching is enabled
        if (m_cache) {
            m_cache->store(image, result);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        m_logger->info("OCR processing took {} ms with confidence {:.2f}",
                       duration.count(), confidence);

        return OCRResult(result, confidence, structuredData, m_config.language);
    }

    // Process a batch of images in parallel
    std::vector<OCRResult> processBatchParallel(
        const std::vector<cv::Mat>& images) {
        std::vector<OCRResult> results(images.size());

        // Create progress reporter
        ProgressReporter progress("Batch OCR", images.size());

        // Calculate optimal chunk size for parallelization
        size_t chunkSize = 1;
        if (images.size() > 100) {
            chunkSize = std::min(16UL, images.size() / m_config.maxThreads);
        }

        // Initialize thread pool
        ThreadPool pool(m_config.maxThreads);

        // Process images in chunks
        for (size_t i = 0; i < images.size(); i += chunkSize) {
            size_t end = std::min(i + chunkSize, images.size());

            pool.enqueue([this, &images, &results, &progress, i, end]() {
                // Create a processor for this thread
                EnhancedOCRProcessor threadProcessor(m_config);

                for (size_t j = i; j < end; ++j) {
                    results[j] = threadProcessor.processImage(images[j]);
                    progress.update();
                }
            });
        }

        // Wait for all tasks to complete
        pool.waitAll();

        return results;
    }

    // Process a video file and extract text frames
    std::vector<std::pair<int, OCRResult>> processVideo(
        const std::string& videoPath, int frameInterval = 30) {
        std::vector<std::pair<int, OCRResult>> results;

        cv::VideoCapture cap(videoPath);
        if (!cap.isOpened()) {
            m_logger->error("Failed to open video file: {}", videoPath);
            return results;
        }

        int totalFrames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
        m_logger->info(
            "Processing video with {} frames, sampling every {} frames",
            totalFrames, frameInterval);

        ProgressReporter progress("Video OCR", totalFrames / frameInterval);

        cv::Mat frame;
        int frameCount = 0;

        while (cap.read(frame)) {
            if (frameCount % frameInterval == 0) {
                auto result = processImage(frame);
                if (!result.text.empty()) {
                    results.emplace_back(frameCount, result);
                }
                progress.update();
            }

            frameCount++;
        }

        cap.release();

        m_logger->info("Extracted text from {} frames in the video",
                       results.size());

        return results;
    }

    // Process a PDF document
    std::vector<OCRResult> processPDF(const std::string& pdfPath) {
        std::vector<OCRResult> results;

        m_logger->info("Processing PDF: {}", pdfPath);

        // This would typically use a PDF library like Poppler or PDFium to
        // extract images For this example, we'll just simulate the process
        m_logger->warning("PDF processing is a placeholder implementation");

        // Simulate processing 5 pages
        for (int i = 0; i < 5; ++i) {
            // In a real implementation, we would extract each page as an image
            // here
            cv::Mat dummyImage = cv::Mat::zeros(1000, 800, CV_8UC3);

            // Put some text on the image for demonstration
            cv::putText(
                dummyImage,
                "This is page " + std::to_string(i + 1) + " of a PDF document",
                cv::Point(50, 100), cv::FONT_HERSHEY_SIMPLEX, 1.0,
                cv::Scalar(255, 255, 255));

            auto result = processImage(dummyImage);
            results.push_back(result);
        }

        m_logger->info("Processed {} pages from PDF", results.size());

        return results;
    }

    // Export results to various formats
    bool exportResults(const std::vector<OCRResult>& results,
                       const std::string& outputPath,
                       const std::string& format = "txt") {
        try {
            if (format == "txt") {
                std::ofstream file(outputPath);
                if (!file.is_open()) {
                    m_logger->error("Failed to open output file: {}",
                                    outputPath);
                    return false;
                }

                for (const auto& result : results) {
                    file << result.text << "\n\n";
                }

            } else if (format == "json") {
                std::ofstream file(outputPath);
                if (!file.is_open()) {
                    m_logger->error("Failed to open output file: {}",
                                    outputPath);
                    return false;
                }

                // In a real implementation, use a JSON library
                file << "[\n";
                for (size_t i = 0; i < results.size(); ++i) {
                    const auto& result = results[i];

                    file << "  {\n";
                    file << "    \"text\": \"" << result.text << "\",\n";
                    file << "    \"confidence\": " << result.confidence
                         << ",\n";
                    file << "    \"language\": \"" << result.language
                         << "\",\n";

                    if (!result.structuredData.empty()) {
                        file << "    \"structuredData\": {\n";
                        size_t dataCount = 0;
                        for (const auto& [key, value] : result.structuredData) {
                            file << "      \"" << key << "\": \"" << value
                                 << "\"";

                            if (++dataCount < result.structuredData.size()) {
                                file << ",";
                            }
                            file << "\n";
                        }
                        file << "    }\n";
                    }

                    file << "  }";
                    if (i < results.size() - 1) {
                        file << ",";
                    }
                    file << "\n";
                }
                file << "]\n";

            } else if (format == "csv") {
                std::ofstream file(outputPath);
                if (!file.is_open()) {
                    m_logger->error("Failed to open output file: {}",
                                    outputPath);
                    return false;
                }

                // Write header
                file << "Text,Confidence,Language\n";

                // Write data
                for (const auto& result : results) {
                    // Escape commas and quotes in the text
                    std::string escapedText = result.text;
                    std::replace(escapedText.begin(), escapedText.end(), '"',
                                 '\'');

                    file << "\"" << escapedText << "\"," << result.confidence
                         << "," << result.language << "\n";
                }
            } else {
                m_logger->error("Unsupported export format: {}", format);
                return false;
            }

            m_logger->info("Exported {} results to {} in {} format",
                           results.size(), outputPath, format);
            return true;

        } catch (const std::exception& e) {
            m_logger->error("Failed to export results: {}", e.what());
            return false;
        }
    }

    // Clean up resources
    void cleanup() {
        if (m_cache) {
            m_cache->clear();
        }
    }

    // A simple thread pool implementation
    class ThreadPool {
    private:
        std::vector<std::thread> m_workers;
        std::queue<std::function<void()>> m_tasks;
        std::mutex m_mutex;
        std::condition_variable m_condition;
        std::atomic<bool> m_stop{false};
        std::atomic<size_t> m_activeWorkers{0};

    public:
        explicit ThreadPool(size_t threads) {
            m_workers.reserve(threads);

            for (size_t i = 0; i < threads; ++i) {
                m_workers.emplace_back([this] {
                    while (true) {
                        std::function<void()> task;

                        {
                            std::unique_lock<std::mutex> lock(m_mutex);
                            m_condition.wait(lock, [this] {
                                return m_stop || !m_tasks.empty();
                            });

                            if (m_stop && m_tasks.empty()) {
                                return;
                            }

                            task = std::move(m_tasks.front());
                            m_tasks.pop();
                        }

                        m_activeWorkers++;
                        task();
                        m_activeWorkers--;

                        m_condition.notify_all();
                    }
                });
            }
        }

        template <class F>
        void enqueue(F&& f) {
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_tasks.emplace(std::forward<F>(f));
            }

            m_condition.notify_one();
        }

        void waitAll() {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_condition.wait(lock, [this] {
                return m_tasks.empty() && m_activeWorkers == 0;
            });
        }

        ~ThreadPool() {
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_stop = true;
            }

            m_condition.notify_all();

            for (std::thread& worker : m_workers) {
                worker.join();
            }
        }
    };
};