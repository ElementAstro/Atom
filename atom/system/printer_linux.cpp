#ifdef PRINT_SYSTEM_LINUX

#include <cups/ppd.h>  // For advanced PPD functionality
#include <unistd.h>
#include <algorithm>
#include <cstring>
#include <format>
#include <fstream>
#include <sstream>
#include <thread>
#include "printer_system_linux.hpp"

namespace print_system {

//====================
// LinuxPrintJob Implementation
//====================

LinuxPrintJob::LinuxPrintJob(int job_id, const std::string& job_name,
                             const std::string& printer_name)
    : m_job_id(job_id),
      m_job_name(job_name),
      m_printer_name(printer_name),
      m_submit_time(std::chrono::system_clock::now()) {}

LinuxPrintJob::~LinuxPrintJob() = default;

JobStatus LinuxPrintJob::getJobStatus() const {
    cups_job_t* job = getJobInfo();
    if (!job) {
        return JobStatus::Failed;
    }

    JobStatus status = convertJobState(job->state);

    cupsFreeJobs(1, job);

    return status;
}

std::string LinuxPrintJob::getStatusString() const {
    cups_job_t* job = getJobInfo();
    if (!job) {
        return "Unknown (job not found)";
    }

    std::string status_str;

    switch (job->state) {
        case IPP_JOB_PENDING:
            status_str = "Pending";
            break;
        case IPP_JOB_HELD:
            status_str = "Held";
            break;
        case IPP_JOB_PROCESSING:
            status_str = "Processing";
            break;
        case IPP_JOB_STOPPED:
            status_str = "Stopped";
            break;
        case IPP_JOB_CANCELED:
            status_str = "Canceled";
            break;
        case IPP_JOB_ABORTED:
            status_str = "Aborted";
            break;
        case IPP_JOB_COMPLETED:
            status_str = "Completed";
            break;
        default:
            status_str = "Unknown";
            break;
    }

    cupsFreeJobs(1, job);

    return status_str;
}

bool LinuxPrintJob::cancel() {
    return cupsCancelJob(m_printer_name.c_str(), m_job_id) == 1;
}

bool LinuxPrintJob::pause() {
    return cupsHoldJob(m_printer_name.c_str(), m_job_id) == 1;
}

bool LinuxPrintJob::resume() {
    return cupsReleaseJob(m_printer_name.c_str(), m_job_id) == 1;
}

float LinuxPrintJob::getCompletionPercentage() const {
    cups_job_t* job = getJobInfo();
    if (!job) {
        return 0.0f;
    }

    float completion = 0.0f;

    // Estimate completion based on state
    switch (job->state) {
        case IPP_JOB_PENDING:
            completion = 0.0f;
            break;
        case IPP_JOB_HELD:
            completion = 0.0f;
            break;
        case IPP_JOB_PROCESSING:
            // CUPS doesn't provide completion percentage, so we estimate 50%
            completion = 50.0f;
            break;
        case IPP_JOB_STOPPED:
            completion = 75.0f;
            break;
        case IPP_JOB_CANCELED:
        case IPP_JOB_ABORTED:
            completion = 100.0f;
            break;
        case IPP_JOB_COMPLETED:
            completion = 100.0f;
            break;
        default:
            completion = 0.0f;
            break;
    }

    cupsFreeJobs(1, job);

    return completion;
}

bool LinuxPrintJob::waitForCompletion(
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

cups_job_t* LinuxPrintJob::getJobInfo() const {
    cups_job_t* jobs = nullptr;
    int num_jobs = 0;

    // Get jobs for this printer
    num_jobs =
        cupsGetJobs(&jobs, m_printer_name.c_str(), 0, CUPS_WHICHJOBS_ALL);

    if (num_jobs <= 0) {
        return nullptr;
    }

    // Find our job
    cups_job_t* our_job = nullptr;
    for (int i = 0; i < num_jobs; i++) {
        if (jobs[i].id == m_job_id) {
            // Found our job - make a copy
            our_job = new cups_job_t;
            *our_job = jobs[i];
            break;
        }
    }

    // Free the jobs array
    cupsFreeJobs(num_jobs, jobs);

    return our_job;
}

JobStatus LinuxPrintJob::convertJobState(ipp_jstate_t cups_state) {
    switch (cups_state) {
        case IPP_JOB_PENDING:
            return JobStatus::Pending;
        case IPP_JOB_HELD:
            return JobStatus::Paused;
        case IPP_JOB_PROCESSING:
            return JobStatus::Processing;
        case IPP_JOB_STOPPED:
            return JobStatus::Paused;
        case IPP_JOB_CANCELED:
            return JobStatus::Canceled;
        case IPP_JOB_ABORTED:
            return JobStatus::Failed;
        case IPP_JOB_COMPLETED:
            return JobStatus::Completed;
        default:
            return JobStatus::Pending;
    }
}

//====================
// LinuxPrinter Implementation
//====================

LinuxPrinter::LinuxPrinter(const std::string& name) : m_name(name) {
    // Verify printer exists in CUPS
    cups_dest_t* dest = findPrinter();
    if (dest == nullptr) {
        throw PrinterNotFoundException(name);
    }
    cupsFreeDests(1, dest);
}

LinuxPrinter::~LinuxPrinter() = default;

std::string LinuxPrinter::getModel() const {
    cups_dest_t* dest = findPrinter();
    if (dest == nullptr) {
        return "Unknown";
    }

    const char* model = cupsGetOption("printer-make-and-model",
                                      dest->num_options, dest->options);
    std::string result = model ? model : "Unknown";

    cupsFreeDests(1, dest);
    return result;
}

std::string LinuxPrinter::getLocation() const {
    cups_dest_t* dest = findPrinter();
    if (dest == nullptr) {
        return "";
    }

    const char* location =
        cupsGetOption("printer-location", dest->num_options, dest->options);
    std::string result = location ? location : "";

    cupsFreeDests(1, dest);
    return result;
}

std::string LinuxPrinter::getDescription() const {
    cups_dest_t* dest = findPrinter();
    if (dest == nullptr) {
        return "";
    }

    const char* info =
        cupsGetOption("printer-info", dest->num_options, dest->options);
    std::string result = info ? info : "";

    cupsFreeDests(1, dest);
    return result;
}

PrinterStatus LinuxPrinter::getStatus() const {
    PrinterStatus status;

    cups_dest_t* dest = findPrinter();
    if (dest == nullptr) {
        status.is_online = false;
        status.is_ready = false;
        status.error_message = "Printer not found";
        return status;
    }

    // Check printer state
    const char* state =
        cupsGetOption("printer-state", dest->num_options, dest->options);
    if (state) {
        int state_value = std::stoi(state);
        status.is_online = (state_value != IPP_PRINTER_STOPPED);
        status.is_ready = (state_value == IPP_PRINTER_IDLE);
    }

    // Check for error message
    const char* state_message = cupsGetOption("printer-state-message",
                                              dest->num_options, dest->options);
    if (state_message && state_message[0] != '\0') {
        status.error_message = state_message;
    }

    // Get pending job count
    cups_job_t* jobs = nullptr;
    status.pending_jobs =
        cupsGetJobs(&jobs, m_name.c_str(), 1, CUPS_WHICHJOBS_ACTIVE);
    cupsFreeJobs(status.pending_jobs, jobs);

    cupsFreeDests(1, dest);
    return status;
}

std::unique_ptr<PrintJob> LinuxPrinter::print(
    const std::filesystem::path& file_path, const PrintSettings& settings) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Validate file existence
    if (!std::filesystem::exists(file_path)) {
        throw PrintJobFailedException("File does not exist: " +
                                      file_path.string());
    }

    // Set up CUPS options
    cups_option_t* options = nullptr;
    int num_options = 0;

    // Apply settings
    applyCupsOptions(&options, &num_options, settings);

    // Determine file type based on extension
    std::string mime_type = getMimeTypeForFile(file_path);

    // Print the file
    std::string title = "Print: " + file_path.filename().string();
    int job_id = cupsPrintFile(m_name.c_str(), file_path.c_str(), title.c_str(),
                               num_options, options);

    // Free options
    cupsFreeOptions(num_options, options);

    if (job_id <= 0) {
        throw PrintJobFailedException(cupsLastErrorString());
    }

    return std::make_unique<LinuxPrintJob>(job_id, title, m_name);
}

std::unique_ptr<PrintJob> LinuxPrinter::printText(
    const std::string& text, const std::string& document_name,
    const PrintSettings& settings) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Create a temporary file for the text
    char temp_filename[] = "/tmp/printXXXXXX";
    int fd = mkstemp(temp_filename);
    if (fd < 0) {
        throw PrintJobFailedException("Failed to create temporary file");
    }

