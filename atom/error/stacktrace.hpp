#ifndef ATOM_ERROR_STACKTRACE_HPP
#define ATOM_ERROR_STACKTRACE_HPP

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// External stacktrace library detection and configuration
#ifdef ATOM_USE_CPPTRACE
#define ATOM_STACKTRACE_BACKEND_CPPTRACE
#elif defined(ATOM_USE_BACKWARD_CPP)
#define ATOM_STACKTRACE_BACKEND_BACKWARD
#elif defined(ATOM_USE_BOOST_STACKTRACE)
#define ATOM_STACKTRACE_BACKEND_BOOST
#else
#define ATOM_STACKTRACE_BACKEND_BUILTIN
#endif

namespace atom::error {

/**
 * @brief Configuration options for stacktrace capture and formatting
 */
struct StackTraceConfig {
    int maxDepth = 128;              ///< Maximum number of frames to capture
    int skipFrames = 1;              ///< Number of frames to skip from the top
    bool includeAddresses = true;    ///< Include memory addresses in output
    bool includeModules = true;      ///< Include module/library names
    bool includeSourceInfo = true;   ///< Include source file and line numbers
    bool demangle = true;            ///< Demangle C++ function names
    bool prettify = true;            ///< Apply prettification to output
    std::string framePrefix = "\t";  ///< Prefix for each frame line
    std::string unknownFunction =
        "<unknown function>";  ///< Placeholder for unknown functions
    std::string unknownModule =
        "<unknown module>";  ///< Placeholder for unknown modules

    /**
     * @brief Custom frame filter function
     * @param frameInfo String representation of the frame
     * @param frameIndex Index of the frame (0-based)
     * @return true if frame should be included, false to filter out
     */
    std::function<bool(const std::string&, int)> frameFilter;
};

/**
 * @brief Information about a single stack frame
 */
struct StackFrame {
    void* address = nullptr;  ///< Memory address of the frame
    std::string function;     ///< Function name (demangled if available)
    std::string module;       ///< Module/library name
    std::string sourceFile;   ///< Source file name
    int sourceLine = 0;       ///< Source line number
    uintptr_t offset = 0;     ///< Offset within the function/module

    /**
     * @brief Convert frame to string representation
     */
    std::string toString(const StackTraceConfig& config = {}) const;
};

/**
 * @brief Stacktrace backend interface for different implementations
 */
class StackTraceBackend {
public:
    virtual ~StackTraceBackend() = default;

    /**
     * @brief Capture current stack trace
     */
    virtual std::vector<StackFrame> capture(const StackTraceConfig& config) = 0;

    /**
     * @brief Get backend name
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Check if backend is available
     */
    virtual bool isAvailable() const = 0;
};

namespace backends {
class BuiltinBackend;
class CpptraceBackend;
class BackwardBackend;
class BoostBackend;
}  // namespace backends

/**
 * @brief Enhanced stack trace class with support for multiple backends
 *
 * This class provides a unified interface for capturing and formatting stack
 * traces using different backend implementations. It supports external
 * libraries like cpptrace, backward-cpp, and boost::stacktrace, with fallback
 * to built-in platform-specific implementations.
 */
class StackTrace {
public:
    /**
     * @brief Default constructor that captures the current stack trace
     */
    StackTrace();

    /**
     * @brief Constructor with custom configuration
     * @param config Configuration options for stack trace capture
     */
    explicit StackTrace(const StackTraceConfig& config);

    /**
     * @brief Copy constructor
     */
    StackTrace(const StackTrace& other);

    /**
     * @brief Move constructor
     */
    StackTrace(StackTrace&& other) noexcept;

    /**
     * @brief Copy assignment operator
     */
    StackTrace& operator=(const StackTrace& other);

    /**
     * @brief Move assignment operator
     */
    StackTrace& operator=(StackTrace&& other) noexcept;

    /**
     * @brief Destructor
     */
    ~StackTrace() = default;

    /**
     * @brief Get the string representation of the stack trace
     * @return A string representing the captured stack trace
     */
    [[nodiscard]] std::string toString() const;

