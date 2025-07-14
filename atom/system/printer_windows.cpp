#ifdef PRINT_SYSTEM_WINDOWS

#include <comdef.h>
#include <gdiplus.h>
#include <algorithm>
#include <format>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <thread>
#include "printer_system_windows.hpp"

#pragma comment(lib, "winspool.lib")
#pragma comment(lib, "gdiplus.lib")

namespace print_system {

// Initialize GDI+ once for the application
class GdiPlusInitializer {
public:
    GdiPlusInitializer() {
        Gdiplus::GdiplusStartupInput input;
        Gdiplus::GdiplusStartup(&m_token, &input, nullptr);
    }

    ~GdiPlusInitializer() { Gdiplus::GdiplusShutdown(m_token); }

private:
    ULONG_PTR m_token = 0;
};

static GdiPlusInitializer s_gdi_plus_initializer;

//====================
// WindowsPrintJob Implementation
//====================

WindowsPrintJob::WindowsPrintJob(int job_id, const std::string& job_name,
                                 const std::string& printer_name)
    : m_job_id(job_id),
      m_job_name(job_name),
      m_printer_name(printer_name),
      m_submit_time(std::chrono::system_clock::now()) {}

WindowsPrintJob::~WindowsPrintJob() = default;

JobStatus WindowsPrintJob::getJobStatus() const {
    JOB_INFO_2* job_info = getJobInfo();
    if (!job_info) {
        return JobStatus::Failed;
    }

    JobStatus status = convertJobStatus(job_info->Status);

    // Free the allocated memory
    delete[] reinterpret_cast<BYTE*>(job_info);

    return status;
}

std::string WindowsPrintJob::getStatusString() const {
    JOB_INFO_2* job_info = getJobInfo();
    if (!job_info) {
        return "Unknown (job not found)";
    }

    std::string status_str;

    if (job_info->Status == 0) {
        status_str = "Processing";
    } else {
        if (job_info->Status & JOB_STATUS_PAUSED)
            status_str += "Paused ";
        if (job_info->Status & JOB_STATUS_ERROR)
            status_str += "Error ";
        if (job_info->Status & JOB_STATUS_DELETING)
            status_str += "Deleting ";
        if (job_info->Status & JOB_STATUS_SPOOLING)
            status_str += "Spooling ";
        if (job_info->Status & JOB_STATUS_PRINTING)
            status_str += "Printing ";
        if (job_info->Status & JOB_STATUS_OFFLINE)
            status_str += "Offline ";
        if (job_info->Status & JOB_STATUS_PAPEROUT)
            status_str += "Out of paper ";
        if (job_info->Status & JOB_STATUS_PRINTED)
            status_str += "Printed ";
        if (job_info->Status & JOB_STATUS_DELETED)
            status_str += "Deleted ";
        if (job_info->Status & JOB_STATUS_BLOCKED_DEVQ)
            status_str += "Blocked ";
        if (job_info->Status & JOB_STATUS_USER_INTERVENTION)
            status_str += "Needs attention ";
        if (job_info->Status & JOB_STATUS_RESTART)
            status_str += "Restarting ";
    }

    // Add more detail if available
    if (job_info->pStatus && job_info->pStatus[0] != L'\0') {
        std::string status_detail = wideToUtf8(job_info->pStatus);
        if (!status_str.empty()) {
            status_str += "- ";
        }
        status_str += status_detail;
    }

    // Trim trailing space
    if (!status_str.empty() && status_str.back() == ' ') {
        status_str.pop_back();
    }

    // If still empty, use a default
    if (status_str.empty()) {
        status_str = "Unknown";
    }

    // Free the allocated memory
    delete[] reinterpret_cast<BYTE*>(job_info);

    return status_str;
}

bool WindowsPrintJob::cancel() {
    HANDLE printer_handle = nullptr;

    // Open the printer
    if (!OpenPrinterW(utf8ToWide(m_printer_name).c_str(), &printer_handle,
                      nullptr)) {
        return false;
    }

    // Cancel the job
    bool success = SetJob(printer_handle, m_job_id, 0, nullptr,
                          JOB_CONTROL_CANCEL) == TRUE;

    // Close the printer
    ClosePrinter(printer_handle);

    return success;
}

bool WindowsPrintJob::pause() {
    HANDLE printer_handle = nullptr;

    // Open the printer
    if (!OpenPrinterW(utf8ToWide(m_printer_name).c_str(), &printer_handle,
                      nullptr)) {
        return false;
    }

    // Pause the job
    bool success =
        SetJob(printer_handle, m_job_id, 0, nullptr, JOB_CONTROL_PAUSE) == TRUE;

    // Close the printer
    ClosePrinter(printer_handle);

    return success;
}

bool WindowsPrintJob::resume() {
    HANDLE printer_handle = nullptr;

    // Open the printer
    if (!OpenPrinterW(utf8ToWide(m_printer_name).c_str(), &printer_handle,
                      nullptr)) {
        return false;
    }

    // Resume the job
    bool success = SetJob(printer_handle, m_job_id, 0, nullptr,
                          JOB_CONTROL_RESUME) == TRUE;

    // Close the printer
    ClosePrinter(printer_handle);

    return success;
}

float WindowsPrintJob::getCompletionPercentage() const {
    JOB_INFO_2* job_info = getJobInfo();
    if (!job_info) {
        return 0.0f;
    }

    float completion = 0.0f;

    // Check if job has page information
    if (job_info->TotalPages > 0) {
        completion = static_cast<float>(job_info->PagesPrinted) /
                     static_cast<float>(job_info->TotalPages) * 100.0f;
    } else {
        // Estimate completion based on status
        JobStatus status = convertJobStatus(job_info->Status);

        switch (status) {
            case JobStatus::Pending:
                completion = 0.0f;
                break;
            case JobStatus::Processing:
                completion = 25.0f;
                break;
            case JobStatus::Printing:
                completion = 50.0f;
                break;
            case JobStatus::Completed:
            case JobStatus::Failed:
            case JobStatus::Canceled:
                completion = 100.0f;
                break;
            case JobStatus::Paused:
                // For paused jobs, we keep the last percentage or use 50%
                if (job_info->Status & JOB_STATUS_SPOOLING) {
                    completion = 25.0f;
                } else if (job_info->Status & JOB_STATUS_PRINTING) {
                    completion = 75.0f;
                } else {
                    completion = 50.0f;
                }
                break;
            default:
                completion = 0.0f;
        }
    }

    // Free the allocated memory
    delete[] reinterpret_cast<BYTE*>(job_info);

    return completion;
}

bool WindowsPrintJob::waitForCompletion(
    std::optional<std::chrono::milliseconds> timeout) {
    auto start_time = std::chrono::steady_clock::now();

    while (true) {
        JobStatus status = getJobStatus();

        // Check if job is done
        if (status == JobStatus::Completed || status == JobStatus::Failed ||
            status == JobStatus::Canceled) {
            return status == JobStatus::Completed;
        }

        // Check for timeout
        if (timeout.has_value()) {
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (elapsed >= timeout.value()) {
                return false;  // Timeout occurred
            }
        }

        // Sleep before checking again
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

JOB_INFO_2* WindowsPrintJob::getJobInfo() const {
    HANDLE printer_handle = nullptr;

    // Open the printer
    if (!OpenPrinterW(utf8ToWide(m_printer_name).c_str(), &printer_handle,
                      nullptr)) {
        return nullptr;
    }

    // Determine the required buffer size
    DWORD needed = 0;
    GetJob(printer_handle, m_job_id, 2, nullptr, 0, &needed);

    if (needed == 0) {
        ClosePrinter(printer_handle);
        return nullptr;
    }

    // Allocate the buffer
    BYTE* buffer = new BYTE[needed];

    // Get the job information
    BOOL result = GetJob(printer_handle, m_job_id, 2, buffer, needed, &needed);

    // Close the printer
    ClosePrinter(printer_handle);

    if (!result) {
        delete[] buffer;
        return nullptr;
    }

    return reinterpret_cast<JOB_INFO_2*>(buffer);
}

JobStatus WindowsPrintJob::convertJobStatus(DWORD win_status) {
    if (win_status & JOB_STATUS_COMPLETE) {
        return JobStatus::Completed;
    }
    if (win_status & JOB_STATUS_PAUSED) {
        return JobStatus::Paused;
    }
    if (win_status & JOB_STATUS_ERROR) {
        return JobStatus::Failed;
    }
    if (win_status & JOB_STATUS_DELETING || win_status & JOB_STATUS_DELETED) {
        return JobStatus::Canceled;
    }
    if (win_status & JOB_STATUS_PRINTING) {
        return JobStatus::Printing;
    }
    if (win_status & JOB_STATUS_SPOOLING) {
        return JobStatus::Processing;
    }

    // Default to pending if no other status applies
    return JobStatus::Pending;
}

//====================
// WindowsPrinter Implementation
//====================

WindowsPrinter::WindowsPrinter(const std::string& name) : m_name(name) {
    // Verify printer existence
    HANDLE printer_handle = openPrinter();
    if (printer_handle == nullptr) {
        throw PrinterNotFoundException(name);
    }
    closePrinter(printer_handle);
}

WindowsPrinter::~WindowsPrinter() = default;

std::string WindowsPrinter::getModel() const {
    PRINTER_INFO_2* printer_info = getPrinterInfo();
    if (!printer_info) {
        return "Unknown";
    }

    std::string model;
    if (printer_info->pDriverName) {
        model = wideToUtf8(printer_info->pDriverName);
    }

    // Free the allocated memory
    delete[] reinterpret_cast<BYTE*>(printer_info);

    return model;
}

std::string WindowsPrinter::getLocation() const {
    PRINTER_INFO_2* printer_info = getPrinterInfo();
    if (!printer_info) {
        return "";
    }

    std::string location;
    if (printer_info->pLocation) {
        location = wideToUtf8(printer_info->pLocation);
    }

    // Free the allocated memory
    delete[] reinterpret_cast<BYTE*>(printer_info);

    return location;
}

std::string WindowsPrinter::getDescription() const {
    PRINTER_INFO_2* printer_info = getPrinterInfo();
    if (!printer_info) {
        return "";
    }

    std::string comment;
    if (printer_info->pComment) {
        comment = wideToUtf8(printer_info->pComment);
    }

    // Free the allocated memory
    delete[] reinterpret_cast<BYTE*>(printer_info);

    return comment;
}

PrinterStatus WindowsPrinter::getStatus() const {
    PrinterStatus status;

    PRINTER_INFO_2* printer_info = getPrinterInfo();
    if (!printer_info) {
        status.is_online = false;
        status.is_ready = false;
        status.error_message = "Failed to get printer information";
        return status;
    }

    // Check printer status
    status.is_online = !(printer_info->Status & PRINTER_STATUS_OFFLINE);
    status.is_ready =
        (printer_info->Status == 0);  // No status flags means ready

    // Set error message based on status flags
    if (printer_info->Status & PRINTER_STATUS_PAPER_JAM) {
        status.error_message = "Paper jam";
    } else if (printer_info->Status & PRINTER_STATUS_PAPER_OUT) {
        status.error_message = "Out of paper";
    } else if (printer_info->Status & PRINTER_STATUS_PAPER_PROBLEM) {
        status.error_message = "Paper problem";
    } else if (printer_info->Status & PRINTER_STATUS_OFFLINE) {
        status.error_message = "Printer is offline";
    } else if (printer_info->Status & PRINTER_STATUS_IO_ACTIVE) {
        status.error_message = "Receiving data";
    } else if (printer_info->Status & PRINTER_STATUS_BUSY) {
        status.error_message = "Printer is busy";
    } else if (printer_info->Status & PRINTER_STATUS_PRINTING) {
        status.error_message = "Printing";
    } else if (printer_info->Status & PRINTER_STATUS_OUTPUT_BIN_FULL) {
        status.error_message = "Output bin is full";
    } else if (printer_info->Status & PRINTER_STATUS_NOT_AVAILABLE) {
        status.error_message = "Printer not available";
    } else if (printer_info->Status & PRINTER_STATUS_WAITING) {
        status.error_message = "Waiting";
    } else if (printer_info->Status & PRINTER_STATUS_PROCESSING) {
        status.error_message = "Processing";
    } else if (printer_info->Status & PRINTER_STATUS_INITIALIZING) {
        status.error_message = "Initializing";
    } else if (printer_info->Status & PRINTER_STATUS_WARMING_UP) {
        status.error_message = "Warming up";
    } else if (printer_info->Status & PRINTER_STATUS_TONER_LOW) {
        status.error_message = "Toner low";
    } else if (printer_info->Status & PRINTER_STATUS_NO_TONER) {
        status.error_message = "No toner";
    } else if (printer_info->Status & PRINTER_STATUS_PAGE_PUNT) {
        status.error_message = "Page punt";
    } else if (printer_info->Status & PRINTER_STATUS_USER_INTERVENTION) {
        status.error_message = "Needs user intervention";
    } else if (printer_info->Status & PRINTER_STATUS_OUT_OF_MEMORY) {
        status.error_message = "Out of memory";
    } else if (printer_info->Status & PRINTER_STATUS_DOOR_OPEN) {
        status.error_message = "Door open";
    } else if (printer_info->Status & PRINTER_STATUS_SERVER_UNKNOWN) {
        status.error_message = "Server unknown";
    } else if (printer_info->Status & PRINTER_STATUS_POWER_SAVE) {
        status.error_message = "Power save mode";
    }

    // Get pending job count
    status.pending_jobs = printer_info->cJobs;

    // Free the allocated memory
    delete[] reinterpret_cast<BYTE*>(printer_info);

    return status;
}

std::unique_ptr<PrintJob> WindowsPrinter::print(
    const std::filesystem::path& file_path, const PrintSettings& settings) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Validate file existence
    if (!std::filesystem::exists(file_path)) {
        throw PrintJobFailedException("File does not exist: " +
                                      file_path.string());
    }

    // Determine file type and use appropriate printing method
    std::string extension = file_path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (extension == ".pdf") {
        return printPDF(file_path, settings);
    } else if (extension == ".jpg" || extension == ".jpeg" ||
               extension == ".png" || extension == ".bmp" ||
               extension == ".gif" || extension == ".tiff" ||
               extension == ".tif") {
        return printImage(file_path, settings);
    } else if (extension == ".txt" || extension == ".log" ||
               extension == ".csv" || extension == ".md") {
        // For text files, read the content and use printText
        std::ifstream file(file_path);
        if (!file) {
            throw PrintJobFailedException("Failed to open file: " +
                                          file_path.string());
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        return printText(buffer.str(), file_path.filename().string(), settings);
    } else {
        // For other file types, try to shell execute with print verb
        SHELLEXECUTEINFOW sei = {sizeof(SHELLEXECUTEINFOW)};
        sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = L"print";
        sei.lpFile = file_path.c_str();
        sei.nShow = SW_HIDE;

        if (!ShellExecuteExW(&sei)) {
            throw PrintJobFailedException("Failed to print file: " +
                                          getLastErrorAsString());
        }

        // Create a job ID (Windows shell printing doesn't give us a job ID)
        int job_id =
            static_cast<int>(reinterpret_cast<uintptr_t>(sei.hProcess));

        // Wait for the process to complete
        if (sei.hProcess) {
            WaitForSingleObject(sei.hProcess, 5000);  // Wait up to 5 seconds
            CloseHandle(sei.hProcess);
        }

        return std::make_unique<WindowsPrintJob>(
            job_id, file_path.filename().string(), m_name);
    }
}

std::unique_ptr<PrintJob> WindowsPrinter::printText(
    const std::string& text, const std::string& document_name,
    const PrintSettings& settings) {
    std::lock_guard<std::mutex> lock(m_mutex);

    HANDLE printer_handle = openPrinter();
    if (printer_handle == nullptr) {
        throw PrintJobFailedException("Unable to open printer: " +
                                      getLastErrorAsString());
    }

    // Setup document info
    std::wstring doc_name = utf8ToWide(document_name);

    DOCINFOW doc_info = {0};
    doc_info.cbSize = sizeof(DOCINFOW);
    doc_info.lpszDocName = doc_name.c_str();
    doc_info.lpszOutput = nullptr;
    doc_info.lpszDatatype = L"RAW";

    // Start document
    int job_id = StartDocPrinterW(printer_handle, 1,
                                  reinterpret_cast<LPBYTE>(&doc_info));
    if (job_id <= 0) {
        std::string error = getLastErrorAsString();
        closePrinter(printer_handle);
        throw PrintJobFailedException("Failed to start print job: " + error);
    }

    // Start page
    if (!StartPagePrinter(printer_handle)) {
        std::string error = getLastErrorAsString();
        EndDocPrinter(printer_handle);
        closePrinter(printer_handle);
        throw PrintJobFailedException("Failed to start page: " + error);
    }

    // Write the text data to the printer
    DWORD bytes_written = 0;
    if (!WritePrinter(printer_handle, text.c_str(),
                      static_cast<DWORD>(text.size()), &bytes_written)) {
        std::string error = getLastErrorAsString();
        EndPagePrinter(printer_handle);
        EndDocPrinter(printer_handle);
        closePrinter(printer_handle);
        throw PrintJobFailedException("Failed to write to printer: " + error);
    }

    // End page and document
    EndPagePrinter(printer_handle);
    EndDocPrinter(printer_handle);

    // Close the printer
    closePrinter(printer_handle);

    return std::make_unique<WindowsPrintJob>(job_id, document_name, m_name);
}

std::unique_ptr<PrintJob> WindowsPrinter::printImage(
    const std::filesystem::path& image_path, const PrintSettings& settings) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Validate file existence
    if (!std::filesystem::exists(image_path)) {
        throw PrintJobFailedException("File does not exist: " +
                                      image_path.string());
    }

    // Load the image with GDI+
    std::wstring wide_path = utf8ToWide(image_path.string());
    Gdiplus::Bitmap* bitmap = new Gdiplus::Bitmap(wide_path.c_str());

    if (bitmap->GetLastStatus() != Gdiplus::Ok) {
        delete bitmap;
        throw PrintJobFailedException("Failed to load image: " +
                                      image_path.string());
    }

    // Get a device context for the printer
    HDC printer_dc =
        CreateDCW(L"WINSPOOL", utf8ToWide(m_name).c_str(), nullptr, nullptr);
    if (printer_dc == nullptr) {
        delete bitmap;
        throw PrintJobFailedException(
            "Failed to create printer device context: " +
            getLastErrorAsString());
    }

    // Apply print settings
    DEVMODE* dev_mode = createDevModeWithSettings(settings);
    if (dev_mode) {
        ResetDCW(printer_dc, dev_mode);
        delete[] reinterpret_cast<BYTE*>(dev_mode);
    }

    // Start the document
    std::wstring doc_name =
        utf8ToWide("Print: " + image_path.filename().string());
    DOCINFOW doc_info = {0};
    doc_info.cbSize = sizeof(DOCINFOW);
    doc_info.lpszDocName = doc_name.c_str();
    doc_info.lpszOutput = nullptr;

    int job_id = StartDocW(printer_dc, &doc_info);
    if (job_id <= 0) {
        std::string error = getLastErrorAsString();
        DeleteDC(printer_dc);
        delete bitmap;
        throw PrintJobFailedException("Failed to start print job: " + error);
    }

    // Start a page
    if (StartPage(printer_dc) <= 0) {
        std::string error = getLastErrorAsString();
        EndDoc(printer_dc);
        DeleteDC(printer_dc);
        delete bitmap;
        throw PrintJobFailedException("Failed to start page: " + error);
    }

    // Get printer page dimensions
    int printer_width = GetDeviceCaps(printer_dc, HORZRES);
    int printer_height = GetDeviceCaps(printer_dc, VERTRES);

    // Get image dimensions
    int image_width = bitmap->GetWidth();
    int image_height = bitmap->GetHeight();

    // Calculate scaling to fit the page while maintaining aspect ratio
    double scale_x = static_cast<double>(printer_width) / image_width;
    double scale_y = static_cast<double>(printer_height) / image_height;
    double scale = std::min(scale_x, scale_y) * settings.scale;

    // Calculate the destination rectangle
    int dest_width = static_cast<int>(image_width * scale);
    int dest_height = static_cast<int>(image_height * scale);

    // Center the image on the page
    int dest_x = (printer_width - dest_width) / 2;
    int dest_y = (printer_height - dest_height) / 2;

    // Create a Graphics object from the printer device context
    Gdiplus::Graphics graphics(printer_dc);

    // Set high quality rendering modes
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

    // Draw the image
    graphics.DrawImage(bitmap, dest_x, dest_y, dest_width, dest_height);

    // End the page and document
    EndPage(printer_dc);
    EndDoc(printer_dc);

    // Clean up
    DeleteDC(printer_dc);
    delete bitmap;

    return std::make_unique<WindowsPrintJob>(
        job_id, image_path.filename().string(), m_name);
}

std::unique_ptr<PrintJob> WindowsPrinter::printPDF(
    const std::filesystem::path& pdf_path, const PrintSettings& settings) {
    // Windows doesn't provide a built-in way to print PDFs directly
    // We use the shell execute method, which relies on the system's PDF reader

    SHELLEXECUTEINFOW sei = {sizeof(SHELLEXECUTEINFOW)};
    sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"print";
    sei.lpFile = pdf_path.c_str();
    sei.nShow = SW_HIDE;

    if (!ShellExecuteExW(&sei)) {
        throw PrintJobFailedException("Failed to print PDF file: " +
                                      getLastErrorAsString());
    }

    // Create a job ID (Windows shell printing doesn't give us a job ID)
    int job_id = static_cast<int>(reinterpret_cast<uintptr_t>(sei.hProcess));

    // Wait for the process to complete
    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, 5000);  // Wait up to 5 seconds
        CloseHandle(sei.hProcess);
    }

