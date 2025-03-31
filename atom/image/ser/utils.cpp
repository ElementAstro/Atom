// utils.cpp
#include "utils.h"
#include <algorithm>
#include <fstream>
#include <numeric>
#include <opencv2/imgproc.hpp>


namespace serastro {
namespace utils {

// Bit depth conversion
cv::Mat convertBitDepth(const cv::Mat& src, int targetDepth, bool normalize) {
    if (src.depth() == targetDepth) {
        return src.clone();
    }

    cv::Mat result;
    double scale = 1.0;
    double offset = 0.0;

    if (normalize) {
        // Calculate scale based on bit depth
        if (src.depth() == CV_8U && targetDepth == CV_16U) {
            scale = 65535.0 / 255.0;
        } else if (src.depth() == CV_16U && targetDepth == CV_8U) {
            scale = 255.0 / 65535.0;
        } else if (src.depth() == CV_8U && targetDepth == CV_32F) {
            scale = 1.0 / 255.0;
        } else if (src.depth() == CV_16U && targetDepth == CV_32F) {
            scale = 1.0 / 65535.0;
        } else if (src.depth() == CV_32F && targetDepth == CV_8U) {
            scale = 255.0;
        } else if (src.depth() == CV_32F && targetDepth == CV_16U) {
            scale = 65535.0;
        }
    }

    src.convertTo(result, CV_MAKETYPE(targetDepth, src.channels()), scale,
                  offset);
    return result;
}

// Color conversion
cv::Mat convertToGrayscale(const cv::Mat& src) {
    if (src.channels() == 1) {
        return src.clone();
    }

    cv::Mat result;
    cv::cvtColor(src, result, cv::COLOR_BGR2GRAY);
    return result;
}

cv::Mat convertToBGR(const cv::Mat& src) {
    if (src.channels() == 3) {
        return src.clone();
    }

    cv::Mat result;
    if (src.channels() == 1) {
        cv::cvtColor(src, result, cv::COLOR_GRAY2BGR);
    } else if (src.channels() == 4) {
        cv::cvtColor(src, result, cv::COLOR_BGRA2BGR);
    } else {
        throw InvalidParameterException(
            "Unsupported channel count for BGR conversion");
    }

    return result;
}

cv::Mat convertToRGB(const cv::Mat& src) {
    if (src.channels() != 3) {
        return convertToBGR(src);
    }

    cv::Mat result;
    cv::cvtColor(src, result, cv::COLOR_BGR2RGB);
    return result;
}

// Normalization
cv::Mat normalize(const cv::Mat& src, double alpha, double beta) {
    cv::Mat result;
    cv::normalize(src, result, alpha, beta, cv::NORM_MINMAX);
    return result;
}

cv::Mat normalizeMinMax(const cv::Mat& src) { return normalize(src, 0.0, 1.0); }

cv::Mat normalizePercentile(const cv::Mat& src, double lowPercentile,
                            double highPercentile) {
    // Convert to single-channel float for processing
    cv::Mat gray;
    if (src.channels() > 1) {
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = src;
    }

    cv::Mat floatImg;
    if (gray.depth() != CV_32F) {
        gray.convertTo(floatImg, CV_32F);
    } else {
        floatImg = gray;
    }

    // Calculate histogram
    const int histSize = 1000;
    float range[] = {0.0f, 1.0f};
    const float* histRange = {range};
    cv::Mat hist;
    cv::calcHist(&floatImg, 1, 0, cv::Mat(), hist, 1, &histSize, &histRange);

    // Calculate cumulative histogram
    std::vector<float> cumHist(histSize);
    float sum = 0.0f;
    for (int i = 0; i < histSize; ++i) {
        sum += hist.at<float>(i);
        cumHist[i] = sum;
    }

    // Normalize cumulative histogram
    float totalPixels = floatImg.total();
    for (int i = 0; i < histSize; ++i) {
        cumHist[i] /= totalPixels;
    }

    // Find low and high percentile values
    float lowVal = 0.0f;
    float highVal = 1.0f;

    for (int i = 0; i < histSize; ++i) {
        if (cumHist[i] >= lowPercentile / 100.0f) {
            lowVal = range[0] + (range[1] - range[0]) * i / histSize;
            break;
        }
    }

    for (int i = 0; i < histSize; ++i) {
        if (cumHist[i] >= highPercentile / 100.0f) {
            highVal = range[0] + (range[1] - range[0]) * i / histSize;
            break;
        }
    }

    // Apply normalization
    cv::Mat result;
    if (src.channels() == 1) {
        // Single channel normalization
        result = (src - lowVal) / (highVal - lowVal);
        result = cv::max(0.0, cv::min(1.0, result));
    } else {
        // Multi-channel normalization (maintain color balance)
        cv::Mat channels[3];
        cv::split(src, channels);

        for (int i = 0; i < 3; ++i) {
            channels[i] = (channels[i] - lowVal) / (highVal - lowVal);
            channels[i] = cv::max(0.0, cv::min(1.0, channels[i]));
        }

        cv::merge(channels, 3, result);
    }

    return result;
}

// File utilities
std::vector<std::filesystem::path> findSerFiles(
    const std::filesystem::path& directory, bool recursive) {
    std::vector<std::filesystem::path> serFiles;

    if (!std::filesystem::exists(directory) ||
        !std::filesystem::is_directory(directory)) {
        return serFiles;
    }

    if (recursive) {
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".ser") {
                serFiles.push_back(entry.path());
            }
        }
    } else {
        for (const auto& entry :
             std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".ser") {
                serFiles.push_back(entry.path());
            }
        }
    }

    return serFiles;
}

