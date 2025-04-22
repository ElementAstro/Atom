#include "fits_utils.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>

#include "atom/error/exception.hpp"

namespace atom {
namespace image {

namespace {
std::vector<std::vector<double>> createGaussianKernel(int size,
                                                      double sigma = 1.0) {
    std::vector<std::vector<double>> kernel(size, std::vector<double>(size));
    double sum = 0.0;
    int center = size / 2;

    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            double dx = x - center;
            double dy = y - center;
            double value = std::exp(-(dx * dx + dy * dy) / (2 * sigma * sigma));
            kernel[y][x] = value;
            sum += value;
        }
    }

    // 归一化
    for (auto& row : kernel) {
        for (auto& val : row) {
            val /= sum;
        }
    }

    return kernel;
}

std::vector<std::vector<double>> createMeanKernel(int size) {
    std::vector<std::vector<double>> kernel(
        size, std::vector<double>(size, 1.0 / (size * size)));
    return kernel;
}

std::vector<std::vector<double>> createSobelKernelX() {
    return {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
}

std::vector<std::vector<double>> createSobelKernelY() {
    return {{-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};
}

std::vector<std::vector<double>> createLaplacianKernel() {
    return {{0, 1, 0}, {1, -4, 1}, {0, 1, 0}};
}

// 将std::vector<std::vector<double>>转换为std::span<const std::span<const
// double>>
std::vector<std::span<const double>> convertToSpan(
    const std::vector<std::vector<double>>& kernel) {
    std::vector<std::span<const double>> result;
    for (const auto& row : kernel) {
        result.push_back(std::span<const double>(row));
    }
    return result;
}

// 数据类型辅助函数
template <FitsNumeric T>
DataType getDataTypeFromTemplate() {
    if constexpr (std::is_same_v<T, uint8_t>) {
        return DataType::BYTE;
    } else if constexpr (std::is_same_v<T, int16_t>) {
        return DataType::SHORT;
    } else if constexpr (std::is_same_v<T, int32_t>) {
        return DataType::INT;
    } else if constexpr (std::is_same_v<T, int64_t>) {
        return DataType::LONG;
    } else if constexpr (std::is_same_v<T, float>) {
        return DataType::FLOAT;
    } else {
        return DataType::DOUBLE;
    }
}

template <typename Func>
void dispatchByDataType(DataType type, Func&& func) {
    switch (type) {
        case DataType::BYTE:
            func.template operator()<uint8_t>();
            break;
        case DataType::SHORT:
            func.template operator()<int16_t>();
            break;
        case DataType::INT:
            func.template operator()<int32_t>();
            break;
        case DataType::LONG:
            func.template operator()<int64_t>();
            break;
        case DataType::FLOAT:
            func.template operator()<float>();
            break;
        case DataType::DOUBLE:
            func.template operator()<double>();
            break;
        default:
            THROW_INVALID_ARGUMENT("不支持的数据类型");
    }
}

// 获取数据类型的最小/最大值
template <FitsNumeric T>
std::pair<double, double> getDataTypeRange() {
    if constexpr (std::is_same_v<T, uint8_t>) {
        return {0, 255};
    } else if constexpr (std::is_same_v<T, int16_t>) {
        return {-32768, 32767};
    } else if constexpr (std::is_same_v<T, int32_t>) {
        return {-2147483648.0, 2147483647.0};
    } else if constexpr (std::is_same_v<T, int64_t>) {
        return {-9223372036854775808.0, 9223372036854775807.0};
    } else if constexpr (std::is_same_v<T, float>) {
        return {-FLT_MAX, FLT_MAX};
    } else {
        return {-DBL_MAX, DBL_MAX};
    }
}

}  // anonymous namespace

// FitsImage实现
FitsImage::FitsImage()
    : fitsFile(std::make_unique<FITSFile>()), dataType(DataType::SHORT) {}

FitsImage::FitsImage(const std::string& filename)
    : fitsFile(std::make_unique<FITSFile>()) {
    load(filename);
}

FitsImage::FitsImage(int width, int height, int channels, DataType dataType)
    : fitsFile(std::make_unique<FITSFile>()), dataType(dataType) {
    fitsFile->createImageHDU(width, height, channels);

    // 设置BITPIX根据数据类型
    std::string bitpixValue;
    switch (dataType) {
        case DataType::BYTE:
            bitpixValue = "8";
            break;
        case DataType::SHORT:
            bitpixValue = "16";
            break;
        case DataType::INT:
            bitpixValue = "32";
            break;
        case DataType::LONG:
            bitpixValue = "64";
            break;
        case DataType::FLOAT:
            bitpixValue = "-32";
            break;
        case DataType::DOUBLE:
            bitpixValue = "-64";
            break;
    }

    getImageHDU().setHeaderKeyword("BITPIX", bitpixValue);
}

std::tuple<int, int, int> FitsImage::getSize() const {
    return getImageHDU().getImageSize();
}

void FitsImage::save(const std::string& filename) const {
    try {
        fitsFile->writeFITS(filename);
    } catch (const std::exception& e) {
        THROW_RUNTIME_ERROR("保存FITS文件失败: " + std::string(e.what()));
    }
}

void FitsImage::load(const std::string& filename) {
    try {
        fitsFile->readFITS(filename);

        // 从BITPIX确定数据类型
        if (fitsFile->getHDUCount() > 0) {
            const std::string bitpixStr =
                getImageHDU().getHeaderKeyword("BITPIX");
            int bitpix = std::stoi(bitpixStr);

            if (bitpix == 8) {
                dataType = DataType::BYTE;
            } else if (bitpix == 16) {
                dataType = DataType::SHORT;
            } else if (bitpix == 32) {
                dataType = DataType::INT;
            } else if (bitpix == 64) {
                dataType = DataType::LONG;
            } else if (bitpix == -32) {
                dataType = DataType::FLOAT;
            } else if (bitpix == -64) {
                dataType = DataType::DOUBLE;
            } else {
                THROW_RUNTIME_ERROR("不支持的BITPIX值: " + bitpixStr);
            }
        }
    } catch (const std::exception& e) {
        THROW_RUNTIME_ERROR("加载FITS文件失败: " + std::string(e.what()));
    }
}

struct ResizeOp {
    ImageHDU& hdu;
    int newWidth;
    int newHeight;

    template <FitsNumeric T>
    void operator()() {
        hdu.resize<T>(newWidth, newHeight);
    }
};

void FitsImage::resize(int newWidth, int newHeight) {
    auto& hdu = getImageHDU();

    dispatchByDataType(dataType, ResizeOp{hdu, newWidth, newHeight});
}

struct ThumbnailOp {
    const ImageHDU& hdu;
    int maxSize;
    std::unique_ptr<ImageHDU> result;

    template <FitsNumeric T>
    void operator()() {
        result = hdu.createThumbnail<T>(maxSize);
    }
};

std::unique_ptr<FitsImage> FitsImage::createThumbnail(int maxSize) const {
    const auto& hdu = getImageHDU();

    ThumbnailOp op{hdu, maxSize, nullptr};
    dispatchByDataType(dataType, op);

    if (!op.result) {
        THROW_RUNTIME_ERROR("创建缩略图失败");
    }

    auto thumbnail = std::make_unique<FitsImage>();
    thumbnail->fitsFile->addHDU(std::move(op.result));
    thumbnail->dataType = dataType;

    return thumbnail;
}

struct ROIOp {
    const ImageHDU& hdu;
    int x, y, width, height;
    std::unique_ptr<ImageHDU> result;

    template <FitsNumeric T>
    void operator()() {
        result = hdu.extractROI<T>(x, y, width, height);
    }
};

std::unique_ptr<FitsImage> FitsImage::extractROI(int x, int y, int width,
                                                 int height) const {
    const auto& hdu = getImageHDU();

    ROIOp op{hdu, x, y, width, height, nullptr};
    dispatchByDataType(dataType, op);

    if (!op.result) {
        THROW_RUNTIME_ERROR("提取ROI失败");
    }

    auto roi = std::make_unique<FitsImage>();
    roi->fitsFile->addHDU(std::move(op.result));
    roi->dataType = dataType;

    return roi;
}

void FitsImage::applyFilter(FilterType filterType, int kernelSize,
                            int channel) {
    std::vector<std::vector<double>> kernel;

    switch (filterType) {
        case FilterType::GAUSSIAN:
            kernel = createGaussianKernel(kernelSize);
            break;
        case FilterType::MEAN:
            kernel = createMeanKernel(kernelSize);
            break;
        case FilterType::SOBEL:
            kernel = createSobelKernelX();  // 默认使用X方向
            break;
        case FilterType::LAPLACIAN:
            kernel = createLaplacianKernel();
            break;
        default:
            THROW_INVALID_ARGUMENT("不支持的滤镜类型");
    }

    applyCustomFilter(kernel, channel);
}

struct FilterOp {
    ImageHDU& hdu;
    std::span<const std::span<const double>> spans;
    int channel;

    template <FitsNumeric T>
    void operator()() {
        hdu.applyFilterParallel<T>(spans, channel);
    }
};

void FitsImage::applyCustomFilter(
    const std::vector<std::vector<double>>& kernel, int channel) {
    if (kernel.empty() || kernel[0].empty()) {
        THROW_INVALID_ARGUMENT("无效的滤镜核");
    }

    auto spans = convertToSpan(kernel);
    auto& hdu = getImageHDU();

    dispatchByDataType(dataType, FilterOp{hdu, spans, channel});
}

struct BlendOp {
    ImageHDU& hdu;
    const ImageHDU& otherHdu;
    double alpha;
    int channel;

    template <FitsNumeric T>
    void operator()() {
        hdu.blendImage<T>(otherHdu, alpha, channel);
    }
};

void FitsImage::blend(const FitsImage& other, double alpha, int channel) {
    if (alpha < 0.0 || alpha > 1.0) {
        THROW_INVALID_ARGUMENT("混合系数alpha必须在0.0到1.0之间");
    }

    auto& hdu = getImageHDU();
    const auto& otherHdu = other.getImageHDU();

    dispatchByDataType(dataType, BlendOp{hdu, otherHdu, alpha, channel});
}

struct HistEqOp {
    ImageHDU& hdu;
    int width, height, channels;
    int channel;

    template <FitsNumeric T>
    void operator()() {
        // 对每个需要处理的通道应用直方图均衡化
        for (int c = 0; c < channels; ++c) {
            if (channel != -1 && c != channel)
                continue;

            // 1. 计算直方图
            std::vector<int> histogram(256, 0);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    T value = hdu.getPixel<T>(x, y, c);
                    // 映射到0-255范围
                    int bin = static_cast<int>((static_cast<double>(value) /
                                                getDataTypeRange<T>().second) *
                                               255);
                    bin = std::clamp(bin, 0, 255);
                    histogram[bin]++;
                }
            }

            // 2. 计算累积直方图
            std::vector<int> cdf(256, 0);
            cdf[0] = histogram[0];
            for (int i = 1; i < 256; ++i) {
                cdf[i] = cdf[i - 1] + histogram[i];
            }

            // 找到cdf的最小非零值
            int cdfMin = 0;
            for (int i = 0; i < 256; ++i) {
                if (cdf[i] > 0) {
                    cdfMin = cdf[i];
                    break;
                }
            }

            // 3. 应用直方图均衡化变换
            int pixelCount = width * height;
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    T value = hdu.getPixel<T>(x, y, c);
                    int bin = static_cast<int>((static_cast<double>(value) /
                                                getDataTypeRange<T>().second) *
                                               255);
                    bin = std::clamp(bin, 0, 255);

                    // 应用直方图均衡化公式
                    double newValue = 0;
                    if (pixelCount > cdfMin) {
                        newValue = static_cast<double>(cdf[bin] - cdfMin) /
                                   (pixelCount - cdfMin);
                    }

                    // 将结果映射回原始数据类型范围
                    T result =
                        static_cast<T>(newValue * getDataTypeRange<T>().second);
                    hdu.setPixel<T>(x, y, result, c);
                }
            }
        }
    }
};
void FitsImage::histogramEqualization(int channel) {
    auto& hdu = getImageHDU();
    auto [width, height, channels] = hdu.getImageSize();

    dispatchByDataType(dataType,
                       HistEqOp{hdu, width, height, channels, channel});
}

