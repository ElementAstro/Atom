/**
 * @file memory_formatter.hpp
 * @brief Memory information formatter
 *
 * This file contains the MemoryFormatter class for formatting memory information
 * into human-readable text with various styling and output format options.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_FORMATTERS_MEMORY_FORMATTER_HPP
#define ATOM_SYSINFO_PRINTER_FORMATTERS_MEMORY_FORMATTER_HPP

#include "base_formatter.hpp"
#include "../memory.hpp"

namespace atom::system {

/**
 * @class MemoryFormatter
 * @brief Formatter for memory information
 *
 * This class provides specialized formatting for memory information including
 * total memory, available memory, usage percentage, and virtual memory details.
 */
class MemoryFormatter : public BaseFormatter {
public:
    /**
     * @brief Default constructor
     */
    MemoryFormatter() = default;

    /**
     * @brief Constructor with options
     * @param options Initial formatting options
     */
    explicit MemoryFormatter(const FormatterOptions& options);

    /**
     * @brief Format memory information as a string
     * @param info The memory information to format
     * @return A formatted string containing memory details
     */
    [[nodiscard]] auto format(const MemoryInfo& info) const -> std::string;

    /**
     * @brief Format memory information with custom title
     * @param info The memory information to format
     * @param title Custom title for the section
     * @return A formatted string containing memory details
     */
    [[nodiscard]] auto format(const MemoryInfo& info, const std::string& title) const -> std::string;

    /**
     * @brief Format only basic memory information
     * @param info The memory information to format
     * @return A formatted string with basic memory details
     */
    [[nodiscard]] auto formatBasic(const MemoryInfo& info) const -> std::string;

    /**
     * @brief Format memory usage summary
     * @param info The memory information to format
     * @return A formatted string with memory usage summary
     */
    [[nodiscard]] auto formatUsageSummary(const MemoryInfo& info) const -> std::string;

    /**
     * @brief Format virtual memory information
     * @param info The memory information to format
     * @return A formatted string with virtual memory details
     */
    [[nodiscard]] auto formatVirtualMemory(const MemoryInfo& info) const -> std::string;

    /**
     * @brief Format memory slots information (if available)
     * @param info The memory information to format
     * @return A formatted string with memory slots details
     */
    [[nodiscard]] auto formatMemorySlots(const MemoryInfo& info) const -> std::string;

private:
    /**
     * @brief Format memory usage with color coding
     * @param usagePercent Memory usage percentage
     * @return Formatted usage string with appropriate color
     */
    [[nodiscard]] auto formatUsageWithColor(float usagePercent) const -> std::string;

    /**
     * @brief Get the appropriate color for memory usage
     * @param usagePercent Memory usage percentage
     * @return Color name for the usage level
     */
    [[nodiscard]] auto getUsageColor(float usagePercent) const -> std::string;

    /**
     * @brief Calculate used memory from total and available
     * @param total Total memory in bytes
     * @param available Available memory in bytes
     * @return Used memory in bytes
     */
    [[nodiscard]] auto calculateUsedMemory(uint64_t total, uint64_t available) const -> uint64_t;

    /**
     * @brief Create a memory usage visualization bar
     * @param usagePercent The usage percentage (0-100)
     * @param width The width of the bar
     * @return ASCII memory usage bar string
     */
    [[nodiscard]] auto createMemoryUsageBar(float usagePercent, int width = 20) const -> std::string;

    /**
     * @brief Format memory slot information
     * @param slot The memory slot information
     * @param slotIndex The index of the slot
     * @return Formatted slot information string
     */
    [[nodiscard]] auto formatMemorySlot(const MemoryInfo::MemorySlot& slot, int slotIndex) const -> std::string;
};

} // namespace atom::system

#endif // ATOM_SYSINFO_PRINTER_FORMATTERS_MEMORY_FORMATTER_HPP