std::optional<size_t> estimateFrameCount(const std::filesystem::path& serFile) {
    try {
        // Read header
        SERHeader header = readSerHeader(serFile);

        // Return frame count from header
        return header.frameCount;
    } catch (...) {
        return std::nullopt;
    }
}

bool isValidSerFile(const std::filesystem::path& serFile) {
    try {
        SERHeader header = readSerHeader(serFile);
        return header.isValid();
    } catch (...) {
        return false;
    }
}

// SER header utilities
SERHeader readSerHeader(const std::filesystem::path& serFile) {
    std::ifstream file(serFile, std::ios::binary);
    if (!file) {
        throw SERIOException(
            std::format("Failed to open SER file: {}", serFile.string()));
    }

    SERHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(SERHeader));

    if (!file) {
        throw SERFormatException(std::format(
            "Failed to read SER header from: {}", serFile.string()));
    }

    return header;
}

std::string serColorIdToString(SERColorID colorId) {
    switch (colorId) {
        case SERColorID::Mono:
            return "Mono";
        case SERColorID::BayerRGGB:
            return "Bayer RGGB";
        case SERColorID::BayerGRBG:
            return "Bayer GRBG";
        case SERColorID::BayerGBRG:
            return "Bayer GBRG";
        case SERColorID::BayerBGGR:
            return "Bayer BGGR";
        case SERColorID::RGB:
            return "RGB";
        case SERColorID::BGR:
            return "BGR";
        // Continuing utils.cpp from where it left off
        default:
            return "Unknown";
    }
}

bool writeSerHeader(const std::filesystem::path& serFile,
                    const SERHeader& header) {
    std::ofstream file(serFile, std::ios::binary | std::ios::in);
    if (!file) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(&header), sizeof(SERHeader));
    return file.good();
}

// Mathematical utilities
std::vector<double> calculateHistogram(const cv::Mat& image, int bins) {
    // Ensure single-channel image
    cv::Mat gray;
    if (image.channels() > 1) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image;
    }

    // Convert to 8-bit for histogram
    cv::Mat img8bit;
    if (gray.depth() != CV_8U) {
        gray.convertTo(img8bit, CV_8U, 255.0);
    } else {
        img8bit = gray;
    }

    // Calculate histogram
    float range[] = {0, 256};
    const float* histRange = {range};
    cv::Mat hist;
    cv::calcHist(&img8bit, 1, 0, cv::Mat(), hist, 1, &bins, &histRange);

    // Convert to vector
    std::vector<double> histValues(bins);
    for (int i = 0; i < bins; ++i) {
        histValues[i] = hist.at<float>(i);
    }

    return histValues;
}

double calculatePSNR(const cv::Mat& reference, const cv::Mat& target) {
    // Ensure both images have the same type and size
    if (reference.size() != target.size() ||
        reference.type() != target.type()) {
        throw InvalidParameterException(
            "Reference and target images must have the same size and type");
    }

    // Convert to float
    cv::Mat refFloat, targetFloat;
    if (reference.depth() != CV_32F) {
        reference.convertTo(refFloat, CV_32F);
        target.convertTo(targetFloat, CV_32F);
    } else {
        refFloat = reference;
        targetFloat = target;
    }

    // Calculate MSE
    cv::Mat diff;
    cv::absdiff(refFloat, targetFloat, diff);
    diff = diff.mul(diff);

    double mse = cv::mean(diff)[0];
    if (mse <= 1e-10) {
        return 100.0;  // Images are identical
    }

    // Calculate PSNR
    double maxVal = 1.0;
    if (reference.depth() == CV_8U) {
        maxVal = 255.0;
    } else if (reference.depth() == CV_16U) {
        maxVal = 65535.0;
    }

    double psnr = 10.0 * log10((maxVal * maxVal) / mse);
    return psnr;
}

