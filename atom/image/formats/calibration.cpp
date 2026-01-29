/**
 * @file calibration.cpp
 * @brief Implementation of astronomical image calibration processing
 *
 * @copyright Copyright (C) 2023-2025
 */

#include "calibration.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace atom::image::fits {

CalibrationProcessor::CalibrationProcessor(const CalibrationParams& params)
    : params_(params) {}

std::unique_ptr<ImageHDU> CalibrationProcessor::createMasterBias(
    const std::vector<std::string>& biasFrames,
    ProgressCallback progressCallback) {
    if (biasFrames.empty()) {
        throw std::invalid_argument("No bias frames provided");
    }

    auto start = std::chrono::high_resolution_clock::now();

    if (progressCallback) {
        progressCallback(0.0f, "Creating master bias");
    }

    auto result =
        stackImagesFromFiles(biasFrames, params_.stackMethod, progressCallback);

    if (result && params_.updateHeader) {
        result->setHeaderKeyword("IMAGETYP", "'MASTER_BIAS'");
        result->setHeaderKeyword("NCOMBINE", std::to_string(biasFrames.size()));
        result->setHeaderKeyword("COMBTYPE",
                                 stackingMethodToString(params_.stackMethod));
    }

    auto end = std::chrono::high_resolution_clock::now();
    stats_.processingTime = std::chrono::duration<double>(end - start).count();
    stats_.framesStacked = static_cast<int>(biasFrames.size());

    return result;
}

