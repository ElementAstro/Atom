@echo off
REM Enhanced build script for Atom project using xmake or CMake
REM Author: Max Qian
setlocal enabledelayedexpansion

echo ===============================================
echo Atom Project Enhanced Build Script
echo ===============================================

REM Parse command-line options with enhanced defaults
set BUILD_TYPE=release
set BUILD_PYTHON=n
set BUILD_SHARED=n
set BUILD_EXAMPLES=n
set BUILD_TESTS=n
set BUILD_CFITSIO=n
set BUILD_SSH=n
set BUILD_SYSTEM=cmake
set CLEAN_BUILD=n
set SHOW_HELP=n
set BUILD_DOCS=n
set BUILD_BENCHMARKS=n
set ENABLE_LTO=n
set ENABLE_COVERAGE=n
set ENABLE_SANITIZERS=n
set PARALLEL_JOBS=
set INSTALL_PREFIX=
set CCACHE_ENABLE=auto
set VERBOSE_BUILD=n

:parse_args
if "%~1"=="" goto end_parse_args

if /i "%~1"=="--debug" (
    set BUILD_TYPE=debug
    goto next_arg
)
if /i "%~1"=="--python" (
    set BUILD_PYTHON=y
    goto next_arg
)
if /i "%~1"=="--shared" (
    set BUILD_SHARED=y
    goto next_arg
)
if /i "%~1"=="--examples" (
    set BUILD_EXAMPLES=y
    goto next_arg
)
if /i "%~1"=="--tests" (
    set BUILD_TESTS=y
    goto next_arg
)
if /i "%~1"=="--cfitsio" (
    set BUILD_CFITSIO=y
    goto next_arg
)
if /i "%~1"=="--ssh" (
    set BUILD_SSH=y
    goto next_arg
)
if /i "%~1"=="--xmake" (
    set BUILD_SYSTEM=xmake
    goto next_arg
)
if /i "%~1"=="--cmake" (
    set BUILD_SYSTEM=cmake
    goto next_arg
)
if /i "%~1"=="--clean" (
    set CLEAN_BUILD=y
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
    echo Usage: build.bat [options]
    echo.
    echo Options:
    echo   --debug        Build in debug mode
    echo   --python       Enable Python bindings
    echo   --shared       Build shared libraries
    echo   --examples     Build examples
    echo   --tests        Build tests
    echo   --cfitsio      Enable CFITSIO support
    echo   --ssh          Enable SSH support
    echo   --xmake        Use XMake build system
    echo   --cmake        Use CMake build system (default)
    echo   --clean        Clean build directory before building
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
echo   CFITSIO support: %BUILD_CFITSIO%
echo   SSH support: %BUILD_SSH%
echo   Build system: %BUILD_SYSTEM%
echo   Clean build: %CLEAN_BUILD%
echo.

REM Check if the selected build system is available
if "%BUILD_SYSTEM%"=="xmake" (
    where xmake >nul 2>nul
    if %ERRORLEVEL% NEQ 0 (
        echo Error: xmake not found in PATH
        echo Please install xmake from https://xmake.io/
        exit /b 1
    )
) else (
    where cmake >nul 2>nul
    if %ERRORLEVEL% NEQ 0 (
        echo Error: cmake not found in PATH
        echo Please install CMake from https://cmake.org/download/
        exit /b 1
    )
)

REM Clean build directory if requested
if "%CLEAN_BUILD%"=="y" (
    echo Cleaning build directory...
    if exist build rmdir /s /q build
    mkdir build
)

REM Build using the selected system
if "%BUILD_SYSTEM%"=="xmake" (
    echo Building with XMake...
    
    REM Configure XMake options
    set XMAKE_ARGS=
    if "%BUILD_TYPE%"=="debug" set XMAKE_ARGS=%XMAKE_ARGS% -m debug
    if "%BUILD_PYTHON%"=="y" set XMAKE_ARGS=%XMAKE_ARGS% --python=y
    if "%BUILD_SHARED%"=="y" set XMAKE_ARGS=%XMAKE_ARGS% --shared=y
    if "%BUILD_EXAMPLES%"=="y" set XMAKE_ARGS=%XMAKE_ARGS% --examples=y
    if "%BUILD_TESTS%"=="y" set XMAKE_ARGS=%XMAKE_ARGS% --tests=y
    if "%BUILD_CFITSIO%"=="y" set XMAKE_ARGS=%XMAKE_ARGS% --cfitsio=y
    if "%BUILD_SSH%"=="y" set XMAKE_ARGS=%XMAKE_ARGS% --ssh=y
    
    REM Run XMake
    echo Configuring XMake project...
    xmake f %XMAKE_ARGS%
    if %ERRORLEVEL% NEQ 0 (
        echo Error: XMake configuration failed
        exit /b 1
    )
    
    echo Building project...
    xmake
    if %ERRORLEVEL% NEQ 0 (
        echo Error: XMake build failed
        exit /b 1
    )
) else (
    echo Building with CMake...
    
    REM Configure CMake options
    set CMAKE_ARGS=-B build
    if "%BUILD_TYPE%"=="debug" set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=Debug
    if "%BUILD_TYPE%"=="release" set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=Release
    if "%BUILD_PYTHON%"=="y" set CMAKE_ARGS=%CMAKE_ARGS% -DATOM_BUILD_PYTHON_BINDINGS=ON
    if "%BUILD_SHARED%"=="y" set CMAKE_ARGS=%CMAKE_ARGS% -DBUILD_SHARED_LIBS=ON
    if "%BUILD_EXAMPLES%"=="y" set CMAKE_ARGS=%CMAKE_ARGS% -DATOM_BUILD_EXAMPLES=ON
    if "%BUILD_TESTS%"=="y" set CMAKE_ARGS=%CMAKE_ARGS% -DATOM_BUILD_TESTS=ON
    if "%BUILD_CFITSIO%"=="y" set CMAKE_ARGS=%CMAKE_ARGS% -DATOM_USE_CFITSIO=ON
    if "%BUILD_SSH%"=="y" set CMAKE_ARGS=%CMAKE_ARGS% -DATOM_USE_SSH=ON
    
    REM Run CMake
    echo Configuring CMake project...
    cmake %CMAKE_ARGS% .
    if %ERRORLEVEL% NEQ 0 (
        echo Error: CMake configuration failed
        exit /b 1
    )
    
    echo Building project...
    cmake --build build --config %BUILD_TYPE%
    if %ERRORLEVEL% NEQ 0 (
        echo Error: CMake build failed
        exit /b 1
    )
)

echo.
echo Build completed successfully!
echo ===============================================
