#ifndef ATOM_ERROR_STACKTRACE_HPP
#define ATOM_ERROR_STACKTRACE_HPP

#include <string>
#include <unordered_map>
#include <vector>

#ifndef _WIN32
#include <memory>
#endif

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
     * @brief Default constructor that captures the current stack trace.
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
     * @brief Capture the current stack trace based on the operating system.
     */
    void capture();

    /**
     * @brief Process a stack frame to extract detailed information.
     *
     * @param frame The stack frame to process.
     * @param frameIndex The index of the frame in the stack.
     * @return A string containing the processed frame information.
     */
    [[nodiscard]] auto processFrame(void* frame, int frameIndex) const
        -> std::string;

#ifdef _WIN32
    std::vector<void*> frames_;
    mutable std::unordered_map<void*, std::string> moduleCache_;

#elif defined(__APPLE__) || defined(__linux__)
    std::unique_ptr<char*, decltype(&free)> symbols_{nullptr, &free};
    std::vector<void*> frames_;
    int num_frames_ = 0;
    mutable std::unordered_map<void*, std::string> symbolCache_;
#endif
};

}  // namespace atom::error

#endif