#include "ocr_processor.hpp"

#include <algorithm>
#include <cmath>
#include <execution>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace fs = std::filesystem;

namespace atom::image::ocr {

OCRProcessor::OCRProcessor(const OCRConfig& config) : m_config(config) {
    // Initialize cache
    if (m_config.cacheResults) {
        m_cache = std::make_unique<OCRCache>(m_config.cache.cacheDir,
                                             m_config.cache.maxCacheSize);
    }

    // Initialize spell checker
    if (m_config.enableSpellCheck) {
        m_spellChecker = std::make_unique<SpellChecker>("dict/english.txt");
    }

    // Initialize Tesseract
    if (m_tessApi.Init(nullptr, m_config.language.c_str()) != 0) {
        throw std::runtime_error("Could not initialize Tesseract OCR engine");
    }
    m_tessApi.SetPageSegMode(tesseract::PSM_AUTO);
}

OCRProcessor::~OCRProcessor() { m_tessApi.End(); }

void OCRProcessor::setConfig(const OCRConfig& config) {
    m_config = config;

    // Reinitialize components if needed
    if (m_config.cacheResults && !m_cache) {
        m_cache = std::make_unique<OCRCache>(m_config.cache.cacheDir,
                                             m_config.cache.maxCacheSize);
    }
    if (m_config.enableSpellCheck && !m_spellChecker) {
        m_spellChecker = std::make_unique<SpellChecker>("dict/english.txt");
    }
}

void OCRProcessor::cleanup() {
    if (m_cache) {
        m_cache->clear();
    }
}

cv::Mat OCRProcessor::applySuperResolution(const cv::Mat& image) {
#if ATOM_OCR_HAS_SUPERRES
    if (!m_superResAvailable) {
        if (!fs::exists(m_config.superResolution.modelPath)) {
            std::cerr << "Super resolution model not found: "
                      << m_config.superResolution.modelPath << std::endl;
            return image;
        }

        m_superRes = std::make_unique<cv::dnn_superres::DnnSuperResImpl>();
        try {
            m_superRes->readModel(m_config.superResolution.modelPath);
            m_superRes->setModel(m_config.superResolution.modelName,
                                 m_config.superResolution.scale);
            m_superResAvailable = true;
        } catch (const cv::Exception& e) {
            std::cerr << "Failed to initialize super resolution: " << e.what()
                      << std::endl;
            return image;
        }
    }

    if (!m_superResAvailable || !m_superRes) {
        return image;
    }

    cv::Mat result;
    try {
        m_superRes->upsample(image, result);
        return result;
    } catch (const cv::Exception& e) {
        std::cerr << "Super resolution failed: " << e.what() << std::endl;
        return image;
    }
#else
    return image;
#endif
}

cv::Mat OCRProcessor::deskew(const cv::Mat& image) {
    cv::Mat gray;
    if (image.channels() > 1) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image.clone();
    }

    cv::Mat binary;
    cv::threshold(gray, binary, 0, 255,
                  cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

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

    cv::RotatedRect minRect = cv::minAreaRect(contours[maxAreaIdx]);
    float angle = minRect.angle;
    if (angle < -45)
        angle += 90;

    cv::Point2f center(image.cols / 2.0f, image.rows / 2.0f);
    cv::Mat rotationMatrix = cv::getRotationMatrix2D(center, angle, 1.0);
    cv::Mat rotated;
    cv::warpAffine(image, rotated, rotationMatrix, image.size(),
                   cv::INTER_CUBIC, cv::BORDER_REPLICATE);

    return rotated;
}

cv::Mat OCRProcessor::applyPerspectiveCorrection(const cv::Mat& image) {
    cv::Mat gray;
    if (image.channels() > 1) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image.clone();
    }

    cv::Mat binary;
    cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL,
                     cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) {
        return image;
    }

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

    std::vector<cv::Point> approx;
    double epsilon = 0.02 * cv::arcLength(contours[maxAreaIdx], true);
    cv::approxPolyDP(contours[maxAreaIdx], approx, epsilon, true);

    if (approx.size() == 4) {
        std::vector<cv::Point2f> srcPoints(4);
        for (int i = 0; i < 4; i++) {
            srcPoints[i] = approx[i];
        }

        float width = static_cast<float>(image.cols);
        float height = static_cast<float>(image.rows);
        std::vector<cv::Point2f> dstPoints = {
            cv::Point2f(0, 0), cv::Point2f(width - 1, 0),
            cv::Point2f(width - 1, height - 1), cv::Point2f(0, height - 1)};

        cv::Mat transform = cv::getPerspectiveTransform(srcPoints, dstPoints);
        cv::Mat corrected;
        cv::warpPerspective(image, corrected, transform, image.size());
        return corrected;
    }

    return image;
}

