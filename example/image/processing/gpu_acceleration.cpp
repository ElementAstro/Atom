/*
 * gpu_acceleration.cpp
 *
 * GPU acceleration example for image processing
 * Part of the Atom project
 * Author: Max Qian
 * License: GPL3
 */

#include <iostream>
#include <vector>
#include <chrono>

#ifdef HAVE_OPENCV
#include <opencv2/opencv.hpp>
#endif

/**
 * @brief Demonstrates GPU-accelerated image processing
 * 
 * This example shows how to use GPU acceleration for image processing
 * operations when available. Falls back to CPU processing when GPU
 * acceleration is not available.
 */
int main() {
    std::cout << "GPU Acceleration Example\n";
    std::cout << "========================\n\n";

#ifdef HAVE_OPENCV
    try {
        // Check if OpenCV was compiled with CUDA support
        int cuda_devices = cv::cuda::getCudaEnabledDeviceCount();
        
        if (cuda_devices > 0) {
            std::cout << "CUDA devices found: " << cuda_devices << std::endl;
            
            // Create a sample image
            cv::Mat cpu_image = cv::Mat::zeros(1024, 1024, CV_8UC3);
            cv::randu(cpu_image, cv::Scalar(0, 0, 0), cv::Scalar(255, 255, 255));
            
            // Upload to GPU
            cv::cuda::GpuMat gpu_image;
            gpu_image.upload(cpu_image);
            
            // Perform GPU-accelerated operations
            cv::cuda::GpuMat gpu_gray, gpu_blurred;
            
            auto start = std::chrono::high_resolution_clock::now();
            
            // Convert to grayscale on GPU
            cv::cuda::cvtColor(gpu_image, gpu_gray, cv::COLOR_BGR2GRAY);
            
            // Apply Gaussian blur on GPU
            cv::cuda::GaussianBlur(gpu_gray, gpu_blurred, cv::Size(15, 15), 0);
            
            auto end = std::chrono::high_resolution_clock::now();
            auto gpu_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            
            std::cout << "GPU processing time: " << gpu_duration.count() << " microseconds" << std::endl;
            
            // Compare with CPU processing
            cv::Mat cpu_gray, cpu_blurred;
            
            start = std::chrono::high_resolution_clock::now();
            cv::cvtColor(cpu_image, cpu_gray, cv::COLOR_BGR2GRAY);
            cv::GaussianBlur(cpu_gray, cpu_blurred, cv::Size(15, 15), 0);
            end = std::chrono::high_resolution_clock::now();
            
            auto cpu_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            std::cout << "CPU processing time: " << cpu_duration.count() << " microseconds" << std::endl;
            
            double speedup = static_cast<double>(cpu_duration.count()) / gpu_duration.count();
            std::cout << "GPU speedup: " << speedup << "x" << std::endl;
            
        } else {
            std::cout << "No CUDA devices found. GPU acceleration not available." << std::endl;
            std::cout << "Falling back to CPU processing..." << std::endl;
            
            // Demonstrate CPU-based processing
            cv::Mat image = cv::Mat::zeros(512, 512, CV_8UC3);
            cv::randu(image, cv::Scalar(0, 0, 0), cv::Scalar(255, 255, 255));
            
            cv::Mat gray, blurred;
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
            cv::GaussianBlur(gray, blurred, cv::Size(15, 15), 0);
            
            std::cout << "CPU processing completed successfully." << std::endl;
        }
        
    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV error: " << e.what() << std::endl;
        return 1;
    }
#else
    std::cout << "OpenCV not available. GPU acceleration example cannot run." << std::endl;
    std::cout << "This is a placeholder implementation." << std::endl;
#endif

    std::cout << "\nGPU acceleration example completed." << std::endl;
    return 0;
}
