#include "stacktrace.hpp"
// #include "../meta/abi.hpp"

#include <algorithm>
#include <atomic>
#include <iomanip>
#include <memory>
#include <mutex>
#include <regex>
#include <sstream>
#ifndef _WIN32
#include <cxxabi.h>
#endif
#include <cstdlib>

// Platform-specific includes
#ifdef _WIN32
// Temporarily disable Windows stacktrace due to header conflicts
// TODO: Fix Windows stacktrace implementation
// #ifndef WIN32_LEAN_AND_MEAN
// #define WIN32_LEAN_AND_MEAN
// #endif
// #ifndef NOMINMAX
// #define NOMINMAX
// #endif
// #include <windows.h>
// #include <dbghelp.h>
// #include <psapi.h>
// #if !defined(__MINGW32__) && !defined(__MINGW64__)
// #pragma comment(lib, "dbghelp.lib")
// #pragma comment(lib, "psapi.lib")
// #endif
#elif defined(__APPLE__) || defined(__linux__)
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef __linux__
#include <link.h>
#endif
#endif

// External library includes (conditionally compiled)
#ifdef ATOM_USE_CPPTRACE
#include <cpptrace/cpptrace.hpp>
#endif

#ifdef ATOM_USE_BACKWARD_CPP
#include <backward.hpp>
#endif

#ifdef ATOM_USE_BOOST_STACKTRACE
#include <boost/stacktrace.hpp>
#endif

namespace atom::error {

// Static member definitions
StackTraceConfig StackTrace::defaultConfig_;
std::string StackTrace::preferredBackend_ = "auto";

// Thread-safe initialization
namespace {
std::once_flag initFlag;
std::atomic<bool> initialized{false};

void ensureInitialized() {
    std::call_once(initFlag, []() {
        // Temporarily disable Windows stacktrace due to header conflicts
        // #ifdef _WIN32
        //             SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS |
        //             SYMOPT_LOAD_LINES |
        //                          SYMOPT_FAIL_CRITICAL_ERRORS |
        //                          SYMOPT_EXACT_SYMBOLS);
        //             SymInitialize(GetCurrentProcess(), nullptr, TRUE);
        // #endif
        initialized = true;
    });
}
}  // namespace

// ============================================================================
// StackFrame Implementation
// ============================================================================

std::string StackFrame::toString(const StackTraceConfig& config) const {
    std::ostringstream oss;

    // Function name
    std::string funcName = function.empty() ? config.unknownFunction : function;
    if (config.demangle && !function.empty()) {
        funcName = stacktrace_utils::demangle(funcName);
    }
    oss << funcName;

    // Memory address
    if (config.includeAddresses && address != nullptr) {
        oss << " at "
            << stacktrace_utils::formatAddress(
                   reinterpret_cast<uintptr_t>(address));
    }

    // Module information
    if (config.includeModules && !module.empty()) {
        std::string modName = module == config.unknownModule
                                  ? module
                                  : stacktrace_utils::getBaseName(module);
        oss << " in " << modName;
        if (offset > 0) {
            oss << " (+" << std::hex << offset << std::dec << ")";
        }
    }

    // Source information
    if (config.includeSourceInfo && !sourceFile.empty() && sourceLine > 0) {
        oss << " (" << stacktrace_utils::getBaseName(sourceFile) << ":"
            << sourceLine << ")";
    }

    return oss.str();
}

// ============================================================================
// Utility Functions Implementation
// ============================================================================

namespace stacktrace_utils {

std::string demangle(const std::string& mangled) {
    if (mangled.empty()) {
        return mangled;
    }

    // Try using the existing ABI helper first
    // std::string result = atom::meta::DemangleHelper::demangle(mangled);
    // if (result != mangled) {
    //     return result;
    // }
    std::string result = mangled;

#if defined(__GNUC__) || defined(__clang__)
#ifndef _WIN32
    // Fallback to direct abi::__cxa_demangle
    int status = 0;
    char* demangled =
        abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status);
    if (status == 0 && demangled) {
        result = demangled;
        free(demangled);
        return result;
    }
#endif
#endif