cv::Mat OCRProcessor::removeNoise(const cv::Mat& image) {
    cv::Mat denoised;
    cv::bilateralFilter(image, denoised, 9, 75, 75);
    return denoised;
}

cv::Mat OCRProcessor::sauvolaBinarization(const cv::Mat& grayImage,
                                          int windowSize, double k) {
    cv::Mat binary = cv::Mat::zeros(grayImage.size(), CV_8UC1);

    if (windowSize % 2 == 0)
        windowSize++;

    const int halfWindow = windowSize / 2;
    const double R = 128.0;

    cv::Mat integralSum, integralSqSum;
    cv::integral(grayImage, integralSum, integralSqSum, CV_64F, CV_64F);

    const int rows = grayImage.rows;
    const int cols = grayImage.cols;

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            int x1 = std::max(0, x - halfWindow);
            int y1 = std::max(0, y - halfWindow);
            int x2 = std::min(cols - 1, x + halfWindow);
            int y2 = std::min(rows - 1, y + halfWindow);

            int area = (x2 - x1 + 1) * (y2 - y1 + 1);

            double sum = integralSum.at<double>(y2 + 1, x2 + 1) -
                         integralSum.at<double>(y1, x2 + 1) -
                         integralSum.at<double>(y2 + 1, x1) +
                         integralSum.at<double>(y1, x1);

            double sqSum = integralSqSum.at<double>(y2 + 1, x2 + 1) -
                           integralSqSum.at<double>(y1, x2 + 1) -
                           integralSqSum.at<double>(y2 + 1, x1) +
                           integralSqSum.at<double>(y1, x1);

            double mean = sum / area;
            double variance = (sqSum / area) - (mean * mean);
            double stddev = std::sqrt(std::max(0.0, variance));

            double threshold = mean * (1.0 + k * (stddev / R - 1.0));

            binary.at<uint8_t>(y, x) =
                (grayImage.at<uint8_t>(y, x) > threshold) ? 255 : 0;
        }
    }

    return binary;
}