    return std::make_unique<WindowsPrintJob>(
        job_id, pdf_path.filename().string(), m_name);
}

std::unique_ptr<PrintJob> WindowsPrinter::printRaw(
    std::span<const std::byte> data, const std::string& document_name,
    const std::string& mime_type, const PrintSettings& settings) {
    std::lock_guard<std::mutex> lock(m_mutex);

    HANDLE printer_handle = openPrinter();
    if (printer_handle == nullptr) {
        throw PrintJobFailedException("Unable to open printer: " +
                                      getLastErrorAsString());
    }

    // Setup document info
    std::wstring doc_name = utf8ToWide(document_name);

    DOCINFOW doc_info = {0};
    doc_info.cbSize = sizeof(DOCINFOW);
    doc_info.lpszDocName = doc_name.c_str();
    doc_info.lpszOutput = nullptr;
    doc_info.lpszDatatype = L"RAW";

    // Start document
    int job_id = StartDocPrinterW(printer_handle, 1,
                                  reinterpret_cast<LPBYTE>(&doc_info));
    if (job_id <= 0) {
        std::string error = getLastErrorAsString();
        closePrinter(printer_handle);
        throw PrintJobFailedException("Failed to start print job: " + error);
    }

    // Apply print settings
    DEVMODE* dev_mode = createDevModeWithSettings(settings);
    if (dev_mode) {
        // Clean up allocated memory
        delete[] reinterpret_cast<BYTE*>(dev_mode);
    }

    // Start page
    if (!StartPagePrinter(printer_handle)) {
        std::string error = getLastErrorAsString();
        EndDocPrinter(printer_handle);
        closePrinter(printer_handle);
        throw PrintJobFailedException("Failed to start page: " + error);
    }

    // Write the data to the printer
    DWORD bytes_written = 0;
    if (!WritePrinter(printer_handle, data.data(),
                      static_cast<DWORD>(data.size_bytes()), &bytes_written)) {
        std::string error = getLastErrorAsString();
        EndPagePrinter(printer_handle);
        EndDocPrinter(printer_handle);
        closePrinter(printer_handle);
        throw PrintJobFailedException("Failed to write to printer: " + error);
    }

    // End page and document
    EndPagePrinter(printer_handle);
    EndDocPrinter(printer_handle);

    // Close the printer
    closePrinter(printer_handle);

    return std::make_unique<WindowsPrintJob>(job_id, document_name, m_name);
}