std::unique_ptr<ImageHDU> CalibrationProcessor::createMasterDark(
    const std::vector<std::string>& darkFrames, const ImageHDU* masterBias,
    ProgressCallback progressCallback) {
    if (darkFrames.empty()) {
        throw std::invalid_argument("No dark frames provided");
    }

    auto start = std::chrono::high_resolution_clock::now();

    if (progressCallback) {
        progressCallback(0.0f, "Creating master dark");
    }

    // Load and optionally bias-subtract each dark
    std::vector<std::unique_ptr<ImageHDU>> processedDarks;
    processedDarks.reserve(darkFrames.size());

    for (size_t i = 0; i < darkFrames.size(); ++i) {
        auto dark = loadImageHDU(darkFrames[i]);

        if (masterBias) {
            dark = subtractBias(*dark, *masterBias);
        }

        processedDarks.push_back(std::move(dark));

        if (progressCallback) {
            float progress =
                static_cast<float>(i + 1) / (darkFrames.size() * 2);
            progressCallback(progress,
                             "Processing dark " + std::to_string(i + 1));
        }
    }

    // Stack the processed darks
    std::vector<const ImageHDU*> darkPtrs;
    darkPtrs.reserve(processedDarks.size());
    for (const auto& dark : processedDarks) {
        darkPtrs.push_back(dark.get());
    }

    auto result = stackImages<float>(darkPtrs, params_.stackMethod);

    if (result && params_.updateHeader) {
        result->setHeaderKeyword("IMAGETYP", "'MASTER_DARK'");
        result->setHeaderKeyword("NCOMBINE", std::to_string(darkFrames.size()));
        result->setHeaderKeyword("COMBTYPE",
                                 stackingMethodToString(params_.stackMethod));
        if (masterBias) {
            result->setHeaderKeyword("BIASCORR", "T");
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    stats_.processingTime = std::chrono::duration<double>(end - start).count();
    stats_.framesStacked = static_cast<int>(darkFrames.size());

    return result;
}

std::unique_ptr<ImageHDU> CalibrationProcessor::createMasterFlat(
    const std::vector<std::string>& flatFrames, const ImageHDU* masterBias,
    const ImageHDU* masterDark, ProgressCallback progressCallback) {
    if (flatFrames.empty()) {
        throw std::invalid_argument("No flat frames provided");
    }

    auto start = std::chrono::high_resolution_clock::now();

    if (progressCallback) {
        progressCallback(0.0f, "Creating master flat");
    }

    // Load and calibrate each flat
    std::vector<std::unique_ptr<ImageHDU>> processedFlats;
    processedFlats.reserve(flatFrames.size());

    for (size_t i = 0; i < flatFrames.size(); ++i) {
        auto flat = loadImageHDU(flatFrames[i]);

        if (masterBias) {
            flat = subtractBias(*flat, *masterBias);
        }

        if (masterDark) {
            double flatExpTime = getExposureTime(*flat);
            double darkExpTime = getExposureTime(*masterDark);
            flat = subtractDark(*flat, *masterDark, flatExpTime, darkExpTime);
        }

        processedFlats.push_back(std::move(flat));

        if (progressCallback) {
            float progress =
                static_cast<float>(i + 1) / (flatFrames.size() * 2);
            progressCallback(progress,
                             "Processing flat " + std::to_string(i + 1));
        }
    }

    // Stack the processed flats
    std::vector<const ImageHDU*> flatPtrs;
    flatPtrs.reserve(processedFlats.size());
    for (const auto& flat : processedFlats) {
        flatPtrs.push_back(flat.get());
    }

    auto result = stackImages<float>(flatPtrs, params_.stackMethod);

    // Normalize the master flat
    if (result) {
        auto [width, height, channels] = result->getImageSize();

        // Get data and normalize
        // Note: This is simplified - actual implementation would use
        // the template methods for proper type handling

        if (params_.updateHeader) {
            result->setHeaderKeyword("IMAGETYP", "'MASTER_FLAT'");
            result->setHeaderKeyword("NCOMBINE",
                                     std::to_string(flatFrames.size()));
            result->setHeaderKeyword(
                "COMBTYPE", stackingMethodToString(params_.stackMethod));
            if (masterBias) {
                result->setHeaderKeyword("BIASCORR", "T");
            }
            if (masterDark) {
                result->setHeaderKeyword("DARKCORR", "T");
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    stats_.processingTime = std::chrono::duration<double>(end - start).count();
    stats_.framesStacked = static_cast<int>(flatFrames.size());

    return result;
}

std::unique_ptr<ImageHDU> CalibrationProcessor::calibrateImage(
    const ImageHDU& image, const ImageHDU* masterBias,
    const ImageHDU* masterDark, const ImageHDU* masterFlat) {
    // Create a copy of the image
    auto result = std::make_unique<ImageHDU>();
    auto [width, height, channels] = image.getImageSize();
    result->setImageSize(width, height, channels);

    // Copy header
    for (const auto& keyword : image.getHeader().getAllKeywords()) {
        try {
            result->setHeaderKeyword(keyword, image.getHeaderKeyword(keyword));
        } catch (...) {
        }
    }

    // Copy pixel data
    for (int c = 0; c < channels; ++c) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                result->setPixel<float>(x, y, image.getPixel<float>(x, y, c),
                                        c);
            }
        }
    }

    // Apply calibrations in order
    if (masterBias) {
        result = subtractBias(*result, *masterBias);
    }

    if (masterDark) {
        double imgExpTime = getExposureTime(*result);
        double darkExpTime = getExposureTime(*masterDark);
        result = subtractDark(*result, *masterDark, imgExpTime, darkExpTime);
    }

    if (masterFlat) {
        result = divideFlat(*result, *masterFlat);
    }

    // Update header
    if (params_.updateHeader) {
        if (masterBias) {
            result->setHeaderKeyword("BIASCORR", "T");
        }
        if (masterDark) {
            result->setHeaderKeyword("DARKCORR", "T");
        }
        if (masterFlat) {
            result->setHeaderKeyword("FLATCORR", "T");
        }
    }

    return result;
}

std::unique_ptr<ImageHDU> CalibrationProcessor::subtractBias(
    const ImageHDU& image, const ImageHDU& bias) {
    auto [width, height, channels] = image.getImageSize();
    auto [biasW, biasH, biasC] = bias.getImageSize();

    if (width != biasW || height != biasH) {
        throw std::invalid_argument("Image and bias dimensions don't match");
    }

    auto result = std::make_unique<ImageHDU>();
    result->setImageSize(width, height, channels);

    // Copy header from original
    for (const auto& keyword : image.getHeader().getAllKeywords()) {
        try {
            result->setHeaderKeyword(keyword, image.getHeaderKeyword(keyword));
        } catch (...) {
        }
    }

    // Subtract bias
    for (int c = 0; c < channels; ++c) {
        int biasChannel = std::min(c, biasC - 1);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float imgVal = image.getPixel<float>(x, y, c);
                float biasVal = bias.getPixel<float>(x, y, biasChannel);
                result->setPixel<float>(x, y, imgVal - biasVal, c);
            }
        }
    }

    return result;
}