cv::Mat OCRProcessor::enhancedPreprocess(const cv::Mat& inputImage) {
    cv::Mat processed = inputImage.clone();

    if (m_config.enableSuperResolution) {
        processed = applySuperResolution(processed);
    }

    if (m_config.enableDeskew) {
        processed = deskew(processed);
    }

    if (m_config.enablePerspectiveCorrection) {
        processed = applyPerspectiveCorrection(processed);
    }

    if (m_config.enableNoiseRemoval) {
        processed = removeNoise(processed);
    }

    cv::Mat gray;
    if (processed.channels() > 1) {
        cv::cvtColor(processed, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = processed;
    }

    cv::Mat binary;
    if (m_config.preprocessing.binarizationMethod == 2) {
        binary = sauvolaBinarization(gray);
    } else if (m_config.preprocessing.binarizationMethod == 1) {
        cv::adaptiveThreshold(gray, binary, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                              cv::THRESH_BINARY,
                              m_config.preprocessing.blockSize,
                              m_config.preprocessing.constantC);
    } else {
        cv::threshold(gray, binary, 0, 255,
                      cv::THRESH_BINARY | cv::THRESH_OTSU);
    }

    return binary;
}

bool OCRProcessor::detectLanguage(const cv::Mat& image,
                                  std::string& detectedLanguage) {
    if (image.empty()) {
        detectedLanguage = m_config.language;
        return false;
    }

    try {
        tesseract::TessBaseAPI osdApi;
        if (osdApi.Init(nullptr, "osd") != 0) {
            detectedLanguage = m_config.language;
            return false;
        }

        osdApi.SetPageSegMode(tesseract::PSM_OSD_ONLY);

        cv::Mat gray;
        if (image.channels() > 1) {
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = image;
        }

        osdApi.SetImage(gray.data, gray.cols, gray.rows, 1, gray.step);

        int orientation;
        float confidence;
        const char* scriptName = nullptr;

        if (osdApi.DetectOrientationScript(&orientation, &confidence,
                                           &scriptName, nullptr)) {
            if (scriptName) {
                static const std::unordered_map<std::string, std::string>
                    scriptToLang = {{"Latin", "eng"},    {"Cyrillic", "rus"},
                                    {"Greek", "ell"},    {"Arabic", "ara"},
                                    {"Hebrew", "heb"},   {"Han", "chi_sim"},
                                    {"Japanese", "jpn"}, {"Korean", "kor"}};

                auto it = scriptToLang.find(scriptName);
                if (it != scriptToLang.end()) {
                    detectedLanguage = it->second;
                    osdApi.End();
                    return true;
                }
            }
        }

        osdApi.End();
    } catch (const std::exception& e) {
        std::cerr << "Language detection failed: " << e.what() << std::endl;
    }

    detectedLanguage = m_config.language;
    return false;
}

std::vector<cv::Rect> OCRProcessor::detectTextRegions(const cv::Mat& image) {
    std::vector<cv::Rect> textBoxes;

    try {
        if (m_textDetector.empty()) {
            m_textDetector = cv::dnn::readNet(m_config.textDetection.modelPath);
            if (m_textDetector.empty()) {
                return textBoxes;
            }
        }

        float height = static_cast<float>(image.rows);
        float width = static_cast<float>(image.cols);
        int detectionSize = m_config.textDetection.detectionSize;

        cv::Mat blob = cv::dnn::blobFromImage(
            image, 1.0, cv::Size(detectionSize, detectionSize),
            cv::Scalar(123.68, 116.78, 103.94), true, false);

        m_textDetector.setInput(blob);
        std::vector<std::string> outNames = {"feature_fusion/Conv_7/Sigmoid",
                                             "feature_fusion/concat_3"};

        std::vector<cv::Mat> outputBlobs;
        m_textDetector.forward(outputBlobs, outNames);

        cv::Mat scores = outputBlobs[0];
        cv::Mat geometry = outputBlobs[1];

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
                if (score < m_config.textDetection.confThreshold) {
                    continue;
                }

                float offsetX = x * 4.0f;
                float offsetY = y * 4.0f;
                float angle = anglesData[x];
                float cosA = std::cos(angle);
                float sinA = std::sin(angle);

                float h = x0Data[x] + x2Data[x];
                float w = x1Data[x] + x3Data[x];

                cv::Point2f offset(
                    offsetX + (cosA * x1Data[x] + sinA * x2Data[x]),
                    offsetY - (sinA * x1Data[x] - cosA * x2Data[x]));
                cv::Point2f p1 = cv::Point2f(-sinA * h, -cosA * h) + offset;
                cv::Point2f p3 = cv::Point2f(-cosA * w, sinA * w) + offset;
                cv::RotatedRect r(0.5f * (p1 + p3), cv::Size2f(w, h),
                                  -angle * 180.0f / static_cast<float>(CV_PI));

                detections.push_back(r);
                confidences.push_back(score);
            }
        }

        std::vector<int> indices;
        cv::dnn::NMSBoxes(detections, confidences,
                          m_config.textDetection.confThreshold,
                          m_config.textDetection.nmsThreshold, indices);

        float rW = width / static_cast<float>(detectionSize);
        float rH = height / static_cast<float>(detectionSize);

        for (size_t i = 0; i < indices.size(); ++i) {
            cv::RotatedRect& box = detections[indices[i]];
            cv::Point2f vertices[4];
            box.points(vertices);

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

            int margin = 10;
            cv::Rect rect(std::max(0, static_cast<int>(minX) - margin),
                          std::max(0, static_cast<int>(minY) - margin),
                          std::min(static_cast<int>(width) - 1,
                                   static_cast<int>(maxX - minX) + 2 * margin),
                          std::min(static_cast<int>(height) - 1,
                                   static_cast<int>(maxY - minY) + 2 * margin));

            textBoxes.push_back(rect);
        }

    } catch (const cv::Exception& e) {
        // Text detection failed
    }

    return textBoxes;
}

std::string OCRProcessor::processTextRegions(const cv::Mat& image) {
    std::vector<cv::Rect> regions = detectTextRegions(image);

    if (regions.empty()) {
        m_tessApi.SetImage(image.data, image.cols, image.rows, image.channels(),
                           image.step);
        char* text = m_tessApi.GetUTF8Text();
        std::string result(text ? text : "");
        delete[] text;
        return result;
    }

    std::string combinedText;
    for (const auto& region : regions) {
        cv::Mat roi = image(region);
        m_tessApi.SetImage(roi.data, roi.cols, roi.rows, roi.channels(),
                           roi.step);
        char* text = m_tessApi.GetUTF8Text();
        if (text) {
            combinedText += text;
            combinedText += "\n";
            delete[] text;
        }
    }

    return combinedText;
}

float OCRProcessor::calculateConfidence(tesseract::TessBaseAPI& api) {
    int* confidences = api.AllWordConfidences();
    if (!confidences) {
        return 0.0f;
    }

    float sum = 0.0f;
    int count = 0;
    for (int i = 0; confidences[i] >= 0; i++) {
        sum += confidences[i];
        count++;
    }

    delete[] confidences;
    return count > 0 ? sum / count : 0.0f;
}

