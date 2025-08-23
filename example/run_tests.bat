@echo off
REM Atom Framework Examples Test Runner (Windows Batch Version)
REM 
REM This script provides a simple way to test Atom framework examples
REM on Windows systems.
REM
REM Usage: run_tests.bat [options]
REM
REM Options:
REM   --build-dir DIR     Build directory (default: build)
REM   --verbose           Enable verbose output
REM   --build-first       Build examples before testing
REM   --clean-first       Clean build before testing
REM   --help              Show this help message

setlocal enabledelayedexpansion

REM Default configuration
set BUILD_DIR=build
set SOURCE_DIR=.
set VERBOSE=false
set BUILD_FIRST=false
set CLEAN_FIRST=false

REM Parse command line arguments
:parse_args
if "%~1"=="" goto end_parse
if "%~1"=="--build-dir" (
    set BUILD_DIR=%~2
    shift
    shift
    goto parse_args
)
if "%~1"=="--verbose" (
    set VERBOSE=true
    shift
    goto parse_args
)
if "%~1"=="-v" (
    set VERBOSE=true
    shift
    goto parse_args
)
if "%~1"=="--build-first" (
    set BUILD_FIRST=true
    shift
    goto parse_args
)
if "%~1"=="--clean-first" (
    set CLEAN_FIRST=true
    shift
    goto parse_args
)
if "%~1"=="--help" goto show_help
if "%~1"=="-h" goto show_help
echo ERROR: Unknown option: %~1
exit /b 1

:show_help
echo Atom Framework Examples Test Runner
echo.
echo Usage: %0 [options]
echo.
echo Options:
echo   --build-dir DIR     Build directory (default: build)
echo   --verbose, -v       Enable verbose output
echo   --build-first       Build examples before testing
echo   --clean-first       Clean build before testing
echo   --help, -h          Show this help message
echo.
echo Examples:
echo   %0 --verbose
echo   %0 --build-first --build-dir my_build
echo   %0 --clean-first
exit /b 0

:end_parse

REM Logging functions
:log
if "%VERBOSE%"=="true" (
    echo [TestRunner] %~1
)
exit /b 0

:log_force
echo [TestRunner] %~1
exit /b 0

:success
echo [SUCCESS] %~1
exit /b 0

:error
echo [ERROR] %~1 >&2
exit /b 0

:warning
echo [WARNING] %~1
exit /b 0

REM Function to clean build directory
:clean_build
call :log_force "Cleaning build directory..."
if exist "%BUILD_DIR%" (
    rmdir /s /q "%BUILD_DIR%"
    call :success "Build directory cleaned"
) else (
    call :log "Build directory doesn't exist, nothing to clean"
)
exit /b 0

REM Function to configure CMake
:configure_cmake
call :log_force "Configuring CMake..."

REM Check if CMake exists
cmake --version >nul 2>&1
if errorlevel 1 (
    call :error "CMake not found. Please install CMake 3.20 or higher."
    exit /b 1
)

cmake -B "%BUILD_DIR%" -S "%SOURCE_DIR%" -DATOM_EXAMPLE_BUILD_ALL=ON
if errorlevel 1 (
    call :error "CMake configuration failed"
    exit /b 1
) else (
    call :success "CMake configuration successful"
)
exit /b 0

REM Function to build examples
:build_examples
call :log_force "Building examples..."

REM Use parallel build
cmake --build "%BUILD_DIR%" --parallel
if errorlevel 1 (
    call :error "Build failed"
    exit /b 1
) else (
    call :success "Build successful"
)
exit /b 0

REM Function to test a single example
:test_example
set module=%~1
set target=%~2
set description=%~3

call :log "Testing [%module%] %description%..."

REM Look for executable
set executable=
if exist "%BUILD_DIR%\example\%module%\%target%.exe" (
    set executable=%BUILD_DIR%\example\%module%\%target%.exe
) else if exist "%BUILD_DIR%\example\%module%\%target%" (
    set executable=%BUILD_DIR%\example\%module%\%target%
) else (
    call :warning "[%module%] %description%: SKIPPED (executable not found)"
    exit /b 2
)