    // Write the text to the file
    ssize_t bytes_written = write(fd, text.c_str(), text.size());
    close(fd);

    if (bytes_written != static_cast<ssize_t>(text.size())) {
        unlink(temp_filename);
        throw PrintJobFailedException("Failed to write to temporary file");
    }

    // Set up CUPS options
    cups_option_t* options = nullptr;
    int num_options = 0;

    // Apply settings
    applyCupsOptions(&options, &num_options, settings);

    // Add text options
    num_options = cupsAddOption("raw", "true", num_options, &options);

    // Print the file
    int job_id = cupsPrintFile(m_name.c_str(), temp_filename,
                               document_name.c_str(), num_options, options);

    // Free options and delete the temporary file
    cupsFreeOptions(num_options, options);
    unlink(temp_filename);

    if (job_id <= 0) {
        throw PrintJobFailedException(cupsLastErrorString());
    }

    return std::make_unique<LinuxPrintJob>(job_id, document_name, m_name);
}

std::unique_ptr<PrintJob> LinuxPrinter::printImage(
    const std::filesystem::path& image_path, const PrintSettings& settings) {
    // For images, we use the standard print method
    // CUPS will automatically detect and handle image files
    return print(image_path, settings);
}

std::unique_ptr<PrintJob> LinuxPrinter::printPDF(
    const std::filesystem::path& pdf_path, const PrintSettings& settings) {
    // For PDFs, we use the standard print method
    // CUPS handles PDFs natively
    return print(pdf_path, settings);
}

