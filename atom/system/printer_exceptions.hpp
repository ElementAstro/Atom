#pragma once

#include <stdexcept>
#include <string>

namespace print_system {

// Base exception class for all printer-related exceptions
class PrinterException : public std::runtime_error {
public:
    explicit PrinterException(const std::string& message)
        : std::runtime_error(message) {}
};

// Thrown when a printer cannot be found
class PrinterNotFoundException : public PrinterException {
public:
    explicit PrinterNotFoundException(const std::string& printer_name)
        : PrinterException("Printer not found: " + printer_name),
          m_printer_name(printer_name) {}

    const std::string& getPrinterName() const { return m_printer_name; }

private:
    std::string m_printer_name;
};

// Thrown when a print job fails
class PrintJobFailedException : public PrinterException {
public:
    explicit PrintJobFailedException(const std::string& message)
        : PrinterException("Print job failed: " + message) {}
};

// Thrown when print job information cannot be retrieved
class PrintJobNotFoundException : public PrinterException {
public:
    explicit PrintJobNotFoundException(int job_id)
        : PrinterException("Print job not found: " + std::to_string(job_id)),
          m_job_id(job_id) {}

    int getJobId() const { return m_job_id; }

private:
    int m_job_id;
};

// Thrown when printing system initialization fails
class PrintSystemInitException : public PrinterException {
public:
    explicit PrintSystemInitException(const std::string& message)
        : PrinterException("Printer system initialization failed: " + message) {
    }
};

// Thrown when an operation is unsupported on a specific printer
class UnsupportedOperationException : public PrinterException {
public:
    explicit UnsupportedOperationException(const std::string& operation)
        : PrinterException("Unsupported operation: " + operation) {}
};

// Thrown when invalid print settings are provided
class InvalidPrintSettingsException : public PrinterException {
public:
    explicit InvalidPrintSettingsException(const std::string& message)
        : PrinterException("Invalid print settings: " + message) {}
};

}  // namespace print_system