struct AutoLevelsOp {
    ImageHDU& hdu;
    int width, height, channels;
    double blackPoint, whitePoint;
    int channel;

    template <FitsNumeric T>
    void operator()() {
        for (int c = 0; c < channels; ++c) {
            if (channel != -1 && c != channel)
                continue;

            // 获取该通道的图像统计信息
            auto stats = hdu.computeImageStats<T>(c);

            // 构建直方图以找到黑点和白点对应的像素值
            std::vector<int> histogram(256, 0);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    T value = hdu.getPixel<T>(x, y, c);
                    // 归一化到0-255
                    double norm = (static_cast<double>(value) - stats.min) /
                                  (stats.max - stats.min);
                    int bin = std::clamp(static_cast<int>(norm * 255), 0, 255);
                    histogram[bin]++;
                }
            }

            // 计算累积直方图
            std::vector<double> cdf(256);
            cdf[0] = histogram[0];
            for (int i = 1; i < 256; ++i) {
                cdf[i] = cdf[i - 1] + histogram[i];
            }
            // 归一化cdf
            for (auto& val : cdf) {
                val /= cdf[255];
            }

            // 找到对应百分比的像素值
            int lowBin = 0, highBin = 255;
            for (int i = 0; i < 256; ++i) {
                if (cdf[i] >= blackPoint) {
                    lowBin = i;
                    break;
                }
            }
            for (int i = 255; i >= 0; --i) {
                if (cdf[i] <= whitePoint) {
                    highBin = i;
                    break;
                }
            }