double calculateSSIM(const cv::Mat& reference, const cv::Mat& target) {
    const double C1 = 6.5025, C2 = 58.5225;  // Constants for stability

    // Ensure both images have the same type and size
    if (reference.size() != target.size() ||
        reference.type() != target.type()) {
        throw InvalidParameterException(
            "Reference and target images must have the same size and type");
    }

    // Convert to float
    cv::Mat I1, I2;
    reference.convertTo(I1, CV_32F);
    target.convertTo(I2, CV_32F);

    cv::Mat I1_sq = I1.mul(I1);
    cv::Mat I2_sq = I2.mul(I2);
    cv::Mat I1_I2 = I1.mul(I2);

    // Compute means
    cv::Mat mu1, mu2;
    cv::GaussianBlur(I1, mu1, cv::Size(11, 11), 1.5);
    cv::GaussianBlur(I2, mu2, cv::Size(11, 11), 1.5);

    cv::Mat mu1_sq = mu1.mul(mu1);
    cv::Mat mu2_sq = mu2.mul(mu2);
    cv::Mat mu1_mu2 = mu1.mul(mu2);

    // Compute variances and covariance
    cv::Mat sigma1_sq, sigma2_sq, sigma12;
    cv::GaussianBlur(I1_sq, sigma1_sq, cv::Size(11, 11), 1.5);
    sigma1_sq -= mu1_sq;

    cv::GaussianBlur(I2_sq, sigma2_sq, cv::Size(11, 11), 1.5);
    sigma2_sq -= mu2_sq;

    cv::GaussianBlur(I1_I2, sigma12, cv::Size(11, 11), 1.5);
    sigma12 -= mu1_mu2;

    // Calculate SSIM
    cv::Mat ssim_map;
    cv::Mat numerator = (2 * mu1_mu2 + C1).mul(2 * sigma12 + C2);
    cv::Mat denominator =
        (mu1_sq + mu2_sq + C1).mul(sigma1_sq + sigma2_sq + C2);
    cv::divide(numerator, denominator, ssim_map);

    return cv::mean(ssim_map)[0];
}

// Image statistics
ImageStatistics calculateImageStatistics(const cv::Mat& image) {
    // Ensure single-channel image
    cv::Mat gray;
    if (image.channels() > 1) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image;
    }

    // Convert to float
    cv::Mat floatImg;
    if (gray.depth() != CV_32F) {
        gray.convertTo(floatImg, CV_32F);
    } else {
        floatImg = gray;
    }

    // Calculate mean and stddev
    cv::Scalar mean, stdDev;
    cv::meanStdDev(floatImg, mean, stdDev);

    // Calculate min/max
    double minVal, maxVal;
    cv::minMaxLoc(floatImg, &minVal, &maxVal);

    // Calculate median and percentiles
    std::vector<float> values;
    values.reserve(floatImg.total());

    for (int y = 0; y < floatImg.rows; ++y) {
        for (int x = 0; x < floatImg.cols; ++x) {
            values.push_back(floatImg.at<float>(y, x));
        }
    }

    std::sort(values.begin(), values.end());

    float median = values[values.size() / 2];
    float p05 = values[static_cast<size_t>(values.size() * 0.05)];
    float p95 = values[static_cast<size_t>(values.size() * 0.95)];

    ImageStatistics stats;
    stats.mean = mean[0];
    stats.stdDev = stdDev[0];
    stats.min = minVal;
    stats.max = maxVal;
    stats.median = median;
    stats.percentile05 = p05;
    stats.percentile95 = p95;

    return stats;
}

// Hot/cold pixel detection
std::vector<cv::Point> detectHotPixels(const cv::Mat& image, double threshold) {
    // Convert to float and grayscale if needed
    cv::Mat gray;
    if (image.channels() > 1) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image;
    }

    cv::Mat floatImg;
    if (gray.depth() != CV_32F) {
        double scale = 1.0;
        if (gray.depth() == CV_8U) {
            scale = 1.0 / 255.0;
        } else if (gray.depth() == CV_16U) {
            scale = 1.0 / 65535.0;
        }
        gray.convertTo(floatImg, CV_32F, scale);
    } else {
        floatImg = gray;
    }

    // Find hot pixels
    std::vector<cv::Point> hotPixels;

    for (int y = 1; y < floatImg.rows - 1; ++y) {
        for (int x = 1; x < floatImg.cols - 1; ++x) {
            float centerValue = floatImg.at<float>(y, x);

            // Check if significantly brighter than neighbors
            if (centerValue > threshold) {
                float sum = 0.0f;
                int count = 0;

                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0)
                            continue;  // Skip center

                        sum += floatImg.at<float>(y + dy, x + dx);
                        count++;
                    }
                }

                float avg = sum / count;
                if (centerValue > avg * 3.0 && centerValue - avg > 0.3) {
                    hotPixels.push_back(cv::Point(x, y));
                }
            }
        }
    }

    return hotPixels;
}

