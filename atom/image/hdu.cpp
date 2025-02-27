#include "hdu.hpp"

#include <algorithm>
#include <cmath>
#include <execution>
#include <future>
#include <numeric>
#include <stdexcept>
#include <thread>

void HDU::setHeaderKeyword(const std::string& keyword,
                           const std::string& value) noexcept {
    header.addKeyword(keyword, value);
}

std::string HDU::getHeaderKeyword(const std::string& keyword) const {
    try {
        return header.getKeywordValue(keyword);
    } catch (const std::exception& e) {
        throw DataFormatException("Failed to get keyword value: " +
                                  std::string(e.what()));
    }
}

bool ImageHDU::validateCoordinates(int x, int y, int channel) const noexcept {
    return x >= 0 && x < width && y >= 0 && y < height && channel >= 0 &&
           channel < channels;
}

void ImageHDU::readHDU(std::ifstream& file) {
    if (!file || !file.good()) {
        throw FileOperationException("Invalid file stream for reading HDU");
    }

    try {
        std::vector<char> headerData(FITSHeader::FITS_HEADER_UNIT_SIZE);
        file.read(headerData.data(), headerData.size());

        if (!file.good()) {
            throw FileOperationException("Failed to read FITS header");
        }

        header.deserialize(headerData);

        // Parse header keywords with validation
        std::string naxis1Str = header.getKeywordValue("NAXIS1");
        std::string naxis2Str = header.getKeywordValue("NAXIS2");
        width = naxis1Str.empty() ? 0 : std::stoi(naxis1Str);
        height = naxis2Str.empty() ? 0 : std::stoi(naxis2Str);

        // Handle optional NAXIS3 with default value 1
        try {
            std::string naxis3Str = header.getKeywordValue("NAXIS3");
            channels = naxis3Str.empty() ? 1 : std::stoi(naxis3Str);
        } catch (...) {
            channels = 1;  // Default to 1 channel if not specified
        }

        if (width <= 0 || height <= 0 || channels <= 0) {
            throw DataFormatException(
                "Invalid image dimensions in FITS header");
        }

        std::string bitpixStr = header.getKeywordValue("BITPIX");
        int bitpix = bitpixStr.empty() ? 0 : std::stoi(bitpixStr);

        switch (bitpix) {
            case 8:
                initializeData<uint8_t>();
                break;
            case 16:
                initializeData<int16_t>();
                break;
            case 32:
                initializeData<int32_t>();
                break;
            case 64:
                initializeData<int64_t>();
                break;
            case -32:
                initializeData<float>();
                break;
            case -64:
                initializeData<double>();
                break;
            default:
                throw DataFormatException("Unsupported BITPIX value: " +
                                          std::to_string(bitpix));
        }

        int64_t dataSize = static_cast<int64_t>(width) * height * channels *
                           std::abs(bitpix) / 8;

        if (dataSize <= 0) {
            throw DataFormatException("Invalid data size calculated");
        }

        data->readData(file, dataSize);
    } catch (const std::exception& e) {
        throw DataFormatException("Failed to read HDU: " +
                                  std::string(e.what()));
    }
}

void ImageHDU::writeHDU(std::ofstream& file) const {
    if (!file || !file.good()) {
        throw FileOperationException("Invalid file stream for writing HDU");
    }

    try {
        auto headerData = header.serialize();
        file.write(headerData.data(), headerData.size());

        if (!file.good()) {
            throw FileOperationException("Failed to write FITS header");
        }

        if (!data) {
            throw DataFormatException("No data available to write");
        }

        data->writeData(file);

        if (!file.good()) {
            throw FileOperationException("Failed to write FITS data");
        }
    } catch (const std::exception& e) {
        throw FileOperationException("Failed to write HDU: " +
                                     std::string(e.what()));
    }
}