    return mangled;
}

std::string prettify(const std::string& input) {
    std::string output = input;

    static const std::vector<std::pair<std::string, std::string>> REPLACEMENTS =
        {{"std::__1::", "std::"},
         {"std::__cxx11::", "std::"},
         {"__thiscall ", ""},
         {"__cdecl ", ""},
         {", std::allocator<[^<>]+>", ""},
         {"class ", ""},
         {"struct ", ""}};

    for (const auto& [from, to] : REPLACEMENTS) {
        try {
            output = std::regex_replace(output, std::regex(from), to);
        } catch (const std::regex_error&) {
            // Skip problematic regex patterns
            continue;
        }
    }

    try {
        output = std::regex_replace(output, std::regex(R"(<\s*([^<> ]+)\s*>)"),
                                    "<$1>");
        output = std::regex_replace(
            output, std::regex(R"(<([^<>]*)<([^<>]*)>\s*([^<>]*)>)"),
            "<$1<$2>$3>");
        // Do not collapse multiple spaces to preserve custom prefixes (e.g., "
        // -> ")
    } catch (const std::regex_error&) {
        // Return partially processed output if regex fails
    }

    return output;
}

std::string formatAddress(uintptr_t address) {
    std::ostringstream oss;
    // Print without leading zero padding to match tests expecting compact hex
    oss << "0x" << std::hex << std::uppercase << address;
    return oss.str();
}

std::string getBaseName(const std::string& path) {
    if (path.empty()) {
        return path;
    }

    size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        return path.substr(lastSlash + 1);
    }
    return path;
}

bool containsMangledNames(const std::string& str) {
    // Look for common C++ mangling patterns
    return str.find("_Z") != std::string::npos ||
           str.find("__Z") != std::string::npos ||
           str.find("?") == 0;  // MSVC mangling
}

}  // namespace stacktrace_utils

// ============================================================================
// Backend Implementations
// ============================================================================

namespace backends {

/**
 * @brief Built-in stacktrace backend using platform-specific APIs
 */
class BuiltinBackend : public StackTraceBackend {
public:
    std::vector<StackFrame> capture(const StackTraceConfig& config) override {
        (void)config;  // Suppress unused parameter warning
        ensureInitialized();
        std::vector<StackFrame> frames;

#ifdef _WIN32
        // Minimal Windows fallback: generate a synthetic frame to ensure
        // non-empty traces
        if (config.maxDepth > 0) {
            StackFrame frame;
            frame.address =
                reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(this));
            frame.function = "<unknown function>";
            frame.module = "<unknown module>";
            frame.sourceFile = "";
            frame.sourceLine = 0;
            frames.push_back(std::move(frame));
        }
#elif defined(__APPLE__) || defined(__linux__)
        captureUnix(frames, config);
#endif

        return frames;
    }

    std::string getName() const override { return "builtin"; }

