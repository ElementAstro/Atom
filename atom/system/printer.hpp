#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>  // C++20 feature
#include <string>
#include <vector>

namespace print_system {

// Forward declarations
class PrintJob;
class Printer;
class PrintManager;

// Core enumerations for printer options
enum class ColorMode { Color, Grayscale, Monochrome };
enum class DuplexMode { None, LongEdge, ShortEdge };
enum class MediaSize {
    A4,
    Letter,
    Legal,
    Executive,
    A3,
    A5,
    B5,
    Envelope10,
    EnvelopeDL,
    EnvelopeC5,
    Custom
};
enum class Orientation { Portrait, Landscape };
enum class PrintQuality { Draft, Normal, High };

// Custom page size dimensions (in millimeters)
struct CustomPageSize {
    double width_mm;
    double height_mm;

    CustomPageSize(double w, double h) : width_mm(w), height_mm(h) {
        if (width_mm <= 0 || height_mm <= 0) {
            throw std::invalid_argument("Page dimensions must be positive");
        }
    }
};

// Print job configuration
struct PrintSettings {
    int copies{1};
    ColorMode color_mode{ColorMode::Color};
    DuplexMode duplex_mode{DuplexMode::None};
    MediaSize media_size{MediaSize::A4};
    std::optional<CustomPageSize> custom_size{};
    Orientation orientation{Orientation::Portrait};
    PrintQuality quality{PrintQuality::Normal};
    std::optional<std::vector<int>> page_ranges{};
    double scale{1.0};
    bool collate{true};
};

// Printer status information
struct PrinterStatus {
    bool is_online{false};
    bool is_ready{false};
    int pending_jobs{0};
    std::optional<std::string> error_message{};
};

// Print job status
enum class JobStatus {
    Pending,
    Processing,
    Printing,
    Completed,
    Failed,
    Canceled,
    Paused
};

// Print job interface
class PrintJob {
public:
    virtual ~PrintJob() = default;

    virtual int getJobId() const = 0;
    virtual std::string getJobName() const = 0;
    virtual JobStatus getJobStatus() const = 0;
    virtual std::string getStatusString() const = 0;
    virtual std::chrono::system_clock::time_point getSubmitTime() const = 0;

    virtual bool cancel() = 0;
    virtual bool pause() = 0;
    virtual bool resume() = 0;
    virtual float getCompletionPercentage() const = 0;

    // Wait for job to complete with optional timeout
    virtual bool waitForCompletion(
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) = 0;
};

// Printer interface
class Printer {
public:
    virtual ~Printer() = default;

    virtual std::string getName() const = 0;
    virtual std::string getModel() const = 0;
    virtual std::string getLocation() const = 0;
    virtual std::string getDescription() const = 0;
    virtual PrinterStatus getStatus() const = 0;

    // Print methods
    virtual std::unique_ptr<PrintJob> print(
        const std::filesystem::path& file_path,
        const PrintSettings& settings = {}) = 0;

    virtual std::unique_ptr<PrintJob> printText(
        const std::string& text,
        const std::string& document_name = "Text Document",
        const PrintSettings& settings = {}) = 0;

    virtual std::unique_ptr<PrintJob> printImage(
        const std::filesystem::path& image_path,
        const PrintSettings& settings = {}) = 0;

    virtual std::unique_ptr<PrintJob> printPDF(
        const std::filesystem::path& pdf_path,
        const PrintSettings& settings = {}) = 0;

    virtual std::unique_ptr<PrintJob> printRaw(
        std::span<const std::byte> data, const std::string& document_name,
        const std::string& mime_type, const PrintSettings& settings = {}) = 0;

    // Capabilities
    virtual bool supportsDuplex() const = 0;
    virtual bool supportsColor() const = 0;
    virtual std::vector<MediaSize> getSupportedMediaSizes() const = 0;
    virtual bool supportsCustomPageSizes() const = 0;
    virtual std::vector<PrintQuality> getSupportedQualitySettings() const = 0;

    // Active jobs
    virtual std::vector<std::unique_ptr<PrintJob>> getActiveJobs() const = 0;
    virtual std::unique_ptr<PrintJob> getJob(int job_id) const = 0;

    virtual bool setAsDefault() = 0;
};

// Print Manager - main entry point for the printing system
class PrintManager {
public:
    // Get singleton instance
    static PrintManager& getInstance();

    // Printer discovery
    virtual std::vector<std::shared_ptr<Printer>> getAvailablePrinters()
        const = 0;
    virtual std::shared_ptr<Printer> getDefaultPrinter() const = 0;
    virtual std::shared_ptr<Printer> getPrinterByName(
        const std::string& name) const = 0;
    virtual void refreshPrinterList() = 0;

    // System-wide capabilities
    virtual bool canPrintToPDF() const = 0;
    virtual std::shared_ptr<Printer> getPDFPrinter() const = 0;

protected:
    // Factory method for platform-specific implementation
    static std::unique_ptr<PrintManager> create();

    PrintManager() = default;
    virtual ~PrintManager() = default;

    // Prevent copying and moving
    PrintManager(const PrintManager&) = delete;
    PrintManager& operator=(const PrintManager&) = delete;
    PrintManager(PrintManager&&) = delete;
    PrintManager& operator=(PrintManager&&) = delete;
};

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
#define PRINT_SYSTEM_WINDOWS
#elif defined(__linux__)
#define PRINT_SYSTEM_LINUX
#elif defined(__APPLE__)
#define PRINT_SYSTEM_MACOS
#else
#error "Unsupported platform"
#endif

}  // namespace print_system