void ImageHDU::setImageSize(int w, int h, int c) {
    if (w <= 0 || h <= 0 || c <= 0) {
        throw std::invalid_argument("Image dimensions must be positive");
    }

    width = w;
    height = h;
    channels = c;

    header.addKeyword("NAXIS1", std::to_string(width));
    header.addKeyword("NAXIS2", std::to_string(height));

    if (channels > 1) {
        header.addKeyword("NAXIS", "3");
        header.addKeyword("NAXIS3", std::to_string(channels));
    } else {
        header.addKeyword("NAXIS", "2");
    }
}

std::tuple<int, int, int> ImageHDU::getImageSize() const noexcept {
    return {width, height, channels};
}

template <FitsNumeric T>
void ImageHDU::setPixel(int x, int y, T value, int channel) {
    if (!validateCoordinates(x, y, channel)) {
        throw std::out_of_range("Pixel coordinates or channel out of range");
    }

    if (!data) {
        throw std::runtime_error("Image data not initialized");
    }

    try {
        auto& typedData = dynamic_cast<TypedFITSData<T>&>(*data);
        typedData.getData()[(y * width + x) * channels + channel] = value;
    } catch (const std::bad_cast& e) {
        throw DataFormatException("Data type mismatch in setPixel");
    }
}

template <FitsNumeric T>
T ImageHDU::getPixel(int x, int y, int channel) const {
    if (!validateCoordinates(x, y, channel)) {
        throw std::out_of_range("Pixel coordinates or channel out of range");
    }

    if (!data) {
        throw std::runtime_error("Image data not initialized");
    }

    try {
        const auto& typedData = dynamic_cast<const TypedFITSData<T>&>(*data);
        return typedData.getData()[(y * width + x) * channels + channel];
    } catch (const std::bad_cast& e) {
        throw DataFormatException("Data type mismatch in getPixel");
    }
}

template <FitsNumeric T>
typename ImageHDU::template ImageStats<T> ImageHDU::computeImageStats(
    int channel) const {
    if (channel < 0 || channel >= channels) {
        throw std::out_of_range("Channel index out of range");
    }

    if (!data) {
        throw std::runtime_error("Image data not initialized");
    }

    try {
        const auto& typedData = dynamic_cast<const TypedFITSData<T>&>(*data);
        const auto& pixelData = typedData.getData();

        // Use parallel algorithms for better performance
        size_t dataSize = pixelData.size();
        std::vector<T> channelData;
        channelData.reserve(width * height);

        for (size_t i = static_cast<size_t>(channel); i < dataSize;
             i += channels) {
            channelData.push_back(pixelData[i]);
        }

        // Find min and max using parallel algorithms
        auto [minIt, maxIt] = std::minmax_element(
            std::execution::par_unseq, channelData.begin(), channelData.end());

        T min = *minIt;
        T max = *maxIt;

        // Calculate mean using parallel reduction
        double sum = std::reduce(
            std::execution::par_unseq, channelData.begin(), channelData.end(),
            0.0, [](double a, T b) { return a + static_cast<double>(b); });

        double mean = sum / channelData.size();

        // Calculate variance using parallel transform-reduce
        double variance = std::transform_reduce(
                              std::execution::par_unseq, channelData.begin(),
                              channelData.end(), 0.0, std::plus<>(),
                              [mean](T val) {
                                  double diff = static_cast<double>(val) - mean;
                                  return diff * diff;
                              }) /
                          channelData.size();

        double stddev = std::sqrt(variance);

        return {min, max, mean, stddev};
    } catch (const std::bad_cast& e) {
        throw DataFormatException("Data type mismatch in computeImageStats");
    }
}

