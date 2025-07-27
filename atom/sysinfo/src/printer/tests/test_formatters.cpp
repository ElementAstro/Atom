/**
 * @file test_formatters.cpp
 * @brief Tests for formatter classes
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <iostream>
#include <cassert>
#include <atom/sysinfo/sysinfo_printer/formatters/cpu_formatter.hpp>
#include <atom/sysinfo/sysinfo_printer/formatters/memory_formatter.hpp>
#include <atom/sysinfo/sysinfo_printer/formatters/battery_formatter.hpp>
#include <atom/sysinfo/sysinfo_printer/formatters/gpu_formatter.hpp>
#include <atom/sysinfo/sysinfo_printer/formatters/locale_formatter.hpp>

using namespace atom::system;

void testCpuFormatter() {
    std::cout << "Testing CPU formatter...\n";

    try {
        // Create test CPU info
        CpuInfo cpuInfo;
        cpuInfo.model = "Test CPU";
        cpuInfo.vendor = "Test Vendor";
        cpuInfo.physicalCores = 4;
        cpuInfo.logicalCores = 8;
        cpuInfo.baseFrequency = 2.4e9;
        cpuInfo.usage = 45.5;
        cpuInfo.temperature = 65.0;

        // Test different formatter styles
        FormatterOptions options;

        // Test minimal style
        options.style = FormatterStyle::MINIMAL;
        CpuFormatter minimalFormatter(options);
        auto minimalOutput = minimalFormatter.format(cpuInfo);
        assert(!minimalOutput.empty());
        std::cout << "✓ Minimal CPU formatting works\n";

        // Test detailed style
        options.style = FormatterStyle::DETAILED;
        CpuFormatter detailedFormatter(options);
        auto detailedOutput = detailedFormatter.format(cpuInfo);
        assert(!detailedOutput.empty());
        assert(detailedOutput.length() > minimalOutput.length());
        std::cout << "✓ Detailed CPU formatting works\n";

        // Test performance formatting
        auto perfOutput = detailedFormatter.formatPerformance(cpuInfo);
        assert(!perfOutput.empty());
        std::cout << "✓ CPU performance formatting works\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in CPU formatter test: " << e.what() << std::endl;
        throw;
    }
}

void testMemoryFormatter() {
    std::cout << "Testing Memory formatter...\n";

    try {
        // Create test memory info
        DetailedMemoryInfo memInfo;
        memInfo.totalPhysicalMemory = 16ULL * 1024 * 1024 * 1024; // 16 GB
        memInfo.availablePhysicalMemory = 8ULL * 1024 * 1024 * 1024; // 8 GB
        memInfo.memoryLoadPercentage = 50.0;

        FormatterOptions options;
        options.style = FormatterStyle::STANDARD;

        MemoryFormatter formatter(options);
        auto output = formatter.format(memInfo);
        assert(!output.empty());
        std::cout << "✓ Memory formatting works\n";

        // Test basic formatting
        auto basicOutput = formatter.formatBasic(memInfo);
        assert(!basicOutput.empty());
        std::cout << "✓ Basic memory formatting works\n";

        // Test usage formatting
        auto usageOutput = formatter.formatUsage(memInfo);
        assert(!usageOutput.empty());
        std::cout << "✓ Memory usage formatting works\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in Memory formatter test: " << e.what() << std::endl;
        throw;
    }
}

void testBatteryFormatter() {
    std::cout << "Testing Battery formatter...\n";

    try {
        // Create test battery info
        BatteryInfo batteryInfo;
        batteryInfo.isPresent = true;
        batteryInfo.isCharging = false;
        batteryInfo.batteryLevel = 75.5;
        batteryInfo.batteryHealth = 95.0;
        batteryInfo.temperature = 35.0;

        FormatterOptions options;
        options.style = FormatterStyle::STANDARD;

        BatteryFormatter formatter(options);
        auto output = formatter.format(batteryInfo);
        assert(!output.empty());
        std::cout << "✓ Battery formatting works\n";

        // Test basic formatting
        auto basicOutput = formatter.formatBasic(batteryInfo);
        assert(!basicOutput.empty());
        std::cout << "✓ Basic battery formatting works\n";

        // Test status formatting
        auto statusOutput = formatter.formatStatus(batteryInfo);
        assert(!statusOutput.empty());
        std::cout << "✓ Battery status formatting works\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in Battery formatter test: " << e.what() << std::endl;
        throw;
    }
}

void testGpuFormatter() {
    std::cout << "Testing GPU formatter...\n";

    try {
        // Create test GPU info
        std::vector<GpuInfo> gpus;
        GpuInfo gpu;
        gpu.name = "Test GPU";
        gpu.vendor = "Test Vendor";
        gpu.memoryTotal = 8ULL * 1024 * 1024 * 1024; // 8 GB
        gpu.memoryUsed = 2ULL * 1024 * 1024 * 1024;  // 2 GB
        gpu.usage = 60.0;
        gpu.temperature = 70.0;
        gpus.push_back(gpu);

        FormatterOptions options;
        options.style = FormatterStyle::STANDARD;

        GpuFormatter formatter(options);
        auto output = formatter.format(gpus);
        assert(!output.empty());
        std::cout << "✓ GPU formatting works\n";

        // Test single GPU formatting
        auto singleOutput = formatter.formatSingle(gpu, 1);
        assert(!singleOutput.empty());
        std::cout << "✓ Single GPU formatting works\n";

        // Test performance formatting
        auto perfOutput = formatter.formatPerformance(gpu);
        assert(!perfOutput.empty());
        std::cout << "✓ GPU performance formatting works\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in GPU formatter test: " << e.what() << std::endl;
        throw;
    }
}

void testLocaleFormatter() {
    std::cout << "Testing Locale formatter...\n";

    try {
        // Create test locale info
        LocaleInfo localeInfo;
        localeInfo.languageCode = "en";
        localeInfo.countryCode = "US";
        localeInfo.languageDisplayName = "English";
        localeInfo.countryDisplayName = "United States";
        localeInfo.characterEncoding = "UTF-8";
        localeInfo.timeFormat = "12-hour";
        localeInfo.dateFormat = "MM/DD/YYYY";

        FormatterOptions options;
        options.style = FormatterStyle::STANDARD;

        LocaleFormatter formatter(options);
        auto output = formatter.format(localeInfo);
        assert(!output.empty());
        std::cout << "✓ Locale formatting works\n";

        // Test basic formatting
        auto basicOutput = formatter.formatBasic(localeInfo);
        assert(!basicOutput.empty());
        std::cout << "✓ Basic locale formatting works\n";

        // Test regional formatting
        auto regionalOutput = formatter.formatRegional(localeInfo);
        assert(!regionalOutput.empty());
        std::cout << "✓ Regional locale formatting works\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in Locale formatter test: " << e.what() << std::endl;
        throw;
    }
}

void testFormatterOptions() {
    std::cout << "Testing formatter options...\n";

    try {
        CpuInfo cpuInfo;
        cpuInfo.model = "Test CPU";
        cpuInfo.usage = 50.0;

        // Test different styles
        std::vector<FormatterStyle> styles = {
            FormatterStyle::MINIMAL,
            FormatterStyle::COMPACT,
            FormatterStyle::STANDARD,
            FormatterStyle::DETAILED,
            FormatterStyle::VERBOSE
        };

        for (auto style : styles) {
            FormatterOptions options;
            options.style = style;

            CpuFormatter formatter(options);
            auto output = formatter.format(cpuInfo);
            assert(!output.empty());
        }

        std::cout << "✓ All formatter styles work\n";

        // Test color options
        FormatterOptions colorOptions;
        colorOptions.colorEnabled = true;
        CpuFormatter colorFormatter(colorOptions);
        auto colorOutput = colorFormatter.format(cpuInfo);
        assert(!colorOutput.empty());
        std::cout << "✓ Color formatting works\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in formatter options test: " << e.what() << std::endl;
        throw;
    }
}

int main() {
    std::cout << "=== Atom System Information Printer - Formatter Tests ===\n\n";

    try {
        testCpuFormatter();
        std::cout << "\n";

        testMemoryFormatter();
        std::cout << "\n";

        testBatteryFormatter();
        std::cout << "\n";

        testGpuFormatter();
        std::cout << "\n";

        testLocaleFormatter();
        std::cout << "\n";

        testFormatterOptions();
        std::cout << "\n";

        std::cout << "All formatter tests passed successfully!\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Formatter test failed: " << e.what() << std::endl;
        return 1;
    }
}