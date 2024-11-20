#include "file_permission.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
#include <boost/system/error_code.hpp>
namespace fs = boost::filesystem;
#else
#ifdef _WIN32
#include <aclapi.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <sys/types.h>
#include <optional>
#endif

namespace atom::io {
#ifdef ATOM_USE_BOOST
std::string getFilePermissions(const std::string &filePath) {
    boost::system::error_code ec;
    fs::perms p = fs::status(filePath, ec).permissions();
    if (ec) {
        boost::log::trivial::error << "Error getting permissions for "
                                   << filePath << ": " << ec.message();
        return "";
    }

    std::string permissions;
    permissions += ((p & fs::perms::owner_read) != fs::perms::none) ? "r" : "-";
    permissions +=
        ((p & fs::perms::owner_write) != fs::perms::none) ? "w" : "-";
    permissions += ((p & fs::perms::owner_exec) != fs::perms::none) ? "x" : "-";
    permissions += ((p & fs::perms::group_read) != fs::perms::none) ? "r" : "-";
    permissions +=
        ((p & fs::perms::group_write) != fs::perms::none) ? "w" : "-";
    permissions += ((p & fs::perms::group_exec) != fs::perms::none) ? "x" : "-";
    permissions +=
        ((p & fs::perms::others_read) != fs::perms::none) ? "r" : "-";
    permissions +=
        ((p & fs::perms::others_write) != fs::perms::none) ? "w" : "-";
    permissions +=
        ((p & fs::perms::others_exec) != fs::perms::none) ? "x" : "-";

    return permissions;
}

std::string getSelfPermissions() {
    boost::system::error_code ec;
    fs::path selfPath =
        fs::current_path(ec);  // Assuming current path as self path
    if (ec) {
        boost::log::trivial::error << "Error getting self path: "
                                   << ec.message();
        return "";
    }
    return getFilePermissions(selfPath.string());
}
#else
#ifdef _WIN32
std::string getFilePermissions(const std::string &filePath) {
    DWORD dwRtnCode = 0;
    PACL pDACL = NULL;
    PSECURITY_DESCRIPTOR pSD = NULL;
    EXPLICIT_ACCESS *pEA = NULL;
    std::string permissions;

    dwRtnCode = GetNamedSecurityInfoA(filePath.c_str(), SE_FILE_OBJECT,
                                      DACL_SECURITY_INFORMATION, NULL, NULL,
                                      &pDACL, NULL, &pSD);
    if (dwRtnCode != ERROR_SUCCESS) {
        std::cerr << "GetNamedSecurityInfoA error: " << dwRtnCode << std::endl;
        return "";
    }

    if (pDACL != NULL) {
        for (DWORD i = 0; i < pDACL->AceCount; i++) {
            ACE_HEADER *aceHeader;
            if (GetAce(pDACL, i, (LPVOID *)&aceHeader)) {
                ACCESS_ALLOWED_ACE *ace = (ACCESS_ALLOWED_ACE *)aceHeader;
                if (ace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE) {
                    permissions += (ace->Mask & GENERIC_READ) ? "r" : "-";
                    permissions += (ace->Mask & GENERIC_WRITE) ? "w" : "-";
                    permissions += (ace->Mask & GENERIC_EXECUTE) ? "x" : "-";
                }
            }
        }
    }

    if (pSD != NULL)
        LocalFree((HLOCAL)pSD);

    return permissions;
}

std::string getSelfPermissions() {
    char path[MAX_PATH];
    if (GetModuleFileNameA(NULL, path, MAX_PATH) == 0) {
        std::cerr << "GetModuleFileNameA error: " << GetLastError()
                  << std::endl;
        return "";
    }
    return getFilePermissions(path);
}
#else
std::string getFilePermissions(const std::string &filePath) {
    struct stat fileStat;
    if (stat(filePath.c_str(), &fileStat) < 0) {
        perror("stat error");
        return "";
    }

    std::string permissions;
    permissions += ((fileStat.st_mode & S_IRUSR) != 0U) ? "r" : "-";
    permissions += ((fileStat.st_mode & S_IWUSR) != 0U) ? "w" : "-";
    permissions += ((fileStat.st_mode & S_IXUSR) != 0U) ? "x" : "-";
    permissions += ((fileStat.st_mode & S_IRGRP) != 0U) ? "r" : "-";
    permissions += ((fileStat.st_mode & S_IWGRP) != 0U) ? "w" : "-";
    permissions += ((fileStat.st_mode & S_IXGRP) != 0U) ? "x" : "-";
    permissions += ((fileStat.st_mode & S_IROTH) != 0U) ? "r" : "-";
    permissions += ((fileStat.st_mode & S_IWOTH) != 0U) ? "w" : "-";
    permissions += ((fileStat.st_mode & S_IXOTH) != 0U) ? "x" : "-";

    return permissions;
}

std::string getSelfPermissions() {
    char path[1024];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len < 0) {
        perror("readlink error");
        return "";
    }
    path[len] = '\0';  // Ensure null-terminated
    return getFilePermissions(path);
}
#endif
#endif

std::optional<bool> compareFileAndSelfPermissions(const std::string &filePath) {
    std::string filePermissions;
    filePermissions = getFilePermissions(filePath);
    if (filePermissions.empty()) {
        return std::nullopt;
    }

    std::string selfPermissions;
    selfPermissions = getSelfPermissions();

    if (selfPermissions.empty()) {
        return std::nullopt;
    }

    return filePermissions == selfPermissions;
}

}  // namespace atom::io