template <FitsNumeric T>
void ImageHDU::applyFilter(std::span<const std::span<const double>> kernel,
                           int channel) {
    if (kernel.empty() || kernel[0].empty()) {
        throw std::invalid_argument("Invalid filter kernel");
    }

    if (!data) {
        throw std::runtime_error("Image data not initialized");
    }

    int kernelHeight = static_cast<int>(kernel.size());
    int kernelWidth = static_cast<int>(kernel[0].size());

    if (kernelHeight % 2 == 0 || kernelWidth % 2 == 0) {
        throw std::invalid_argument("Filter kernel dimensions must be odd");
    }

    try {
        auto& typedData = dynamic_cast<TypedFITSData<T>&>(*data);
        auto& pixelData = typedData.getData();

        int kernelCenterY = kernelHeight / 2;
        int kernelCenterX = kernelWidth / 2;

        std::vector<T> newPixelData(pixelData.size());

        for (int c = 0; c < channels; ++c) {
            if (channel != -1 && c != channel)
                continue;

            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    double sum = 0.0;
                    for (int ky = 0; ky < kernelHeight; ++ky) {
                        for (int kx = 0; kx < kernelWidth; ++kx) {
                            int imgY = y + ky - kernelCenterY;
                            int imgX = x + kx - kernelCenterX;
                            if (imgY >= 0 && imgY < height && imgX >= 0 &&
                                imgX < width) {
                                sum +=
                                    kernel[ky][kx] *
                                    pixelData[(imgY * width + imgX) * channels +
                                              c];
                            }
                        }
                    }
                    newPixelData[(y * width + x) * channels + c] =
                        static_cast<T>(sum);
                }
            }
        }

        pixelData = std::move(newPixelData);
    } catch (const std::bad_cast& e) {
        throw DataFormatException("Data type mismatch in applyFilter");
    }
}

template <FitsNumeric T>
void ImageHDU::applyFilterParallel(
    std::span<const std::span<const double>> kernel, int channel) {
    if (kernel.empty() || kernel[0].empty()) {
        throw std::invalid_argument("Invalid filter kernel");
    }

    if (!data) {
        throw std::runtime_error("Image data not initialized");
    }

    int kernelHeight = static_cast<int>(kernel.size());
    int kernelWidth = static_cast<int>(kernel[0].size());

    if (kernelHeight % 2 == 0 || kernelWidth % 2 == 0) {
        throw std::invalid_argument("Filter kernel dimensions must be odd");
    }

    try {
        auto& typedData = dynamic_cast<TypedFITSData<T>&>(*data);
        auto& pixelData = typedData.getData();

        int kernelCenterY = kernelHeight / 2;
        int kernelCenterX = kernelWidth / 2;

        std::vector<T> newPixelData(pixelData.size());

        // Determine the number of threads to use
        unsigned int threadCount = std::thread::hardware_concurrency();
        if (threadCount == 0)
            threadCount = 4;  // Fallback if hardware_concurrency returns 0

        // Create a vector to store futures
        std::vector<std::future<void>> futures;

        // Process channels in parallel if needed
        for (int c = 0; c < channels; ++c) {
            if (channel != -1 && c != channel)
                continue;

            // Divide the image into horizontal strips for parallel processing
            int rowsPerThread = height / threadCount;
            if (rowsPerThread < 1)
                rowsPerThread = 1;

            for (unsigned int t = 0;
                 t < threadCount &&
                 t * rowsPerThread < static_cast<unsigned int>(height);
                 ++t) {
                int startY = t * rowsPerThread;
                int endY =
                    (t == threadCount - 1) ? height : (t + 1) * rowsPerThread;

                futures.push_back(std::async(std::launch::async, [&, startY,
                                                                  endY, c]() {
                    for (int y = startY; y < endY; ++y) {
                        for (int x = 0; x < width; ++x) {
                            double sum = 0.0;
                            for (int ky = 0; ky < kernelHeight; ++ky) {
                                for (int kx = 0; kx < kernelWidth; ++kx) {
                                    int imgY = y + ky - kernelCenterY;
                                    int imgX = x + kx - kernelCenterX;
                                    if (imgY >= 0 && imgY < height &&
                                        imgX >= 0 && imgX < width) {
                                        sum += kernel[ky][kx] *
                                               pixelData[(imgY * width + imgX) *
                                                             channels +
                                                         c];
                                    }
                                }
                            }
                            newPixelData[(y * width + x) * channels + c] =
                                static_cast<T>(sum);
                        }
                    }
                }));
            }
        }

        // Wait for all threads to complete
        for (auto& future : futures) {
            future.wait();
        }

        pixelData = std::move(newPixelData);
    } catch (const std::bad_cast& e) {
        throw DataFormatException("Data type mismatch in applyFilterParallel");
    }
}