std::unique_ptr<PrintJob> LinuxPrinter::printRaw(
    std::span<const std::byte> data, const std::string& document_name,
    const std::string& mime_type, const PrintSettings& settings) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Create a temporary file for the raw data
    char temp_filename[] = "/tmp/printXXXXXX";
    int fd = mkstemp(temp_filename);
    if (fd < 0) {
        throw PrintJobFailedException("Failed to create temporary file");
    }

    // Write the data to the file
    ssize_t bytes_written = write(fd, data.data(), data.size_bytes());
    close(fd);

    if (bytes_written != static_cast<ssize_t>(data.size_bytes())) {
        unlink(temp_filename);
        throw PrintJobFailedException("Failed to write to temporary file");
    }

    // Set up CUPS options
    cups_option_t* options = nullptr;
    int num_options = 0;

    // Apply settings
    applyCupsOptions(&options, &num_options, settings);

    // Add raw options if needed
    if (mime_type == "application/vnd.cups-raw") {
        num_options = cupsAddOption("raw", "true", num_options, &options);
    }

    // Print the file with specified MIME type
    int job_id;
    if (!mime_type.empty() && mime_type != "application/octet-stream") {
        http_t* http =
            httpConnect2(cupsServer(), ippPort(), NULL, AF_UNSPEC,
                         HTTP_ENCRYPTION_IF_REQUESTED, 1, 30000, NULL);
        if (http) {
            job_id = cupsCreateJob(http, m_name.c_str(), document_name.c_str(),
                                   num_options, options);

            if (job_id > 0) {
                // Start the document
                if (cupsStartDocument(http, m_name.c_str(), job_id,
                                      document_name.c_str(), mime_type.c_str(),
                                      1) != HTTP_STATUS_CONTINUE) {
                    job_id = 0;
                } else {
                    // Write the document data
                    std::ifstream file(temp_filename, std::ios::binary);
                    char buffer[4096];
                    while (file.good()) {
                        file.read(buffer, sizeof(buffer));
                        if (cupsWriteRequestData(http, buffer, file.gcount()) !=
                            HTTP_STATUS_CONTINUE) {
                            job_id = 0;
                            break;
                        }
                    }

                    // Finish the document
                    if (job_id > 0 &&
                        cupsFinishDocument(http, m_name.c_str()) !=
                            IPP_STATUS_OK) {
                        job_id = 0;
                    }
                }
            }

            httpClose(http);
        } else {
            job_id = 0;
        }
    } else {
        // Default case: let CUPS detect the file type
        job_id = cupsPrintFile(m_name.c_str(), temp_filename,
                               document_name.c_str(), num_options, options);
    }

    // Free options and delete the temporary file
    cupsFreeOptions(num_options, options);
    unlink(temp_filename);

    if (job_id <= 0) {
        throw PrintJobFailedException(cupsLastErrorString());
    }

    return std::make_unique<LinuxPrintJob>(job_id, document_name, m_name);
}