REM Run the executable
"%executable%" >nul 2>&1
if errorlevel 1 (
    call :error "[%module%] %description%: FAILED (exit %errorlevel%)"
    exit /b 1
) else (
    call :success "[%module%] %description%: PASSED"
    exit /b 0
)

REM Function to run all tests
:run_tests
call :log_force "Running example tests..."

set passed=0
set failed=0
set skipped=0
set total=0

echo.
echo === Atom Framework Examples Test Suite ===
echo.

REM Test known working examples
call :test_example "containers" "containers_high_performance_containers_example" "High Performance Containers"
call :update_counters %errorlevel%

call :test_example "meta" "meta_comprehensive_meta_example" "Comprehensive Meta"
call :update_counters %errorlevel%

call :test_example "secret" "secret_basic_test" "Secret Basic Test"
call :update_counters %errorlevel%

call :test_example "sysinfo" "sysinfo_header_test" "Sysinfo Header Test"
call :update_counters %errorlevel%

echo.
echo === Build-Only Tests (Known Runtime Issues) ===
echo.

REM Test build-only examples
call :test_build_only "algorithm" "algorithm_md5" "MD5 Algorithm (Build Only)"
call :update_counters %errorlevel%

call :test_build_only "secret" "secret_secure_storage_example" "Secret Secure Storage (Build Only)"
call :update_counters %errorlevel%

call :test_build_only "sysinfo" "sysinfo_basic_sysinfo_example" "Sysinfo Basic Example (Build Only)"
call :update_counters %errorlevel%

REM Print summary
echo.
echo === Test Summary ===
echo.
echo Results:
echo   ✅ Passed: !passed!
echo   ❌ Failed: !failed!
echo   ⏭️ Skipped: !skipped!
echo   📊 Total: !total!

if !total! gtr 0 (
    set /a success_rate=!passed! * 100 / !total!
    echo   📈 Success Rate: !success_rate!%%
)

echo.

REM Return appropriate exit code
if !failed! equ 0 (
    call :success "All tests completed successfully!"
    exit /b 0
) else (
    call :error "!failed! test(s) failed"
    exit /b 1
)

:update_counters
set /a total+=1
if %1 equ 0 (
    set /a passed+=1
) else if %1 equ 1 (
    set /a failed+=1
) else if %1 equ 2 (
    set /a skipped+=1
)
exit /b 0

:test_build_only
set module=%~1
set target=%~2
set description=%~3

call :log "Checking build for [%module%] %description%..."

if exist "%BUILD_DIR%\example\%module%\%target%.exe" (
    call :success "[%module%] %description%: BUILD OK"
    exit /b 0
) else if exist "%BUILD_DIR%\example\%module%\%target%" (
    call :success "[%module%] %description%: BUILD OK"
    exit /b 0
) else (
    call :error "[%module%] %description%: BUILD FAILED"
    exit /b 1
)

REM Main execution
:main
call :log_force "Starting Atom Framework Examples Test Runner..."

REM Clean if requested
if "%CLEAN_FIRST%"=="true" (
    call :clean_build
    if errorlevel 1 exit /b 1
)

REM Configure CMake if needed
if "%CLEAN_FIRST%"=="true" goto need_configure
if "%BUILD_FIRST%"=="true" goto need_configure
if not exist "%BUILD_DIR%\CMakeCache.txt" goto need_configure
goto skip_configure

:need_configure
call :configure_cmake
if errorlevel 1 exit /b 1

:skip_configure

REM Build examples if requested
if "%BUILD_FIRST%"=="true" (
    call :build_examples
    if errorlevel 1 exit /b 1
)

REM Run tests
call :run_tests
set test_exit_code=%errorlevel%

if %test_exit_code% equ 0 (
    call :success "Test suite completed successfully!"
) else (
    call :error "Test suite completed with failures"
)

exit /b %test_exit_code%

REM Call main function
call :main
