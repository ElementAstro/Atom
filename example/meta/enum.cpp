/**
 * Comprehensive examples for atom::meta::enum utilities
 *
 * This file demonstrates the use of all enum utility functionalities:
 * 1. Basic enum conversions (to/from string)
 * 2. Integer conversions
 * 3. Enum validation and checking
 * 4. Enum collections and sorting
 * 5. Fuzzy matching
 * 6. Flag enum operations
 * 7. Enum aliases
 * 8. Enum descriptions
 * 9. Serialization/deserialization
 * 10. Range checking and bitmasks
 */

#include "atom/meta/enum.hpp"
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

using namespace atom::meta;

// Define some example enums to work with
enum class Color {
    Red = 0,
    Green = 1,
    Blue = 2,
    Yellow = 3,
    Magenta = 4,
    Cyan = 5,
    Black = 6,
    White = 7
};

// Define flags enum for bitwise operations
enum class Permission : uint8_t {
    None = 0x00,
    Read = 0x01,
    Write = 0x02,
    Execute = 0x04,
    Admin = 0x08,
    All = Read | Write | Execute | Admin
};

// Define an enum with descriptions
enum class HttpStatus {
    OK = 200,
    Created = 201,
    Accepted = 202,
    NoContent = 204,
    BadRequest = 400,
    Unauthorized = 401,
    Forbidden = 403,
    NotFound = 404,
    ServerError = 500
};

// Implement EnumTraits specialization for Color
template <>
struct atom::meta::EnumTraits<Color> {
    static constexpr std::array values = {
        Color::Red,     Color::Green, Color::Blue,  Color::Yellow,
        Color::Magenta, Color::Cyan,  Color::Black, Color::White};

    static constexpr std::array names = {
        std::string_view{"Red"},     std::string_view{"Green"},
        std::string_view{"Blue"},    std::string_view{"Yellow"},
        std::string_view{"Magenta"}, std::string_view{"Cyan"},
        std::string_view{"Black"},   std::string_view{"White"}};
};

// Implement EnumTraits specialization for Permission
template <>
struct atom::meta::EnumTraits<Permission> {
    static constexpr std::array values = {
        Permission::None,    Permission::Read,  Permission::Write,
        Permission::Execute, Permission::Admin, Permission::All};

    static constexpr std::array names = {
        std::string_view{"None"},  std::string_view{"Read"},
        std::string_view{"Write"}, std::string_view{"Execute"},
        std::string_view{"Admin"}, std::string_view{"All"}};
};

// EnumAliasTraits specialization for Permission
template <>
struct atom::meta::EnumAliasTraits<Permission> {
    static constexpr std::array aliases = {
        std::string_view{"0"}, std::string_view{"R"}, std::string_view{"W"},
        std::string_view{"X"}, std::string_view{"A"}, std::string_view{"RWX"}};
};

// Implement EnumTraits specialization for HttpStatus with descriptions
template <>
struct atom::meta::EnumTraits<HttpStatus> {
    static constexpr std::array values = {
        HttpStatus::OK,        HttpStatus::Created,    HttpStatus::Accepted,
        HttpStatus::NoContent, HttpStatus::BadRequest, HttpStatus::Unauthorized,
        HttpStatus::Forbidden, HttpStatus::NotFound,   HttpStatus::ServerError};

    static constexpr std::array names = {
        std::string_view{"OK"},         std::string_view{"Created"},
        std::string_view{"Accepted"},   std::string_view{"NoContent"},
        std::string_view{"BadRequest"}, std::string_view{"Unauthorized"},
        std::string_view{"Forbidden"},  std::string_view{"NotFound"},
        std::string_view{"ServerError"}};

    static constexpr std::array descriptions = {
        std::string_view{"Request succeeded"},
        std::string_view{"Resource created successfully"},
        std::string_view{"Request accepted for processing"},
        std::string_view{"Request succeeded with no content to return"},
        std::string_view{"Invalid request format or parameters"},
        std::string_view{"Authentication required"},
        std::string_view{"Authenticated but not authorized"},
        std::string_view{"Resource not found"},
        std::string_view{"Server encountered an error"}};
};

