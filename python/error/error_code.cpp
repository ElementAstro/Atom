#include "atom/error/error_code.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(error_code, m) {
    m.doc() =
        "Error code system with enhanced classification for the atom package";

    // ErrorSeverity enum
    py::enum_<atom::error::ErrorSeverity>(
        m, "ErrorSeverity",
        R"(Error severity levels for categorizing error importance.

This enum defines different severity levels for errors, from trace information
to fatal conditions that may cause termination.

Examples:
    >>> from atom.error import ErrorSeverity
    >>> severity = ErrorSeverity.Error
    >>> print(severity)
)")
        .value("Trace", atom::error::ErrorSeverity::Trace,
               "Detailed trace information")
        .value("Debug", atom::error::ErrorSeverity::Debug, "Debug information")
        .value("Info", atom::error::ErrorSeverity::Info,
               "Informational messages")
        .value("Warning", atom::error::ErrorSeverity::Warning,
               "Warning conditions")
        .value("Error", atom::error::ErrorSeverity::Error, "Error conditions")
        .value("Critical", atom::error::ErrorSeverity::Critical,
               "Critical conditions")
        .value("Fatal", atom::error::ErrorSeverity::Fatal,
               "Fatal conditions that may cause termination")
        .export_values();

    // ErrorCategory enum
    py::enum_<atom::error::ErrorCategory>(
        m, "ErrorCategory",
        R"(Error categories for grouping related errors.

This enum defines different categories of errors to help organize and
classify errors by their domain or source.

Examples:
    >>> from atom.error import ErrorCategory
    >>> category = ErrorCategory.Network
    >>> print(category)
)")
        .value("Unknown", atom::error::ErrorCategory::Unknown,
               "Unknown category")
        .value("System", atom::error::ErrorCategory::System,
               "System-level errors")
        .value("Application", atom::error::ErrorCategory::Application,
               "Application-level errors")
        .value("Network", atom::error::ErrorCategory::Network,
               "Network-related errors")
        .value("IO", atom::error::ErrorCategory::IO, "Input/Output errors")
        .value("Memory", atom::error::ErrorCategory::Memory,
               "Memory-related errors")
        .value("Security", atom::error::ErrorCategory::Security,
               "Security-related errors")
        .value("Configuration", atom::error::ErrorCategory::Configuration,
               "Configuration errors")
        .value("Validation", atom::error::ErrorCategory::Validation,
               "Data validation errors")
        .value("Business", atom::error::ErrorCategory::Business,
               "Business logic errors")
        .value("External", atom::error::ErrorCategory::External,
               "External service errors")
        .export_values();

    // ErrorRecoveryStrategy enum
    py::enum_<atom::error::ErrorRecoveryStrategy>(m, "ErrorRecoveryStrategy",
                                                  R"(Error recovery strategies.

This enum defines different strategies for recovering from errors,
indicating what actions can be taken when an error occurs.

Examples:
    >>> from atom.error import ErrorRecoveryStrategy
    >>> strategy = ErrorRecoveryStrategy.Retry
    >>> print(strategy)
)")
        .value("None", atom::error::ErrorRecoveryStrategy::None,
               "No recovery possible")
        .value("Retry", atom::error::ErrorRecoveryStrategy::Retry,
               "Can be retried")
        .value("Fallback", atom::error::ErrorRecoveryStrategy::Fallback,
               "Has fallback mechanism")
        .value("UserIntervention",
               atom::error::ErrorRecoveryStrategy::UserIntervention,
               "Requires user intervention")
        .value("Restart", atom::error::ErrorRecoveryStrategy::Restart,
               "Requires restart")
        .value("Ignore", atom::error::ErrorRecoveryStrategy::Ignore,
               "Can be safely ignored")
        .export_values();

    // ErrorCodeBase enum
    py::enum_<atom::error::ErrorCodeBase>(m, "ErrorCodeBase",
                                          R"(Base error codes.

Basic error codes for common operation results.
)")
        .value("Success", atom::error::ErrorCodeBase::Success,
               "Operation succeeded")
        .value("Failed", atom::error::ErrorCodeBase::Failed, "Operation failed")
        .value("Cancelled", atom::error::ErrorCodeBase::Cancelled,
               "Operation was cancelled")
        .export_values();

    // FileError enum
    py::enum_<atom::error::FileError>(m, "FileError",
                                      R"(File operation error codes.

Error codes specific to file operations including reading, writing,
and file system operations.

Examples:
    >>> from atom.error import FileError
    >>> error = FileError.NotFound
    >>> print(error)
)")
        .value("None", atom::error::FileError::None, "No error")
        .value("NotFound", atom::error::FileError::NotFound, "File not found")
        .value("OpenError", atom::error::FileError::OpenError,
               "Cannot open file")
        .value("AccessDenied", atom::error::FileError::AccessDenied,
               "Access denied")
        .value("ReadError", atom::error::FileError::ReadError, "Read error")
        .value("WriteError", atom::error::FileError::WriteError, "Write error")
        .value("PermissionDenied", atom::error::FileError::PermissionDenied,
               "Permission denied")
        .value("ParseError", atom::error::FileError::ParseError, "Parse error")
        .value("InvalidPath", atom::error::FileError::InvalidPath,
               "Invalid path")
        .value("FileExists", atom::error::FileError::FileExists,
               "File already exists")
        .value("DirectoryNotEmpty", atom::error::FileError::DirectoryNotEmpty,
               "Directory not empty")
        .value("TooManyOpenFiles", atom::error::FileError::TooManyOpenFiles,
               "Too many open files")
        .value("DiskFull", atom::error::FileError::DiskFull, "Disk full")
        .value("LoadError", atom::error::FileError::LoadError,
               "Dynamic library load error")
        .value("UnLoadError", atom::error::FileError::UnLoadError,
               "Dynamic library unload error")
        .value("LockError", atom::error::FileError::LockError,
               "File lock error")
        .value("FormatError", atom::error::FileError::FormatError,
               "File format error")
        .value("PathTooLong", atom::error::FileError::PathTooLong,
               "Path too long")
        .value("FileCorrupted", atom::error::FileError::FileCorrupted,
               "File corrupted")
        .value("UnsupportedFormat", atom::error::FileError::UnsupportedFormat,
               "Unsupported file format")
        .export_values();

    // DeviceError enum
    py::enum_<atom::error::DeviceError>(m, "DeviceError",
                                        R"(Device operation error codes.

Error codes for device-related operations including cameras, telescopes,
and other hardware devices.
)")
        .value("None", atom::error::DeviceError::None, "No error")
        .value("NotSpecific", atom::error::DeviceError::NotSpecific,
               "Non-specific device error")
        .value("NotFound", atom::error::DeviceError::NotFound,
               "Device not found")
        .value("NotSupported", atom::error::DeviceError::NotSupported,
               "Device not supported")
        .value("NotConnected", atom::error::DeviceError::NotConnected,
               "Device not connected")
        .value("MissingValue", atom::error::DeviceError::MissingValue,
               "Missing required value")
        .value("InvalidValue", atom::error::DeviceError::InvalidValue,
               "Invalid value")
        .value("Busy", atom::error::DeviceError::Busy, "Device busy")
        .value("ExposureError", atom::error::DeviceError::ExposureError,
               "Camera exposure error")
        .value("GainError", atom::error::DeviceError::GainError,
               "Camera gain error")
        .value("OffsetError", atom::error::DeviceError::OffsetError,
               "Camera offset error")
        .value("ISOError", atom::error::DeviceError::ISOError,
               "Camera ISO error")
        .value("CoolingError", atom::error::DeviceError::CoolingError,
               "Camera cooling error")
        .value("GotoError", atom::error::DeviceError::GotoError,
               "Telescope goto error")
        .value("ParkError", atom::error::DeviceError::ParkError,
               "Telescope park error")
        .value("UnParkError", atom::error::DeviceError::UnParkError,
               "Telescope unpark error")
        .value("ParkedError", atom::error::DeviceError::ParkedError,
               "Telescope is parked")
        .value("HomeError", atom::error::DeviceError::HomeError,
               "Telescope home error")
        .value("InitializationError",
               atom::error::DeviceError::InitializationError,
               "Initialization error")
        .value("ResourceExhausted", atom::error::DeviceError::ResourceExhausted,
               "Resource exhausted")
        .value("FirmwareUpdateFailed",
               atom::error::DeviceError::FirmwareUpdateFailed,
               "Firmware update failed")
        .value("CalibrationError", atom::error::DeviceError::CalibrationError,
               "Calibration error")
        .value("Overheating", atom::error::DeviceError::Overheating,
               "Device overheating")
        .value("PowerFailure", atom::error::DeviceError::PowerFailure,
               "Power failure")
        .export_values();

    // NetworkError enum
    py::enum_<atom::error::NetworkError>(m, "NetworkError",
                                         R"(Network operation error codes.

Error codes for network-related operations including connections,
protocols, and data transfer.
)")
        .value("None", atom::error::NetworkError::None, "No error")
        .value("ConnectionLost", atom::error::NetworkError::ConnectionLost,
               "Network connection lost")
        .value("ConnectionRefused",
               atom::error::NetworkError::ConnectionRefused,
               "Connection refused")
        .value("DNSLookupFailed", atom::error::NetworkError::DNSLookupFailed,
               "DNS lookup failed")
        .value("ProtocolError", atom::error::NetworkError::ProtocolError,
               "Protocol error")
        .value("SSLHandshakeFailed",
               atom::error::NetworkError::SSLHandshakeFailed,
               "SSL handshake failed")
        .value("AddressInUse", atom::error::NetworkError::AddressInUse,
               "Address already in use")
        .value("AddressNotAvailable",
               atom::error::NetworkError::AddressNotAvailable,
               "Address not available")
        .value("NetworkDown", atom::error::NetworkError::NetworkDown,
               "Network is down")
        .value("HostUnreachable", atom::error::NetworkError::HostUnreachable,
               "Host unreachable")
        .value("MessageTooLarge", atom::error::NetworkError::MessageTooLarge,
               "Message too large")
        .value("BufferOverflow", atom::error::NetworkError::BufferOverflow,
               "Buffer overflow")
        .value("TimeoutError", atom::error::NetworkError::TimeoutError,
               "Network timeout")
        .value("BandwidthExceeded",
               atom::error::NetworkError::BandwidthExceeded,
               "Bandwidth exceeded")
        .value("NetworkCongested", atom::error::NetworkError::NetworkCongested,
               "Network congested")
        .export_values();

    // DatabaseError enum
    py::enum_<atom::error::DatabaseError>(m, "DatabaseError",
                                          R"(Database operation error codes.

Error codes for database operations including queries, transactions,
and data integrity.
)")
        .value("None", atom::error::DatabaseError::None, "No error")
        .value("ConnectionFailed", atom::error::DatabaseError::ConnectionFailed,
               "Database connection failed")
        .value("QueryFailed", atom::error::DatabaseError::QueryFailed,
               "Query failed")
        .value("TransactionFailed",
               atom::error::DatabaseError::TransactionFailed,
               "Transaction failed")
        .value("IntegrityConstraintViolation",
               atom::error::DatabaseError::IntegrityConstraintViolation,
               "Integrity constraint violation")
        .value("NoSuchTable", atom::error::DatabaseError::NoSuchTable,
               "Table does not exist")
        .value("DuplicateEntry", atom::error::DatabaseError::DuplicateEntry,
               "Duplicate entry")
        .value("DataTooLong", atom::error::DatabaseError::DataTooLong,
               "Data too long")
        .value("DataTruncated", atom::error::DatabaseError::DataTruncated,
               "Data truncated")
        .value("Deadlock", atom::error::DatabaseError::Deadlock,
               "Deadlock detected")
        .value("LockTimeout", atom::error::DatabaseError::LockTimeout,
               "Lock timeout")
        .value("IndexOutOfBounds", atom::error::DatabaseError::IndexOutOfBounds,
               "Index out of bounds")
        .value("ConnectionTimeout",
               atom::error::DatabaseError::ConnectionTimeout,
               "Connection timeout")
        .value("InvalidQuery", atom::error::DatabaseError::InvalidQuery,
               "Invalid query")
        .export_values();

    // MemoryError enum
    py::enum_<atom::error::MemoryError>(m, "MemoryError",
                                        R"(Memory management error codes.

Error codes for memory-related operations and issues.
)")
        .value("None", atom::error::MemoryError::None, "No error")
        .value("AllocationFailed", atom::error::MemoryError::AllocationFailed,
               "Memory allocation failed")
        .value("OutOfMemory", atom::error::MemoryError::OutOfMemory,
               "Out of memory")
        .value("AccessViolation", atom::error::MemoryError::AccessViolation,
               "Memory access violation")
        .value("BufferOverflow", atom::error::MemoryError::BufferOverflow,
               "Buffer overflow")
        .value("DoubleFree", atom::error::MemoryError::DoubleFree,
               "Double free")
        .value("InvalidPointer", atom::error::MemoryError::InvalidPointer,
               "Invalid pointer")
        .value("MemoryLeak", atom::error::MemoryError::MemoryLeak,
               "Memory leak detected")
        .value("StackOverflow", atom::error::MemoryError::StackOverflow,
               "Stack overflow")
        .value("CorruptedHeap", atom::error::MemoryError::CorruptedHeap,
               "Heap corrupted")
        .export_values();

    // UserInputError enum
    py::enum_<atom::error::UserInputError>(m, "UserInputError",
                                           R"(User input validation error codes.

Error codes for user input validation and processing.
)")
        .value("None", atom::error::UserInputError::None, "No error")
        .value("InvalidInput", atom::error::UserInputError::InvalidInput,
               "Invalid input")
        .value("OutOfRange", atom::error::UserInputError::OutOfRange,
               "Input value out of range")
        .value("MissingInput", atom::error::UserInputError::MissingInput,
               "Missing required input")
        .value("FormatError", atom::error::UserInputError::FormatError,
               "Input format error")
        .value("UnsupportedType", atom::error::UserInputError::UnsupportedType,
               "Unsupported input type")
        .value("InputTooLong", atom::error::UserInputError::InputTooLong,
               "Input too long")
        .value("InputTooShort", atom::error::UserInputError::InputTooShort,
               "Input too short")
        .value("InvalidCharacter",
               atom::error::UserInputError::InvalidCharacter,
               "Invalid character in input")
        .export_values();

    // ConfigError enum
    py::enum_<atom::error::ConfigError>(m, "ConfigError",
                                        R"(Configuration error codes.

Error codes for configuration file and settings operations.
)")
        .value("None", atom::error::ConfigError::None, "No error")
        .value("MissingConfig", atom::error::ConfigError::MissingConfig,
               "Missing configuration file")
        .value("InvalidConfig", atom::error::ConfigError::InvalidConfig,
               "Invalid configuration")
        .value("ConfigParseError", atom::error::ConfigError::ConfigParseError,
               "Configuration parse error")
        .value("UnsupportedConfig", atom::error::ConfigError::UnsupportedConfig,
               "Unsupported configuration")
        .value("ConfigConflict", atom::error::ConfigError::ConfigConflict,
               "Configuration conflict")
        .value("InvalidOption", atom::error::ConfigError::InvalidOption,
               "Invalid option")
        .value("ConfigNotSaved", atom::error::ConfigError::ConfigNotSaved,
               "Configuration not saved")
        .value("ConfigLocked", atom::error::ConfigError::ConfigLocked,
               "Configuration locked")
        .export_values();

    // ProcessError enum
    py::enum_<atom::error::ProcessError>(m, "ProcessError",
                                         R"(Process and thread error codes.

Error codes for process and thread management operations.
)")
        .value("None", atom::error::ProcessError::None, "No error")
        .value("ProcessNotFound", atom::error::ProcessError::ProcessNotFound,
               "Process not found")
        .value("ProcessFailed", atom::error::ProcessError::ProcessFailed,
               "Process failed")
        .value("ThreadCreationFailed",
               atom::error::ProcessError::ThreadCreationFailed,
               "Thread creation failed")
        .value("ThreadJoinFailed", atom::error::ProcessError::ThreadJoinFailed,
               "Thread join failed")
        .value("ThreadTimeout", atom::error::ProcessError::ThreadTimeout,
               "Thread timeout")
        .value("DeadlockDetected", atom::error::ProcessError::DeadlockDetected,
               "Deadlock detected")
        .value("ProcessTerminated",
               atom::error::ProcessError::ProcessTerminated,
               "Process terminated")
        .value("InvalidProcessState",
               atom::error::ProcessError::InvalidProcessState,
               "Invalid process state")
        .value("InsufficientResources",
               atom::error::ProcessError::InsufficientResources,
               "Insufficient resources")
        .value("InvalidThreadPriority",
               atom::error::ProcessError::InvalidThreadPriority,
               "Invalid thread priority")
        .export_values();

    // ServerError enum
    py::enum_<atom::error::ServerError>(m, "ServerError",
                                        R"(Server operation error codes.

Error codes for server-side operations and API errors.
)")
        .value("None", atom::error::ServerError::None, "No error")
        .value("InvalidParameters", atom::error::ServerError::InvalidParameters,
               "Invalid parameters")
        .value("InvalidFormat", atom::error::ServerError::InvalidFormat,
               "Invalid format")
        .value("MissingParameters", atom::error::ServerError::MissingParameters,
               "Missing parameters")
        .value("RunFailed", atom::error::ServerError::RunFailed, "Run failed")
        .value("UnknownError", atom::error::ServerError::UnknownError,
               "Unknown error")
        .value("UnknownCommand", atom::error::ServerError::UnknownCommand,
               "Unknown command")
        .value("UnknownDevice", atom::error::ServerError::UnknownDevice,
               "Unknown device")
        .value("UnknownDeviceType", atom::error::ServerError::UnknownDeviceType,
               "Unknown device type")
        .value("UnknownDeviceName", atom::error::ServerError::UnknownDeviceName,
               "Unknown device name")
        .value("UnknownDeviceID", atom::error::ServerError::UnknownDeviceID,
               "Unknown device ID")
        .value("NetworkError", atom::error::ServerError::NetworkError,
               "Network error")
        .value("TimeoutError", atom::error::ServerError::TimeoutError,
               "Request timeout")
        .value("AuthenticationError",
               atom::error::ServerError::AuthenticationError,
               "Authentication failed")
        .value("PermissionDenied", atom::error::ServerError::PermissionDenied,
               "Permission denied")
        .value("ServerOverload", atom::error::ServerError::ServerOverload,
               "Server overload")
        .value("MaintenanceMode", atom::error::ServerError::MaintenanceMode,
               "Maintenance mode")
        .export_values();

    // ErrorMetadata struct
    py::class_<atom::error::ErrorMetadata>(
        m, "ErrorMetadata",
        R"(Error metadata structure for enhanced error information.

This class contains detailed metadata about an error including severity,
category, recovery strategy, and descriptive information.

Attributes:
    severity (ErrorSeverity): Error severity level
    category (ErrorCategory): Error category
    recovery (ErrorRecoveryStrategy): Recovery strategy
    description (str): Human-readable error description
    solution (str): Suggested solution or remediation steps
    retry_count (int): Current retry count
    max_retries (int): Maximum number of retries allowed

Examples:
    >>> from atom.error import ErrorMetadata, ErrorSeverity, ErrorCategory, ErrorRecoveryStrategy
    >>> metadata = ErrorMetadata()
    >>> metadata.severity = ErrorSeverity.Error
    >>> metadata.category = ErrorCategory.Network
    >>> metadata.recovery = ErrorRecoveryStrategy.Retry
    >>> metadata.description = "Connection failed"
    >>> metadata.solution = "Check network connectivity"
)")
        .def(py::init<>(), "Constructs an ErrorMetadata with default values.")
        .def(py::init<atom::error::ErrorSeverity, atom::error::ErrorCategory,
                      atom::error::ErrorRecoveryStrategy, std::string,
                      std::string>(),
             py::arg("severity"), py::arg("category"), py::arg("recovery"),
             py::arg("description") = "", py::arg("solution") = "",
             R"(Constructs an ErrorMetadata with specified values.

Args:
    severity (ErrorSeverity): Error severity level
    category (ErrorCategory): Error category
    recovery (ErrorRecoveryStrategy): Recovery strategy
    description (str, optional): Error description
    solution (str, optional): Suggested solution
)")
        .def_readwrite("severity", &atom::error::ErrorMetadata::severity,
                       "Error severity level")
        .def_readwrite("category", &atom::error::ErrorMetadata::category,
                       "Error category")
        .def_readwrite("recovery", &atom::error::ErrorMetadata::recovery,
                       "Recovery strategy")
        .def_readwrite("description", &atom::error::ErrorMetadata::description,
                       "Error description")
        .def_readwrite("solution", &atom::error::ErrorMetadata::solution,
                       "Suggested solution")
        .def_readwrite("retry_count", &atom::error::ErrorMetadata::retryCount,
                       "Current retry count")
        .def_readwrite("max_retries", &atom::error::ErrorMetadata::maxRetries,
                       "Maximum retries allowed");

    // ErrorCodeMapper class
    py::class_<atom::error::ErrorCodeMapper>(m, "ErrorCodeMapper",
                                             R"(Error code mapping utilities.

This class provides static methods for retrieving metadata and information
about error codes, including descriptions, severity levels, and recovery strategies.

Examples:
    >>> from atom.error import ErrorCodeMapper, FileError
    >>> description = ErrorCodeMapper.get_description(int(FileError.NotFound))
    >>> print(description)
    >>> metadata = ErrorCodeMapper.get_metadata(int(FileError.NotFound))
    >>> print(metadata.severity)
)")
        .def_static("get_metadata", &atom::error::ErrorCodeMapper::getMetadata,
                    py::arg("error_code"),
                    R"(Get error metadata for a specific error code.

Args:
    error_code (int): The error code to look up

Returns:
    ErrorMetadata: Metadata associated with the error code

Examples:
    >>> from atom.error import ErrorCodeMapper, NetworkError
    >>> metadata = ErrorCodeMapper.get_metadata(int(NetworkError.ConnectionLost))
    >>> print(metadata.description)
)")
        .def_static("get_description",
                    &atom::error::ErrorCodeMapper::getDescription,
                    py::arg("error_code"),
                    R"(Get human-readable description for error code.

Args:
    error_code (int): The error code to look up

Returns:
    str: Human-readable description of the error

Examples:
    >>> from atom.error import ErrorCodeMapper, DeviceError
    >>> desc = ErrorCodeMapper.get_description(int(DeviceError.NotFound))
    >>> print(desc)
)")
        .def_static("get_severity", &atom::error::ErrorCodeMapper::getSeverity,
                    py::arg("error_code"),
                    R"(Get error severity for error code.

Args:
    error_code (int): The error code to look up

Returns:
    ErrorSeverity: Severity level of the error
)")
        .def_static("get_category", &atom::error::ErrorCodeMapper::getCategory,
                    py::arg("error_code"),
                    R"(Get error category for error code.

Args:
    error_code (int): The error code to look up

Returns:
    ErrorCategory: Category of the error
)")
        .def_static("get_recovery_strategy",
                    &atom::error::ErrorCodeMapper::getRecoveryStrategy,
                    py::arg("error_code"),
                    R"(Get recovery strategy for error code.

Args:
    error_code (int): The error code to look up

Returns:
    ErrorRecoveryStrategy: Recovery strategy for the error
)")
        .def_static("is_recoverable",
                    &atom::error::ErrorCodeMapper::isRecoverable,
                    py::arg("error_code"),
                    R"(Check if error code is recoverable.

Args:
    error_code (int): The error code to check

Returns:
    bool: True if the error is recoverable

Examples:
    >>> from atom.error import ErrorCodeMapper, NetworkError
    >>> if ErrorCodeMapper.is_recoverable(int(NetworkError.TimeoutError)):
    ...     print("Error can be recovered")
)")
        .def_static("is_retryable", &atom::error::ErrorCodeMapper::isRetryable,
                    py::arg("error_code"),
                    R"(Check if error code is retryable.

Args:
    error_code (int): The error code to check

Returns:
    bool: True if the operation can be retried

Examples:
    >>> from atom.error import ErrorCodeMapper, DatabaseError
    >>> if ErrorCodeMapper.is_retryable(int(DatabaseError.ConnectionFailed)):
    ...     print("Operation can be retried")
)");

    // Utility functions
    m.def("severity_to_string", &atom::error::severityToString,
          py::arg("severity"),
          R"(Convert error severity to string.

Args:
    severity (ErrorSeverity): The severity level to convert

Returns:
    str: String representation of the severity level

Examples:
    >>> from atom.error import severity_to_string, ErrorSeverity
    >>> print(severity_to_string(ErrorSeverity.Critical))
    CRITICAL
)");

    m.def("category_to_string", &atom::error::categoryToString,
          py::arg("category"),
          R"(Convert error category to string.

Args:
    category (ErrorCategory): The category to convert

Returns:
    str: String representation of the category

Examples:
    >>> from atom.error import category_to_string, ErrorCategory
    >>> print(category_to_string(ErrorCategory.Network))
    NETWORK
)");

    m.def("recovery_strategy_to_string", &atom::error::recoveryStrategyToString,
          py::arg("strategy"),
          R"(Convert recovery strategy to string.

Args:
    strategy (ErrorRecoveryStrategy): The strategy to convert

Returns:
    str: String representation of the recovery strategy

Examples:
    >>> from atom.error import recovery_strategy_to_string, ErrorRecoveryStrategy
    >>> print(recovery_strategy_to_string(ErrorRecoveryStrategy.Retry))
    RETRY
)");
}
