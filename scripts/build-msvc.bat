@echo off
setlocal enabledelayedexpansion

REM Enhanced MSVC Build script for Atom project
REM Author: Enhanced for MSVC support
REM Adds Visual Studio environment detection and setup

REM Script configuration
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..
set BUILD_DIR=%PROJECT_ROOT%\build-msvc
set DIST_DIR=%PROJECT_ROOT%\dist
set LOG_DIR=%PROJECT_ROOT%\logs

REM Create timestamp for logging
for /f "tokens=2 delims==" %%a in ('wmic OS Get localdatetime /value') do set "dt=%%a"
set TIMESTAMP=%dt:~0,8%_%dt:~8,6%
set LOG_FILE=%LOG_DIR%\build_msvc_%TIMESTAMP%.log

REM Ensure log directory exists
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

echo ===============================================
echo Atom Project MSVC Enhanced Build Script
echo ===============================================
echo Build script started at %date% %time%
echo Project root: %PROJECT_ROOT%
echo Build log: %LOG_FILE%

REM Initialize build options
set BUILD_TYPE=Release
set BUILD_PYTHON=OFF
set BUILD_SHARED=OFF
set BUILD_EXAMPLES=ON
set BUILD_TESTS=OFF
set BUILD_SSH=OFF
set CLEAN_BUILD=n
set SHOW_HELP=n
set PARALLEL_JOBS=
set INSTALL_PREFIX=
set VERBOSE=n
set USE_VCPKG=OFF
set VS_VERSION=
set ARCHITECTURE=x64

REM Parse command-line options
:parse_args
if "%~1"=="" goto end_parse_args

if /i "%~1"=="--debug" (
    set BUILD_TYPE=Debug
    goto next_arg
)
if /i "%~1"=="--release" (
    set BUILD_TYPE=Release
    goto next_arg
)
if /i "%~1"=="--relwithdebinfo" (
    set BUILD_TYPE=RelWithDebInfo
    goto next_arg
)
if /i "%~1"=="--python" (
    set BUILD_PYTHON=ON
    goto next_arg
)
if /i "%~1"=="--shared" (
    set BUILD_SHARED=ON
    goto next_arg
)
if /i "%~1"=="--examples" (
    set BUILD_EXAMPLES=ON
    goto next_arg
)
if /i "%~1"=="--tests" (
    set BUILD_TESTS=ON
    goto next_arg
)
if /i "%~1"=="--ssh" (
    set BUILD_SSH=ON
    goto next_arg
)
if /i "%~1"=="--clean" (
    set CLEAN_BUILD=y
    goto next_arg
)
if /i "%~1"=="--vcpkg" (
    set USE_VCPKG=ON
    goto next_arg
)
if /i "%~1"=="--vs2019" (
    set VS_VERSION=16
    goto next_arg
)
if /i "%~1"=="--vs2022" (
    set VS_VERSION=17
    goto next_arg
)
if /i "%~1"=="--x86" (
    set ARCHITECTURE=Win32
    goto next_arg
)
if /i "%~1"=="--x64" (
    set ARCHITECTURE=x64
    goto next_arg
)
if /i "%~1"=="--jobs" (
    set PARALLEL_JOBS=%2
    shift
    goto next_arg
)
if /i "%~1"=="--prefix" (
    set INSTALL_PREFIX=%2
    shift
    goto next_arg
)
if /i "%~1"=="--verbose" (
    set VERBOSE=y
    goto next_arg
)
if /i "%~1"=="--help" (
    set SHOW_HELP=y
    goto next_arg
) else (
    echo Unknown option: %1
    set SHOW_HELP=y
    goto next_arg
)

:next_arg
shift
goto parse_args

:end_parse_args