template <FitsNumeric T>
void ImageHDU::initializeData() {
    data = std::make_unique<TypedFITSData<T>>();
    auto& typedData = dynamic_cast<TypedFITSData<T>&>(*data);
    typedData.getData().resize(width * height * channels);
}

template <FitsNumeric T>
T ImageHDU::bilinearInterpolate(const TypedFITSData<T>& srcData, double x,
                                double y, int channel) const {
    // Get integer and fractional parts of coordinates
    int x0 = static_cast<int>(std::floor(x));
    int y0 = static_cast<int>(std::floor(y));
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    // Ensure coordinates are within bounds
    x0 = std::clamp(x0, 0, width - 1);
    y0 = std::clamp(y0, 0, height - 1);
    x1 = std::clamp(x1, 0, width - 1);
    y1 = std::clamp(y1, 0, height - 1);

    // Get fractional part
    double dx = x - x0;
    double dy = y - y0;

    // Get pixel values
    const auto& pixelData = srcData.getData();
    T p00 = pixelData[(y0 * width + x0) * channels + channel];
    T p01 = pixelData[(y0 * width + x1) * channels + channel];
    T p10 = pixelData[(y1 * width + x0) * channels + channel];
    T p11 = pixelData[(y1 * width + x1) * channels + channel];

    // Interpolate
    double result = (1.0 - dx) * (1.0 - dy) * p00 + dx * (1.0 - dy) * p01 +
                    (1.0 - dx) * dy * p10 + dx * dy * p11;

    return static_cast<T>(std::round(result));
}

template <FitsNumeric T>
void ImageHDU::resize(int newWidth, int newHeight) {
    if (newWidth <= 0 || newHeight <= 0) {
        throw std::invalid_argument("New dimensions must be positive");
    }

    if (!data) {
        throw std::runtime_error("Image data not initialized");
    }

    try {
        auto& typedData = dynamic_cast<TypedFITSData<T>&>(*data);
        [[maybe_unused]] const auto& originalData = typedData.getData();

        // Create new data array
        std::vector<T> newImageData(static_cast<size_t>(newWidth) * newHeight *
                                    channels);

        // Calculate scale factors
        double scaleX = static_cast<double>(width) / newWidth;
        double scaleY = static_cast<double>(height) / newHeight;

        // Use parallel algorithm for better performance
        std::vector<int> rows(newHeight);
        std::iota(rows.begin(), rows.end(), 0);

        std::for_each(
            std::execution::par_unseq, rows.begin(), rows.end(), [&](int y) {
                for (int x = 0; x < newWidth; ++x) {
                    // Find corresponding position in original image
                    double srcX = x * scaleX;
                    double srcY = y * scaleY;

                    // Apply bilinear interpolation for each channel
                    for (int c = 0; c < channels; ++c) {
                        newImageData[(y * newWidth + x) * channels + c] =
                            bilinearInterpolate(typedData, srcX, srcY, c);
                    }
                }
            });

        // Update image data and dimensions
        typedData.getData() = std::move(newImageData);
        width = newWidth;
        height = newHeight;

        // Update header keywords
        header.addKeyword("NAXIS1", std::to_string(width));
        header.addKeyword("NAXIS2", std::to_string(height));

    } catch (const std::bad_cast& e) {
        throw DataFormatException("Data type mismatch in resize");
    }
}