            // 计算新的范围
            double newMin =
                stats.min + (stats.max - stats.min) * lowBin / 255.0;
            double newMax =
                stats.min + (stats.max - stats.min) * highBin / 255.0;

            // 拉伸对比度
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    T value = hdu.getPixel<T>(x, y, c);
                    double normalizedValue =
                        (static_cast<double>(value) - newMin) /
                        (newMax - newMin);
                    normalizedValue = std::clamp(normalizedValue, 0.0, 1.0);
                    T result = static_cast<T>(
                        normalizedValue * (stats.max - stats.min) + stats.min);
                    hdu.setPixel<T>(x, y, result, c);
                }
            }
        }
    }
};

void FitsImage::autoLevels(double blackPoint, double whitePoint, int channel) {
    if (blackPoint < 0.0 || blackPoint > 1.0 || whitePoint < 0.0 ||
        whitePoint > 1.0 || blackPoint >= whitePoint) {
        THROW_INVALID_ARGUMENT("黑点和白点必须在0-1范围内，且黑点必须小于白点");
    }

    auto& hdu = getImageHDU();
    auto [width, height, channels] = hdu.getImageSize();

    dispatchByDataType(dataType, AutoLevelsOp{hdu, width, height, channels,
                                              blackPoint, whitePoint, channel});
}