bool WindowsPrinter::supportsDuplex() const {
    HANDLE printer_handle = openPrinter();
    if (printer_handle == nullptr) {
        return false;
    }

    // Get printer capabilities
    HDC printer_dc =
        CreateDCW(L"WINSPOOL", utf8ToWide(m_name).c_str(), nullptr, nullptr);
    if (printer_dc == nullptr) {
        closePrinter(printer_handle);
        return false;
    }

    // Query duplex capability
    PRINTER_INFO_2* printer_info = getPrinterInfo();
    if (!printer_info) {
        DeleteDC(printer_dc);
        closePrinter(printer_handle);
        return false;
    }

    int capability =
        DeviceCapabilitiesW(utf8ToWide(m_name).c_str(), printer_info->pPortName,
                            DC_DUPLEX, nullptr, nullptr);

    delete[] reinterpret_cast<BYTE*>(printer_info);
    DeleteDC(printer_dc);
    closePrinter(printer_handle);

    return capability == 1;
}

bool WindowsPrinter::supportsColor() const {
    HANDLE printer_handle = openPrinter();
    if (printer_handle == nullptr) {
        return false;
    }

    // Get printer capabilities
    HDC printer_dc =
        CreateDCW(L"WINSPOOL", utf8ToWide(m_name).c_str(), nullptr, nullptr);
    if (printer_dc == nullptr) {
        closePrinter(printer_handle);
        return false;
    }

    // Check color capabilities
    int color_support = GetDeviceCaps(printer_dc, NUMCOLORS);
    bool supports_color = color_support != 2;  // 2 means monochrome

    DeleteDC(printer_dc);
    closePrinter(printer_handle);

    return supports_color;
}