template <FitsNumeric T>
std::unique_ptr<ImageHDU> ImageHDU::createThumbnail(int maxSize) const {
    if (maxSize <= 0) {
        throw std::invalid_argument("Thumbnail max size must be positive");
    }

    if (!data) {
        throw std::runtime_error("Image data not initialized");
    }

    try {
        // Calculate new dimensions while preserving aspect ratio
        int newWidth, newHeight;
        if (width > height) {
            newWidth = maxSize;
            newHeight =
                static_cast<int>(static_cast<double>(height) * maxSize / width);
        } else {
            newHeight = maxSize;
            newWidth =
                static_cast<int>(static_cast<double>(width) * maxSize / height);
        }

        if (newWidth <= 0)
            newWidth = 1;
        if (newHeight <= 0)
            newHeight = 1;

        // Create new HDU
        auto thumbnail = std::make_unique<ImageHDU>();
        thumbnail->setImageSize(newWidth, newHeight, channels);

        // Copy and set basic header information
        auto keywords = header.getAllKeywords();
        for (const auto& keyword : keywords) {
            if (keyword != "NAXIS1" && keyword != "NAXIS2") {
                try {
                    thumbnail->setHeaderKeyword(
                        keyword, header.getKeywordValue(keyword));
                } catch (...) {
                    // Skip if keyword can't be copied
                }
            }
        }

        // Add thumbnail specific headers
        thumbnail->setHeaderKeyword("COMMENT",
                                    "Thumbnail generated from original image");
        thumbnail->setHeaderKeyword(
            "THUMBSCL", std::to_string(static_cast<double>(width) / newWidth));

        // Copy and resize image data
        const auto& typedData = dynamic_cast<const TypedFITSData<T>&>(*data);
        auto& thumbnailData = dynamic_cast<TypedFITSData<T>&>(*thumbnail->data);

        std::vector<T>& newImageData = thumbnailData.getData();

        // Calculate scale factors
        double scaleX = static_cast<double>(width) / newWidth;
        double scaleY = static_cast<double>(height) / newHeight;

        for (int y = 0; y < newHeight; ++y) {
            for (int x = 0; x < newWidth; ++x) {
                // Find corresponding position in original image
                double srcX = x * scaleX;
                double srcY = y * scaleY;

                // Apply bilinear interpolation for each channel
                for (int c = 0; c < channels; ++c) {
                    newImageData[(y * newWidth + x) * channels + c] =
                        bilinearInterpolate(typedData, srcX, srcY, c);
                }
            }
        }

        return thumbnail;

    } catch (const std::bad_cast& e) {
        throw DataFormatException("Data type mismatch in createThumbnail");
    }
}

template <FitsNumeric T>
std::unique_ptr<ImageHDU> ImageHDU::extractROI(int x, int y, int roiWidth,
                                               int roiHeight) const {
    if (x < 0 || y < 0 || roiWidth <= 0 || roiHeight <= 0) {
        throw std::invalid_argument("Invalid ROI parameters");
    }

    if (x + roiWidth > width || y + roiHeight > height) {
        throw std::out_of_range("ROI exceeds image boundaries");
    }

    if (!data) {
        throw std::runtime_error("Image data not initialized");
    }

    try {
        // Create new HDU for ROI
        auto roi = std::make_unique<ImageHDU>();
        roi->setImageSize(roiWidth, roiHeight, channels);

        // Copy header information
        auto keywords = header.getAllKeywords();
        for (const auto& keyword : keywords) {
            if (keyword != "NAXIS1" && keyword != "NAXIS2") {
                try {
                    roi->setHeaderKeyword(keyword,
                                          header.getKeywordValue(keyword));
                } catch (...) {
                    // Skip if keyword can't be copied
                }
            }
        }

        // Add ROI specific headers
        roi->setHeaderKeyword("COMMENT", "ROI extracted from original image");
        roi->setHeaderKeyword("ROI_X", std::to_string(x));
        roi->setHeaderKeyword("ROI_Y", std::to_string(y));

        // Copy data from the ROI region
        const auto& typedData = dynamic_cast<const TypedFITSData<T>&>(*data);
        auto& roiData = dynamic_cast<TypedFITSData<T>&>(*roi->data);

        std::vector<T>& newImageData = roiData.getData();
        const auto& originalData = typedData.getData();

        for (int dy = 0; dy < roiHeight; ++dy) {
            for (int dx = 0; dx < roiWidth; ++dx) {
                for (int c = 0; c < channels; ++c) {
                    int srcIdx = ((y + dy) * width + (x + dx)) * channels + c;
                    int dstIdx = (dy * roiWidth + dx) * channels + c;
                    newImageData[dstIdx] = originalData[srcIdx];
                }
            }
        }

        return roi;

    } catch (const std::bad_cast& e) {
        throw DataFormatException("Data type mismatch in extractROI");
    }
}

