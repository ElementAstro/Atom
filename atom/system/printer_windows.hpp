#pragma once

#ifdef PRINT_SYSTEM_WINDOWS

#include <Windows.h>
#include <winspool.h>
#include <map>
#include <mutex>
#include <unordered_map>
#include "printer_exceptions.hpp"
#include "printer_system.hpp"

namespace print_system {

// Windows-specific print job implementation
class WindowsPrintJob : public PrintJob {
public:
    WindowsPrintJob(int job_id, const std::string& job_name,
                    const std::string& printer_name);
    ~WindowsPrintJob() override;

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

    // Get current job information from Windows
    JOB_INFO_2* getJobInfo() const;

    // Convert Windows job status to our enum
    static JobStatus convertJobStatus(DWORD win_status);
};

// Windows-specific printer implementation
class WindowsPrinter : public Printer {
public:
    explicit WindowsPrinter(const std::string& name);
    ~WindowsPrinter() override;

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

    // Utility conversion functions for internal use
    static std::wstring utf8ToWide(const std::string& str);
    static std::string wideToUtf8(const std::wstring& wstr);
    static std::string getLastErrorAsString();

private:
    std::string m_name;
    mutable std::mutex m_mutex;

    // Helper methods for Windows printing
    HANDLE openPrinter() const;
    void closePrinter(HANDLE printer_handle) const;
    PRINTER_INFO_2* getPrinterInfo() const;
    DEVMODE* createDevMode() const;
    DEVMODE* createDevModeWithSettings(const PrintSettings& settings) const;
    void applyPrintSettings(DEVMODE* dev_mode,
                            const PrintSettings& settings) const;

    // Windows-specific conversions
    static short duplexToDevMode(DuplexMode mode);
    static short colorToDevMode(ColorMode mode);
    static short orientationToDevMode(Orientation orientation);
    static short mediaSizeToDevMode(MediaSize size);
    static std::pair<short, short> qualityToDpi(PrintQuality quality);

    // Helper to print a memory buffer
    std::unique_ptr<PrintJob> printBuffer(const void* data, size_t size,
                                          const std::string& document_name,
                                          const PrintSettings& settings);
};

// Windows implementation of PrintManager
class WindowsPrintManager : public PrintManager {
public:
    WindowsPrintManager();
    ~WindowsPrintManager() override;

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

#endif  // PRINT_SYSTEM_WINDOWS