bool LinuxPrinter::supportsDuplex() const {
    // Check if the printer supports duplex printing
    const char* printer_uri = nullptr;
    ppd_file_t* ppd = nullptr;
    bool has_duplex = false;

    cups_dest_t* dest = findPrinter();
    if (dest) {
        printer_uri =
            cupsGetOption("device-uri", dest->num_options, dest->options);
        if (printer_uri) {
            // Get the PPD file
            ppd = ppdOpenFile(cupsGetPPD(m_name.c_str()));
            if (ppd) {
                // Look for duplex options
                ppd_option_t* option = ppdFindOption(ppd, "Duplex");
                if (option) {
                    has_duplex = true;
                }
                ppdClose(ppd);
            }
        }
        cupsFreeDests(1, dest);
    }

    return has_duplex;
}

bool LinuxPrinter::supportsColor() const {
    // Check if the printer supports color printing
    ppd_file_t* ppd = nullptr;
    bool has_color = false;

    ppd = ppdOpenFile(cupsGetPPD(m_name.c_str()));
    if (ppd) {
        // Look for color options
        ppd_option_t* option = ppdFindOption(ppd, "ColorModel");
        if (option) {
            // Look for a color choice
            for (int i = 0; i < option->num_choices; i++) {
                if (strstr(option->choices[i].choice, "Color") ||
                    strstr(option->choices[i].choice, "RGB") ||
                    strstr(option->choices[i].choice, "CMY")) {
                    has_color = true;
                    break;
                }
            }
        }
        ppdClose(ppd);
    }

    return has_color;
}

