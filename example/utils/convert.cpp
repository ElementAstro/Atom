#ifdef _WIN32

#include "atom/utils/convert.hpp"
#include <iostream>

using namespace atom::utils;

int main() {

    // Example string and wide string
    std::string exampleStr = "Hello, World!";
    std::wstring exampleWStr = L"Hello, World!";

    // Convert std::string to LPWSTR
    LPWSTR lpwstr1 = StringToLPWSTR(exampleStr);
    std::wcout << L"String to LPWSTR: " << lpwstr1 << std::endl;
    LocalFree(lpwstr1);  // Free the allocated memory

    // Convert std::wstring to LPWSTR
    LPWSTR lpwstr2 = WStringToLPWSTR(exampleWStr);
    std::wcout << L"WString to LPWSTR: " << lpwstr2 << std::endl;
    LocalFree(lpwstr2);  // Free the allocated memory

    // Convert LPWSTR to std::string
    LPWSTR lpwstr3 = StringToLPWSTR(exampleStr);
    std::string strFromLPWSTR = LPWSTRToString(lpwstr3);
    std::cout << "LPWSTR to String: " << strFromLPWSTR << std::endl;
    LocalFree(lpwstr3);  // Free the allocated memory

    // Convert LPCWSTR to std::string
    LPCWSTR lpcwstr = exampleWStr.c_str();
    std::string strFromLPCWSTR = LPCWSTRToString(lpcwstr);
    std::cout << "LPCWSTR to String: " << strFromLPCWSTR << std::endl;

    // Convert LPWSTR to std::wstring
    LPWSTR lpwstr4 = WStringToLPWSTR(exampleWStr);
    std::wstring wstrFromLPWSTR = LPWSTRToWString(lpwstr4);
    std::wcout << L"LPWSTR to WString: " << wstrFromLPWSTR << std::endl;
    LocalFree(lpwstr4);  // Free the allocated memory

    // Convert LPCWSTR to std::wstring
    std::wstring wstrFromLPCWSTR = LPCWSTRToWString(lpcwstr);
    std::wcout << L"LPCWSTR to WString: " << wstrFromLPCWSTR << std::endl;

    // Convert std::string to LPSTR
    LPSTR lpstr1 = StringToLPSTR(exampleStr);
    std::cout << "String to LPSTR: " << lpstr1 << std::endl;
    LocalFree(lpstr1);  // Free the allocated memory

    // Convert std::wstring to LPSTR
    LPSTR lpstr2 = WStringToLPSTR(exampleWStr);
    std::cout << "WString to LPSTR: " << lpstr2 << std::endl;
    LocalFree(lpstr2);  // Free the allocated memory

    // Convert WCHAR array to std::string
    WCHAR wCharArray[] = L"Hello, World!";
    std::string strFromWCharArray = WCharArrayToString(wCharArray);
    std::cout << "WCHAR array to String: " << strFromWCharArray << std::endl;

    // Convert std::string_view to LPWSTR
    std::string_view strView = "Hello, World!";
    LPWSTR lpwstrFromStrView = CharToLPWSTR(strView);
    std::wcout << L"String_view to LPWSTR: " << lpwstrFromStrView << std::endl;
    LocalFree(lpwstrFromStrView);  // Free the allocated memory


    return 0;
}

#endif