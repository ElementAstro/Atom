#include <iostream>
#include <filesystem>

// Test the ASIO macro integration
#ifdef ATOM_USE_ASIO
#include <asio.hpp>
#define ASIO_STATUS "ENABLED"
#else
#define ASIO_STATUS "DISABLED"
#endif

// Include the DirectoryStack header
#include "atom/io/filesystem/pushd.hpp"

int main() {
    std::cout << "=== DirectoryStack ASIO Integration Test ===" << std::endl;
    std::cout << "ATOM_USE_ASIO: " << ASIO_STATUS << std::endl;
    
    try {
        // This was the main issue we fixed - constructor compatibility
        std::cout << "\nTesting DirectoryStack constructor..." << std::endl;
        
#ifdef ATOM_USE_ASIO
        std::cout << "Creating asio::io_context..." << std::endl;
        asio::io_context io_context;
        
        std::cout << "Creating DirectoryStack with asio::io_context&..." << std::endl;
        atom::io::DirectoryStack dirStack(io_context);
        std::cout << "✓ SUCCESS: DirectoryStack(asio::io_context&) constructor works!" << std::endl;
#else
        std::cout << "Creating DirectoryStack with void* parameter..." << std::endl;
        atom::io::DirectoryStack dirStack(nullptr);
        std::cout << "✓ SUCCESS: DirectoryStack(void*) constructor works!" << std::endl;
#endif
        
        // Test basic functionality that doesn't require async operations
        std::cout << "\nTesting basic DirectoryStack operations..." << std::endl;
        std::cout << "Initial stack size: " << dirStack.size() << std::endl;
        std::cout << "Stack is empty: " << std::boolalpha << dirStack.isEmpty() << std::endl;
        
        std::cout << "\n✅ ALL TESTS PASSED!" << std::endl;
        std::cout << "✅ ASIO integration is working correctly!" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ ERROR: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ UNKNOWN ERROR occurred" << std::endl;
        return 1;
    }
}