std::vector<MediaSize> LinuxPrinter::getSupportedMediaSizes() const {
    std::vector<MediaSize> sizes;
    ppd_file_t* ppd = nullptr;

    // Default media sizes that most printers support
    sizes.push_back(MediaSize::A4);
    sizes.push_back(MediaSize::Letter);

    // Try to get detailed information from the PPD
    ppd = ppdOpenFile(cupsGetPPD(m_name.c_str()));
    if (ppd) {
        ppd_option_t* option = ppdFindOption(ppd, "PageSize");
        if (option) {
            // Reset the vector and add sizes based on PPD
            sizes.clear();

            for (int i = 0; i < option->num_choices; i++) {
                const char* choice = option->choices[i].choice;

                // Map known page sizes
                if (strcmp(choice, "A4") == 0)
                    sizes.push_back(MediaSize::A4);
                else if (strcmp(choice, "Letter") == 0)
                    sizes.push_back(MediaSize::Letter);
                else if (strcmp(choice, "Legal") == 0)
                    sizes.push_back(MediaSize::Legal);
                else if (strcmp(choice, "Executive") == 0)
                    sizes.push_back(MediaSize::Executive);
                else if (strcmp(choice, "A3") == 0)
                    sizes.push_back(MediaSize::A3);
                else if (strcmp(choice, "A5") == 0)
                    sizes.push_back(MediaSize::A5);
                else if (strcmp(choice, "B5") == 0)
                    sizes.push_back(MediaSize::B5);
                else if (strcmp(choice, "Env10") == 0)
                    sizes.push_back(MediaSize::Envelope10);
                else if (strcmp(choice, "EnvDL") == 0)
                    sizes.push_back(MediaSize::EnvelopeDL);
                else if (strcmp(choice, "EnvC5") == 0)
                    sizes.push_back(MediaSize::EnvelopeC5);
            }

            // Always add custom if supported
            if (supportsCustomPageSizes()) {
                sizes.push_back(MediaSize::Custom);
            }
        }
        ppdClose(ppd);
    }

    return sizes;
}

bool LinuxPrinter::supportsCustomPageSizes() const {
    ppd_file_t* ppd = nullptr;
    bool has_custom = false;

    ppd = ppdOpenFile(cupsGetPPD(m_name.c_str()));
    if (ppd) {
        has_custom = (ppd->custom_min[0] > 0 && ppd->custom_min[1] > 0 &&
                      ppd->custom_max[0] > 0 && ppd->custom_max[1] > 0);
        ppdClose(ppd);
    }

    return has_custom;
}

std::vector<PrintQuality> LinuxPrinter::getSupportedQualitySettings() const {
    std::vector<PrintQuality> qualities;

    // Default qualities
    qualities.push_back(PrintQuality::Draft);
    qualities.push_back(PrintQuality::Normal);
    qualities.push_back(PrintQuality::High);

    // We could check the PPD for more specific quality options,
    // but most printers support these three basic levels

    return qualities;
}

std::vector<std::unique_ptr<PrintJob>> LinuxPrinter::getActiveJobs() const {
    std::vector<std::unique_ptr<PrintJob>> jobs;
    cups_job_t* cups_jobs = nullptr;
    int num_jobs = 0;

    // Get all active jobs for this printer
    num_jobs =
        cupsGetJobs(&cups_jobs, m_name.c_str(), 1, CUPS_WHICHJOBS_ACTIVE);

    // Create PrintJob objects for each job
    for (int i = 0; i < num_jobs; i++) {
        jobs.push_back(std::make_unique<LinuxPrintJob>(
            cups_jobs[i].id,
            cups_jobs[i].title ? cups_jobs[i].title : "Unknown", m_name));
    }

    // Free CUPS jobs
    cupsFreeJobs(num_jobs, cups_jobs);

    return jobs;
}

std::unique_ptr<PrintJob> LinuxPrinter::getJob(int job_id) const {
    cups_job_t* cups_jobs = nullptr;
    int num_jobs = 0;

    // Get all jobs for this printer
    num_jobs = cupsGetJobs(&cups_jobs, m_name.c_str(), 0, CUPS_WHICHJOBS_ALL);

    // Find the specified job
    std::unique_ptr<PrintJob> job;
    for (int i = 0; i < num_jobs; i++) {
        if (cups_jobs[i].id == job_id) {
            job = std::make_unique<LinuxPrintJob>(
                cups_jobs[i].id,
                cups_jobs[i].title ? cups_jobs[i].title : "Unknown", m_name);
            break;
        }
    }

    // Free CUPS jobs
    cupsFreeJobs(num_jobs, cups_jobs);

    if (!job) {
        throw PrintJobNotFoundException(job_id);
    }

    return job;
}

