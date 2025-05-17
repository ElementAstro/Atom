#include "stacktrace.hpp"
#include "atom/meta/abi.hpp"

#include <iomanip>
#include <regex>
#include <sstream>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
// clang-format on
#if !defined(__MINGW32__) && !defined(__MINGW64__)
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")
#endif
#elif defined(__APPLE__) || defined(__linux__)
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef __linux__
#include <link.h>
#endif
#endif

#ifdef ATOM_USE_BOOST
#include <boost/stacktrace.hpp>
#endif

namespace atom::error {

namespace {
#if defined(__linux__) || defined(__APPLE__)
auto processString(const std::string& input) -> std::string {
    // Find mangled symbol name (starts with _Z)
    size_t startIndex = input.find("_Z");
    if (startIndex == std::string::npos) {
        return input;
    }
    
    // Find the end of the symbol (usually marked by '+')
    size_t endIndex = input.find('+', startIndex);
    if (endIndex == std::string::npos) {
        return input;
    }
    
    // Extract and demangle the symbol
    std::string abiName = input.substr(startIndex, endIndex - startIndex);
    abiName = meta::DemangleHelper::demangle(abiName);
    
    // Replace the mangled part with the demangled symbol
    std::string result = input;
    result.replace(startIndex, endIndex - startIndex, abiName);
    return result;
}
#endif

auto prettifyStacktrace(const std::string& input) -> std::string {
    std::string output = input;
    
    // Common replacements to make the output more readable
    static const std::vector<std::pair<std::string, std::string>> REPLACEMENTS =
        {{"std::__1::", "std::"},
         {"std::__cxx11::", "std::"},
         {"__thiscall ", ""},
         {"__cdecl ", ""},
         {", std::allocator<[^<>]+>", ""},
         {"class ", ""},
         {"struct ", ""}};

    // Apply all replacements
    for (const auto& [from, to] : REPLACEMENTS) {
        output = std::regex_replace(output, std::regex(from), to);
    }

    // Clean up spaces in template arguments
    output = std::regex_replace(output, std::regex(R"(<\s*([^<> ]+)\s*>)"), "<$1>");
    
    // Clean up nested templates
    output = std::regex_replace(output, std::regex(R"(<([^<>]*)<([^<>]*)>\s*([^<>]*)>)"), "<$1<$2>$3>");
    
    // Clean up multiple spaces
    output = std::regex_replace(output, std::regex(R"(\s{2,})"), " ");

    return output;
}

// Format memory addresses consistently
auto formatAddress(uintptr_t address) -> std::string {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << std::setfill('0') 
        << std::setw(sizeof(void*) * 2) << address;
    return oss.str();
}

// Extract file basename from a path
auto getBaseName(const std::string& path) -> std::string {
    size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        return path.substr(lastSlash + 1);
    }
    return path;
}

}  // unnamed namespace

StackTrace::StackTrace() { capture(); }

auto StackTrace::toString() const -> std::string {
    std::ostringstream oss;
    
    oss << "Stack trace:\n";

#ifdef ATOM_USE_BOOST
    oss << boost::stacktrace::stacktrace();
#elif defined(_WIN32)
    for (size_t i = 0; i < frames_.size(); ++i) {
        oss << "\t[" << i << "] " << processFrame(frames_[i], static_cast<int>(i)) << "\n";
    }

#elif defined(__APPLE__) || defined(__linux__)
    for (int i = 0; i < num_frames_; ++i) {
        oss << "\t[" << i << "] " << processFrame(frames_[i], i) << "\n";
    }

#else
    oss << "\tStack trace not available on this platform.\n";
#endif

    return prettifyStacktrace(oss.str());
}