std::vector<MediaSize> WindowsPrinter::getSupportedMediaSizes() const {
    std::vector<MediaSize> sizes;

    // Default media sizes that most printers support
    sizes.push_back(MediaSize::A4);
    sizes.push_back(MediaSize::Letter);

    HANDLE printer_handle = openPrinter();
    if (printer_handle == nullptr) {
        return sizes;
    }

    // Get printer capabilities
    PRINTER_INFO_2* printer_info = getPrinterInfo();
    if (!printer_info) {
        closePrinter(printer_handle);
        return sizes;
    }

    // Query supported paper sizes
    DWORD num_sizes =
        DeviceCapabilitiesW(utf8ToWide(m_name).c_str(), printer_info->pPortName,
                            DC_PAPERS, nullptr, nullptr);

    if (num_sizes > 0) {
        // Clear the default sizes and get the actual supported sizes
        sizes.clear();

        std::vector<WORD> paper_sizes(num_sizes);
        DeviceCapabilitiesW(
            utf8ToWide(m_name).c_str(), printer_info->pPortName, DC_PAPERS,
            reinterpret_cast<LPWSTR>(paper_sizes.data()), nullptr);

        for (DWORD i = 0; i < num_sizes; i++) {
            // Map Windows paper sizes to our media sizes
            switch (paper_sizes[i]) {
                case DMPAPER_A4:
                    sizes.push_back(MediaSize::A4);
                    break;
                case DMPAPER_LETTER:
                    sizes.push_back(MediaSize::Letter);
                    break;
                case DMPAPER_LEGAL:
                    sizes.push_back(MediaSize::Legal);
                    break;
                case DMPAPER_EXECUTIVE:
                    sizes.push_back(MediaSize::Executive);
                    break;
                case DMPAPER_A3:
                    sizes.push_back(MediaSize::A3);
                    break;
                case DMPAPER_A5:
                    sizes.push_back(MediaSize::A5);
                    break;
                case DMPAPER_B5:
                    sizes.push_back(MediaSize::B5);
                    break;
                case DMPAPER_ENV_10:
                    sizes.push_back(MediaSize::Envelope10);
                    break;
                case DMPAPER_ENV_DL:
                    sizes.push_back(MediaSize::EnvelopeDL);
                    break;
                case DMPAPER_ENV_C5:
                    sizes.push_back(MediaSize::EnvelopeC5);
                    break;
            }
        }
    }

    // Check for custom size support
    if (supportsCustomPageSizes()) {
        sizes.push_back(MediaSize::Custom);
    }

    delete[] reinterpret_cast<BYTE*>(printer_info);
    closePrinter(printer_handle);

    return sizes;
}