bool LinuxPrinter::setAsDefault() {
    return cupsSetDefault(m_name.c_str()) == 1;
}

cups_dest_t* LinuxPrinter::findPrinter() const {
    cups_dest_t* dests = nullptr;
    cups_dest_t* dest = nullptr;
    int num_dests = cupsGetDests(&dests);

    dest = cupsGetDest(m_name.c_str(), nullptr, num_dests, dests);

    if (dest) {
        // We found the printer, create a copy
        cups_dest_t* result = new cups_dest_t;
        memcpy(result, dest, sizeof(cups_dest_t));

        // Copy options
        result->options = new cups_option_t[dest->num_options];
        memcpy(result->options, dest->options,
               sizeof(cups_option_t) * dest->num_options);

        // Free all destinations
        cupsFreeDests(num_dests, dests);

        return result;
    }

    // Free all destinations
    cupsFreeDests(num_dests, dests);

    return nullptr;
}

void LinuxPrinter::applyCupsOptions(cups_option_t** options, int* num_options,
                                    const PrintSettings& settings) const {
    // Number of copies
    *num_options =
        cupsAddOption("copies", std::to_string(settings.copies).c_str(),
                      *num_options, options);

    // Duplex mode
    *num_options =
        cupsAddOption("sides", duplexToCupsOption(settings.duplex_mode).c_str(),
                      *num_options, options);

    // Color mode
    *num_options = cupsAddOption("print-color-mode",
                                 colorToCupsOption(settings.color_mode).c_str(),
                                 *num_options, options);

    // Media size
    if (settings.media_size == MediaSize::Custom &&
        settings.custom_size.has_value()) {
        // Custom page size in points (1/72 inch)
        double width_pt = settings.custom_size->width_mm * 72.0 / 25.4;
        double height_pt = settings.custom_size->height_mm * 72.0 / 25.4;

        std::string page_size =
            std::format("{:.0f}x{:.0f}", width_pt, height_pt);
        *num_options = cupsAddOption("page-size", page_size.c_str(),
                                     *num_options, options);
    } else {
        *num_options = cupsAddOption(
            "media", mediaSizeToCupsOption(settings.media_size).c_str(),
            *num_options, options);
    }

    // Orientation
    *num_options =
        cupsAddOption("orientation-requested",
                      orientationToCupsOption(settings.orientation).c_str(),
                      *num_options, options);

    // Quality
    *num_options = cupsAddOption("print-quality",
                                 qualityToCupsOption(settings.quality).c_str(),
                                 *num_options, options);

    // Scaling
    if (settings.scale != 1.0) {
        std::string scale =
            std::to_string(static_cast<int>(settings.scale * 100)) + "%";
        *num_options = cupsAddOption("fitplot", "true", *num_options, options);
        *num_options =
            cupsAddOption("scaling", scale.c_str(), *num_options, options);
    }

    // Page ranges
    if (settings.page_ranges.has_value() && !settings.page_ranges->empty()) {
        std::string ranges;
        bool first = true;

        for (int page : *settings.page_ranges) {
            if (!first) {
                ranges += ",";
            }
            ranges += std::to_string(page);
            first = false;
        }

        *num_options =
            cupsAddOption("page-ranges", ranges.c_str(), *num_options, options);
    }

    // Collate
    *num_options = cupsAddOption("collate", settings.collate ? "true" : "false",
                                 *num_options, options);
}

std::string LinuxPrinter::getMimeTypeForFile(
    const std::filesystem::path& file_path) {
    std::string extension = file_path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Map common file extensions to MIME types
    if (extension == ".pdf")
        return "application/pdf";
    if (extension == ".ps")
        return "application/postscript";
    if (extension == ".txt")
        return "text/plain";
    if (extension == ".html" || extension == ".htm")
        return "text/html";
    if (extension == ".png")
        return "image/png";
    if (extension == ".jpg" || extension == ".jpeg")
        return "image/jpeg";
    if (extension == ".gif")
        return "image/gif";
    if (extension == ".tiff" || extension == ".tif")
        return "image/tiff";

    // Default to octet-stream for unknown types
    return "application/octet-stream";
}