template <FitsNumeric T>
Task<typename ImageHDU::template ImageStats<T>>
ImageHDU::computeImageStatsAsync(int channel) const {
    if (channel < 0 || channel >= channels) {
        throw std::out_of_range("Channel index out of range");
    }

    if (!data) {
        throw std::runtime_error("Image data not initialized");
    }

    try {
        const auto& typedData = dynamic_cast<const TypedFITSData<T>&>(*data);
        const auto& pixelData = typedData.getData();

        // Extract channel data
        std::vector<T> channelData;
        channelData.reserve(width * height);

        for (size_t i = static_cast<size_t>(channel); i < pixelData.size();
             i += static_cast<size_t>(channels)) {
            channelData.push_back(pixelData[i]);
        }

        // Use parallel algorithms for better performance
        auto [minIt, maxIt] = std::minmax_element(
            std::execution::par_unseq, channelData.begin(), channelData.end());

        T min = *minIt;
        T max = *maxIt;

        // Calculate mean
        double sum = std::reduce(
            std::execution::par_unseq, channelData.begin(), channelData.end(),
            0.0, [](double a, T b) { return a + static_cast<double>(b); });

        double mean = sum / channelData.size();

        // Calculate variance
        double variance = std::transform_reduce(
                              std::execution::par_unseq, channelData.begin(),
                              channelData.end(), 0.0, std::plus<>(),
                              [mean](T val) {
                                  double diff = static_cast<double>(val) - mean;
                                  return diff * diff;
                              }) /
                          channelData.size();

        double stddev = std::sqrt(variance);

        co_return ImageStats<T>{min, max, mean, stddev};
    } catch (const std::bad_cast& e) {
        throw DataFormatException(
            "Data type mismatch in computeImageStatsAsync");
    }
}

// Explicit template instantiations
template void ImageHDU::setPixel<uint8_t>(int, int, uint8_t, int);
template void ImageHDU::setPixel<int16_t>(int, int, int16_t, int);
template void ImageHDU::setPixel<int32_t>(int, int, int32_t, int);
template void ImageHDU::setPixel<int64_t>(int, int, int64_t, int);
template void ImageHDU::setPixel<float>(int, int, float, int);
template void ImageHDU::setPixel<double>(int, int, double, int);

template uint8_t ImageHDU::getPixel<uint8_t>(int, int, int) const;
template int16_t ImageHDU::getPixel<int16_t>(int, int, int) const;
template int32_t ImageHDU::getPixel<int32_t>(int, int, int) const;
template int64_t ImageHDU::getPixel<int64_t>(int, int, int) const;
template float ImageHDU::getPixel<float>(int, int, int) const;
template double ImageHDU::getPixel<double>(int, int, int) const;

template ImageHDU::ImageStats<uint8_t> ImageHDU::computeImageStats<uint8_t>(
    int) const;
template ImageHDU::ImageStats<int16_t> ImageHDU::computeImageStats<int16_t>(
    int) const;
template ImageHDU::ImageStats<int32_t> ImageHDU::computeImageStats<int32_t>(
    int) const;
template ImageHDU::ImageStats<int64_t> ImageHDU::computeImageStats<int64_t>(
    int) const;