std::unique_ptr<ImageHDU> CalibrationProcessor::subtractDark(
    const ImageHDU& image, const ImageHDU& dark, double imageExpTime,
    double darkExpTime) {
    auto [width, height, channels] = image.getImageSize();
    auto [darkW, darkH, darkC] = dark.getImageSize();

    if (width != darkW || height != darkH) {
        throw std::invalid_argument("Image and dark dimensions don't match");
    }

    // Calculate scaling factor
    double scale = 1.0;
    if (params_.scaleDarks && imageExpTime > 0.0 && darkExpTime > 0.0) {
        scale = imageExpTime / darkExpTime;
    }

    auto result = std::make_unique<ImageHDU>();
    result->setImageSize(width, height, channels);

    // Copy header
    for (const auto& keyword : image.getHeader().getAllKeywords()) {
        try {
            result->setHeaderKeyword(keyword, image.getHeaderKeyword(keyword));
        } catch (...) {
        }
    }

    // Subtract scaled dark
    for (int c = 0; c < channels; ++c) {
        int darkChannel = std::min(c, darkC - 1);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float imgVal = image.getPixel<float>(x, y, c);
                float darkVal = dark.getPixel<float>(x, y, darkChannel);
                result->setPixel<float>(
                    x, y, imgVal - static_cast<float>(darkVal * scale), c);
            }
        }
    }

    return result;
}

std::unique_ptr<ImageHDU> CalibrationProcessor::divideFlat(
    const ImageHDU& image, const ImageHDU& flat) {
    auto [width, height, channels] = image.getImageSize();
    auto [flatW, flatH, flatC] = flat.getImageSize();

    if (width != flatW || height != flatH) {
        throw std::invalid_argument("Image and flat dimensions don't match");
    }

    auto result = std::make_unique<ImageHDU>();
    result->setImageSize(width, height, channels);

    // Copy header
    for (const auto& keyword : image.getHeader().getAllKeywords()) {
        try {
            result->setHeaderKeyword(keyword, image.getHeaderKeyword(keyword));
        } catch (...) {
        }
    }

    // Divide by flat (avoid division by zero)
    constexpr float MIN_FLAT = 0.001f;

    for (int c = 0; c < channels; ++c) {
        int flatChannel = std::min(c, flatC - 1);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float imgVal = image.getPixel<float>(x, y, c);
                float flatVal = flat.getPixel<float>(x, y, flatChannel);

                if (std::abs(flatVal) < MIN_FLAT) {
                    flatVal = MIN_FLAT;
                }

                result->setPixel<float>(x, y, imgVal / flatVal, c);
            }
        }
    }

    return result;
}

BadPixelMap CalibrationProcessor::createBadPixelMap(const ImageHDU& dark) {
    auto [width, height, channels] = dark.getImageSize();

    BadPixelMap map;
    map.width = width;
    map.height = height;
    map.pixels.resize(width * height, false);

    // Calculate statistics
    std::vector<float> values;
    values.reserve(width * height);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            values.push_back(dark.getPixel<float>(x, y, 0));
        }
    }

    // Calculate median and MAD (median absolute deviation)
    std::vector<float> sortedValues = values;
    std::sort(sortedValues.begin(), sortedValues.end());
    float median = sortedValues[sortedValues.size() / 2];

    std::vector<float> deviations(values.size());
    for (size_t i = 0; i < values.size(); ++i) {
        deviations[i] = std::abs(values[i] - median);
    }
    std::sort(deviations.begin(), deviations.end());
    float mad = deviations[deviations.size() / 2];

    // Estimate sigma from MAD
    float sigma = mad * 1.4826f;  // MAD to sigma conversion

    // Mark bad pixels
    float hotThreshold = median + params_.hotPixelThreshold * sigma;
    float coldThreshold = median - params_.coldPixelThreshold * sigma;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float val = values[y * width + x];
            if (val > hotThreshold) {
                map.setBad(x, y, true);
                ++stats_.hotPixelCount;
            } else if (val < coldThreshold) {
                map.setBad(x, y, true);
                ++stats_.coldPixelCount;
            }
        }
    }

    stats_.badPixelCount = map.countBad();

    return map;
}