std::string LinuxPrinter::duplexToCupsOption(DuplexMode mode) {
    switch (mode) {
        case DuplexMode::None:
            return "one-sided";
        case DuplexMode::LongEdge:
            return "two-sided-long-edge";
        case DuplexMode::ShortEdge:
            return "two-sided-short-edge";
        default:
            return "one-sided";
    }
}

std::string LinuxPrinter::colorToCupsOption(ColorMode mode) {
    switch (mode) {
        case ColorMode::Color:
            return "color";
        case ColorMode::Grayscale:
            return "monochrome";
        case ColorMode::Monochrome:
            return "bi-level";
        default:
            return "color";
    }
}

std::string LinuxPrinter::mediaSizeToCupsOption(MediaSize size) {
    switch (size) {
        case MediaSize::A4:
            return "iso_a4_210x297mm";
        case MediaSize::Letter:
            return "na_letter_8.5x11in";
        case MediaSize::Legal:
            return "na_legal_8.5x14in";
        case MediaSize::Executive:
            return "na_executive_7.25x10.5in";
        case MediaSize::A3:
            return "iso_a3_297x420mm";
        case MediaSize::A5:
            return "iso_a5_148x210mm";
        case MediaSize::B5:
            return "iso_b5_176x250mm";
        case MediaSize::Envelope10:
            return "na_number-10_4.125x9.5in";
        case MediaSize::EnvelopeDL:
            return "iso_dl_110x220mm";
        case MediaSize::EnvelopeC5:
            return "iso_c5_162x229mm";
        case MediaSize::Custom:
            return "custom";
        default:
            return "iso_a4_210x297mm";
    }
}

std::string LinuxPrinter::qualityToCupsOption(PrintQuality quality) {
    switch (quality) {
        case PrintQuality::Draft:
            return "3";  // IPP_QUALITY_DRAFT
        case PrintQuality::Normal:
            return "4";  // IPP_QUALITY_NORMAL
        case PrintQuality::High:
            return "5";  // IPP_QUALITY_HIGH
        default:
            return "4";  // IPP_QUALITY_NORMAL
    }
}

std::string LinuxPrinter::orientationToCupsOption(Orientation orientation) {
    switch (orientation) {
        case Orientation::Portrait:
            return "3";  // IPP_PORTRAIT
        case Orientation::Landscape:
            return "4";  // IPP_LANDSCAPE
        default:
            return "3";  // IPP_PORTRAIT
    }
}

//====================
// LinuxPrintManager Implementation
//====================

LinuxPrintManager::LinuxPrintManager()
    : m_last_refresh(std::chrono::steady_clock::now() - std::chrono::hours(1)) {
    // Initialize CUPS
    cupsSetUser(getenv("USER"));

    // Force an initial refresh
    refreshPrinterList();
}

LinuxPrintManager::~LinuxPrintManager() = default;

std::vector<std::shared_ptr<Printer>> LinuxPrintManager::getAvailablePrinters()
    const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Refresh printer list if needed
    refreshIfNeeded();

    std::vector<std::shared_ptr<Printer>> result;

    // Get the list of destinations from CUPS
    cups_dest_t* dests = nullptr;
    int num_dests = cupsGetDests(&dests);

    for (int i = 0; i < num_dests; i++) {
        std::string name(dests[i].name);

        // Skip the implicit class destinations
        if (name.find('@') != std::string::npos) {
            continue;
        }

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
            auto printer = std::make_shared<LinuxPrinter>(name);
            m_printers[name] = printer;
            result.push_back(printer);
        } catch (const PrinterException&) {
            // Ignore printers that can't be accessed
        }
    }

    // Free the destinations
    cupsFreeDests(num_dests, dests);

    return result;
}