std::vector<cv::Point> detectColdPixels(const cv::Mat& image,
                                        double threshold) {
    // Convert to float and grayscale if needed
    cv::Mat gray;
    if (image.channels() > 1) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image;
    }

    cv::Mat floatImg;
    if (gray.depth() != CV_32F) {
        double scale = 1.0;
        if (gray.depth() == CV_8U) {
            scale = 1.0 / 255.0;
        } else if (gray.depth() == CV_16U) {
            scale = 1.0 / 65535.0;
        }
        gray.convertTo(floatImg, CV_32F, scale);
    } else {
        floatImg = gray;
    }

    // Find cold pixels
    std::vector<cv::Point> coldPixels;

    for (int y = 1; y < floatImg.rows - 1; ++y) {
        for (int x = 1; x < floatImg.cols - 1; ++x) {
            float centerValue = floatImg.at<float>(y, x);

            // Check if significantly darker than neighbors
            if (centerValue < threshold) {
                float sum = 0.0f;
                int count = 0;

                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0)
                            continue;  // Skip center

                        sum += floatImg.at<float>(y + dy, x + dx);
                        count++;
                    }
                }

                float avg = sum / count;
                if (centerValue < avg * 0.33 && avg - centerValue > 0.1) {
                    coldPixels.push_back(cv::Point(x, y));
                }
            }
        }
    }

    return coldPixels;
}

// Create bad pixel map
cv::Mat createBadPixelMask(const cv::Mat& image, double hotThreshold,
                           double coldThreshold) {
    // Find hot and cold pixels
    std::vector<cv::Point> hotPixels = detectHotPixels(image, hotThreshold);
    std::vector<cv::Point> coldPixels = detectColdPixels(image, coldThreshold);

    // Create mask (0 = good pixel, 255 = bad pixel)
    cv::Mat mask = cv::Mat::zeros(image.size(), CV_8U);

    // Mark hot pixels
    for (const auto& pt : hotPixels) {
        mask.at<uint8_t>(pt) = 255;
    }

    // Mark cold pixels
    for (const auto& pt : coldPixels) {
        mask.at<uint8_t>(pt) = 255;
    }

    return mask;
}

// Fix bad pixels
cv::Mat fixBadPixels(const cv::Mat& image, const cv::Mat& badPixelMask,
                     int method) {
    if (image.size() != badPixelMask.size()) {
        throw InvalidParameterException("Image and mask must be the same size");
    }

    // Create output image
    cv::Mat result = image.clone();

    // Fix bad pixels
    for (int y = 0; y < result.rows; ++y) {
        for (int x = 0; x < result.cols; ++x) {
            if (badPixelMask.at<uint8_t>(y, x) > 0) {
                // Replace bad pixel based on method
                if (method == 0) {
                    // Median of surrounding pixels
                    std::vector<cv::Mat> channels;
                    cv::split(result, channels);

                    for (size_t c = 0; c < channels.size(); ++c) {
                        std::vector<uint8_t> neighbors;

                        for (int dy = -1; dy <= 1; ++dy) {
                            for (int dx = -1; dx <= 1; ++dx) {
                                int nx = x + dx;
                                int ny = y + dy;

                                if (nx >= 0 && nx < result.cols && ny >= 0 &&
                                    ny < result.rows) {
                                    if (dx != 0 || dy != 0) {  // Skip center
                                        neighbors.push_back(
                                            channels[c].at<uint8_t>(ny, nx));
                                    }
                                }
                            }
                        }

                        if (!neighbors.empty()) {
                            std::sort(neighbors.begin(), neighbors.end());
                            channels[c].at<uint8_t>(y, x) =
                                neighbors[neighbors.size() / 2];
                        }
                    }

                    cv::merge(channels, result);
                } else if (method == 1) {
                    // Inpainting
                    cv::Mat singleMask =
                        cv::Mat::zeros(badPixelMask.size(), CV_8U);
                    singleMask.at<uint8_t>(y, x) = 255;

                    cv::inpaint(result, singleMask, result, 1,
                                cv::INPAINT_TELEA);
                }
            }
        }
    }

    return result;
}

// Version information
std::string getLibraryVersion() {
    return std::format("{}.{}.{}", SERASTRO_VERSION_MAJOR,
                       SERASTRO_VERSION_MINOR, SERASTRO_VERSION_PATCH);
}

std::string getOpenCVVersion() { return CV_VERSION; }

}  // namespace utils
}  // namespace serastro