REM Show help if requested
if "%SHOW_HELP%"=="y" (
    echo Usage: build-msvc.bat [options]
    echo.
    echo Build Options:
    echo   --debug        Build in debug mode
    echo   --release      Build in release mode (default)
    echo   --relwithdebinfo Build in release with debug info mode
    echo   --python       Enable Python bindings
    echo   --shared       Build shared libraries
    echo   --examples     Build examples (default ON)
    echo   --tests        Build tests
    echo   --ssh          Enable SSH support
    echo   --clean        Clean build directory before building
    echo   --vcpkg        Use vcpkg for dependency management
    echo.
    echo MSVC Options:
    echo   --vs2019       Force Visual Studio 2019 (v16)
    echo   --vs2022       Force Visual Studio 2022 (v17)
    echo   --x86          Build for x86 architecture
    echo   --x64          Build for x64 architecture (default)
    echo.
    echo Other Options:
    echo   --jobs N       Use N parallel jobs for building
    echo   --prefix PATH  Set installation prefix
    echo   --verbose      Enable verbose output
    echo   --help         Show this help message
    echo.
    exit /b 0
)

echo Build configuration:
echo   Build type: %BUILD_TYPE%
echo   Python bindings: %BUILD_PYTHON%
echo   Shared libraries: %BUILD_SHARED%
echo   Build examples: %BUILD_EXAMPLES%
echo   Build tests: %BUILD_TESTS%
echo   SSH support: %BUILD_SSH%
echo   Clean build: %CLEAN_BUILD%
echo   Use vcpkg: %USE_VCPKG%
echo   Architecture: %ARCHITECTURE%
echo   VS Version: %VS_VERSION%
echo.

REM Function to detect Visual Studio installations
call :detect_visual_studio

REM Function to setup Visual Studio environment
call :setup_vs_environment

REM Check if CMake is available
where cmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo Error: cmake not found in PATH
    echo Please install CMake from https://cmake.org/download/
    echo Or ensure it's available in your Visual Studio installation
    exit /b 1
)

REM Set parallel jobs if not specified
if "%PARALLEL_JOBS%"=="" (
    for /f %%i in ('echo %NUMBER_OF_PROCESSORS%') do set PARALLEL_JOBS=%%i
    echo Auto-detected %PARALLEL_JOBS% CPU cores for parallel building
)

REM Clean build directory if requested
if "%CLEAN_BUILD%"=="y" (
    echo Cleaning build directory...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
)

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM Configure CMake arguments
set CMAKE_ARGS=-B "%BUILD_DIR%"
set CMAKE_ARGS=%CMAKE_ARGS% -S "%PROJECT_ROOT%"
set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
set CMAKE_ARGS=%CMAKE_ARGS% -DATOM_BUILD_PYTHON_BINDINGS=%BUILD_PYTHON%
set CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_SHARED_LIBS=%BUILD_SHARED%
set CMAKE_ARGS=%CMAKE_ARGS% -DATOM_BUILD_EXAMPLES=%BUILD_EXAMPLES%
set CMAKE_ARGS=%CMAKE_ARGS% -DATOM_BUILD_TESTS=%BUILD_TESTS%
set CMAKE_ARGS=%CMAKE_ARGS% -DATOM_USE_SSH=%BUILD_SSH%
set CMAKE_ARGS=%CMAKE_ARGS% -DUSE_VCPKG=%USE_VCPKG%

REM Add architecture specification
if "%ARCHITECTURE%"=="x64" (
    set CMAKE_ARGS=%CMAKE_ARGS% -A x64
) else (
    set CMAKE_ARGS=%CMAKE_ARGS% -A Win32
)

REM Add Visual Studio version if specified
if not "%VS_VERSION%"=="" (
    if "%VS_VERSION%"=="17" (
        set CMAKE_ARGS=%CMAKE_ARGS% -G "Visual Studio 17 2022"
    ) else if "%VS_VERSION%"=="16" (
        set CMAKE_ARGS=%CMAKE_ARGS% -G "Visual Studio 16 2019"
    )
) else (
    REM Let CMake auto-detect the generator
    echo Letting CMake auto-detect Visual Studio version...
)

REM Add install prefix if specified
if not "%INSTALL_PREFIX%"=="" (
    set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_INSTALL_PREFIX="%INSTALL_PREFIX%"
)

REM Enable verbose output if requested
if "%VERBOSE%"=="y" (
    set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_VERBOSE_MAKEFILE=ON
)

REM Run CMake configuration
echo Configuring CMake project with MSVC...
echo Command: cmake %CMAKE_ARGS%
echo.
cmake %CMAKE_ARGS%
if %ERRORLEVEL% NEQ 0 (
    echo Error: CMake configuration failed
    exit /b 1
)