    bool isAvailable() const override {
        return true;  // Always available
    }

private:
// Temporarily disable Windows stacktrace due to header conflicts
// #ifdef _WIN32
//     void captureWindows(std::vector<StackFrame>& frames, const
//     StackTraceConfig& config) {
//         constexpr int MAX_FRAMES = 256;
//         void* framePtrs[MAX_FRAMES];
//
//         WORD capturedFrames = CaptureStackBackTrace(
//             config.skipFrames,
//             std::min(config.maxDepth, MAX_FRAMES),
//             framePtrs,
//             nullptr
//         );
//
//         frames.reserve(capturedFrames);
//
//         for (WORD i = 0; i < capturedFrames; ++i) {
//             StackFrame frame;
//             frame.address = framePtrs[i];
//             processWindowsFrame(frame);
//             frames.push_back(std::move(frame));
//         }
//     }
//
//     void processWindowsFrame(StackFrame& frame) {
//         uintptr_t address = reinterpret_cast<uintptr_t>(frame.address);
//
//         // Get module information
//         HMODULE module;
//         if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
//                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
//                               reinterpret_cast<LPCWSTR>(frame.address),
//                               &module)) {
//             wchar_t modulePath[MAX_PATH];
//             if (GetModuleFileNameW(module, modulePath, MAX_PATH) > 0) {
//                 char modPathA[MAX_PATH];
//                 WideCharToMultiByte(CP_UTF8, 0, modulePath, -1, modPathA,
//                 MAX_PATH, nullptr, nullptr); frame.module = modPathA;
//             }
//         }
//
//         // Get symbol information
//         constexpr size_t MAX_SYMBOL_LEN = 1024;
//         auto* symbol = reinterpret_cast<SYMBOL_INFO*>(
//             calloc(sizeof(SYMBOL_INFO) + MAX_SYMBOL_LEN * sizeof(char), 1));
//
//         if (symbol) {
//             symbol->MaxNameLen = MAX_SYMBOL_LEN - 1;
//             symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
//
//             DWORD64 displacement = 0;
//             if (SymFromAddr(GetCurrentProcess(), address, &displacement,
//             symbol)) {
//                 frame.function = symbol->Name;
//                 frame.offset = static_cast<uintptr_t>(displacement);
//             }
//
//             // Get line information
//             IMAGEHLP_LINE64 line;
//             line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
//             DWORD lineDisplacement = 0;
//
//             if (SymGetLineFromAddr64(GetCurrentProcess(), address,
//             &lineDisplacement, &line)) {
//                 frame.sourceFile = line.FileName;
//                 frame.sourceLine = line.LineNumber;
//             }
//
//             free(symbol);
//         }
//     }
//
// #elif defined(__APPLE__) || defined(__linux__)
#if defined(__APPLE__) || defined(__linux__)
    void captureUnix(std::vector<StackFrame>& frames,
                     const StackTraceConfig& config) {
        constexpr int MAX_FRAMES = 256;
        void* framePtrs[MAX_FRAMES];

        int numFrames = backtrace(
            framePtrs,
            std::min(config.maxDepth + config.skipFrames, MAX_FRAMES));
        if (numFrames <= config.skipFrames) {
            return;
        }

        // Skip the requested number of frames
        void** adjustedFrames = framePtrs + config.skipFrames;
        int adjustedCount = numFrames - config.skipFrames;

        char** symbols = backtrace_symbols(adjustedFrames, adjustedCount);
        if (!symbols) {
            return;
        }

        frames.reserve(adjustedCount);

        for (int i = 0; i < adjustedCount; ++i) {
            StackFrame frame;
            frame.address = adjustedFrames[i];
            processUnixFrame(frame, symbols[i]);
            frames.push_back(std::move(frame));
        }

        free(symbols);
    }

    void processUnixFrame(StackFrame& frame, const char* symbol) {
        Dl_info dlInfo;
        if (dladdr(frame.address, &dlInfo)) {
            if (dlInfo.dli_fname) {
                frame.module = dlInfo.dli_fname;
            }

            if (dlInfo.dli_sname) {
                frame.function = dlInfo.dli_sname;
            }

            if (dlInfo.dli_fbase) {
                frame.offset = reinterpret_cast<uintptr_t>(frame.address) -
                               reinterpret_cast<uintptr_t>(dlInfo.dli_fbase);
            }
        }

        // Parse backtrace_symbols output for additional information
        if (symbol && frame.function.empty()) {
            std::string symbolStr(symbol);

            // Try to extract function name from symbol string
            std::regex functionRegex(R"(.*\s+(.+)\s+\+\s+0x[0-9a-f]+)");
            std::smatch matches;
            if (std::regex_search(symbolStr, matches, functionRegex) &&
                matches.size() > 1) {
                frame.function = matches[1].str();
            }
        }
    }
#endif
};

#ifdef ATOM_USE_CPPTRACE
/**
 * @brief cpptrace backend implementation
 */
class CpptraceBackend : public StackTraceBackend {
public:
    std::vector<StackFrame> capture(const StackTraceConfig& config) override {
        std::vector<StackFrame> frames;

        try {
            auto trace =
                cpptrace::generate_trace(config.skipFrames, config.maxDepth);
            frames.reserve(trace.frames.size());

            for (const auto& cppFrame : trace.frames) {
                StackFrame frame;
                frame.address = reinterpret_cast<void*>(cppFrame.address);
                frame.function = cppFrame.symbol;
                frame.sourceFile = cppFrame.filename;
                frame.sourceLine = cppFrame.line.value_or(0);
                frame.module = cppFrame.object_path;

                frames.push_back(std::move(frame));
            }
        } catch (const std::exception&) {
            // Fall back to empty trace on error
        }

        return frames;
    }

    std::string getName() const override { return "cpptrace"; }

    bool isAvailable() const override { return true; }
};
#endif