template ImageHDU::ImageStats<float> ImageHDU::computeImageStats<float>(
    int) const;
template ImageHDU::ImageStats<double> ImageHDU::computeImageStats<double>(
    int) const;

template void ImageHDU::applyFilter<uint8_t>(
    std::span<const std::span<const double>>, int);
template void ImageHDU::applyFilter<int16_t>(
    std::span<const std::span<const double>>, int);
template void ImageHDU::applyFilter<int32_t>(
    std::span<const std::span<const double>>, int);
template void ImageHDU::applyFilter<int64_t>(
    std::span<const std::span<const double>>, int);
template void ImageHDU::applyFilter<float>(
    std::span<const std::span<const double>>, int);
template void ImageHDU::applyFilter<double>(
    std::span<const std::span<const double>>, int);

template void ImageHDU::applyFilterParallel<uint8_t>(
    std::span<const std::span<const double>>, int);
template void ImageHDU::applyFilterParallel<int16_t>(
    std::span<const std::span<const double>>, int);
template void ImageHDU::applyFilterParallel<int32_t>(
    std::span<const std::span<const double>>, int);
template void ImageHDU::applyFilterParallel<int64_t>(
    std::span<const std::span<const double>>, int);
template void ImageHDU::applyFilterParallel<float>(
    std::span<const std::span<const double>>, int);
template void ImageHDU::applyFilterParallel<double>(
    std::span<const std::span<const double>>, int);

template void ImageHDU::resize<uint8_t>(int, int);
template void ImageHDU::resize<int16_t>(int, int);
template void ImageHDU::resize<int32_t>(int, int);
template void ImageHDU::resize<int64_t>(int, int);
template void ImageHDU::resize<float>(int, int);
template void ImageHDU::resize<double>(int, int);

template std::unique_ptr<ImageHDU> ImageHDU::createThumbnail<uint8_t>(
    int) const;
template std::unique_ptr<ImageHDU> ImageHDU::createThumbnail<int16_t>(
    int) const;
template std::unique_ptr<ImageHDU> ImageHDU::createThumbnail<int32_t>(
    int) const;
template std::unique_ptr<ImageHDU> ImageHDU::createThumbnail<int64_t>(
    int) const;
template std::unique_ptr<ImageHDU> ImageHDU::createThumbnail<float>(int) const;
template std::unique_ptr<ImageHDU> ImageHDU::createThumbnail<double>(int) const;

template std::unique_ptr<ImageHDU> ImageHDU::extractROI<uint8_t>(int, int, int,
                                                                 int) const;
template std::unique_ptr<ImageHDU> ImageHDU::extractROI<int16_t>(int, int, int,
                                                                 int) const;
template std::unique_ptr<ImageHDU> ImageHDU::extractROI<int32_t>(int, int, int,
                                                                 int) const;
template std::unique_ptr<ImageHDU> ImageHDU::extractROI<int64_t>(int, int, int,
                                                                 int) const;
template std::unique_ptr<ImageHDU> ImageHDU::extractROI<float>(int, int, int,
                                                               int) const;
template std::unique_ptr<ImageHDU> ImageHDU::extractROI<double>(int, int, int,
                                                                int) const;

template Task<ImageHDU::ImageStats<uint8_t>>
ImageHDU::computeImageStatsAsync<uint8_t>(int) const;
template Task<ImageHDU::ImageStats<int16_t>>
ImageHDU::computeImageStatsAsync<int16_t>(int) const;
template Task<ImageHDU::ImageStats<int32_t>>
ImageHDU::computeImageStatsAsync<int32_t>(int) const;
template Task<ImageHDU::ImageStats<int64_t>>
ImageHDU::computeImageStatsAsync<int64_t>(int) const;
template Task<ImageHDU::ImageStats<float>>
ImageHDU::computeImageStatsAsync<float>(int) const;
template Task<ImageHDU::ImageStats<double>>
ImageHDU::computeImageStatsAsync<double>(int) const;
