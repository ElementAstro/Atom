#include "file_permission_change.hpp"

#include <stdexcept>
#include <string>

#ifdef ATOM_USE_BOOST
#include <boost/filesystem.hpp>
#endif

#include <spdlog/spdlog.h>

namespace atom::io {

namespace fs = std::filesystem;

void changeFilePermissions(const fs::path& filePath,
                           const atom::containers::String& permissions) {
    if (filePath.empty()) {
        spdlog::error("Empty file path provided to changeFilePermissions");
        throw std::invalid_argument("Empty file path provided");
    }

    try {
        if (!fs::exists(filePath)) {
            spdlog::error("File does not exist: '{}'", filePath.string());
            throw std::runtime_error("File does not exist: " +
                                     filePath.string());
        }

        fs::perms newPerms = fs::perms::none;

        if (permissions.length() != 9) {
            spdlog::error(
                "Invalid permission format: '{}'. Expected 'rwxrwxrwx'",
                permissions);
            throw std::invalid_argument(
                "Invalid permission format. Expected format: 'rwxrwxrwx'");
        }

        if (permissions[0] == 'r')
            newPerms |= fs::perms::owner_read;
        if (permissions[1] == 'w')
            newPerms |= fs::perms::owner_write;
        if (permissions[2] == 'x')
            newPerms |= fs::perms::owner_exec;
        if (permissions[3] == 'r')
            newPerms |= fs::perms::group_read;
        if (permissions[4] == 'w')
            newPerms |= fs::perms::group_write;
        if (permissions[5] == 'x')
            newPerms |= fs::perms::group_exec;
        if (permissions[6] == 'r')
            newPerms |= fs::perms::others_read;
        if (permissions[7] == 'w')
            newPerms |= fs::perms::others_write;
        if (permissions[8] == 'x')
            newPerms |= fs::perms::others_exec;

        spdlog::debug("Setting permissions for '{}' to {:#o}",
                      filePath.string(), static_cast<int>(newPerms));
        fs::permissions(filePath, newPerms, fs::perm_options::replace);
        spdlog::info("Successfully changed permissions for '{}'",
                     filePath.string());

    } catch (const fs::filesystem_error& e) {
        spdlog::error("Failed to change permissions for '{}': {}",
                      filePath.string(), e.what());
        throw std::runtime_error("Failed to change permissions for '" +
                                 filePath.string() + "': " + e.what());
    } catch (const std::invalid_argument& e) {
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Error changing file permissions for '{}': {}",
                      filePath.string(), e.what());
        throw std::runtime_error("Error changing file permissions: " +
                                 std::string(e.what()));
    } catch (...) {
        spdlog::error("Unknown error changing file permissions for '{}'",
                      filePath.string());
        throw std::runtime_error(
            "Unknown error changing file permissions for '" +
            filePath.string() + "'");
    }
}

}  // namespace atom::io
