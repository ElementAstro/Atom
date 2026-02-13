#pragma once

#ifdef PRINT_SYSTEM_LINUX

#include <cups/cups.h>
#include <mutex>
#include <unordered_map>
#include "printer_exceptions.hpp"
#include "printer_system.hpp"

namespace print_system {

// CUPS-specific print job implementation
class LinuxPrintJob : public PrintJob {
public:
    LinuxPrintJob(int job_id, const std::string& job_name,
                  const std::string& printer_name);
    ~LinuxPrintJob() override;

    int getJobId() const override { return m_job_id; }
    std::string getJobName() const override { return m_job_name; }
    JobStatus getJobStatus() const override;
    std::string getStatusString() const override;
    std::chrono::system_clock::time_point getSubmitTime() const override {
        return m_submit_time;
    }

    bool cancel() override;
    bool pause() override;
    bool resume() override;
    float getCompletionPercentage() const override;

    bool waitForCompletion(std::optional<std::chrono::milliseconds> timeout =
                               std::nullopt) override;

private:
    int m_job_id;
    std::string m_job_name;
    std::string m_printer_name;
    std::chrono::system_clock::time_point m_submit_time;

    // Get current job information from CUPS
    cups_job_t* getJobInfo() const;

    // Convert CUPS job state to our enum
    static JobStatus convertJobState(ipp_jstate_t cups_state);
};

// Linux/CUPS printer implementation
class LinuxPrinter : public Printer {
public:
    explicit LinuxPrinter(const std::string& name);
    ~LinuxPrinter() override;

    std::string getName() const override { return m_name; }
    std::string getModel() const override;
    std::string getLocation() const override;
    std::string getDescription() const override;
    PrinterStatus getStatus() const override;

    std::unique_ptr<PrintJob> print(
        const std::filesystem::path& file_path,
        const PrintSettings& settings = {}) override;

    std::unique_ptr<PrintJob> printText(
        const std::string& text,
        const std::string& document_name = "Text Document",
        const PrintSettings& settings = {}) override;

    std::unique_ptr<PrintJob> printImage(
        const std::filesystem::path& image_path,
        const PrintSettings& settings = {}) override;

    std::unique_ptr<PrintJob> printPDF(
        const std::filesystem::path& pdf_path,
        const PrintSettings& settings = {}) override;

    std::unique_ptr<PrintJob> printRaw(
        std::span<const std::byte> data, const std::string& document_name,
        const std::string& mime_type,
        const PrintSettings& settings = {}) override;

    bool supportsDuplex() const override;
    bool supportsColor() const override;
    std::vector<MediaSize> getSupportedMediaSizes() const override;
    bool supportsCustomPageSizes() const override;
    std::vector<PrintQuality> getSupportedQualitySettings() const override;

    std::vector<std::unique_ptr<PrintJob>> getActiveJobs() const override;
    std::unique_ptr<PrintJob> getJob(int job_id) const override;

    bool setAsDefault() override;

private:
    std::string m_name;
    mutable std::mutex m_mutex;

    // Helper methods for CUPS printing
    cups_dest_t* findPrinter() const;
    void applyCupsOptions(cups_option_t** options, int* num_options,
                          const PrintSettings& settings) const;

    // Helper to determine MIME type from file extension
    static std::string getMimeTypeForFile(
        const std::filesystem::path& file_path);

    // CUPS-specific conversions
    static std::string duplexToCupsOption(DuplexMode mode);
    static std::string colorToCupsOption(ColorMode mode);
    static std::string mediaSizeToCupsOption(MediaSize size);
    static std::string qualityToCupsOption(PrintQuality quality);
    static std::string orientationToCupsOption(Orientation orientation);
};

// Linux implementation of PrintManager
class LinuxPrintManager : public PrintManager {
public:
    LinuxPrintManager();
    ~LinuxPrintManager() override;

    std::vector<std::shared_ptr<Printer>> getAvailablePrinters() const override;
    std::shared_ptr<Printer> getDefaultPrinter() const override;
    std::shared_ptr<Printer> getPrinterByName(
        const std::string& name) const override;
    void refreshPrinterList() override;

    bool canPrintToPDF() const override;
    std::shared_ptr<Printer> getPDFPrinter() const override;

private:
    mutable std::mutex m_mutex;
    mutable std::unordered_map<std::string, std::weak_ptr<Printer>> m_printers;
    mutable std::chrono::steady_clock::time_point m_last_refresh;

    // Cache refresh interval in seconds
    static constexpr int CACHE_REFRESH_SECONDS = 30;

    // Helper to refresh printer list if cache is expired
    void refreshIfNeeded() const;
};

}  // namespace print_system

#endif  // PRINT_SYSTEM_LINUX
