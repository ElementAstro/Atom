#ifndef ATOM_ERROR_STACKTRACE_HPP
#define ATOM_ERROR_STACKTRACE_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace atom::error {

/**
 * @brief Class for capturing and representing a stack trace with enhanced
 * details.
 *
 * This class captures the stack trace of the current execution context and
 * represents it as a string, including file names, line numbers, function
 * names, module information, and memory addresses when available.
 */
class StackTrace {
public:
    /**
     * @brief Default constructor.
     *
     * Constructs a StackTrace object and captures the current stack trace.
     */
    StackTrace();

    /**
     * @brief Get the string representation of the stack trace.
     *
     * @return A string representing the captured stack trace with enhanced
     * details.
     */
    [[nodiscard]] auto toString() const -> std::string;

private:
    /**
     * @brief Capture the current stack trace.
     *
     * This method captures the current stack trace based on the operating
     * system with enhanced information gathering.
     */
    void capture();

    /**
     * @brief Process a stack frame to extract detailed information.
     *
     * @param frame The stack frame to process.
     * @param frameIndex The index of the frame in the stack.
     * @return A string containing the processed frame information.
     */
    [[nodiscard]] auto processFrame(void* frame,
                                    int frameIndex) const -> std::string;

#ifdef _WIN32
    std::vector<void*> frames_; /**< Vector to store stack frames on Windows. */

    // Cache for module information to avoid redundant lookups
    mutable std::unordered_map<void*, std::string> moduleCache_;

#elif defined(__APPLE__) || defined(__linux__)
    std::unique_ptr<char*, decltype(&free)> symbols_{
        nullptr,
        &free}; /**< Pointer to store stack symbols on macOS or Linux. */
    std::vector<void*>
        frames_;         /**< Vector to store raw stack frame pointers. */
    int num_frames_ = 0; /**< Number of stack frames captured. */

    // Cache for symbol resolution to improve performance
    mutable std::unordered_map<void*, std::string> symbolCache_;
#endif
};

}  // namespace atom::error

#endif