struct EdgeDetectionOp {
    ImageHDU& hdu;
    int width, height, channels;
    std::vector<std::vector<double>> kernelX, kernelY;
    int channel;

    template <FitsNumeric T>
    void operator()() {
        std::vector<std::span<const double>> spansX = convertToSpan(kernelX);
        std::vector<std::span<const double>> spansY = convertToSpan(kernelY);

        // Create a temporary HDU to store intermediate results
        auto tempHdu = std::make_unique<ImageHDU>();
        tempHdu->setImageSize(width, height, channels);

        // Copy original data to the temporary HDU
        for (int c = 0; c < channels; ++c) {
            if (channel != -1 && c != channel)
                continue;

            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    T value = hdu.getPixel<T>(x, y, c);
                    tempHdu->setPixel<T>(x, y, value, c);
                }
            }
        }

        // Apply the X-direction filter
        tempHdu->applyFilterParallel<T>(spansX, channel);

        // Create another temporary HDU for the Y-direction
        auto tempHduY = std::make_unique<ImageHDU>();
        tempHduY->setImageSize(width, height, channels);

        // Copy original data to tempHduY
        for (int c = 0; c < channels; ++c) {
            if (channel != -1 && c != channel)
                continue;

            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    T value = hdu.getPixel<T>(x, y, c);
                    tempHduY->setPixel<T>(x, y, value, c);
                }
            }
        }

        // Apply the Y-direction filter
        tempHduY->applyFilterParallel<T>(spansY, channel);

        // Calculate the gradient magnitude
        for (int c = 0; c < channels; ++c) {
            if (channel != -1 && c != channel)
                continue;

            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    T gx = tempHdu->getPixel<T>(x, y, c);
                    T gy = tempHduY->getPixel<T>(x, y, c);

                    // Calculate the gradient magnitude
                    double magnitude = std::sqrt(static_cast<double>(gx) * gx +
                                                 static_cast<double>(gy) * gy);

                    // Set the result (potential normalization might be needed
                    // depending on the desired output range)
                    hdu.setPixel<T>(x, y, static_cast<T>(magnitude), c);
                }
            }
        }
    }
};

void FitsImage::detectEdges(FilterType filterType, int channel) {
    std::vector<std::vector<double>> kernelX, kernelY;

    switch (filterType) {
        case FilterType::SOBEL:
            kernelX = createSobelKernelX();
            kernelY = createSobelKernelY();
            break;
        case FilterType::LAPLACIAN:
            // 拉普拉斯算子不需要X和Y方向，直接应用拉普拉斯滤镜
            applyFilter(FilterType::LAPLACIAN, 3, channel);
            return;
        default:
            THROW_INVALID_ARGUMENT("不支持的边缘检测类型");
    }

    auto& hdu = getImageHDU();
    auto [width, height, channels] = hdu.getImageSize();

    dispatchByDataType(dataType, EdgeDetectionOp{hdu, width, height, channels,
                                                 kernelX, kernelY, channel});
}

struct MorphologyOp {
    ImageHDU& hdu;
    int width, height, channels;
    std::vector<std::vector<double>> kernel;
    MorphologicalOperation operation;
    int kernelSize;
    int channel;

    template <FitsNumeric T>
    void operator()() {
        // 对每个需要处理的通道应用形态学操作
        for (int c = 0; c < channels; ++c) {
            if (channel != -1 && c != channel)
                continue;

            // 创建临时图像
            auto tempHdu = std::make_unique<ImageHDU>();
            tempHdu->setImageSize(width, height, channels);

            // 复制数据
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    tempHdu->setPixel<T>(x, y, hdu.getPixel<T>(x, y, c), c);
                }
            }

            int radius = kernelSize / 2;