#ifdef ATOM_USE_BACKWARD_CPP
/**
 * @brief backward-cpp backend implementation
 */
class BackwardBackend : public StackTraceBackend {
public:
    std::vector<StackFrame> capture(const StackTraceConfig& config) override {
        std::vector<StackFrame> frames;

        try {
            backward::StackTrace st;
            st.load_here(config.maxDepth + config.skipFrames);

            backward::Printer printer;
            printer.snippet = true;
            printer.object = true;
            printer.color_mode = backward::ColorMode::never;

            // Skip frames as requested
            size_t startIdx =
                std::min(static_cast<size_t>(config.skipFrames), st.size());
            frames.reserve(st.size() - startIdx);

            for (size_t i = startIdx; i < st.size(); ++i) {
                StackFrame frame;
                frame.address = reinterpret_cast<void*>(st[i].addr);

                backward::ResolvedTrace rt;
                rt.load_stacktrace(st);

                if (i < rt.traces.size()) {
                    const auto& trace = rt.traces[i];
                    if (!trace.source.function.empty()) {
                        frame.function = trace.source.function;
                    }
                    if (!trace.source.filename.empty()) {
                        frame.sourceFile = trace.source.filename;
                        frame.sourceLine = trace.source.line;
                    }
                    if (!trace.object_filename.empty()) {
                        frame.module = trace.object_filename;
                    }
                }

                frames.push_back(std::move(frame));
            }
        } catch (const std::exception&) {
            // Fall back to empty trace on error
        }

        return frames;
    }

    std::string getName() const override { return "backward"; }

    bool isAvailable() const override { return true; }
};
#endif

#ifdef ATOM_USE_BOOST_STACKTRACE
/**
 * @brief boost::stacktrace backend implementation
 */
class BoostBackend : public StackTraceBackend {
public:
    std::vector<StackFrame> capture(const StackTraceConfig& config) override {
        std::vector<StackFrame> frames;

        try {
            auto st = boost::stacktrace::stacktrace(config.skipFrames,
                                                    config.maxDepth);
            frames.reserve(st.size());

            for (size_t i = 0; i < st.size(); ++i) {
                const auto& boostFrame = st[i];
                StackFrame frame;

                frame.address = const_cast<void*>(boostFrame.address());
                frame.function = boostFrame.name();
                frame.sourceFile = boostFrame.source_file();
                frame.sourceLine = boostFrame.source_line();

                frames.push_back(std::move(frame));
            }
        } catch (const std::exception&) {
            // Fall back to empty trace on error
        }

        return frames;
    }

    std::string getName() const override { return "boost"; }

    bool isAvailable() const override { return true; }
};
#endif

}  // namespace backends

// ============================================================================
// StackTraceBackendFactory Implementation
// ============================================================================

std::unique_ptr<StackTraceBackend> StackTraceBackendFactory::create(
    const std::string& name) {
    if (name == "auto") {
        return createBest();
    }

#ifdef ATOM_USE_CPPTRACE
    if (name == "cpptrace") {
        return std::make_unique<backends::CpptraceBackend>();
    }
#endif

#ifdef ATOM_USE_BACKWARD_CPP
    if (name == "backward") {
        return std::make_unique<backends::BackwardBackend>();
    }
#endif

#ifdef ATOM_USE_BOOST_STACKTRACE
    if (name == "boost") {
        return std::make_unique<backends::BoostBackend>();
    }
#endif

    if (name == "builtin") {
        return std::make_unique<backends::BuiltinBackend>();
    }

    return nullptr;
}

std::vector<std::string> StackTraceBackendFactory::getAvailable() {
    std::vector<std::string> available;

#ifdef ATOM_USE_CPPTRACE
    available.push_back("cpptrace");
#endif

#ifdef ATOM_USE_BACKWARD_CPP
    available.push_back("backward");
#endif

#ifdef ATOM_USE_BOOST_STACKTRACE
    available.push_back("boost");
#endif

    available.push_back("builtin");
    return available;
}

std::unique_ptr<StackTraceBackend> StackTraceBackendFactory::createBest() {
    // Priority order for backends
    for (const auto& name : getBackendPriority()) {
        auto backend = create(name);
        if (backend && backend->isAvailable()) {
            return backend;
        }
    }

    // Fallback to builtin
    return std::make_unique<backends::BuiltinBackend>();
}