std::unordered_map<std::string, std::string>
OCRProcessor::extractStructuredData(const std::string& text) {
    std::unordered_map<std::string, std::string> data;

    // Email pattern
    size_t atPos = text.find('@');
    if (atPos != std::string::npos) {
        size_t start = text.rfind(' ', atPos);
        size_t end = text.find(' ', atPos);
        if (start == std::string::npos)
            start = 0;
        if (end == std::string::npos)
            end = text.length();
        data["email"] = text.substr(start, end - start);
    }

    // Phone number pattern
    for (size_t i = 0; i < text.length(); i++) {
        if (std::isdigit(text[i])) {
            std::string number;
            while (i < text.length() && (std::isdigit(text[i]) ||
                                         text[i] == '-' || text[i] == ' ')) {
                if (std::isdigit(text[i])) {
                    number += text[i];
                }
                i++;
            }
            if (number.length() >= 10) {
                data["phone"] = number;
                break;
            }
        }
    }

    return data;
}

OCRResult OCRProcessor::processImage(const cv::Mat& image) {
    OCRResult result;

    if (image.empty()) {
        return result;
    }

    // Check cache
    if (m_cache) {
        auto cached = m_cache->get(image);
        if (cached.has_value()) {
            result.text = cached.value();
            result.confidence = 100.0f;
            result.language = m_config.language;
            return result;
        }
    }

    result.language = m_config.language;

    // Preprocess
    cv::Mat preprocessed = enhancedPreprocess(image);

    // OCR
    if (m_config.enableTextDetection) {
        result.text = processTextRegions(preprocessed);
    } else {
        m_tessApi.SetImage(preprocessed.data, preprocessed.cols,
                           preprocessed.rows, preprocessed.channels(),
                           preprocessed.step);
        char* text = m_tessApi.GetUTF8Text();
        result.text = text ? text : "";
        delete[] text;
    }

    result.confidence = calculateConfidence(m_tessApi);

    // Spell check
    if (m_spellChecker && m_config.enableSpellCheck) {
        result.text = m_spellChecker->correctText(result.text);
    }

    result.structuredData = extractStructuredData(result.text);

    // Cache
    if (m_cache) {
        m_cache->store(image, result.text);
    }

    return result;
}

std::vector<OCRResult> OCRProcessor::processBatchParallel(
    const std::vector<cv::Mat>& images) {
    std::vector<OCRResult> results(images.size());

    std::transform(
        std::execution::par, images.begin(), images.end(), results.begin(),
        [this](const cv::Mat& img) { return this->processImage(img); });

    return results;
}

std::vector<std::pair<int, OCRResult>> OCRProcessor::processVideo(
    const std::string& videoPath, int frameInterval) {
    std::vector<std::pair<int, OCRResult>> results;

    cv::VideoCapture cap(videoPath);
    if (!cap.isOpened()) {
        return results;
    }

    int frameNumber = 0;
    cv::Mat frame;

    while (cap.read(frame)) {
        if (frameNumber % frameInterval == 0) {
            OCRResult result = processImage(frame);
            results.emplace_back(frameNumber, result);
        }
        frameNumber++;
    }

    return results;
}

std::vector<OCRResult> OCRProcessor::processPDF(const std::string& pdfPath) {
    std::vector<OCRResult> results;
    // PDF processing requires external library (e.g., Poppler)
    (void)pdfPath;
    return results;
}

bool OCRProcessor::exportResults(const std::vector<OCRResult>& results,
                                 const std::string& outputPath,
                                 const std::string& format) {
    std::ofstream outFile(outputPath);
    if (!outFile.is_open()) {
        return false;
    }

    if (format == "txt") {
        for (const auto& result : results) {
            outFile << result.text << "\n\n";
        }
    } else if (format == "json") {
        outFile << "[\n";
        for (size_t i = 0; i < results.size(); i++) {
            outFile << "  {\n";
            outFile << "    \"text\": \"" << results[i].text << "\",\n";
            outFile << "    \"confidence\": " << results[i].confidence << ",\n";
            outFile << "    \"language\": \"" << results[i].language << "\"\n";
            outFile << "  }";
            if (i < results.size() - 1) {
                outFile << ",";
            }
            outFile << "\n";
        }
        outFile << "]\n";
    } else if (format == "csv") {
        outFile << "Text,Confidence,Language\n";
        for (const auto& result : results) {
            outFile << "\"" << result.text << "\"," << result.confidence << ","
                    << result.language << "\n";
        }
    }

    return true;
}

std::unique_ptr<OCRProcessor> createOCRProcessor() {
    return std::make_unique<OCRProcessor>();
}

std::unique_ptr<OCRProcessor> createOCRProcessor(
    const std::string& configPath) {
    OCRConfig config = OCRConfig::fromFile(configPath);
    return std::make_unique<OCRProcessor>(config);
}

}  // namespace atom::image::ocr