// Helper function to print section headers
void printHeader(const std::string& title) {
    std::cout << "\n==========================================================="
              << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "==========================================================="
              << std::endl;
}

// Helper function for formatting
void printValue(const std::string& label, const std::string& value) {
    std::cout << std::left << std::setw(30) << label << ": " << value
              << std::endl;
}

void printValue(const std::string& label, int value) {
    std::cout << std::left << std::setw(30) << label << ": " << value
              << std::endl;
}

void printValue(const std::string& label, bool value) {
    std::cout << std::left << std::setw(30) << label << ": "
              << (value ? "true" : "false") << std::endl;
}

// 修复: 将默认参数改为适当的类型，并修复const引用无法接受nullptr的问题
template <typename T>
void printOptional(
    const std::string& label, const std::optional<T>& value,
    std::function<std::string(const T&)> formatter = [](const T& v) {
        return std::to_string(static_cast<int>(enum_to_integer(v)));
    }) {
    std::cout << std::left << std::setw(30) << label << ": ";

    if (value.has_value()) {
        std::cout << formatter(value.value()) << std::endl;
    } else {
        std::cout << "nullopt" << std::endl;
    }
}

int main() {
    std::cout << "================================================="
              << std::endl;
    std::cout << "   Comprehensive Enum Utilities Examples          "
              << std::endl;
    std::cout << "================================================="
              << std::endl;

    //=========================================================================
    // 1. Basic Enum Conversions
    //=========================================================================
    printHeader("1. Basic Enum Conversions");

    Color redColor = Color::Red;
    Color blueColor = Color::Blue;

    // Convert enum to string
    std::string_view redName = enum_name(redColor);
    printValue("enum_name(Color::Red)", std::string(redName));

    // Convert string to enum
    auto greenOpt = enum_cast<Color>("Green");
    printOptional<Color>(
        "enum_cast<Color>(\"Green\")", greenOpt,
        [](const Color& c) { return std::string(enum_name(c)); });

    // Try with invalid name
    auto invalidColor = enum_cast<Color>("Purple");
    printOptional<Color>(
        "enum_cast<Color>(\"Purple\")", invalidColor,
        [](const Color& c) { return std::string(enum_name(c)); });

    //=========================================================================
    // 2. Integer Conversions
    //=========================================================================
    printHeader("2. Integer Conversions");

    // Convert enum to integer
    int blueValue = enum_to_integer(blueColor);
    printValue("enum_to_integer(Color::Blue)", blueValue);

    // Convert integer to enum
    auto colorFromInt = integer_to_enum<Color>(3);
    printOptional<Color>(
        "integer_to_enum<Color>(3)", colorFromInt,
        [](const Color& c) { return std::string(enum_name(c)); });

    // Try with invalid integer
    auto invalidColorInt = integer_to_enum<Color>(10);
    printOptional<Color>(
        "integer_to_enum<Color>(10)", invalidColorInt,
        [](const Color& c) { return std::string(enum_name(c)); });

    //=========================================================================
    // 3. Enum Validation and Checking
    //=========================================================================
    printHeader("3. Enum Validation and Checking");

    // Check if enum value is valid
    bool isYellowValid = enum_contains(Color::Yellow);
    printValue("enum_contains(Color::Yellow)", isYellowValid);

    // Check if integer is in enum range
    bool is3InRange = integer_in_enum_range<Color>(3);
    printValue("integer_in_enum_range<Color>(3)", is3InRange);

    bool is10InRange = integer_in_enum_range<Color>(10);
    printValue("integer_in_enum_range<Color>(10)", is10InRange);

    // Get default enum value
    Color defaultColor = enum_default<Color>();
    printValue("enum_default<Color>()", std::string(enum_name(defaultColor)));

    //=========================================================================
    // 4. Enum Collections and Sorting
    //=========================================================================
    printHeader("4. Enum Collections and Sorting");

    // Get all enum entries
    std::cout << "All Color enum entries:" << std::endl;
    auto colorEntries = enum_entries<Color>();
    for (const auto& [value, name] : colorEntries) {
        std::cout << "  " << std::left << std::setw(10) << name << " = "
                  << enum_to_integer(value) << std::endl;
    }

    // Sort by name
    std::cout << "\nColor enums sorted by name:" << std::endl;
    auto colorsByName = enum_sorted_by_name<Color>();
    for (const auto& [value, name] : colorsByName) {
        std::cout << "  " << std::left << std::setw(10) << name << " = "
                  << enum_to_integer(value) << std::endl;
    }

    // Sort by value
    std::cout << "\nColor enums sorted by value:" << std::endl;
    auto colorsByValue = enum_sorted_by_value<Color>();
    for (const auto& [value, name] : colorsByValue) {
        std::cout << "  " << std::left << std::setw(10) << name << " = "
                  << enum_to_integer(value) << std::endl;
    }

    //=========================================================================
    // 5. Fuzzy Matching
    //=========================================================================
    printHeader("5. Fuzzy Matching");

    // Fuzzy match with partial string
    auto magentaFuzzy = enum_cast_fuzzy<Color>("Mage");
    printOptional<Color>(
        "enum_cast_fuzzy<Color>(\"Mage\")", magentaFuzzy,
        [](const Color& c) { return std::string(enum_name(c)); });

    // 修复: 修改为有效的匹配字符串
    auto yellowFuzzy = enum_cast_fuzzy<Color>("Yell");
    printOptional<Color>(
        "enum_cast_fuzzy<Color>(\"Yell\")", yellowFuzzy,
        [](const Color& c) { return std::string(enum_name(c)); });

    auto noneFuzzy = enum_cast_fuzzy<Color>("orange");
    printOptional<Color>(
        "enum_cast_fuzzy<Color>(\"orange\")", noneFuzzy,
        [](const Color& c) { return std::string(enum_name(c)); });

    //=========================================================================
    // 6. Flag Enum Operations
    //=========================================================================
    printHeader("6. Flag Enum Operations");

    // Bitwise operations on enums
    Permission userPermission = Permission::Read;
    printValue("Initial permission", std::string(enum_name(userPermission)));

    // Add write permission
    userPermission |= Permission::Write;
    printValue("After adding Write", std::string(enum_name(userPermission)));

    // Test if has permission
    Permission readPerm = Permission::Read;
    bool hasRead = (userPermission & readPerm) == readPerm;
    printValue("Has Read permission", hasRead);

    // Create permission set with multiple flags
    Permission rwPerm = Permission::Read | Permission::Write;

    // Check composite permissions
    bool hasRW = (userPermission & rwPerm) == rwPerm;
    printValue("Has Read+Write permissions", hasRW);

    // Remove write permission
    userPermission &= ~Permission::Write;
    printValue("After removing Write", std::string(enum_name(userPermission)));

    // Toggle permissions
    userPermission ^= Permission::Execute;
    printValue("After toggling Execute",
               std::string(enum_name(userPermission)));

    // Clear all permissions
    userPermission = Permission::None;
    printValue("After clearing permissions",
               std::string(enum_name(userPermission)));

    // Set all permissions
    userPermission = Permission::All;
    printValue("With all permissions", std::string(enum_name(userPermission)));

    // Get the underlying bitmask
    auto permBitmask = enum_bitmask(userPermission);
    printValue("Permission bitmask", static_cast<int>(permBitmask));

    // Convert bitmask back to enum
    auto permFromBitmask = bitmask_to_enum<Permission>(0x03);  // Read + Write
    printOptional<Permission>(
        "bitmask_to_enum<Permission>(0x03)", permFromBitmask,
        [](const Permission& p) { return std::string(enum_name(p)); });

    //=========================================================================
    // 7. Enum Aliases
    //=========================================================================
    printHeader("7. Enum Aliases");

    // Use alias to get enum value
    auto readPerm1 = enum_cast_with_alias<Permission>("Read");
    printOptional<Permission>(
        "enum_cast_with_alias<Permission>(\"Read\")", readPerm1,
        [](const Permission& p) { return std::string(enum_name(p)); });

    auto readPerm2 = enum_cast_with_alias<Permission>("R");  // Using alias
    printOptional<Permission>(
        "enum_cast_with_alias<Permission>(\"R\")", readPerm2,
        [](const Permission& p) { return std::string(enum_name(p)); });

    //=========================================================================
    // 8. Enum Descriptions
    //=========================================================================
    printHeader("8. Enum Descriptions");

    // Get descriptions for HTTP status codes
    HttpStatus ok = HttpStatus::OK;
    std::string_view okName = enum_name(ok);
    std::string_view okDesc = enum_description(ok);

    printValue("HTTP Status", std::string(okName));
    printValue("Description", std::string(okDesc));

    // Print all HTTP statuses with descriptions
    std::cout << "\nAll HTTP Status Codes with Descriptions:" << std::endl;
    auto httpEntries = enum_entries<HttpStatus>();
    for (const auto& [status, name] : httpEntries) {
        std::cout << "  " << std::left << std::setw(4)
                  << enum_to_integer(status) << " " << std::setw(15) << name
                  << " - " << enum_description(status) << std::endl;
    }

    //=========================================================================
    // 9. Serialization/Deserialization
    //=========================================================================
    printHeader("9. Serialization/Deserialization");

    // Serialize enum to string
    std::string serializedColor = serialize_enum(Color::Cyan);
    printValue("serialize_enum(Color::Cyan)", serializedColor);

    // Deserialize string to enum
    auto deserializedColor = deserialize_enum<Color>("Magenta");
    printOptional<Color>(
        "deserialize_enum<Color>(\"Magenta\")", deserializedColor,
        [](const Color& c) { return std::string(enum_name(c)); });

    //=========================================================================
    // 10. Range Checking
    //=========================================================================
    printHeader("10. Range Checking and Additional Operations");

    // Check if enum value is within range
    bool inColorRange = enum_in_range(Color::Yellow, Color::Red, Color::Blue);
    printValue("enum_in_range(Yellow, Red, Blue)", inColorRange);

    bool inColorRange2 = enum_in_range(Color::Magenta, Color::Red, Color::Blue);
    printValue("enum_in_range(Magenta, Red, Blue)", inColorRange2);

    // Additional usage examples
    std::cout << "\nPractical examples:" << std::endl;

    // Example 1: Parse color from user input
    std::string userInput = "blue";
    std::transform(userInput.begin(), userInput.end(), userInput.begin(),
                   [](unsigned char c) { return std::toupper(c); });

    auto userColor = enum_cast<Color>(userInput);
    if (userColor) {
        std::cout << "Parsed user color: " << enum_name(*userColor)
                  << std::endl;
    } else {
        std::cout << "Invalid color name!" << std::endl;
    }

    // Example 2: Using HTTP status codes in a response handler
    auto handleResponse = [](HttpStatus status) {
        if (enum_in_range(status, HttpStatus::OK, HttpStatus::NoContent)) {
            std::cout << "Success: " << enum_description(status) << std::endl;
        } else if (enum_in_range(status, HttpStatus::BadRequest,
                                 HttpStatus::NotFound)) {
            std::cout << "Client error: " << enum_description(status)
                      << std::endl;
        } else {
            std::cout << "Server error: " << enum_description(status)
                      << std::endl;
        }
    };

    handleResponse(HttpStatus::OK);
    handleResponse(HttpStatus::NotFound);
    handleResponse(HttpStatus::ServerError);

    // Example 3: Using flag enums for file permissions
    auto checkAndUpdatePermissions = [](Permission& perms, bool canExecute) {
        std::cout << "Current permissions: ";

        if ((perms & Permission::Read) == Permission::Read)
            std::cout << "Read ";
        if ((perms & Permission::Write) == Permission::Write)
            std::cout << "Write ";
        if ((perms & Permission::Execute) == Permission::Execute)
            std::cout << "Execute ";
        if ((perms & Permission::Admin) == Permission::Admin)
            std::cout << "Admin";

        std::cout << std::endl;

        // Update permissions
        if (canExecute) {
            perms |= Permission::Execute;
        } else {
            perms &= ~Permission::Execute;
        }

        return perms;
    };

    Permission filePerms = Permission::Read | Permission::Write;
    filePerms = checkAndUpdatePermissions(filePerms, true);   // Add execute
    filePerms = checkAndUpdatePermissions(filePerms, false);  // Remove execute

    return 0;
}