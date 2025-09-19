// WindowsCompat.hpp - Windows compatibility definitions for MSVC
// This file ensures proper Windows SDK definitions are available

#pragma once

#ifdef _WIN32

// Ensure Windows API definitions are available before including Windows headers
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

// Target Windows 10
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#ifndef WINVER
#define WINVER 0x0A00
#endif

#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0A00
#endif

// Suppress common warnings
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996) // deprecated functions
#pragma warning(disable: 4100) // unreferenced formal parameter  
#pragma warning(disable: 4127) // conditional expression is constant
#pragma warning(disable: 4201) // nameless struct/union
#pragma warning(disable: 4244) // conversion possible loss of data
#pragma warning(disable: 4245) // signed/unsigned mismatch
#endif

// CRITICAL: Include winsock2.h before windows.h to prevent ASIO conflicts
// This fixes the "WinSock.h has already been included" error with ASIO
#ifndef _WINSOCKAPI_
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

// Include Windows headers in correct order
#include <windef.h>
#include <winbase.h>
#include <windows.h>

// Restore warnings
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // _WIN32