BadPixelMap CalibrationProcessor::createBadPixelMap(
    const std::vector<std::string>& darkFrames,
    ProgressCallback progressCallback) {
    if (darkFrames.empty()) {
        throw std::invalid_argument("No dark frames provided");
    }

    // Create master dark first
    auto masterDark = createMasterDark(darkFrames, nullptr, progressCallback);

    // Generate bad pixel map from master dark
    return createBadPixelMap(*masterDark);
}

std::unique_ptr<ImageHDU> CalibrationProcessor::fixBadPixels(
    const ImageHDU& image, const BadPixelMap& badPixels) {
    auto [width, height, channels] = image.getImageSize();

    if (width != badPixels.width || height != badPixels.height) {
        throw std::invalid_argument(
            "Image and bad pixel map dimensions don't match");
    }

    auto result = std::make_unique<ImageHDU>();
    result->setImageSize(width, height, channels);

    // Copy header
    for (const auto& keyword : image.getHeader().getAllKeywords()) {
        try {
            result->setHeaderKeyword(keyword, image.getHeaderKeyword(keyword));
        } catch (...) {
        }
    }

    // Copy data, interpolating bad pixels
    for (int c = 0; c < channels; ++c) {
        std::vector<float> data(width * height);

        // First copy all data
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                data[y * width + x] = image.getPixel<float>(x, y, c);
            }
        }

        // Then fix bad pixels
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (badPixels.isBad(x, y)) {
                    data[y * width + x] = interpolatePixel<float>(
                        data, x, y, width, height, &badPixels);
                }
            }
        }

        // Store result
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                result->setPixel<float>(x, y, data[y * width + x], c);
            }
        }
    }

    if (params_.updateHeader) {
        result->setHeaderKeyword("BADPXFIX", "T");
        result->setHeaderKeyword("NBADPIX",
                                 std::to_string(badPixels.countBad()));
    }

    return result;
}

std::unique_ptr<ImageHDU> CalibrationProcessor::removeCosmicRays(
    const ImageHDU& image) {
    auto [width, height, channels] = image.getImageSize();

    auto result = std::make_unique<ImageHDU>();
    result->setImageSize(width, height, channels);

    // Copy header
    for (const auto& keyword : image.getHeader().getAllKeywords()) {
        try {
            result->setHeaderKeyword(keyword, image.getHeaderKeyword(keyword));
        } catch (...) {
        }
    }

    stats_.cosmicRayCount = 0;

    // Simple cosmic ray detection using Laplacian edge detection
    for (int c = 0; c < channels; ++c) {
        std::vector<float> data(width * height);
        std::vector<float> laplacian(width * height);

        // Copy data
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                data[y * width + x] = image.getPixel<float>(x, y, c);
            }
        }

        // Calculate Laplacian
        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                float center = data[y * width + x];
                float sum =
                    data[(y - 1) * width + x] + data[(y + 1) * width + x] +
                    data[y * width + (x - 1)] + data[y * width + (x + 1)];
                laplacian[y * width + x] = 4.0f * center - sum;
            }
        }

        // Calculate statistics of Laplacian
        std::vector<float> lapValues;
        lapValues.reserve((width - 2) * (height - 2));
        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                lapValues.push_back(std::abs(laplacian[y * width + x]));
            }
        }

        std::sort(lapValues.begin(), lapValues.end());
        float medLap = lapValues[lapValues.size() / 2];

        // Detect and fix cosmic rays
        float threshold = medLap * params_.cosmicRayThreshold;

        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                if (std::abs(laplacian[y * width + x]) > threshold &&
                    laplacian[y * width + x] > 0) {  // Positive = bright

                    // Replace with median of neighbors
                    std::vector<float> neighbors = {
                        data[(y - 1) * width + x], data[(y + 1) * width + x],
                        data[y * width + (x - 1)], data[y * width + (x + 1)]};
                    std::sort(neighbors.begin(), neighbors.end());
                    data[y * width + x] = (neighbors[1] + neighbors[2]) / 2.0f;

                    ++stats_.cosmicRayCount;
                }
            }
        }

        // Store result
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                result->setPixel<float>(x, y, data[y * width + x], c);
            }
        }
    }

    if (params_.updateHeader) {
        result->setHeaderKeyword("CRCORR", "T");
        result->setHeaderKeyword("NCR", std::to_string(stats_.cosmicRayCount));
    }

    return result;
}