            // 应用形态学操作
            switch (operation) {
                case MorphologicalOperation::DILATE: {
                    // 膨胀操作
                    for (int y = 0; y < height; ++y) {
                        for (int x = 0; x < width; ++x) {
                            T maxVal = std::numeric_limits<T>::min();

                            // 在核范围内寻找最大值
                            for (int ky = -radius; ky <= radius; ++ky) {
                                for (int kx = -radius; kx <= radius; ++kx) {
                                    int nx = x + kx;
                                    int ny = y + ky;

                                    if (nx >= 0 && nx < width && ny >= 0 &&
                                        ny < height) {
                                        T val = hdu.getPixel<T>(nx, ny, c);
                                        maxVal = std::max(maxVal, val);
                                    }
                                }
                            }

                            tempHdu->setPixel<T>(x, y, maxVal, c);
                        }
                    }
                    break;
                }
                case MorphologicalOperation::ERODE: {
                    // 腐蚀操作
                    for (int y = 0; y < height; ++y) {
                        for (int x = 0; x < width; ++x) {
                            T minVal = std::numeric_limits<T>::max();

                            // 在核范围内寻找最小值
                            for (int ky = -radius; ky <= radius; ++ky) {
                                for (int kx = -radius; kx <= radius; ++kx) {
                                    int nx = x + kx;
                                    int ny = y + ky;

                                    if (nx >= 0 && nx < width && ny >= 0 &&
                                        ny < height) {
                                        T val = hdu.getPixel<T>(nx, ny, c);
                                        minVal = std::min(minVal, val);
                                    }
                                }
                            }

                            tempHdu->setPixel<T>(x, y, minVal, c);
                        }
                    }
                    break;
                }
                // 其他形态学操作可以类似地实现
                default:
                    THROW_INVALID_ARGUMENT("暂不支持的形态学操作");
            }

            // 将结果复制回原图像
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    hdu.setPixel<T>(x, y, tempHdu->getPixel<T>(x, y, c), c);
                }
            }
        }
    }
};

void FitsImage::applyMorphology(MorphologicalOperation operation,
                                int kernelSize, int channel) {
    if (kernelSize % 2 == 0 || kernelSize < 3) {
        THROW_INVALID_ARGUMENT("核大小必须是大于等于3的奇数");
    }

    auto& hdu = getImageHDU();
    auto [width, height, channels] = hdu.getImageSize();

    // 创建形态学操作用的核
    std::vector<std::vector<double>> kernel(
        kernelSize, std::vector<double>(kernelSize, 1.0));

    dispatchByDataType(
        dataType, MorphologyOp{hdu, width, height, channels, kernel, operation,
                               kernelSize, channel});
}

void FitsImage::removeNoise(FilterType filterType, int strength, int channel) {
    switch (filterType) {
        case FilterType::MEDIAN:
            // 中值滤波是去噪的常用方法
            applyFilter(FilterType::MEDIAN, strength, channel);
            break;
        case FilterType::GAUSSIAN:
            // 高斯滤波也可以用于去噪
            applyFilter(FilterType::GAUSSIAN, strength, channel);
            break;
        case FilterType::MEAN:
            applyFilter(FilterType::MEAN, strength, channel);
            break;
        default:
            THROW_INVALID_ARGUMENT("不支持的去噪滤镜类型");
    }
}

struct AddNoiseOp {
    ImageHDU& hdu;
    int width, height, channels;
    NoiseType noiseType;
    double strength;
    int channel;
    std::mt19937& gen;

    template <FitsNumeric T>
    void operator()() {
        auto [minVal, maxVal] = getDataTypeRange<T>();

        for (int c = 0; c < channels; ++c) {
            if (channel != -1 && c != channel)
                continue;

            switch (noiseType) {
                case NoiseType::GAUSSIAN: {
                    // 高斯噪声
                    std::normal_distribution<double> dist(0.0,
                                                          strength * maxVal);

                    for (int y = 0; y < height; ++y) {
                        for (int x = 0; x < width; ++x) {
                            T value = hdu.getPixel<T>(x, y, c);
                            double noise = dist(gen);
                            double newValue =
                                static_cast<double>(value) + noise;
                            newValue = std::clamp(newValue, minVal, maxVal);
                            hdu.setPixel<T>(x, y, static_cast<T>(newValue), c);
                        }
                    }
                    break;
                }
                case NoiseType::SALT_PEPPER: {
                    // 椒盐噪声
                    std::uniform_real_distribution<double> dist(0.0, 1.0);

                    for (int y = 0; y < height; ++y) {
                        for (int x = 0; x < width; ++x) {
                            double rand = dist(gen);
                            if (rand < strength) {
                                T value = (rand < strength / 2)
                                              ? static_cast<T>(minVal)
                                              : static_cast<T>(maxVal);
                                hdu.setPixel<T>(x, y, value, c);
                            }
                        }
                    }
                    break;
                }
                default:
                    THROW_INVALID_ARGUMENT("暂不支持的噪声类型");
            }
        }
    }
};

void FitsImage::addNoise(NoiseType noiseType, double strength, int channel) {
    auto& hdu = getImageHDU();
    auto [width, height, channels] = hdu.getImageSize();

    // 创建随机数生成器
    std::random_device rd;
    std::mt19937 gen(rd());

    dispatchByDataType(dataType, AddNoiseOp{hdu, width, height, channels,
                                            noiseType, strength, channel, gen});
}

ImageHDU& FitsImage::getImageHDU() {
    if (fitsFile->isEmpty()) {
        fitsFile->createImageHDU(0, 0);
    }
    return fitsFile->getHDUAs<ImageHDU>(0);
}

