#include "atom/connection/sshserver.hpp"
#include <iostream>
#include <filesystem>

int main() {
    try {
        // Create a temporary config file path
        std::filesystem::path configFile = std::filesystem::temp_directory_path() / "test_ssh_config";

        // Create SSH server instance
        atom::connection::SshServer sshServer(configFile);

        // Test basic configuration
        sshServer.setPort(2222);  // Use non-standard port for testing
        sshServer.setListenAddress("127.0.0.1");  // Localhost only
        sshServer.setPasswordAuthentication(false);  // Disable password auth for security
        sshServer.allowRootLogin(false);  // Disable root login

        std::cout << "SSH Server Configuration Test:" << std::endl;
        std::cout << "Port: " << sshServer.getPort() << std::endl;
        std::cout << "Listen Address: " << sshServer.getListenAddress() << std::endl;
        std::cout << "Password Auth: " << (sshServer.isPasswordAuthenticationEnabled() ? "Enabled" : "Disabled") << std::endl;
        std::cout << "Root Login: " << (sshServer.isRootLoginAllowed() ? "Allowed" : "Denied") << std::endl;

        // Test server status
        std::cout << "Server Running: " << (sshServer.isRunning() ? "Yes" : "No") << std::endl;

        std::cout << "\n✅ SSH Server implementation test completed successfully!" << std::endl;
        std::cout << "The SSH server can be configured and queried without errors." << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
}