REM Build project
echo.
echo Building project with %PARALLEL_JOBS% parallel jobs...
set BUILD_ARGS=--build "%BUILD_DIR%"
set BUILD_ARGS=%BUILD_ARGS% --config %BUILD_TYPE%
set BUILD_ARGS=%BUILD_ARGS% --parallel %PARALLEL_JOBS%

if "%VERBOSE%"=="y" (
    set BUILD_ARGS=%BUILD_ARGS% --verbose
)

cmake %BUILD_ARGS%
if %ERRORLEVEL% NEQ 0 (
    echo Error: Build failed
    exit /b 1
)

echo.
echo Build completed successfully!
echo Build directory: %BUILD_DIR%
echo Configuration: %BUILD_TYPE%
echo Architecture: %ARCHITECTURE%
echo ===============================================
exit /b 0

REM =============================================================================
REM Helper Functions
REM =============================================================================

:detect_visual_studio
echo Detecting Visual Studio installations...

REM Check for VS 2022
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" (
    set VS2022_PATH=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise
    set VS_FOUND=2022
    echo Found Visual Studio 2022 Enterprise
    goto :vs_detected
)
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" (
    set VS2022_PATH=%ProgramFiles%\Microsoft Visual Studio\2022\Professional
    set VS_FOUND=2022
    echo Found Visual Studio 2022 Professional
    goto :vs_detected
)
if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" (
    set VS2022_PATH=%ProgramFiles%\Microsoft Visual Studio\2022\Community
    set VS_FOUND=2022
    echo Found Visual Studio 2022 Community
    goto :vs_detected
)

REM Check for VS 2019
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise\Common7\Tools\VsDevCmd.bat" (
    set VS2019_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise
    set VS_FOUND=2019
    echo Found Visual Studio 2019 Enterprise
    goto :vs_detected
)
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\Common7\Tools\VsDevCmd.bat" (
    set VS2019_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional
    set VS_FOUND=2019
    echo Found Visual Studio 2019 Professional
    goto :vs_detected
)
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\Common7\Tools\VsDevCmd.bat" (
    set VS2019_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community
    set VS_FOUND=2019
    echo Found Visual Studio 2019 Community
    goto :vs_detected
)

echo Warning: No Visual Studio installation detected
echo Please ensure Visual Studio 2019 or 2022 is installed
set VS_FOUND=none
goto :eof

:vs_detected
echo Using Visual Studio %VS_FOUND%
goto :eof

:setup_vs_environment
echo Setting up clean Visual Studio environment...

REM Save original PATH
set ORIGINAL_PATH=%PATH%

REM Remove MSYS2/MinGW paths from PATH to avoid header conflicts
echo Cleaning PATH from MSYS2/MinGW entries...
set CLEAN_PATH=
for %%i in ("%PATH:;=" "%") do (
    set "entry=%%~i"
    echo !entry! | findstr /i "msys\|mingw" >nul || (
        if defined CLEAN_PATH (
            set "CLEAN_PATH=!CLEAN_PATH!;!entry!"
        ) else (
            set "CLEAN_PATH=!entry!"
        )
    )
)
set PATH=%CLEAN_PATH%

REM Skip if already set up or if VS not found
if defined VSCMD_VER (
    echo Visual Studio environment already configured
    goto :eof
)

if "%VS_FOUND%"=="none" (
    echo Skipping VS environment setup - no Visual Studio found
    goto :eof
)

REM Setup VS 2022 environment
if "%VS_FOUND%"=="2022" (
    if "%ARCHITECTURE%"=="x64" (
        call "%VS2022_PATH%\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64
    ) else (
        call "%VS2022_PATH%\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=amd64
    )
    goto :eof
)

REM Setup VS 2019 environment
if "%VS_FOUND%"=="2019" (
    if "%ARCHITECTURE%"=="x64" (
        call "%VS2019_PATH%\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64
    ) else (
        call "%VS2019_PATH%\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=amd64
    )
    goto :eof
)

echo Current PATH after cleanup:
echo %PATH%
echo.
goto :eof
