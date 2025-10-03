/*
 * medical_formats.cpp
 *
 * Medical image format processing example
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
 * @brief Demonstrates medical image format processing
 * 
 * This example shows how to handle medical image formats like DICOM
 * and perform basic medical image processing operations.
 */
int main() {
    std::cout << "Medical Image Format Processing Example\n";
    std::cout << "=======================================\n\n";

    std::cout << "Supported medical image formats:\n";
    std::cout << "- DICOM (.dcm, .dicom)\n";
    std::cout << "- NIfTI (.nii, .nii.gz)\n";
    std::cout << "- Analyze (.hdr/.img)\n";
    std::cout << "- MetaImage (.mhd/.raw)\n";
    std::cout << "- NRRD (.nrrd)\n\n";

#ifdef HAVE_OPENCV
    try {
        std::cout << "Simulating medical image processing workflow...\n";

        // Create a simulated medical image (16-bit grayscale)
        cv::Mat medical_image = cv::Mat::zeros(256, 256, CV_16UC1);
        
        // Simulate medical image data with typical intensity ranges
        cv::randu(medical_image, cv::Scalar(0), cv::Scalar(4095));
        
        // Add some anatomical-like structures
        cv::circle(medical_image, cv::Point(128, 128), 80, cv::Scalar(3000), -1);
        cv::circle(medical_image, cv::Point(128, 128), 60, cv::Scalar(2000), -1);
        cv::circle(medical_image, cv::Point(128, 128), 40, cv::Scalar(1000), -1);

        std::cout << "1. Loading medical image (simulated)\n";
        std::cout << "   Image size: " << medical_image.cols << "x" << medical_image.rows << std::endl;
        std::cout << "   Bit depth: 16-bit grayscale\n";
        std::cout << "   Intensity range: 0-4095\n";

        // Window/Level adjustment (common in medical imaging)
        cv::Mat windowed;
        double window_center = 2000;
        double window_width = 1000;
        double min_val = window_center - window_width / 2;
        double max_val = window_center + window_width / 2;
        
        medical_image.convertTo(windowed, CV_8UC1, 255.0 / window_width, -min_val * 255.0 / window_width);
        std::cout << "2. Window/Level adjustment applied\n";
        std::cout << "   Window center: " << window_center << std::endl;
        std::cout << "   Window width: " << window_width << std::endl;

        // Histogram equalization for better contrast
        cv::Mat equalized;
        cv::equalizeHist(windowed, equalized);
        std::cout << "3. Histogram equalization applied\n";

        // Noise reduction (important for medical images)
        cv::Mat denoised;
        cv::medianBlur(equalized, denoised, 3);
        std::cout << "4. Median filtering for noise reduction\n";

        // Edge enhancement
        cv::Mat enhanced;
        cv::Mat kernel = (cv::Mat_<float>(3,3) << 
                         0, -1, 0,
                         -1, 5, -1,
                         0, -1, 0);
        cv::filter2D(denoised, enhanced, -1, kernel);
        std::cout << "5. Edge enhancement applied\n";

        // Region of Interest (ROI) analysis
        cv::Rect roi(64, 64, 128, 128);
        cv::Mat roi_image = enhanced(roi);
        
        cv::Scalar mean_intensity = cv::mean(roi_image);
        cv::Scalar std_intensity;
        cv::meanStdDev(roi_image, mean_intensity, std_intensity);
        
        std::cout << "6. ROI analysis completed\n";
        std::cout << "   ROI location: (" << roi.x << ", " << roi.y << ") " 
                  << roi.width << "x" << roi.height << std::endl;
        std::cout << "   Mean intensity: " << mean_intensity[0] << std::endl;
        std::cout << "   Std deviation: " << std_intensity[0] << std::endl;

        // Thresholding for segmentation
        cv::Mat segmented;
        cv::threshold(enhanced, segmented, 128, 255, cv::THRESH_BINARY);
        std::cout << "7. Binary segmentation applied\n";

        // Morphological operations
        cv::Mat morphed;
        cv::Mat element = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
        cv::morphologyEx(segmented, morphed, cv::MORPH_CLOSE, element);
        std::cout << "8. Morphological closing applied\n";

        std::cout << "\nMedical image processing pipeline completed!\n";
        std::cout << "Typical medical imaging applications:\n";
        std::cout << "- Diagnostic imaging enhancement\n";
        std::cout << "- Anatomical structure segmentation\n";
        std::cout << "- Quantitative analysis\n";
        std::cout << "- 3D reconstruction\n";
        std::cout << "- Computer-aided diagnosis\n";

    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV error: " << e.what() << std::endl;
        return 1;
    }
#else
    std::cout << "OpenCV not available. Medical image processing example cannot run." << std::endl;
    std::cout << "This is a placeholder implementation." << std::endl;
    
    std::cout << "\nMedical image processing would typically include:\n";
    std::cout << "- DICOM file reading/writing\n";
    std::cout << "- Window/Level adjustments\n";
    std::cout << "- Multi-planar reconstruction\n";
    std::cout << "- Volume rendering\n";
    std::cout << "- Measurement tools\n";
#endif

    std::cout << "\nMedical image format processing example completed." << std::endl;
    return 0;
}