    /**
     * @brief Get the string representation with custom configuration
     * @param config Configuration for formatting
     * @return A string representing the stack trace with custom formatting
     */
    [[nodiscard]] std::string toString(const StackTraceConfig& config) const;

    /**
     * @brief Get individual stack frames
     * @return Vector of stack frames
     */
    [[nodiscard]] const std::vector<StackFrame>& getFrames() const;

    /**
     * @brief Get the number of captured frames
     * @return Number of frames in the stack trace
     */
    [[nodiscard]] size_t size() const;

    /**
     * @brief Check if stack trace is empty
     * @return true if no frames were captured
     */
    [[nodiscard]] bool empty() const;

    /**
     * @brief Get the backend used for capturing this stack trace
     * @return Name of the backend used
     */
    [[nodiscard]] std::string getBackendName() const;

    /**
     * @brief Set global default configuration
     * @param config Default configuration to use for new StackTrace instances
     */
    static void setDefaultConfig(const StackTraceConfig& config);

    /**
     * @brief Get global default configuration
     * @return Current default configuration
     */
    static const StackTraceConfig& getDefaultConfig();

    /**
     * @brief Get available backends
     * @return Vector of available backend names
     */
    static std::vector<std::string> getAvailableBackends();

    /**
     * @brief Force use of specific backend
     * @param backendName Name of backend to use ("auto" for automatic
     * selection)
     */
    static void setPreferredBackend(const std::string& backendName);

private:
    std::vector<StackFrame> frames_;
    std::string backendName_;
    StackTraceConfig config_;

    static StackTraceConfig defaultConfig_;
    static std::string preferredBackend_;

    /**
     * @brief Capture stack trace using the best available backend
     */
    void capture();

    /**
     * @brief Get the best available backend
     */
    static std::unique_ptr<StackTraceBackend> getBestBackend();

    /**
     * @brief Create backend by name
     */
    static std::unique_ptr<StackTraceBackend> createBackend(
        const std::string& name);
};

/**
 * @brief Factory for creating stacktrace backends
 */
class StackTraceBackendFactory {
public:
    /**
     * @brief Create backend by name
     * @param name Backend name ("builtin", "cpptrace", "backward", "boost",
     * "auto")
     * @return Unique pointer to backend, nullptr if not available
     */
    static std::unique_ptr<StackTraceBackend> create(const std::string& name);

    /**
     * @brief Get list of available backends
     * @return Vector of available backend names
     */
    static std::vector<std::string> getAvailable();

    /**
     * @brief Get the best available backend
     * @return Unique pointer to the best backend
     */
    static std::unique_ptr<StackTraceBackend> createBest();

private:
    static std::vector<std::string> getBackendPriority();
};

/**
 * @brief Utility functions for stacktrace processing
 */
namespace stacktrace_utils {
/**
 * @brief Demangle C++ function name
 * @param mangled Mangled function name
 * @return Demangled function name, or original if demangling fails
 */
std::string demangle(const std::string& mangled);

/**
 * @brief Prettify stacktrace output
 * @param input Raw stacktrace string
 * @return Prettified stacktrace string
 */
std::string prettify(const std::string& input);

/**
 * @brief Format memory address
 * @param address Memory address
 * @return Formatted address string
 */
std::string formatAddress(uintptr_t address);

/**
 * @brief Get base name from file path
 * @param path Full file path
 * @return Base name (filename only)
 */
std::string getBaseName(const std::string& path);

/**
 * @brief Check if a string contains a mangled C++ name
 * @param str String to check
 * @return true if string appears to contain mangled names
 */
bool containsMangledNames(const std::string& str);
}  // namespace stacktrace_utils

/**
 * @brief Convenience functions for quick stacktrace capture
 */
namespace stacktrace {
/**
 * @brief Capture current stack trace with default settings
 * @return String representation of stack trace
 */
std::string current();

/**
 * @brief Capture current stack trace with custom depth
 * @param maxDepth Maximum number of frames to capture
 * @return String representation of stack trace
 */
std::string current(int maxDepth);

/**
 * @brief Capture current stack trace with custom configuration
 * @param config Configuration options
 * @return String representation of stack trace
 */
std::string current(const StackTraceConfig& config);
}  // namespace stacktrace

}  // namespace atom::error

#endif
