@echo off
setlocal enabledelayedexpansion

echo ====================================
echo Atom Project vcpkg Dependency Installation Script
echo ====================================

REM Define color codes
set "GREEN=[92m"
set "YELLOW=[93m"
set "RED=[91m"
set "RESET=[0m"

REM Check if vcpkg is installed
echo %GREEN%Checking for vcpkg...%RESET%

set "VCPKG_PATH="

REM Check possible vcpkg locations by priority
if defined VCPKG_ROOT (
    if exist "%VCPKG_ROOT%\vcpkg.exe" (
        set "VCPKG_PATH=%VCPKG_ROOT%"
        echo %GREEN%Found existing VCPKG_ROOT: %VCPKG_PATH%%RESET%
        goto :found_vcpkg
    )
)

if exist "C:\vcpkg\vcpkg.exe" (
    set "VCPKG_PATH=C:\vcpkg"
    echo %GREEN%Found vcpkg: %VCPKG_PATH%%RESET%
    goto :found_vcpkg
)

if exist "%USERPROFILE%\vcpkg\vcpkg.exe" (
    set "VCPKG_PATH=%USERPROFILE%\vcpkg"
    echo %GREEN%Found vcpkg: %VCPKG_PATH%%RESET%
    goto :found_vcpkg
)

if exist "%cd%\vcpkg\vcpkg.exe" (
    set "VCPKG_PATH=%cd%\vcpkg"
    echo %GREEN%Found vcpkg: %VCPKG_PATH%%RESET%
    goto :found_vcpkg
)

:vcpkg_not_found
echo %YELLOW%vcpkg not found. Do you want to install it? (Y/N)%RESET%
set /p INSTALL_CHOICE="> "
if /i "%INSTALL_CHOICE%"=="Y" (
    echo %GREEN%Installing vcpkg...%RESET%
    
    REM Determine installation path
    echo %YELLOW%Please select vcpkg installation location:%RESET%
    echo 1. User home directory (%USERPROFILE%\vcpkg)
    echo 2. C drive root (C:\vcpkg)
    echo 3. Current directory (%cd%\vcpkg)
    set /p INSTALL_LOCATION="> "
    
    if "%INSTALL_LOCATION%"=="1" (
        set "VCPKG_PATH=%USERPROFILE%\vcpkg"
    ) else if "%INSTALL_LOCATION%"=="2" (
        set "VCPKG_PATH=C:\vcpkg"
    ) else if "%INSTALL_LOCATION%"=="3" (
        set "VCPKG_PATH=%cd%\vcpkg"
    ) else (
        echo %RED%Invalid choice. Using default location (%USERPROFILE%\vcpkg)%RESET%
        set "VCPKG_PATH=%USERPROFILE%\vcpkg"
    )
    
    REM Clone and bootstrap vcpkg
    if exist "%VCPKG_PATH%" (
        echo %YELLOW%Directory %VCPKG_PATH% already exists. Continue? (Y/N)%RESET%
        set /p CONTINUE_CHOICE="> "
        if /i not "%CONTINUE_CHOICE%"=="Y" goto :eof
    )
    
    echo %GREEN%Cloning vcpkg to %VCPKG_PATH%...%RESET%
    git clone https://github.com/microsoft/vcpkg.git "%VCPKG_PATH%"
    if %ERRORLEVEL% neq 0 (
        echo %RED%Failed to clone vcpkg%RESET%
        goto :eof
    )
    
    echo %GREEN%Bootstrapping vcpkg...%RESET%
    call "%VCPKG_PATH%\bootstrap-vcpkg.bat" -disableMetrics
    if %ERRORLEVEL% neq 0 (
        echo %RED%Failed to bootstrap vcpkg%RESET%
        goto :eof
    )
    
    REM Set VCPKG_ROOT environment variable
    echo %GREEN%Setting VCPKG_ROOT environment variable...%RESET%
    setx VCPKG_ROOT "%VCPKG_PATH%"
    if %ERRORLEVEL% neq 0 (
        echo %YELLOW%Warning: Failed to set VCPKG_ROOT environment variable%RESET%
    )
    set "VCPKG_ROOT=%VCPKG_PATH%"
) else (
    echo %RED%Operation cancelled. vcpkg is required to continue.%RESET%
    goto :eof
)

:found_vcpkg
echo %GREEN%Using vcpkg: %VCPKG_PATH%%RESET%

REM Detect current system architecture
set "ARCH=x64"
if exist "%PROGRAMFILES(X86)%" (
    set "ARCH=x64"
) else (
    set "ARCH=x86"
)