template <typename T>
std::unique_ptr<ImageHDU> CalibrationProcessor::stackImages(
    const std::vector<const ImageHDU*>& images, StackingMethod method) {
    if (images.empty()) {
        throw std::invalid_argument("No images to stack");
    }

    // Get dimensions from first image
    auto [width, height, channels] = images[0]->getImageSize();

    // Verify all images have same dimensions
    for (size_t i = 1; i < images.size(); ++i) {
        auto [w, h, c] = images[i]->getImageSize();
        if (w != width || h != height) {
            throw std::invalid_argument(
                "Image dimensions don't match for stacking");
        }
    }

    auto result = std::make_unique<ImageHDU>();
    result->setImageSize(width, height, channels);

    // Stack each pixel
    for (int c = 0; c < channels; ++c) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                std::vector<T> values;
                values.reserve(images.size());

                for (const auto* img : images) {
                    values.push_back(img->getPixel<T>(x, y, c));
                }

                T stackedValue = stackPixel<T>(values, method);
                result->setPixel<T>(x, y, stackedValue, c);
            }
        }
    }

    return result;
}

std::unique_ptr<ImageHDU> CalibrationProcessor::stackImagesFromFiles(
    const std::vector<std::string>& filePaths, StackingMethod method,
    ProgressCallback progressCallback) {
    if (filePaths.empty()) {
        throw std::invalid_argument("No files to stack");
    }

    // Load all images
    std::vector<std::unique_ptr<ImageHDU>> loadedImages;
    loadedImages.reserve(filePaths.size());

    for (size_t i = 0; i < filePaths.size(); ++i) {
        loadedImages.push_back(loadImageHDU(filePaths[i]));

        if (progressCallback) {
            float progress = static_cast<float>(i + 1) / (filePaths.size() * 2);
            progressCallback(progress, "Loading " + std::to_string(i + 1) +
                                           "/" +
                                           std::to_string(filePaths.size()));
        }
    }

    // Create pointer vector
    std::vector<const ImageHDU*> imagePtrs;
    imagePtrs.reserve(loadedImages.size());
    for (const auto& img : loadedImages) {
        imagePtrs.push_back(img.get());
    }

    if (progressCallback) {
        progressCallback(0.5f, "Stacking images");
    }

    return stackImages<float>(imagePtrs, method);
}

template <typename T>
T CalibrationProcessor::stackPixel(const std::vector<T>& values,
                                   StackingMethod method) const {
    if (values.empty()) {
        return T{};
    }

    std::vector<T> sorted = values;

    switch (method) {
        case StackingMethod::AVERAGE: {
            T sum = std::accumulate(values.begin(), values.end(), T{});
            return sum / static_cast<T>(values.size());
        }

        case StackingMethod::MEDIAN:
            return calculateMedian(sorted);

        case StackingMethod::SIGMA_CLIP:
            return calculateSigmaClippedMean(sorted, params_.sigmaLow,
                                             params_.sigmaHigh,
                                             params_.maxIterations);

        case StackingMethod::WINSORIZED: {
            std::sort(sorted.begin(), sorted.end());
            int trim = static_cast<int>(sorted.size() * params_.trimFraction);
            T sum{};
            for (size_t i = trim; i < sorted.size() - trim; ++i) {
                sum += sorted[i];
            }
            return sum / static_cast<T>(sorted.size() - 2 * trim);
        }

        case StackingMethod::MIN:
            return *std::min_element(values.begin(), values.end());

        case StackingMethod::MAX:
            return *std::max_element(values.begin(), values.end());

        case StackingMethod::SUM:
            return std::accumulate(values.begin(), values.end(), T{});

        default:
            return calculateMedian(sorted);
    }
}