bool WindowsPrinter::supportsCustomPageSizes() const {
    HANDLE printer_handle = openPrinter();
    if (printer_handle == nullptr) {
        return false;
    }

    // Get printer capabilities
    PRINTER_INFO_2* printer_info = getPrinterInfo();
    if (!printer_info) {
        closePrinter(printer_handle);
        return false;
    }

    // Query custom page size capability
    int capability =
        DeviceCapabilitiesW(utf8ToWide(m_name).c_str(), printer_info->pPortName,
                            DC_PAPERSIZE, nullptr, nullptr);

    delete[] reinterpret_cast<BYTE*>(printer_info);
    closePrinter(printer_handle);

    return capability != -1;
}

std::vector<PrintQuality> WindowsPrinter::getSupportedQualitySettings() const {
    std::vector<PrintQuality> qualities;

    // Default qualities
    qualities.push_back(PrintQuality::Draft);
    qualities.push_back(PrintQuality::Normal);
    qualities.push_back(PrintQuality::High);

    return qualities;
}

std::vector<std::unique_ptr<PrintJob>> WindowsPrinter::getActiveJobs() const {
    std::vector<std::unique_ptr<PrintJob>> jobs;

    HANDLE printer_handle = openPrinter();
    if (printer_handle == nullptr) {
        return jobs;
    }

    // Determine required buffer size for job info
    DWORD needed = 0;
    DWORD returned = 0;
    EnumJobs(printer_handle, 0, DWORD_MAX, 2, nullptr, 0, &needed, &returned);

    if (needed == 0) {
        closePrinter(printer_handle);
        return jobs;
    }

    // Allocate buffer for job info
    BYTE* buffer = new BYTE[needed];

    // Get job info
    if (EnumJobs(printer_handle, 0, DWORD_MAX, 2, buffer, needed, &needed,
                 &returned)) {
        JOB_INFO_2* job_info = reinterpret_cast<JOB_INFO_2*>(buffer);

        for (DWORD i = 0; i < returned; i++) {
            // Skip jobs that are already completed
            if (job_info[i].Status & JOB_STATUS_COMPLETE ||
                job_info[i].Status & JOB_STATUS_DELETED) {
                continue;
            }

            std::string job_name;
            if (job_info[i].pDocument) {
                job_name = wideToUtf8(job_info[i].pDocument);
            } else {
                job_name = "Job " + std::to_string(job_info[i].JobId);
            }

            jobs.push_back(std::make_unique<WindowsPrintJob>(job_info[i].JobId,
                                                             job_name, m_name));
        }
    }

    // Clean up
    delete[] buffer;
    closePrinter(printer_handle);

    return jobs;
}

