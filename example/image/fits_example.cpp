#include <iostream>
#include <string>
#include "atom/image/fits_utils.hpp"

using namespace atom::image;

// 这个示例展示了如何使用简化的FITS API进行常见图像处理操作
int main(int argc, char** argv) {
    // 检查命令行参数
    if (argc < 3) {
        std::cerr << "用法: " << argv[0] << " <输入FITS文件> <输出FITS文件>"
                  << std::endl;
        return 1;
    }

    std::string inputFilename = argv[1];
    std::string outputFilename = argv[2];

    try {
        // 检查文件是否有效
        if (!isValidFits(inputFilename)) {
            std::cerr << "无效的FITS文件: " << inputFilename << std::endl;
            return 1;
        }

        // 获取FITS文件基本信息
        auto imageInfo = getFitsImageInfo(inputFilename);
        if (!imageInfo) {
            std::cerr << "无法获取FITS文件信息: " << inputFilename << std::endl;
            return 1;
        }

        auto [width, height, channels] = *imageInfo;
        std::cout << "FITS图像信息:\n";
        std::cout << "  宽度: " << width << "\n";
        std::cout << "  高度: " << height << "\n";
        std::cout << "  通道数: " << channels << "\n";

        // 加载图像
        auto image = loadFitsImage(inputFilename);
        std::cout << "成功加载FITS图像\n";

        // 图像处理操作示例

        // 1. 自动调整色阶增强对比度
        std::cout << "应用自动色阶调整...\n";
        image->autoLevels(0.01, 0.99);

        // 2. 应用高斯滤镜平滑图像
        std::cout << "应用高斯滤镜...\n";
        image->applyFilter(FilterType::GAUSSIAN, 3);

        // 3. 生成缩略图
        std::cout << "创建缩略图...\n";
        auto thumbnail = image->createThumbnail(256);

        // 4. 提取感兴趣区域
        int roiX = width / 4;
        int roiY = height / 4;
        int roiWidth = width / 2;
        int roiHeight = height / 2;
        std::cout << "提取中心区域 (" << roiX << "," << roiY << "," << roiWidth
                  << "," << roiHeight << ")...\n";
        auto roi = image->extractROI(roiX, roiY, roiWidth, roiHeight);

        // 5. 边缘检测
        std::cout << "应用边缘检测...\n";
        roi->detectEdges(FilterType::SOBEL);

        // 保存处理后的图像和缩略图
        std::cout << "保存处理后的图像: " << outputFilename << std::endl;
        image->save(outputFilename);

        std::string thumbnailFilename =
            outputFilename.substr(0, outputFilename.find_last_of(".")) +
            "_thumb.fits";
        std::cout << "保存缩略图: " << thumbnailFilename << std::endl;
        thumbnail->save(thumbnailFilename);

        std::string roiFilename =
            outputFilename.substr(0, outputFilename.find_last_of(".")) +
            "_roi.fits";
        std::cout << "保存ROI: " << roiFilename << std::endl;
        roi->save(roiFilename);

#ifdef ATOM_ENABLE_OPENCV
        // 如果启用了OpenCV支持，展示OpenCV特定功能
        std::cout << "\nOpenCV功能示例:\n";

        // 1. 转换为OpenCV Mat处理
        std::cout << "将FITS转换为OpenCV Mat...\n";
        cv::Mat cvImage = image->toMat();

        // 2. 应用特定OpenCV函数
        std::cout << "应用OpenCV特定的处理...\n";
        image->processWithOpenCV("GaussianBlur",
                                 {{"ksize", 5}, {"sigma", 1.5}});

        // 3. 自定义OpenCV滤镜
        std::cout << "应用自定义OpenCV滤镜...\n";
        image->applyOpenCVFilter([](const cv::Mat& src) {
            cv::Mat dst;
            cv::medianBlur(src, dst, 3);
            cv::Laplacian(dst, dst, -1, 3);
            return dst;
        });

        // 保存OpenCV处理后的图像
        std::string opencvFilename =
            outputFilename.substr(0, outputFilename.find_last_of(".")) +
            "_opencv.fits";
        std::cout << "保存OpenCV处理的图像: " << opencvFilename << std::endl;
        image->save(opencvFilename);
#endif

        std::cout << "所有处理完成！\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
}