template <typename T>
T CalibrationProcessor::calculateMedian(std::vector<T>& values) const {
    if (values.empty()) {
        return T{};
    }

    std::sort(values.begin(), values.end());
    size_t n = values.size();

    if (n % 2 == 0) {
        return (values[n / 2 - 1] + values[n / 2]) / T{2};
    }
    return values[n / 2];
}

template <typename T>
T CalibrationProcessor::calculateSigmaClippedMean(std::vector<T>& values,
                                                  double sigmaLow,
                                                  double sigmaHigh,
                                                  int maxIterations) const {
    if (values.empty()) {
        return T{};
    }

    for (int iter = 0; iter < maxIterations; ++iter) {
        // Calculate mean and standard deviation
        T sum = std::accumulate(values.begin(), values.end(), T{});
        T mean = sum / static_cast<T>(values.size());

        T sqSum{};
        for (const auto& v : values) {
            T diff = v - mean;
            sqSum += diff * diff;
        }
        T stddev = std::sqrt(sqSum / static_cast<T>(values.size()));

        // Clip values outside range
        T lowThreshold = mean - static_cast<T>(sigmaLow) * stddev;
        T highThreshold = mean + static_cast<T>(sigmaHigh) * stddev;

        size_t oldSize = values.size();
        values.erase(std::remove_if(values.begin(), values.end(),
                                    [lowThreshold, highThreshold](T v) {
                                        return v < lowThreshold ||
                                               v > highThreshold;
                                    }),
                     values.end());

        // Stop if no values were clipped
        if (values.size() == oldSize || values.empty()) {
            break;
        }
    }

    if (values.empty()) {
        return T{};
    }

    T sum = std::accumulate(values.begin(), values.end(), T{});
    return sum / static_cast<T>(values.size());
}

double CalibrationProcessor::getExposureTime(const ImageHDU& image) const {
    try {
        std::string exptime = image.getHeaderKeyword("EXPTIME");
        return std::stod(exptime);
    } catch (...) {
        try {
            std::string exposure = image.getHeaderKeyword("EXPOSURE");
            return std::stod(exposure);
        } catch (...) {
            return 0.0;
        }
    }
}

template <typename T>
T CalibrationProcessor::interpolatePixel(const std::vector<T>& data, int x,
                                         int y, int width, int height,
                                         const BadPixelMap* badPixels) const {
    // Collect good neighbors
    std::vector<T> neighbors;
    neighbors.reserve(8);

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0)
                continue;

            int nx = x + dx;
            int ny = y + dy;

            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                if (!badPixels || !badPixels->isBad(nx, ny)) {
                    neighbors.push_back(data[ny * width + nx]);
                }
            }
        }
    }

    if (neighbors.empty()) {
        return data[y * width + x];  // Can't interpolate
    }

    // Return median of neighbors
    std::sort(neighbors.begin(), neighbors.end());
    return neighbors[neighbors.size() / 2];
}