#ifdef _WIN32
auto StackTrace::processFrame(void* frame, int frameIndex) const -> std::string {
    std::ostringstream oss;
    uintptr_t address = reinterpret_cast<uintptr_t>(frame);
    
    // Check if we've already cached module info for this address
    std::string moduleName;
    auto it = moduleCache_.find(frame);
    if (it != moduleCache_.end()) {
        moduleName = it->second;
    } else {
        // Get module information
        HMODULE module;
        if (GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(frame),
                &module)) {
            wchar_t modulePath[MAX_PATH];
            if (GetModuleFileNameW(module, modulePath, MAX_PATH) > 0) {
                char modPathA[MAX_PATH];
                WideCharToMultiByte(CP_UTF8, 0, modulePath, -1, modPathA, MAX_PATH, nullptr, nullptr);
                moduleName = modPathA;
                moduleCache_[frame] = moduleName;
            }
        }
    }
    
    // Get symbol information
    constexpr size_t MAX_SYMBOL_LEN = 1024;
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(
        calloc(sizeof(SYMBOL_INFO) + MAX_SYMBOL_LEN * sizeof(char), 1));
    if (!symbol) {
        oss << "<memory allocation failed> at " << formatAddress(address);
        return oss.str();
    }
    
    symbol->MaxNameLen = MAX_SYMBOL_LEN - 1;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    
    DWORD64 displacement = 0;
    std::string functionName = "<unknown function>";
    if (SymFromAddr(GetCurrentProcess(), address, &displacement, symbol)) {
        functionName = meta::DemangleHelper::demangle(std::string("_") + symbol->Name);
    }
    
    // Get line information
    IMAGEHLP_LINE64 line;
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    DWORD lineDisplacement = 0;
    std::string fileName;
    int lineNumber = 0;
    
    if (SymGetLineFromAddr64(GetCurrentProcess(), address, &lineDisplacement, &line)) {
        fileName = line.FileName;
        lineNumber = line.LineNumber;
    }
    
    free(symbol);
    
    // Format the output
    oss << functionName << " at " << formatAddress(address);
    
    if (!moduleName.empty()) {
        oss << " in " << getBaseName(moduleName);
    }
    
    if (!fileName.empty() && lineNumber > 0) {
        oss << " (" << getBaseName(fileName) << ":" << lineNumber << ")";
    }
    
    return oss.str();
}
#elif defined(__APPLE__) || defined(__linux__)
auto StackTrace::processFrame(void* frame, int frameIndex) const -> std::string {
    std::ostringstream oss;
    uintptr_t address = reinterpret_cast<uintptr_t>(frame);
    
    // Check if we already have this symbol in cache
    auto it = symbolCache_.find(frame);
    if (it != symbolCache_.end()) {
        return it->second;
    }
    
    // Get detailed symbol information
    Dl_info dlInfo;
    std::string functionName = "<unknown function>";
    std::string moduleName;
    uintptr_t offset = 0;
    
    if (dladdr(frame, &dlInfo)) {
        // Get module name
        if (dlInfo.dli_fname) {
            moduleName = dlInfo.dli_fname;
        }
        
        // Calculate offset from the base of the module
        if (dlInfo.dli_fbase) {
            offset = address - reinterpret_cast<uintptr_t>(dlInfo.dli_fbase);
        }
        
        // Get function name
        if (dlInfo.dli_sname) {
            functionName = meta::DemangleHelper::demangle(dlInfo.dli_sname);
        }
    }
    
    // If we couldn't get the function name from dladdr, try to parse it from backtrace_symbols
    if (functionName == "<unknown function>" && frameIndex < num_frames_ && symbols_) {
        std::string symbol(symbols_.get()[frameIndex]);
        
        // Try to extract the function name using regex
        std::regex functionRegex(R"((?:.*$$
0x[0-9a-f]+
$$)\s+(.+)\s+\+\s+0x[0-9a-f]+)");
        std::smatch matches;
        if (std::regex_search(symbol, matches, functionRegex) && matches.size() > 1) {
            functionName = meta::DemangleHelper::demangle(matches[1].str());
        } else {
            // If regex failed, use the processString function as fallback
            functionName = processString(symbol);
        }
    }
    
    // Format the output
    oss << functionName << " at " << formatAddress(address);
    
    if (!moduleName.empty()) {
        oss << " in " << getBaseName(moduleName);
        if (offset > 0) {
            oss << " (+" << std::hex << offset << ")";
        }
    }
    
    // Store in cache for future lookups
    std::string result = oss.str();
    symbolCache_[frame] = result;
    
    return result;
}
#else
auto StackTrace::processFrame(void* frame, int frameIndex) const -> std::string {
    std::ostringstream oss;
    oss << "<frame information unavailable> at " 
        << formatAddress(reinterpret_cast<uintptr_t>(frame));
    return oss.str();
}
#endif

void StackTrace::capture() {
#ifdef ATOM_USE_BOOST
    // Boost stacktrace automatically captures the stack trace
#elif defined(_WIN32)
    constexpr int max_frames = 128;
    frames_.resize(max_frames);
    
    // Initialize symbol handler with improved options
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | 
                 SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_EXACT_SYMBOLS);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);

    // Capture stack frames
    void* framePtrs[max_frames];
    WORD capturedFrames = CaptureStackBackTrace(1, max_frames, framePtrs, nullptr);

    frames_.resize(capturedFrames);
    std::copy_n(framePtrs, capturedFrames, frames_.begin());
    
    // Clear module cache
    moduleCache_.clear();

#elif defined(__APPLE__) || defined(__linux__)
    constexpr int MAX_FRAMES = 128;
    void* framePtrs[MAX_FRAMES];

    // Skip the first frame (which is this function)
    num_frames_ = backtrace(framePtrs, MAX_FRAMES);
    if (num_frames_ > 1) {
        symbols_.reset(backtrace_symbols(framePtrs + 1, num_frames_ - 1));
        frames_.assign(framePtrs + 1, framePtrs + num_frames_);
        num_frames_--;
    } else {
        symbols_.reset(nullptr);
        frames_.clear();
        num_frames_ = 0;
    }
    
    // Clear symbol cache
    symbolCache_.clear();

#else
    num_frames_ = 0;
#endif
}

}  // namespace atom::error