std::shared_ptr<Printer> LinuxPrintManager::getDefaultPrinter() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Refresh printer list if needed
    refreshIfNeeded();

    // Get the default destination
    cups_dest_t* dests = nullptr;
    int num_dests = cupsGetDests(&dests);
    cups_dest_t* default_dest = cupsGetDest(NULL, NULL, num_dests, dests);

    if (!default_dest) {
        cupsFreeDests(num_dests, dests);
        return nullptr;
    }

    std::string default_name(default_dest->name);
    cupsFreeDests(num_dests, dests);

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
        auto printer = std::make_shared<LinuxPrinter>(default_name);
        m_printers[default_name] = printer;
        return printer;
    } catch (const PrinterException&) {
        // Return nullptr if the printer can't be accessed
        return nullptr;
    }
}

std::shared_ptr<Printer> LinuxPrintManager::getPrinterByName(
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

    // Check if the printer exists in CUPS
    cups_dest_t* dests = nullptr;
    int num_dests = cupsGetDests(&dests);
    cups_dest_t* dest = cupsGetDest(name.c_str(), NULL, num_dests, dests);

    if (!dest) {
        cupsFreeDests(num_dests, dests);
        return nullptr;
    }

    cupsFreeDests(num_dests, dests);

    // Create a new printer object
    try {
        auto printer = std::make_shared<LinuxPrinter>(name);
        m_printers[name] = printer;
        return printer;
    } catch (const PrinterException&) {
        // Return nullptr if the printer can't be accessed
        return nullptr;
    }
}

void LinuxPrintManager::refreshPrinterList() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Clear the cache
    m_printers.clear();

    // Update the refresh time
    m_last_refresh = std::chrono::steady_clock::now();

    // Force a refresh by calling getAvailablePrinters
    cups_dest_t* dests = nullptr;
    int num_dests = cupsGetDests(&dests);

    for (int i = 0; i < num_dests; i++) {
        std::string name(dests[i].name);

        // Skip the implicit class destinations
        if (name.find('@') != std::string::npos) {
            continue;
        }

        // Create a new printer object
        try {
            auto printer = std::make_shared<LinuxPrinter>(name);
            m_printers[name] = printer;
        } catch (const PrinterException&) {
            // Ignore printers that can't be accessed
        }
    }

    // Free the destinations
    cupsFreeDests(num_dests, dests);
}

bool LinuxPrintManager::canPrintToPDF() const {
    // Check if PDF printer exists
    return getPDFPrinter() != nullptr;
}

std::shared_ptr<Printer> LinuxPrintManager::getPDFPrinter() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Refresh printer list if needed
    refreshIfNeeded();

    // Common names for PDF printers in various systems
    const std::vector<std::string> pdf_printer_names = {
        "PDF", "Print to PDF", "cups-pdf", "PDF Writer"};

    // Check for any of the common PDF printer names
    for (const auto& name : pdf_printer_names) {
        auto printer = getPrinterByName(name);
        if (printer) {
            return printer;
        }
    }

    // If no specific PDF printer found, look for any printer with "PDF" in the
    // name
    cups_dest_t* dests = nullptr;
    int num_dests = cupsGetDests(&dests);

    for (int i = 0; i < num_dests; i++) {
        std::string name(dests[i].name);
        if (name.find("PDF") != std::string::npos ||
            name.find("pdf") != std::string::npos) {
            cupsFreeDests(num_dests, dests);
            return getPrinterByName(name);
        }
    }

    cupsFreeDests(num_dests, dests);
    return nullptr;
}

void LinuxPrintManager::refreshIfNeeded() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::seconds>(now - m_last_refresh)
            .count();

    if (elapsed > CACHE_REFRESH_SECONDS) {
        // Remove const for internal cache update
        const_cast<LinuxPrintManager*>(this)->refreshPrinterList();
    }
}

}  // namespace print_system

#endif  // PRINT_SYSTEM_LINUX