std::unique_ptr<PrintJob> WindowsPrinter::getJob(int job_id) const {
    HANDLE printer_handle = openPrinter();
    if (printer_handle == nullptr) {
        throw PrintJobNotFoundException(job_id);
    }

    // Get job info
    DWORD needed = 0;
    GetJob(printer_handle, job_id, 2, nullptr, 0, &needed);

    if (needed == 0) {
        closePrinter(printer_handle);
        throw PrintJobNotFoundException(job_id);
    }

    // Allocate buffer for job info
    BYTE* buffer = new BYTE[needed];

    // Get job info
    bool success =
        GetJob(printer_handle, job_id, 2, buffer, needed, &needed) == TRUE;

    if (!success) {
        delete[] buffer;
        closePrinter(printer_handle);
        throw PrintJobNotFoundException(job_id);
    }

    // Create job object
    JOB_INFO_2* job_info = reinterpret_cast<JOB_INFO_2*>(buffer);

    std::string job_name;
    if (job_info->pDocument) {
        job_name = wideToUtf8(job_info->pDocument);
    } else {
        job_name = "Job " + std::to_string(job_info->JobId);
    }

    std::unique_ptr<PrintJob> job =
        std::make_unique<WindowsPrintJob>(job_info->JobId, job_name, m_name);

    // Clean up
    delete[] buffer;
    closePrinter(printer_handle);

    return job;
}

bool WindowsPrinter::setAsDefault() {
    return SetDefaultPrinterW(utf8ToWide(m_name).c_str()) == TRUE;
}

HANDLE WindowsPrinter::openPrinter() const {
    HANDLE printer_handle = nullptr;
    OpenPrinterW(utf8ToWide(m_name).c_str(), &printer_handle, nullptr);
    return printer_handle;
}

void WindowsPrinter::closePrinter(HANDLE printer_handle) const {
    if (printer_handle) {
        ClosePrinter(printer_handle);
    }
}

PRINTER_INFO_2* WindowsPrinter::getPrinterInfo() const {
    HANDLE printer_handle = openPrinter();
    if (printer_handle == nullptr) {
        return nullptr;
    }

    // Get required buffer size
    DWORD needed = 0;
    GetPrinterW(printer_handle, 2, nullptr, 0, &needed);

    if (needed == 0) {
        closePrinter(printer_handle);
        return nullptr;
    }

    // Allocate buffer
    BYTE* buffer = new BYTE[needed];

    // Get printer info
    BOOL result = GetPrinterW(printer_handle, 2, buffer, needed, &needed);

    // Close the printer
    closePrinter(printer_handle);

    if (!result) {
        delete[] buffer;
        return nullptr;
    }

    return reinterpret_cast<PRINTER_INFO_2*>(buffer);
}

DEVMODE* WindowsPrinter::createDevMode() const {
    // Get the printer's default DEVMODE
    HANDLE printer_handle = openPrinter();
    if (printer_handle == nullptr) {
        return nullptr;
    }

    // Get printer info to get the driver name
    PRINTER_INFO_2* printer_info = getPrinterInfo();
    if (!printer_info) {
        closePrinter(printer_handle);
        return nullptr;
    }

    // Get the size of the DEVMODE structure
    DWORD dev_mode_size =
        DocumentPropertiesW(nullptr,                     // Parent window
                            printer_handle,              // Printer handle
                            utf8ToWide(m_name).c_str(),  // Printer name
                            nullptr,                     // Output buffer
                            nullptr,                     // Input buffer
                            0                            // Query size
        );

    if (dev_mode_size <= 0) {
        delete[] reinterpret_cast<BYTE*>(printer_info);
        closePrinter(printer_handle);
        return nullptr;
    }

    // Allocate memory for the DEVMODE structure
    DEVMODE* dev_mode = reinterpret_cast<DEVMODE*>(new BYTE[dev_mode_size]);
    ZeroMemory(dev_mode, dev_mode_size);

    // Get the default DEVMODE
    DWORD result =
        DocumentPropertiesW(nullptr,                     // Parent window
                            printer_handle,              // Printer handle
                            utf8ToWide(m_name).c_str(),  // Printer name
                            dev_mode,                    // Output buffer
                            nullptr,                     // Input buffer
                            DM_OUT_BUFFER                // Get current settings
        );

    // Clean up
    delete[] reinterpret_cast<BYTE*>(printer_info);
    closePrinter(printer_handle);

    if (result != IDOK) {
        delete[] reinterpret_cast<BYTE*>(dev_mode);
        return nullptr;
    }

    return dev_mode;
}

DEVMODE* WindowsPrinter::createDevModeWithSettings(
    const PrintSettings& settings) const {
    // Get the default DEVMODE
    DEVMODE* dev_mode = createDevMode();
    if (!dev_mode) {
        return nullptr;
    }

    // Apply the settings
    applyPrintSettings(dev_mode, settings);

    return dev_mode;
}