REM Detect if in MSYS2 environment
set "IS_MSYS2=0"
if defined MSYSTEM (
    set "IS_MSYS2=1"
    echo %GREEN%MSYS2 environment detected: %MSYSTEM%%RESET%
)

REM Determine triplet
set "TRIPLET=%ARCH%-windows"
if %IS_MSYS2% equ 1 (
    set "TRIPLET=%ARCH%-mingw-dynamic"
    echo %GREEN%MSYS2: Using triplet %TRIPLET%%RESET%
    
    REM Check if MinGW triplet needs to be created
    if not exist "%VCPKG_PATH%\triplets\community\%TRIPLET%.cmake" (
        echo %YELLOW%Need to create MinGW triplet file: %TRIPLET%%RESET%
        
        mkdir "%VCPKG_PATH%\triplets\community" 2>nul
        
        echo set(VCPKG_TARGET_ARCHITECTURE %ARCH%) > "%VCPKG_PATH%\triplets\community\%TRIPLET%.cmake"
        echo set(VCPKG_CRT_LINKAGE dynamic) >> "%VCPKG_PATH%\triplets\community\%TRIPLET%.cmake"
        echo set(VCPKG_LIBRARY_LINKAGE dynamic) >> "%VCPKG_PATH%\triplets\community\%TRIPLET%.cmake"
        echo set(VCPKG_CMAKE_SYSTEM_NAME MinGW) >> "%VCPKG_PATH%\triplets\community\%TRIPLET%.cmake"
        
        echo %GREEN%Triplet file created: %TRIPLET%%RESET%
    fi
)

REM Install dependencies
echo %GREEN%Preparing to install Atom dependencies...%RESET%
echo %GREEN%Using triplet: %TRIPLET%%RESET%

echo %GREEN%Installing core dependencies...%RESET%
"%VCPKG_PATH%\vcpkg.exe" install openssl zlib sqlite3 fmt readline pybind11 boost[container] --triplet=%TRIPLET%
if %ERRORLEVEL% neq 0 (
    echo %RED%Failed to install core dependencies%RESET%
    goto :eof
)

echo %YELLOW%Install optional dependencies? (Y/N)%RESET%
set /p OPTIONAL_DEPS="> "
if /i "%OPTIONAL_DEPS%"=="Y" (
    echo %GREEN%Installing optional Boost components...%RESET%
    "%VCPKG_PATH%\vcpkg.exe" install boost[atomic,thread,graph] --triplet=%TRIPLET%
    if %ERRORLEVEL% neq 0 (
        echo %YELLOW%Warning: Failed to install optional Boost components%RESET%
    )
    
    echo %GREEN%Installing test components...%RESET%
    "%VCPKG_PATH%\vcpkg.exe" install gtest --triplet=%TRIPLET%
    if %ERRORLEVEL% neq 0 (
        echo %YELLOW%Warning: Failed to install test components%RESET%
    )
)

echo %GREEN%Integrating vcpkg with the system...%RESET%
"%VCPKG_PATH%\vcpkg.exe" integrate install
if %ERRORLEVEL% neq 0 (
    echo %YELLOW%Warning: vcpkg integration failed%RESET%
)

echo ====================================
echo %GREEN%vcpkg setup complete!%RESET%
echo ====================================
echo.
echo You can now configure the project using the following command:
echo %YELLOW%cmake -B build -G "Ninja" -DCMAKE_TOOLCHAIN_FILE="%VCPKG_PATH%/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=%TRIPLET%%RESET%
echo.
echo Then build:
echo %YELLOW%cmake --build build%RESET%
echo ====================================

echo %YELLOW%Configure the project now? (Y/N)%RESET%
set /p CONFIG_NOW="> "
if /i "%CONFIG_NOW%"=="Y" (
    echo %GREEN%Configuring project...%RESET%
    cmake -B build -G "Ninja" -DCMAKE_TOOLCHAIN_FILE="%VCPKG_PATH%/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=%TRIPLET%
    if %ERRORLEVEL% neq 0 (
        echo %RED%Project configuration failed%RESET%
    ) else {
        echo %GREEN%Project configured successfully!%RESET%
        
        echo %YELLOW%Start build now? (Y/N)%RESET%
        set /p BUILD_NOW="> "
        if /i "%BUILD_NOW%"=="Y" (
            echo %GREEN%Building project...%RESET%
            cmake --build build
            if %ERRORLEVEL% neq 0 (
                echo %RED%Project build failed%RESET%
            ) else {
                echo %GREEN%Project built successfully!%RESET%
            }
        )
    }
)

pause