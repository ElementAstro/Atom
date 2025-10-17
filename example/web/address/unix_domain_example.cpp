/*
 * unix_domain_example.cpp
 *
 * Copyright (C) 2025 Developers <example.com>
 *
 * A comprehensive example demonstrating the use of the Atom UnixDomain address
 * class
 */

#include "atom/log/loguru.hpp"
#include "atom/web/address.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using namespace atom::web;

void demonstrateBasicUnixDomainOperations() {
    std::cout << "\n=== Basic Unix Domain Socket Operations ===\n";

    try {
#ifdef _WIN32
        // Windows Named Pipes
        UnixDomain addr1("\\\\.\\pipe\\my_pipe");
        UnixDomain addr2("\\\\.\\pipe\\another_pipe");
        UnixDomain addr3;  // Default constructor

        std::cout << "Created Windows Named Pipe addresses:\n";
        std::cout << "  addr1: " << addr1.getAddress() << "\n";
        std::cout << "  addr2: " << addr2.getAddress() << "\n";

        // Parse a named pipe path into addr3
        if (addr3.parse("\\\\.\\pipe\\test_pipe")) {
            std::cout << "  addr3: " << addr3.getAddress() << " (parsed)\n";
        }
#else
        // Unix Domain Sockets
        UnixDomain addr1("/tmp/socket1");
        UnixDomain addr2("/var/run/socket2");
        UnixDomain addr3;  // Default constructor

        std::cout << "Created Unix Domain Socket addresses:\n";
        std::cout << "  addr1: " << addr1.getAddress() << "\n";
        std::cout << "  addr2: " << addr2.getAddress() << "\n";

        // Parse a socket path into addr3
        if (addr3.parse("/tmp/test_socket")) {
            std::cout << "  addr3: " << addr3.getAddress() << " (parsed)\n";
        }
#endif

        // Print address types
        addr1.printAddressType();

        // Get address type
        std::cout << "Address type: " << addr1.getType() << "\n";

    } catch (const InvalidAddressFormat& e) {
        std::cerr << "Invalid address format: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}

void demonstrateUnixDomainValidation() {
    std::cout << "\n=== Unix Domain Socket Validation ===\n";

    std::vector<std::string> testPaths;

#ifdef _WIN32
    testPaths = {
        "\\\\.\\pipe\\valid_pipe",    // Valid named pipe
        "\\\\.\\pipe\\another_pipe",  // Valid named pipe
        "\\\\.\\pipe\\",              // Invalid - no pipe name
        "\\\\invalid\\pipe\\test",    // Invalid - wrong format
        "C:\\temp\\not_a_pipe",       // Invalid - regular file path
        "",                           // Invalid - empty
        "\\\\.\\pipe\\very_long_pipe_name_that_might_exceed_limits_in_some_"
        "systems_but_should_still_be_tested_for_validation_purposes"};

    std::cout << "Testing Windows Named Pipe paths:\n";
#else
    testPaths = {
        "/tmp/valid_socket",        // Valid socket path
        "/var/run/another_socket",  // Valid socket path
        "/home/user/socket",        // Valid socket path
        "relative/path/socket",     // Valid relative path
        "",                         // Invalid - empty
        "/",                        // Invalid - root directory
        std::string(1000, 'a')      // Invalid - too long
    };

    std::cout << "Testing Unix Domain Socket paths:\n";
#endif

    for (const auto& path : testPaths) {
        try {
            UnixDomain unixDomain;
            bool valid = unixDomain.parse(path);
            std::string displayPath =
                path.empty()
                    ? "(empty)"
                    : (path.length() > 50 ? path.substr(0, 47) + "..." : path);
            std::cout << "  " << displayPath << " -> "
                      << (valid ? "VALID" : "INVALID") << "\n";
        } catch (const InvalidAddressFormat& e) {
            std::string displayPath =
                path.empty()
                    ? "(empty)"
                    : (path.length() > 50 ? path.substr(0, 47) + "..." : path);
            std::cout << "  " << displayPath << " -> INVALID (exception)\n";
        }
    }
}

void demonstrateUnixDomainConversions() {
    std::cout << "\n=== Unix Domain Socket Conversions ===\n";

    try {
#ifdef _WIN32
        std::vector<std::string> paths = {"\\\\.\\pipe\\test_pipe",
                                          "\\\\.\\pipe\\another_pipe"};

        std::cout << "Windows Named Pipe conversion examples:\n";
#else
        std::vector<std::string> paths = {"/tmp/socket1", "/var/run/socket2",
                                          "/home/user/app_socket"};

        std::cout << "Unix Domain Socket conversion examples:\n";
#endif

        std::cout << "Path                           | Binary (first 32 chars) "
                     "        | Hexadecimal\n";
        std::cout << "-------------------------------|-------------------------"
                     "---------|------------\n";

        for (const auto& path : paths) {
            UnixDomain addr(path);
            std::string binary = addr.toBinary();
            std::string hex = addr.toHex();

            // Show only first 32 chars of binary for readability
            std::string shortBinary =
                binary.length() > 32 ? binary.substr(0, 32) + "..." : binary;

            std::string displayPath =
                path.length() > 30 ? path.substr(0, 27) + "..." : path;

            std::cout << std::left << std::setw(30) << displayPath << " | "
                      << std::setw(32) << shortBinary << " | "
                      << (hex.length() > 20 ? hex.substr(0, 17) + "..." : hex)
                      << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in conversions: " << e.what() << "\n";
    }
}

void demonstrateUnixDomainComparison() {
    std::cout << "\n=== Unix Domain Socket Comparison ===\n";

    try {
#ifdef _WIN32
        UnixDomain addr1("\\\\.\\pipe\\test_pipe");
        UnixDomain addr2("\\\\.\\pipe\\test_pipe");  // Same as addr1
        UnixDomain addr3(
            "\\\\.\\pipe\\different_pipe");  // Different from addr1

        std::cout << "Comparing Windows Named Pipe addresses:\n";
#else
        UnixDomain addr1("/tmp/socket1");
        UnixDomain addr2("/tmp/socket1");  // Same as addr1
        UnixDomain addr3("/tmp/socket2");  // Different from addr1

        std::cout << "Comparing Unix Domain Socket addresses:\n";
#endif

        std::cout << "  " << addr1.getAddress() << " == " << addr2.getAddress()
                  << ": " << (addr1.isEqual(addr2) ? "YES" : "NO") << "\n";
        std::cout << "  " << addr1.getAddress() << " == " << addr3.getAddress()
                  << ": " << (addr1.isEqual(addr3) ? "YES" : "NO") << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in comparison: " << e.what() << "\n";
    }
}

void demonstrateUnixDomainNetworkOperations() {
    std::cout << "\n=== Unix Domain Socket Network Operations ===\n";

    try {
#ifdef _WIN32
        UnixDomain addr("\\\\.\\pipe\\test_pipe");

        std::cout << "Network operations for Windows Named Pipe:\n";
        std::cout << "  Path: " << addr.getAddress() << "\n";

        // For named pipes, network address typically returns the pipe directory
        std::string networkAddr = addr.getNetworkAddress("");
        std::cout << "  Network address (directory): " << networkAddr << "\n";
#else
        UnixDomain addr("/tmp/app/socket1");

        std::cout << "Network operations for Unix Domain Socket:\n";
        std::cout << "  Path: " << addr.getAddress() << "\n";

        // For Unix sockets, network address typically returns the directory
        std::string networkAddr = addr.getNetworkAddress("");
        std::cout << "  Network address (directory): " << networkAddr << "\n";

        // Demonstrate with different socket paths
        std::vector<std::string> socketPaths = {
            "/tmp/socket", "/var/run/app/socket",
            "/home/user/.config/app/socket"};

        std::cout << "\nDirectory extraction examples:\n";
        std::cout << "Socket Path                    | Directory\n";
        std::cout << "-------------------------------|------------------\n";

        for (const auto& path : socketPaths) {
            try {
                UnixDomain socket(path);
                std::string dir = socket.getNetworkAddress("");

                std::string displayPath =
                    path.length() > 30 ? path.substr(0, 27) + "..." : path;

                std::cout << std::left << std::setw(30) << displayPath << " | "
                          << dir << "\n";
            } catch (const std::exception& e) {
                std::cout << std::left << std::setw(30) << path << " | ERROR\n";
            }
        }
#endif

    } catch (const std::exception& e) {
        std::cerr << "Error in network operations: " << e.what() << "\n";
    }
}

void demonstrateUnixDomainPlatformDifferences() {
    std::cout << "\n=== Platform-Specific Differences ===\n";

#ifdef _WIN32
    std::cout << "Running on Windows - Using Named Pipes:\n";
    std::cout << "  - Named pipes use the format: \\\\.\\pipe\\<name>\n";
    std::cout
        << "  - They provide similar functionality to Unix domain sockets\n";
    std::cout << "  - Named pipes can be accessed across the network (if "
                 "configured)\n";
    std::cout << "  - Maximum pipe name length is typically 256 characters\n";

    try {
        UnixDomain pipe("\\\\.\\pipe\\example");
        std::cout << "  - Example pipe: " << pipe.getAddress() << "\n";
        std::cout << "  - Type: " << pipe.getType() << "\n";
    } catch (const std::exception& e) {
        std::cout << "  - Error creating example pipe: " << e.what() << "\n";
    }
#else
    std::cout << "Running on Unix-like system - Using Unix Domain Sockets:\n";
    std::cout << "  - Unix domain sockets use filesystem paths\n";
    std::cout << "  - They are local to the machine (no network access)\n";
    std::cout << "  - Maximum path length is typically 108 characters\n";
    std::cout << "  - Socket files are created in the filesystem\n";

    try {
        UnixDomain socket("/tmp/example_socket");
        std::cout << "  - Example socket: " << socket.getAddress() << "\n";
        std::cout << "  - Type: " << socket.getType() << "\n";
    } catch (const std::exception& e) {
        std::cout << "  - Error creating example socket: " << e.what() << "\n";
    }
#endif
}

int main(int argc, char** argv) {
    // Initialize logging
    loguru::init(argc, argv);
    loguru::add_file("unix_domain_example.log", loguru::Append,
                     loguru::Verbosity_MAX);

    std::cout << "============================================\n";
#ifdef _WIN32
    std::cout << "     ATOM WINDOWS NAMED PIPE DEMO          \n";
#else
    std::cout << "     ATOM UNIX DOMAIN SOCKET DEMO          \n";
#endif
    std::cout << "============================================\n";

    try {
        demonstrateBasicUnixDomainOperations();
        demonstrateUnixDomainValidation();
        demonstrateUnixDomainConversions();
        demonstrateUnixDomainComparison();
        demonstrateUnixDomainNetworkOperations();
        demonstrateUnixDomainPlatformDifferences();

        std::cout << "\n============================================\n";
#ifdef _WIN32
        std::cout << "     NAMED PIPE DEMO COMPLETED             \n";
#else
        std::cout << "     UNIX DOMAIN SOCKET DEMO COMPLETED     \n";
#endif
        std::cout << "============================================\n";

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