void WindowsPrinter::applyPrintSettings(DEVMODE* dev_mode,
                                        const PrintSettings& settings) const {
    if (dev_mode == nullptr)
        return;

    // Copies
    dev_mode->dmCopies = static_cast<short>(settings.copies);
    dev_mode->dmFields |= DM_COPIES;

    // Duplex
    dev_mode->dmDuplex = duplexToDevMode(settings.duplex_mode);
    dev_mode->dmFields |= DM_DUPLEX;

    // Color mode
    dev_mode->dmColor = colorToDevMode(settings.color_mode);
    dev_mode->dmFields |= DM_COLOR;

    // Paper size
    if (settings.media_size == MediaSize::Custom &&
        settings.custom_size.has_value()) {
        // Custom page size in 1/10 mm
        dev_mode->dmPaperWidth =
            static_cast<short>(settings.custom_size->width_mm * 10.0);
        dev_mode->dmPaperLength =
            static_cast<short>(settings.custom_size->height_mm * 10.0);
        dev_mode->dmFields |= DM_PAPERWIDTH | DM_PAPERLENGTH;
        dev_mode->dmPaperSize = DMPAPER_USER;
    } else {
        // Standard paper size
        dev_mode->dmPaperSize = mediaSizeToDevMode(settings.media_size);
        dev_mode->dmFields |= DM_PAPERSIZE;
    }

    // Orientation
    dev_mode->dmOrientation = orientationToDevMode(settings.orientation);
    dev_mode->dmFields |= DM_ORIENTATION;

    // Quality
    auto [dpi_x, dpi_y] = qualityToDpi(settings.quality);
    dev_mode->dmPrintQuality = static_cast<short>(dpi_y);
    dev_mode->dmYResolution = static_cast<short>(dpi_y);
    dev_mode->dmFields |= DM_PRINTQUALITY | DM_YRESOLUTION;

    // Collate
    dev_mode->dmCollate = settings.collate ? DMCOLLATE_TRUE : DMCOLLATE_FALSE;
    dev_mode->dmFields |= DM_COLLATE;
}

std::wstring WindowsPrinter::utf8ToWide(const std::string& str) {
    if (str.empty())
        return std::wstring();

    // Calculate required buffer size
    int size_needed = MultiByteToWideChar(
        CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);

    // Allocate buffer
    std::wstring result(size_needed, 0);

    // Convert
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()),
                        &result[0], size_needed);

    return result;
}

std::string WindowsPrinter::wideToUtf8(const std::wstring& wstr) {
    if (wstr.empty())
        return std::string();

    // Calculate required buffer size
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(),
                                          static_cast<int>(wstr.size()),
                                          nullptr, 0, nullptr, nullptr);

    // Allocate buffer
    std::string result(size_needed, 0);

    // Convert
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()),
                        &result[0], size_needed, nullptr, nullptr);

    return result;
}

std::string WindowsPrinter::getLastErrorAsString() {
    DWORD error_code = GetLastError();
    if (error_code == 0) {
        return "No error";
    }

    LPWSTR buffer = nullptr;

    DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);

    if (size == 0) {
        return "Error code " + std::to_string(error_code);
    }

    std::wstring wide_message(buffer, size);
    LocalFree(buffer);

    // Remove trailing newlines
    while (!wide_message.empty() &&
           (wide_message.back() == L'\n' || wide_message.back() == L'\r')) {
        wide_message.pop_back();
    }

    return wideToUtf8(wide_message);
}

short WindowsPrinter::duplexToDevMode(DuplexMode mode) {
    switch (mode) {
        case DuplexMode::None:
            return DMDUP_SIMPLEX;
        case DuplexMode::LongEdge:
            return DMDUP_VERTICAL;
        case DuplexMode::ShortEdge:
            return DMDUP_HORIZONTAL;
        default:
            return DMDUP_SIMPLEX;
    }
}

short WindowsPrinter::colorToDevMode(ColorMode mode) {
    switch (mode) {
        case ColorMode::Color:
            return DMCOLOR_COLOR;
        case ColorMode::Grayscale:
        case ColorMode::Monochrome:
            return DMCOLOR_MONOCHROME;
        default:
            return DMCOLOR_COLOR;
    }
}

short WindowsPrinter::orientationToDevMode(Orientation orientation) {
    switch (orientation) {
        case Orientation::Portrait:
            return DMORIENT_PORTRAIT;
        case Orientation::Landscape:
            return DMORIENT_LANDSCAPE;
        default:
            return DMORIENT_PORTRAIT;
    }
}

short WindowsPrinter::mediaSizeToDevMode(MediaSize size) {
    switch (size) {
        case MediaSize::A4:
            return DMPAPER_A4;
        case MediaSize::Letter:
            return DMPAPER_LETTER;
        case MediaSize::Legal:
            return DMPAPER_LEGAL;
        case MediaSize::Executive:
            return DMPAPER_EXECUTIVE;
        case MediaSize::A3:
            return DMPAPER_A3;
        case MediaSize::A5:
            return DMPAPER_A5;
        case MediaSize::B5:
            return DMPAPER_B5;
        case MediaSize::Envelope10:
            return DMPAPER_ENV_10;
        case MediaSize::EnvelopeDL:
            return DMPAPER_ENV_DL;
        case MediaSize::EnvelopeC5:
            return DMPAPER_ENV_C5;
        case MediaSize::Custom:
            return DMPAPER_USER;
        default:
            return DMPAPER_A4;
    }
}

std::pair<short, short> WindowsPrinter::qualityToDpi(PrintQuality quality) {
    switch (quality) {
        case PrintQuality::Draft:
            return {300, 300};
        case PrintQuality::Normal:
            return {600, 600};
        case PrintQuality::High:
            return {1200, 1200};
        default:
            return {600, 600};
    }
}

//====================
// WindowsPrintManager Implementation
//====================

WindowsPrintManager::WindowsPrintManager()
    : m_last_refresh(std::chrono::steady_clock::now() - std::chrono::hours(1)) {
    // Force an initial refresh
    refreshPrinterList();
}

WindowsPrintManager::~WindowsPrintManager() = default;