std::unique_ptr<ImageHDU> CalibrationProcessor::loadImageHDU(
    const std::string& filename) const {
    FITSFile fits;
    fits.readFITS(filename);

    if (fits.getHDUCount() == 0) {
        throw std::runtime_error("No HDUs in file: " + filename);
    }

    auto& hdu = fits.getHDU(0);
    auto* imageHDU = dynamic_cast<ImageHDU*>(&hdu);

    if (!imageHDU) {
        throw std::runtime_error("Not an image HDU: " + filename);
    }

    // Create a copy
    auto result = std::make_unique<ImageHDU>();
    auto [width, height, channels] = imageHDU->getImageSize();
    result->setImageSize(width, height, channels);

    // Copy header
    for (const auto& keyword : imageHDU->getHeader().getAllKeywords()) {
        try {
            result->setHeaderKeyword(keyword,
                                     imageHDU->getHeaderKeyword(keyword));
        } catch (...) {
        }
    }

    // Copy data
    for (int c = 0; c < channels; ++c) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                result->setPixel<float>(x, y,
                                        imageHDU->getPixel<float>(x, y, c), c);
            }
        }
    }

    return result;
}
template <typename T>
CalibrationStats CalibrationProcessor::calculateStats(const ImageHDU& image) {
    CalibrationStats stats;

    auto [width, height, channels] = image.getImageSize();
    if (width <= 0 || height <= 0) {
        return stats;
    }

    // Calculate basic statistics using getPixel
    double sum = 0.0;
    double sumSq = 0.0;
    T minVal = std::numeric_limits<T>::max();
    T maxVal = std::numeric_limits<T>::lowest();
    size_t count = 0;

    for (int c = 0; c < channels; ++c) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                T val = image.getPixel<T>(x, y, c);
                sum += static_cast<double>(val);
                sumSq += static_cast<double>(val) * static_cast<double>(val);
                minVal = std::min(minVal, val);
                maxVal = std::max(maxVal, val);
                ++count;
            }
        }
    }

    if (count > 0) {
        stats.meanValue = sum / count;
        stats.stdDev =
            std::sqrt((sumSq / count) - (stats.meanValue * stats.meanValue));
        stats.minValue = static_cast<double>(minVal);
        stats.maxValue = static_cast<double>(maxVal);
        stats.medianValue =
            stats.meanValue;  // Simplified - proper median requires sorting
    }

    return stats;
}

// Explicit template instantiations
template std::unique_ptr<ImageHDU> CalibrationProcessor::stackImages<float>(
    const std::vector<const ImageHDU*>&, StackingMethod);
template std::unique_ptr<ImageHDU> CalibrationProcessor::stackImages<double>(
    const std::vector<const ImageHDU*>&, StackingMethod);
template std::unique_ptr<ImageHDU> CalibrationProcessor::stackImages<int16_t>(
    const std::vector<const ImageHDU*>&, StackingMethod);
template std::unique_ptr<ImageHDU> CalibrationProcessor::stackImages<int32_t>(
    const std::vector<const ImageHDU*>&, StackingMethod);

template CalibrationStats CalibrationProcessor::calculateStats<float>(
    const ImageHDU&);
template CalibrationStats CalibrationProcessor::calculateStats<double>(
    const ImageHDU&);

// Utility functions

std::string stackingMethodToString(StackingMethod method) {
    switch (method) {
        case StackingMethod::AVERAGE:
            return "AVERAGE";
        case StackingMethod::MEDIAN:
            return "MEDIAN";
        case StackingMethod::SIGMA_CLIP:
            return "SIGMA_CLIP";
        case StackingMethod::WINSORIZED:
            return "WINSORIZED";
        case StackingMethod::MIN:
            return "MIN";
        case StackingMethod::MAX:
            return "MAX";
        case StackingMethod::SUM:
            return "SUM";
    }
    return "UNKNOWN";
}

StackingMethod stackingMethodFromString(const std::string& name) {
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    if (upper == "AVERAGE" || upper == "MEAN") {
        return StackingMethod::AVERAGE;
    }
    if (upper == "MEDIAN") {
        return StackingMethod::MEDIAN;
    }
    if (upper == "SIGMA_CLIP" || upper == "SIGMACLIP") {
        return StackingMethod::SIGMA_CLIP;
    }
    if (upper == "WINSORIZED" || upper == "WINSOR") {
        return StackingMethod::WINSORIZED;
    }
    if (upper == "MIN" || upper == "MINIMUM") {
        return StackingMethod::MIN;
    }
    if (upper == "MAX" || upper == "MAXIMUM") {
        return StackingMethod::MAX;
    }
    if (upper == "SUM") {
        return StackingMethod::SUM;
    }

    return StackingMethod::MEDIAN;  // Default
}

}  // namespace atom::image::fits