const ImageHDU& FitsImage::getImageHDU() const {
    if (fitsFile->isEmpty()) {
        THROW_RUNTIME_ERROR("FITS文件为空");
    }
    return fitsFile->getHDUAs<ImageHDU>(0);
}

struct GetPixelOp {
    const ImageHDU& hdu;
    int x, y, channel;
    double result = 0.0;

    template <FitsNumeric T>
    void operator()() {
        result = static_cast<double>(hdu.getPixel<T>(x, y, channel));
    }
};

double FitsImage::getPixel(int x, int y, int channel) const {
    const auto& hdu = getImageHDU();

    GetPixelOp op{hdu, x, y, channel};
    dispatchByDataType(dataType, op);

    return op.result;
}

struct SetPixelOp {
    ImageHDU& hdu;
    int x, y, channel;
    double value;

    template <FitsNumeric T>
    void operator()() {
        hdu.setPixel<T>(x, y, static_cast<T>(value), channel);
    }
};

void FitsImage::setPixel(int x, int y, double value, int channel) {
    auto& hdu = getImageHDU();

    dispatchByDataType(dataType, SetPixelOp{hdu, x, y, channel, value});
}

// 工具函数实现
std::unique_ptr<FitsImage> loadFitsImage(const std::string& filename) {
    try {
        return std::make_unique<FitsImage>(filename);
    } catch (const std::exception& e) {
        THROW_RUNTIME_ERROR("加载FITS图像失败: " + std::string(e.what()));
    }
}

std::unique_ptr<FitsImage> loadFitsThumbnail(const std::string& filename,
                                             int maxSize) {
    try {
        auto image = loadFitsImage(filename);
        return image->createThumbnail(maxSize);
    } catch (const std::exception& e) {
        THROW_RUNTIME_ERROR("加载FITS缩略图失败: " + std::string(e.what()));
    }
}

std::unique_ptr<FitsImage> createFitsImage(int width, int height, int channels,
                                           DataType dataType) {
    try {
        return std::make_unique<FitsImage>(width, height, channels, dataType);
    } catch (const std::exception& e) {
        THROW_RUNTIME_ERROR("创建FITS图像失败: " + std::string(e.what()));
    }
}

bool isValidFits(const std::string& filename) {
    if (!std::filesystem::exists(filename)) {
        return false;
    }

    try {
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            return false;
        }

        // 检查FITS文件标识符 "SIMPLE  ="
        char header[80];
        file.read(header, 80);
        std::string headerStr(header, 80);

        return headerStr.substr(0, 9) == "SIMPLE  =";
    } catch (...) {
        return false;
    }
}

std::optional<std::tuple<int, int, int>> getFitsImageInfo(
    const std::string& filename) {
    if (!isValidFits(filename)) {
        return std::nullopt;
    }

    try {
        FITSFile file(filename);
        if (file.isEmpty()) {
            return std::nullopt;
        }

        const auto& hdu = file.getHDUAs<ImageHDU>(0);
        return hdu.getImageSize();
    } catch (...) {
        return std::nullopt;
    }
}

#ifdef ATOM_ENABLE_OPENCV
// OpenCV相关功能实现

FitsImage::FitsImage(const cv::Mat& mat, DataType dataType)
    : fitsFile(std::make_unique<FITSFile>()), dataType(dataType) {
    // 如果dataType是自动选择，根据Mat类型确定
    if (dataType == DataType::SHORT) {
        dataType = opencvTypeToFitsType(mat.type());
    }

    // 获取尺寸和通道数
    int width = mat.cols;
    int height = mat.rows;
    int channels = mat.channels();

    // 创建图像
    fitsFile->createImageHDU(width, height, channels);

    // 设置BITPIX
    std::string bitpixValue;
    switch (dataType) {
        case DataType::BYTE:
            bitpixValue = "8";
            break;
        case DataType::SHORT:
            bitpixValue = "16";
            break;
        case DataType::INT:
            bitpixValue = "32";
            break;
        case DataType::LONG:
            bitpixValue = "64";
            break;
        case DataType::FLOAT:
            bitpixValue = "-32";
            break;
        case DataType::DOUBLE:
            bitpixValue = "-64";
            break;
    }
    getImageHDU().setHeaderKeyword("BITPIX", bitpixValue);

    // 根据数据类型复制数据
    switch (dataType) {
        case DataType::BYTE: {
            cv::Mat convertedMat;
            if (mat.type() != CV_8UC(channels)) {
                mat.convertTo(convertedMat, CV_8UC(channels));
            } else {
                convertedMat = mat;
            }

            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    for (int c = 0; c < channels; ++c) {
                        uint8_t value =
                            convertedMat
                                .data[y * convertedMat.step + x * channels + c];
                        setPixel(x, y, static_cast<double>(value), c);
                    }
                }
            }
            break;
        }
        case DataType::SHORT: {
            cv::Mat convertedMat;
            if (mat.type() != CV_16SC(channels)) {
                mat.convertTo(convertedMat, CV_16SC(channels));
            } else {
                convertedMat = mat;
            }

            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    for (int c = 0; c < channels; ++c) {
                        int16_t* ptr =
                            reinterpret_cast<int16_t*>(convertedMat.data);
                        int16_t value =
                            ptr[y * (convertedMat.step / sizeof(int16_t)) +
                                x * channels + c];
                        setPixel(x, y, static_cast<double>(value), c);
                    }
                }
            }
            break;
        }
        // 其他数据类型类似实现
        default:
            THROW_RUNTIME_ERROR("暂不支持此数据类型的OpenCV转换");
    }
}