std::vector<std::shared_ptr<Printer>>
WindowsPrintManager::getAvailablePrinters() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Refresh printer list if needed
    refreshIfNeeded();

    std::vector<std::shared_ptr<Printer>> result;

    // Get the list of printers
    DWORD flags = PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS;
    DWORD needed = 0;
    DWORD returned = 0;

    // First call to get required buffer size
    EnumPrintersW(flags, nullptr, 2, nullptr, 0, &needed, &returned);

    if (needed == 0) {
        return result;
    }

    // Allocate buffer
    BYTE* buffer = new BYTE[needed];

    // Second call to get printer info
    if (EnumPrintersW(flags, nullptr, 2, buffer, needed, &needed, &returned)) {
        PRINTER_INFO_2* printer_info =
            reinterpret_cast<PRINTER_INFO_2*>(buffer);

        for (DWORD i = 0; i < returned; i++) {
            std::string name = wideToUtf8(printer_info[i].pPrinterName);

            // Check if we already have this printer
            auto it = m_printers.find(name);
            if (it != m_printers.end()) {
                // Check if the weak pointer is still valid
                if (auto printer = it->second.lock()) {
                    result.push_back(printer);
                    continue;
                }
            }

            // Create a new printer object
            try {
                auto printer = std::make_shared<WindowsPrinter>(name);
                m_printers[name] = printer;
                result.push_back(printer);
            } catch (const PrinterException&) {
                // Ignore printers that can't be accessed
            }
        }
    }

    // Clean up
    delete[] buffer;

    return result;
}

std::shared_ptr<Printer> WindowsPrintManager::getDefaultPrinter() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Refresh printer list if needed
    refreshIfNeeded();

    // Get the default printer name
    DWORD needed = 0;
    GetDefaultPrinterW(nullptr, &needed);

    if (needed == 0) {
        return nullptr;
    }

    std::vector<wchar_t> buffer(needed);
    if (!GetDefaultPrinterW(buffer.data(), &needed)) {
        return nullptr;
    }

    std::string default_name = wideToUtf8(buffer.data());

    // Check if we already have this printer
    auto it = m_printers.find(default_name);
    if (it != m_printers.end()) {
        // Check if the weak pointer is still valid
        if (auto printer = it->second.lock()) {
            return printer;
        }
    }

    // Create a new printer object
    try {
        auto printer = std::make_shared<WindowsPrinter>(default_name);
        m_printers[default_name] = printer;
        return printer;
    } catch (const PrinterException&) {
        // Return nullptr if the printer can't be accessed
        return nullptr;
    }
}

std::shared_ptr<Printer> WindowsPrintManager::getPrinterByName(
    const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if we already have this printer
    auto it = m_printers.find(name);
    if (it != m_printers.end()) {
        // Check if the weak pointer is still valid
        if (auto printer = it->second.lock()) {
            return printer;
        }
    }

    // Create a new printer object
    try {
        auto printer = std::make_shared<WindowsPrinter>(name);
        m_printers[name] = printer;
        return printer;
    } catch (const PrinterException&) {
        // Return nullptr if the printer can't be accessed
        return nullptr;
    }
}

void WindowsPrintManager::refreshPrinterList() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Clear the cache
    m_printers.clear();

    // Update the refresh time
    m_last_refresh = std::chrono::steady_clock::now();

    // Force a refresh by calling EnumPrinters
    DWORD flags = PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS;
    DWORD needed = 0;
    DWORD returned = 0;

    // First call to get required buffer size
    EnumPrintersW(flags, nullptr, 2, nullptr, 0, &needed, &returned);

    if (needed == 0) {
        return;
    }

    // Allocate buffer
    BYTE* buffer = new BYTE[needed];

    // Second call to get printer info
    if (EnumPrintersW(flags, nullptr, 2, buffer, needed, &needed, &returned)) {
        PRINTER_INFO_2* printer_info =
            reinterpret_cast<PRINTER_INFO_2*>(buffer);

        for (DWORD i = 0; i < returned; i++) {
            std::string name = wideToUtf8(printer_info[i].pPrinterName);

            // Create a new printer object
            try {
                auto printer = std::make_shared<WindowsPrinter>(name);
                m_printers[name] = printer;
            } catch (const PrinterException&) {
                // Ignore printers that can't be accessed
            }
        }
    }

    // Clean up
    delete[] buffer;
}

bool WindowsPrintManager::canPrintToPDF() const {
    return getPDFPrinter() != nullptr;
}

std::shared_ptr<Printer> WindowsPrintManager::getPDFPrinter() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Refresh printer list if needed
    refreshIfNeeded();

    // Common names for PDF printers in Windows
    const std::vector<std::string> pdf_printer_names = {
        "Microsoft Print to PDF",
        "Adobe PDF",
        "PDF Writer",
        "PDF Printer",
        "Bullzip PDF Printer",
        "Foxit PDF Printer",
        "PDFCreator"};

    // Check for any of the common PDF printer names
    for (const auto& name : pdf_printer_names) {
        auto printer = getPrinterByName(name);
        if (printer) {
            return printer;
        }
    }

    // If no specific PDF printer found, look for any printer with "PDF" in the
    // name
    DWORD flags = PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS;
    DWORD needed = 0;
    DWORD returned = 0;

    EnumPrintersW(flags, nullptr, 2, nullptr, 0, &needed, &returned);

    if (needed == 0) {
        return nullptr;
    }

    BYTE* buffer = new BYTE[needed];

    if (EnumPrintersW(flags, nullptr, 2, buffer, needed, &needed, &returned)) {
        PRINTER_INFO_2* printer_info =
            reinterpret_cast<PRINTER_INFO_2*>(buffer);

        for (DWORD i = 0; i < returned; i++) {
            std::string name = wideToUtf8(printer_info[i].pPrinterName);
            if (name.find("PDF") != std::string::npos ||
                name.find("pdf") != std::string::npos) {
                delete[] buffer;
                return getPrinterByName(name);
            }
        }
    }

    delete[] buffer;
    return nullptr;
}

void WindowsPrintManager::refreshIfNeeded() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::seconds>(now - m_last_refresh)
            .count();

    if (elapsed > CACHE_REFRESH_SECONDS) {
        // Remove const for internal cache update
        const_cast<WindowsPrintManager*>(this)->refreshPrinterList();
    }
}

}  // namespace print_system

#endif  // PRINT_SYSTEM_WINDOWS