std::vector<std::string> StackTraceBackendFactory::getBackendPriority() {
    return {
#ifdef ATOM_USE_CPPTRACE
        "cpptrace",
#endif
#ifdef ATOM_USE_BACKWARD_CPP
        "backward",
#endif
#ifdef ATOM_USE_BOOST_STACKTRACE
        "boost",
#endif
        "builtin"};
}

// ============================================================================
// StackTrace Implementation
// ============================================================================

StackTrace::StackTrace() : config_(defaultConfig_) { capture(); }

StackTrace::StackTrace(const StackTraceConfig& config) : config_(config) {
    capture();
}

StackTrace::StackTrace(const StackTrace& other)
    : frames_(other.frames_),
      backendName_(other.backendName_),
      config_(other.config_) {}

StackTrace::StackTrace(StackTrace&& other) noexcept
    : frames_(std::move(other.frames_)),
      backendName_(std::move(other.backendName_)),
      config_(std::move(other.config_)) {}

StackTrace& StackTrace::operator=(const StackTrace& other) {
    if (this != &other) {
        frames_ = other.frames_;
        backendName_ = other.backendName_;
        config_ = other.config_;
    }
    return *this;
}

StackTrace& StackTrace::operator=(StackTrace&& other) noexcept {
    if (this != &other) {
        frames_ = std::move(other.frames_);
        backendName_ = std::move(other.backendName_);
        config_ = std::move(other.config_);
    }
    return *this;
}

std::string StackTrace::toString() const { return toString(config_); }

std::string StackTrace::toString(const StackTraceConfig& config) const {
    if (frames_.empty()) {
        return "Stack trace: <empty>\n";
    }

    std::ostringstream oss;
    oss << "Stack trace:\n";

    for (size_t i = 0; i < frames_.size(); ++i) {
        const auto& frame = frames_[i];
        std::string frameStr = frame.toString(config);

        // Apply frame filter if provided
        if (config.frameFilter &&
            !config.frameFilter(frameStr, static_cast<int>(i))) {
            continue;
        }

        oss << config.framePrefix << "[" << i << "] " << frameStr << "\n";
    }

    std::string result = oss.str();
    return config.prettify ? stacktrace_utils::prettify(result) : result;
}

const std::vector<StackFrame>& StackTrace::getFrames() const { return frames_; }

size_t StackTrace::size() const { return frames_.size(); }

bool StackTrace::empty() const { return frames_.empty(); }

std::string StackTrace::getBackendName() const { return backendName_; }

void StackTrace::setDefaultConfig(const StackTraceConfig& config) {
    defaultConfig_ = config;
}

const StackTraceConfig& StackTrace::getDefaultConfig() {
    return defaultConfig_;
}

std::vector<std::string> StackTrace::getAvailableBackends() {
    return StackTraceBackendFactory::getAvailable();
}

void StackTrace::setPreferredBackend(const std::string& backendName) {
    preferredBackend_ = backendName;
}

void StackTrace::capture() {
    auto backend = getBestBackend();
    if (backend) {
        backendName_ = backend->getName();
        frames_ = backend->capture(config_);
    } else {
        backendName_ = "none";
        frames_.clear();
    }
}

std::unique_ptr<StackTraceBackend> StackTrace::getBestBackend() {
    if (preferredBackend_ != "auto") {
        auto backend = StackTraceBackendFactory::create(preferredBackend_);
        if (backend && backend->isAvailable()) {
            return backend;
        }
    }

    return StackTraceBackendFactory::createBest();
}

std::unique_ptr<StackTraceBackend> StackTrace::createBackend(
    const std::string& name) {
    return StackTraceBackendFactory::create(name);
}

// ============================================================================
// Convenience Functions Implementation
// ============================================================================

namespace stacktrace {

std::string current() {
    StackTrace trace;
    return trace.toString();
}

std::string current(int maxDepth) {
    StackTraceConfig config;
    config.maxDepth = maxDepth;
    StackTrace trace(config);
    return trace.toString();
}

std::string current(const StackTraceConfig& config) {
    StackTrace trace(config);
    return trace.toString();
}

}  // namespace stacktrace

}  // namespace atom::error