cv::Mat FitsImage::toMat() const {
    auto [width, height, channels] = getSize();
    int cvType = fitsTypeToOpenCVType(dataType);

    cv::Mat result(height, width, cvType);

    // 根据数据类型复制数据
    switch (dataType) {
        case DataType::BYTE: {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    for (int c = 0; c < channels; ++c) {
                        double value = getPixel(x, y, c);
                        result.data[y * result.step + x * channels + c] =
                            static_cast<uint8_t>(value);
                    }
                }
            }
            break;
        }
        case DataType::SHORT: {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    for (int c = 0; c < channels; ++c) {
                        double value = getPixel(x, y, c);
                        int16_t* ptr = reinterpret_cast<int16_t*>(result.data);
                        ptr[y * (result.step / sizeof(int16_t)) + x * channels +
                            c] = static_cast<int16_t>(value);
                    }
                }
            }
            break;
        }
        // 其他数据类型类似实现
        default:
            THROW_RUNTIME_ERROR("暂不支持此数据类型的OpenCV转换");
    }

    return result;
}

void FitsImage::applyOpenCVFilter(
    const std::function<cv::Mat(const cv::Mat&)>& filter, int channel) {
    cv::Mat image = toMat();
    auto [width, height, channels] = getSize();

    // 处理channel参数
    if (channel == -1) {
        // 处理所有通道
        cv::Mat result = filter(image);

        // 确保结果尺寸和通道数一致
        if (result.cols != width || result.rows != height ||
            result.channels() != channels) {
            THROW_RUNTIME_ERROR("OpenCV滤镜更改了图像尺寸或通道数，无法应用");
        }

        // 将结果复制回FITS图像
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                for (int c = 0; c < channels; ++c) {
                    if (dataType == DataType::BYTE) {
                        uint8_t value =
                            result.data[y * result.step + x * channels + c];
                        setPixel(x, y, static_cast<double>(value), c);
                    } else if (dataType == DataType::SHORT) {
                        int16_t* ptr = reinterpret_cast<int16_t*>(result.data);
                        int16_t value =
                            ptr[y * (result.step / sizeof(int16_t)) +
                                x * channels + c];
                        setPixel(x, y, static_cast<double>(value), c);
                    }
                    // 其他数据类型类似实现
                }
            }
        }
    } else {
        // 处理单个通道
        if (channel < 0 || channel >= channels) {
            throw std::out_of_range("通道索引超出范围");
        }

        // 提取单通道
        std::vector<cv::Mat> channelMats;
        cv::split(image, channelMats);

        // 应用滤镜到指定通道
        channelMats[channel] = filter(channelMats[channel]);

        // 合并通道
        cv::merge(channelMats, image);

        // 将结果复制回FITS图像
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                for (int c = 0; c < channels; ++c) {
                    if (c == channel) {
                        if (dataType == DataType::BYTE) {
                            uint8_t value =
                                image.data[y * image.step + x * channels + c];
                            setPixel(x, y, static_cast<double>(value), c);
                        } else if (dataType == DataType::SHORT) {
                            int16_t* ptr =
                                reinterpret_cast<int16_t*>(image.data);
                            int16_t value =
                                ptr[y * (image.step / sizeof(int16_t)) +
                                    x * channels + c];
                            setPixel(x, y, static_cast<double>(value), c);
                        }
                        // 其他数据类型类似实现
                    }
                }
            }
        }
    }
}

