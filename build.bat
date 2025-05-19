@echo off
REM Build script for Atom project using xmake
REM Author: Max Qian

echo ===============================================
echo Atom Project Build Script
echo ===============================================

REM Check if xmake is installed
where xmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo Error: xmake not found in PATH
    echo Please install xmake from https://xmake.io/
    exit /b 1
)

echo Configuring Atom project...

REM Parse command-line options
set BUILD_TYPE=release
set BUILD_PYTHON=n
set BUILD_SHARED=n
set BUILD_EXAMPLES=n
set BUILD_TESTS=n
set BUILD_CFITSIO=n
set BUILD_SSH=n

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
if /i "%~1"=="--help" (
    echo Usage: build.bat [options]
    echo Options:
    echo   --debug       Build in debug mode
    echo   --python      Build with Python bindings
    echo   --shared      Build shared libraries instead of static
    echo   --examples    Build examples
    echo   --tests       Build tests
    echo   --cfitsio     Build with CFITSIO support
    echo   --ssh         Build with SSH support
    echo   --help        Show this help message
    exit /b 0
)

echo Unknown option: %~1
echo Use --help for usage information
exit /b 1

:next_arg
shift
goto parse_args

:end_parse_args

echo Configuration:
echo   Build type: %BUILD_TYPE%
echo   Python bindings: %BUILD_PYTHON%
echo   Shared libraries: %BUILD_SHARED%
echo   Examples: %BUILD_EXAMPLES%
echo   Tests: %BUILD_TESTS%
echo   CFITSIO support: %BUILD_CFITSIO%
echo   SSH support: %BUILD_SSH%

REM Configure build
xmake config -m %BUILD_TYPE% --build_python=%BUILD_PYTHON% --shared_libs=%BUILD_SHARED% ^
    --build_examples=%BUILD_EXAMPLES% --build_tests=%BUILD_TESTS% ^
    --enable_ssh=%BUILD_SSH%

if %ERRORLEVEL% NEQ 0 (
    echo Configuration failed!
    exit /b %ERRORLEVEL%
)

echo ===============================================
echo Building Atom...
echo ===============================================

xmake build -v

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b %ERRORLEVEL%
)

echo ===============================================
echo Build completed successfully!
echo ===============================================

echo To install, run: xmake install
echo To run tests (if built), run: xmake run -g test
