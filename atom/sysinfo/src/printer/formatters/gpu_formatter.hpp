/**
 * @file gpu_formatter.hpp
 * @brief GPU information formatter
 *
 * This file contains the GpuFormatter class for formatting GPU information
 * into human-readable text with various styling and output format options.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_FORMATTERS_GPU_FORMATTER_HPP
#define ATOM_SYSINFO_PRINTER_FORMATTERS_GPU_FORMATTER_HPP

#include "base_formatter.hpp"
#include <vector>

namespace atom::system {

/**
 * @struct GpuInfo
 * @brief Structure to hold GPU information
 */
struct GpuInfo {
    std::string name;
    std::string vendor;
    std::string driverVersion;
    uint64_t memoryTotal = 0;
    uint64_t memoryUsed = 0;
    float temperature = 0.0f;
    float usage = 0.0f;
    int coreCount = 0;
    float baseClock = 0.0f;
    float memoryClock = 0.0f;
    std::string apiSupport; // DirectX, OpenGL, Vulkan versions
    bool isIntegrated = false;
};

/**
 * @class GpuFormatter
 * @brief Formatter for GPU information
 *
 * This class provides specialized formatting for GPU information including
 * name, vendor, memory, temperature, usage, and performance metrics.
 */
class GpuFormatter : public BaseFormatter {
public:
    /**
     * @brief Default constructor
     */
    GpuFormatter() = default;

    /**
     * @brief Constructor with options
     * @param options Initial formatting options
     */
    explicit GpuFormatter(const FormatterOptions& options);

    /**
     * @brief Format GPU information as a string
     * @param gpus Vector of GPU information to format
     * @return A formatted string containing GPU details
     */
    [[nodiscard]] auto format(const std::vector<GpuInfo>& gpus) const -> std::string;

    /**
     * @brief Format GPU information with custom title
     * @param gpus Vector of GPU information to format
     * @param title Custom title for the section
     * @return A formatted string containing GPU details
     */
    [[nodiscard]] auto format(const std::vector<GpuInfo>& gpus, const std::string& title) const -> std::string;

    /**
     * @brief Format single GPU information
     * @param gpu The GPU information to format
     * @param index GPU index (for multi-GPU systems)
     * @return A formatted string with GPU details
     */
    [[nodiscard]] auto formatSingle(const GpuInfo& gpu, int index = 0) const -> std::string;

    /**
     * @brief Format GPU performance metrics
     * @param gpu The GPU information to format
     * @return A formatted string with performance details
     */
    [[nodiscard]] auto formatPerformance(const GpuInfo& gpu) const -> std::string;

private:
    /**
     * @brief Format GPU usage with color coding
     * @param usage GPU usage percentage
     * @return Formatted usage string with appropriate color
     */
    [[nodiscard]] auto formatUsageWithColor(float usage) const -> std::string;

    /**
     * @brief Format GPU temperature with color coding
     * @param temperature Temperature in Celsius
     * @return Formatted temperature string with appropriate color
     */
    [[nodiscard]] auto formatTemperatureWithColor(float temperature) const -> std::string;

    /**
     * @brief Get the appropriate color for GPU usage
     * @param usage GPU usage percentage
     * @return Color name for the usage level
     */
    [[nodiscard]] auto getUsageColor(float usage) const -> std::string;

    /**
     * @brief Get the appropriate color for GPU temperature
     * @param temperature Temperature in Celsius
     * @return Color name for the temperature level
     */
    [[nodiscard]] auto getTemperatureColor(float temperature) const -> std::string;

    /**
     * @brief Create a GPU usage visualization bar
     * @param usage The usage percentage (0-100)
     * @param width The width of the bar
     * @return ASCII usage bar string
     */
    [[nodiscard]] auto createUsageBar(float usage, int width = 20) const -> std::string;

    /**
     * @brief Format memory usage information
     * @param used Used memory in bytes
     * @param total Total memory in bytes
     * @return Formatted memory usage string
     */
    [[nodiscard]] auto formatMemoryUsage(uint64_t used, uint64_t total) const -> std::string;
};

} // namespace atom::system

#endif // ATOM_SYSINFO_PRINTER_FORMATTERS_GPU_FORMATTER_HPP