void FitsImage::processWithOpenCV(const std::string& functionName,
                                  const std::map<std::string, double>& params) {
    cv::Mat image = toMat();
    cv::Mat result;

    // 支持常用的OpenCV函数
    if (functionName == "GaussianBlur") {
        int ksize = static_cast<int>(params.at("ksize"));
        double sigma = params.count("sigma") ? params.at("sigma") : 0;
        cv::GaussianBlur(image, result, cv::Size(ksize, ksize), sigma);
    } else if (functionName == "Canny") {
        double threshold1 = params.at("threshold1");
        double threshold2 = params.at("threshold2");
        cv::Canny(image, result, threshold1, threshold2);
    } else if (functionName == "adaptiveThreshold") {
        double maxValue = params.at("maxValue");
        int adaptiveMethod = static_cast<int>(params.at("adaptiveMethod"));
        int thresholdType = static_cast<int>(params.at("thresholdType"));
        int blockSize = static_cast<int>(params.at("blockSize"));
        double C = params.at("C");
        cv::adaptiveThreshold(image, result, maxValue, adaptiveMethod,
                              thresholdType, blockSize, C);
    } else if (functionName == "medianBlur") {
        int ksize = static_cast<int>(params.at("ksize"));
        cv::medianBlur(image, result, ksize);
    } else if (functionName == "resize") {
        int width = static_cast<int>(params.at("width"));
        int height = static_cast<int>(params.at("height"));
        int interpolation = params.count("interpolation")
                                ? static_cast<int>(params.at("interpolation"))
                                : cv::INTER_LINEAR;
        cv::resize(image, result, cv::Size(width, height), 0, 0, interpolation);
    } else {
        THROW_RUNTIME_ERROR("不支持的OpenCV函数: " + functionName);
    }

    // 将结果转回FITS图像
    auto resultImage = createFitsFromMat(result, dataType);

    // 将结果复制到当前图像
    auto [width, height, channels] = resultImage->getSize();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < channels; ++c) {
                setPixel(x, y, resultImage->getPixel(x, y, c), c);
            }
        }
    }
}

DataType FitsImage::opencvTypeToFitsType(int cvType) {
    switch (cvType & CV_MAT_DEPTH_MASK) {
        case CV_8U:
            return DataType::BYTE;
        case CV_8S:
            return DataType::BYTE;
        case CV_16S:
            return DataType::SHORT;
        case CV_16U:
            return DataType::SHORT;
        case CV_32S:
            return DataType::INT;
        case CV_32F:
            return DataType::FLOAT;
        case CV_64F:
            return DataType::DOUBLE;
        default:
            return DataType::SHORT;  // 默认使用SHORT
    }
}

int FitsImage::fitsTypeToOpenCVType(DataType type) {
    int cvType;
    switch (type) {
        case DataType::BYTE:
            cvType = CV_8U;
            break;
        case DataType::SHORT:
            cvType = CV_16S;
            break;
        case DataType::INT:
            cvType = CV_32S;
            break;
        case DataType::LONG:
            cvType = CV_32S;
            break;  // OpenCV没有64位整数类型，使用32位代替
        case DataType::FLOAT:
            cvType = CV_32F;
            break;
        case DataType::DOUBLE:
            cvType = CV_64F;
            break;
        default:
            cvType = CV_16S;
            break;  // 默认使用16位有符号整数
    }

    auto [width, height, channels] = getSize();
    return CV_MAKETYPE(cvType, channels);
}

std::unique_ptr<FitsImage> createFitsFromMat(const cv::Mat& mat,
                                             DataType dataType) {
    return std::make_unique<FitsImage>(mat, dataType);
}

int processFitsDirectory(const std::string& inputDir,
                         const std::string& outputDir,
                         const std::function<void(FitsImage&)>& processor,
                         bool recursive) {
    namespace fs = std::filesystem;

    // 确保输出目录存在
    if (!fs::exists(outputDir)) {
        fs::create_directories(outputDir);
    }

    int processedCount = 0;

    // 遍历目录
    fs::directory_iterator end_iter;
    fs::directory_iterator iter(inputDir);

    if (recursive) {
        fs::recursive_directory_iterator recIter(inputDir);
        fs::recursive_directory_iterator recEndIter;

        for (; recIter != recEndIter; ++recIter) {
            if (fs::is_regular_file(*recIter) &&
                recIter->path().extension() == ".fits") {
                try {
                    // 计算输出路径
                    fs::path relativePath =
                        fs::relative(recIter->path(), inputDir);
                    fs::path outputPath = fs::path(outputDir) / relativePath;

                    // 确保输出目录存在
                    fs::create_directories(outputPath.parent_path());

                    // 加载、处理和保存图像
                    auto image = loadFitsImage(recIter->path().string());
                    processor(*image);
                    image->save(outputPath.string());

                    processedCount++;
                } catch (const std::exception& e) {
                    std::cerr << "处理文件 " << recIter->path().string()
                              << " 失败: " << e.what() << std::endl;
                }
            }
        }
    } else {
        for (; iter != end_iter; ++iter) {
            if (fs::is_regular_file(*iter) &&
                iter->path().extension() == ".fits") {
                try {
                    // 计算输出路径
                    fs::path outputPath =
                        fs::path(outputDir) / iter->path().filename();

                    // 加载、处理和保存图像
                    auto image = loadFitsImage(iter->path().string());
                    processor(*image);
                    image->save(outputPath.string());

                    processedCount++;
                } catch (const std::exception& e) {
                    std::cerr << "处理文件 " << iter->path().string()
                              << " 失败: " << e.what() << std::endl;
                }
            }
        }
    }

    return processedCount;
}
#endif  // ATOM_ENABLE_OPENCV

}  // namespace image
}  // namespace atom