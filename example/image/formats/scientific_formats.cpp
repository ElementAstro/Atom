/*
 * scientific_formats.cpp
 *
 * Scientific image format processing example
 * Part of the Atom project
 * Author: Max Qian
 * License: GPL3
 */

#include <iostream>
#include <string>
#include <vector>

#ifdef HAVE_OPENCV
#include <opencv2/opencv.hpp>
#endif

/**
 * @brief Demonstrates scientific image format processing
 *
 * This example shows how to handle scientific image formats commonly
 * used in research, astronomy, and scientific imaging applications.
 */
int main() {
    std::cout << "Scientific Image Format Processing Example\n";
    std::cout << "==========================================\n\n";

    std::cout << "Supported scientific image formats:\n";
    std::cout << "- FITS (Flexible Image Transport System)\n";
    std::cout << "- HDF5 (Hierarchical Data Format)\n";
    std::cout << "- NetCDF (Network Common Data Form)\n";
    std::cout << "- TIFF (Tagged Image File Format) - scientific variants\n";
    std::cout << "- OpenEXR (high dynamic range)\n";
    std::cout << "- PFM (Portable Float Map)\n\n";

#ifdef HAVE_OPENCV
    try {
        std::cout << "Simulating scientific image processing workflow...\n";

        // Create a simulated scientific image with high dynamic range
        cv::Mat scientific_image = cv::Mat::zeros(512, 512, CV_32FC1);

        // Simulate scientific data with wide dynamic range
        cv::randu(scientific_image, cv::Scalar(0.0), cv::Scalar(65535.0));

        // Add some scientific features (e.g., astronomical objects)
        cv::circle(scientific_image, cv::Point(128, 128), 30,
                   cv::Scalar(50000.0), -1);
        cv::circle(scientific_image, cv::Point(384, 384), 20,
                   cv::Scalar(45000.0), -1);
        cv::circle(scientific_image, cv::Point(256, 100), 15,
                   cv::Scalar(40000.0), -1);

        std::cout << "1. Loading scientific image (simulated)\n";
        std::cout << "   Image size: " << scientific_image.cols << "x"
                  << scientific_image.rows << std::endl;
        std::cout << "   Data type: 32-bit floating point\n";
        std::cout << "   Dynamic range: 0.0 - 65535.0\n";

        // Calculate statistics
        double min_val, max_val;
        cv::minMaxLoc(scientific_image, &min_val, &max_val);
        cv::Scalar mean_val = cv::mean(scientific_image);
        cv::Scalar std_val;
        cv::meanStdDev(scientific_image, mean_val, std_val);

        std::cout << "2. Image statistics calculated\n";
        std::cout << "   Min value: " << min_val << std::endl;
        std::cout << "   Max value: " << max_val << std::endl;
        std::cout << "   Mean value: " << mean_val[0] << std::endl;
        std::cout << "   Std deviation: " << std_val[0] << std::endl;

        // Logarithmic scaling (common in astronomy)
        cv::Mat log_scaled;
        cv::log(scientific_image + 1.0, log_scaled);
        std::cout << "3. Logarithmic scaling applied\n";

        // Percentile-based contrast stretching
        std::vector<float> data;
        if (log_scaled.isContinuous()) {
            data.assign((float*)log_scaled.data,
                        (float*)log_scaled.data + log_scaled.total());
        }
        std::sort(data.begin(), data.end());

        float p1 =
            data[static_cast<size_t>(data.size() * 0.01)];  // 1st percentile
        float p99 =
            data[static_cast<size_t>(data.size() * 0.99)];  // 99th percentile

        cv::Mat stretched;
        log_scaled.convertTo(stretched, CV_8UC1, 255.0 / (p99 - p1),
                             -p1 * 255.0 / (p99 - p1));
        std::cout << "4. Percentile-based contrast stretching applied\n";
        std::cout << "   1st percentile: " << p1 << std::endl;
        std::cout << "   99th percentile: " << p99 << std::endl;

        // Background subtraction (common in scientific imaging)
        cv::Mat background;
        cv::medianBlur(stretched, background,
                       51);  // Large kernel for background estimation
        cv::Mat background_subtracted;
        cv::subtract(stretched, background, background_subtracted);
        std::cout << "5. Background subtraction applied\n";

        // Noise reduction with edge preservation
        cv::Mat denoised;
        cv::bilateralFilter(background_subtracted, denoised, 9, 75, 75);
        std::cout << "6. Edge-preserving noise reduction applied\n";

        // Feature detection (e.g., for astronomical objects)
        cv::Mat binary;
        cv::threshold(denoised, binary, 50, 255, cv::THRESH_BINARY);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(binary, contours, cv::RETR_EXTERNAL,
                         cv::CHAIN_APPROX_SIMPLE);

        std::cout << "7. Feature detection completed\n";
        std::cout << "   Features detected: " << contours.size() << std::endl;

        // Analyze detected features
        for (size_t i = 0; i < contours.size() && i < 5; ++i) {
            cv::Moments moments = cv::moments(contours[i]);
            if (moments.m00 > 0) {
                double cx = moments.m10 / moments.m00;
                double cy = moments.m01 / moments.m00;
                double area = cv::contourArea(contours[i]);

                std::cout << "   Feature " << i + 1 << ": center(" << cx << ", "
                          << cy << "), area=" << area << std::endl;
            }
        }

        // Photometry (intensity measurement)
        cv::Mat photometry_mask =
            cv::Mat::zeros(scientific_image.size(), CV_8UC1);
        for (const auto& contour : contours) {
            cv::fillPoly(photometry_mask,
                         std::vector<std::vector<cv::Point>>{contour},
                         cv::Scalar(255));
        }

        cv::Scalar total_flux =
            cv::sum(scientific_image.mul(photometry_mask / 255.0));
        std::cout << "8. Photometry analysis completed\n";
        std::cout << "   Total flux in detected features: " << total_flux[0]
                  << std::endl;

        std::cout << "\nScientific image processing pipeline completed!\n";
        std::cout << "Common scientific imaging applications:\n";
        std::cout << "- Astronomical object detection and photometry\n";
        std::cout << "- Microscopy image analysis\n";
        std::cout << "- Spectroscopic data processing\n";
        std::cout << "- High dynamic range imaging\n";
        std::cout << "- Quantitative measurements\n";

    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV error: " << e.what() << std::endl;
        return 1;
    }
#else
    std::cout << "OpenCV not available. Scientific image processing example "
                 "cannot run."
              << std::endl;
    std::cout << "This is a placeholder implementation." << std::endl;

    std::cout << "\nScientific image processing would typically include:\n";
    std::cout << "- FITS file handling with header metadata\n";
    std::cout << "- Calibration frame processing (dark, flat, bias)\n";
    std::cout << "- Astrometric and photometric calibration\n";
    std::cout << "- Multi-wavelength image registration\n";
    std::cout << "- Statistical analysis and uncertainty propagation\n";
#endif

    std::cout << "\nScientific image format processing example completed."
              << std::endl;
    return 0;
}
