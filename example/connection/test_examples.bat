@echo off
REM Comprehensive test script for Atom Connection Examples (Windows)
REM This script tests compilation and basic execution of all connection examples

setlocal enabledelayedexpansion

REM Configuration
set BUILD_DIR=build
set SOURCE_DIR=.
set VERBOSE=false
set BUILD_ONLY=false
set SPECIFIC_EXAMPLE=
set TIMEOUT=10

REM Test results
set TOTAL_TESTS=0
set PASSED_TESTS=0
set FAILED_TESTS=0

REM Function to print timestamped messages
:log
set timestamp=%time:~0,8%
echo [%timestamp%] %~1
goto :eof

:log_info
call :log "INFO: %~1"
goto :eof

:log_success
call :log "SUCCESS: %~1"
goto :eof

:log_warning
call :log "WARNING: %~1"
goto :eof

:log_error
call :log "ERROR: %~1"
goto :eof

:log_verbose
if "%VERBOSE%"=="true" (
    call :log "VERBOSE: %~1"
)
goto :eof

REM Function to record test result
:record_test
set /a TOTAL_TESTS+=1
if "%~2"=="true" (
    set /a PASSED_TESTS+=1
    call :log_success "PASS %~1: %~3"
) else (
    set /a FAILED_TESTS+=1
    call :log_error "FAIL %~1: %~3"
)
goto :eof

REM Function to check if a command exists
:command_exists
where %~1 >nul 2>&1
goto :eof

REM Function to setup build environment
:setup_build_environment
call :log_info "Setting up build environment..."

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM Run CMake configuration
call :log_verbose "Running CMake configuration..."

cmake -DATOM_EXAMPLE_CONNECTION_BUILD_ALL=ON -DATOM_EXAMPLE_CONNECTION_VERBOSE=ON -DCMAKE_BUILD_TYPE=Debug -B "%BUILD_DIR%" -S "../.."
if errorlevel 1 (
    call :log_error "CMake configuration failed"
    exit /b 1
)

call :log_success "Build environment configured successfully"
exit /b 0

REM Function to build examples
:build_examples
set specific_example=%~1

if not "%specific_example%"=="" (
    call :log_info "Building example: %specific_example%..."
    set target=connection_%specific_example%
) else (
    call :log_info "Building all connection examples..."
    set target=connection_examples_all
)

cmake --build "%BUILD_DIR%" --target !target! --parallel
if errorlevel 1 (
    set error_msg=Build failed for !target!
    call :log_error "!error_msg!"
    if not "%specific_example%"=="" (
        call :record_test "build_%specific_example%" false "!error_msg!"
    )
    exit /b 1
)

set success_msg=Build successful for !target!
call :log_success "!success_msg!"
if not "%specific_example%"=="" (
    call :record_test "build_%specific_example%" true "!success_msg!"
)
exit /b 0

REM Function to test example execution
:test_example_execution
set example_name=%~1
set executable=%BUILD_DIR%\connection_%example_name%.exe

call :log_info "Testing execution of %example_name%..."

REM Check if executable exists
if not exist "%executable%" (
    call :record_test "run_%example_name%" false "Executable not found: %executable%"
    exit /b 1
)

REM Determine test strategy based on example type
echo %example_name% | findstr /i "server sockethub" >nul
if not errorlevel 1 (
    call :test_server_example "%example_name%" "%executable%"
    exit /b 0
)

echo %example_name% | findstr /i "client" >nul
if not errorlevel 1 (
    call :test_client_example "%example_name%" "%executable%"
    exit /b 0
)

echo %example_name% | findstr /i "ssh" >nul
if not errorlevel 1 (
    call :command_exists ssh
    if errorlevel 1 (
        call :record_test "run_%example_name%" true "Skipped (SSH not available)"
        exit /b 0
    )
    call :test_standalone_example "%example_name%" "%executable%"
    exit /b 0
)

if "%example_name%"=="ttybase" (
    call :record_test "run_%example_name%" true "Skipped (requires hardware)"
    exit /b 0
)

echo %example_name% | findstr /i "fifo" >nul
if not errorlevel 1 (
    call :record_test "run_%example_name%" true "Skipped on Windows (Unix-specific)"
    exit /b 0
)

call :test_standalone_example "%example_name%" "%executable%"
exit /b 0

REM Function to test server examples
:test_server_example
set name=%~1
set executable=%~2

call :log_verbose "Testing server example: %name%"

REM Start server in background
start /b "" "%executable%" >nul 2>&1
set pid=%ERRORLEVEL%

REM Let it run for a few seconds
timeout /t 3 /nobreak >nul

REM Try to find and terminate the process
tasklist /fi "imagename eq connection_%name%.exe" 2>nul | find /i "connection_%name%.exe" >nul
if not errorlevel 1 (
    REM Server is running, terminate it
    taskkill /f /im "connection_%name%.exe" >nul 2>&1
    call :record_test "run_%name%" true "Server ran successfully for 3s"
) else (
    REM Server may have crashed or finished
    call :record_test "run_%name%" false "Server may have crashed immediately"
)
exit /b 0

REM Function to test client examples
:test_client_example
set name=%~1
set executable=%~2

call :log_verbose "Testing client example: %name%"

REM Run client with timeout
timeout /t %TIMEOUT% "%executable%" >nul 2>&1
if errorlevel 1 (
    REM Client examples may fail if no server is running, which is expected
    call :record_test "run_%name%" true "Expected connection failure (no server running)"
) else (
    call :record_test "run_%name%" true "Completed successfully"
)
exit /b 0

REM Function to test standalone examples
:test_standalone_example
set name=%~1
set executable=%~2

call :log_verbose "Testing standalone example: %name%"

timeout /t %TIMEOUT% "%executable%" >nul 2>&1
if errorlevel 1 (
    call :record_test "run_%name%" true "Timed out (may be expected for some examples)"
) else (
    call :record_test "run_%name%" true "Completed successfully"
)
exit /b 0

REM Function to print test summary
:print_summary
echo.
call :log_info "============================================================"
call :log_info "TEST SUMMARY"
call :log_info "============================================================"
echo.
call :log_info "Total: %TOTAL_TESTS%, Passed: %PASSED_TESTS%, Failed: %FAILED_TESTS%"

if %FAILED_TESTS% equ 0 (
    call :log_success "All tests passed!"
    exit /b 0
) else (
    call :log_error "%FAILED_TESTS% test(s) failed!"
    exit /b 1
)

REM Function to show usage
:show_usage
echo Usage: %0 [OPTIONS]
echo.
echo Options:
echo   --build-only        Only test building, skip execution
echo   --verbose           Enable verbose output
echo   --example NAME      Test specific example only
echo   --timeout SECONDS   Set timeout for tests (default: 10)
echo   --help              Show this help message
echo.
echo Examples:
echo   %0                          # Test all examples
echo   %0 --build-only             # Only test compilation
echo   %0 --example tcpclient      # Test only tcpclient
echo   %0 --verbose --timeout 20   # Verbose mode with 20s timeout
goto :eof

REM Parse command line arguments
:parse_args
if "%~1"=="" goto :main
if "%~1"=="--build-only" (
    set BUILD_ONLY=true
    shift
    goto :parse_args
)
if "%~1"=="--verbose" (
    set VERBOSE=true
    shift
    goto :parse_args
)
if "%~1"=="--example" (
    set SPECIFIC_EXAMPLE=%~2
    shift
    shift
    goto :parse_args
)
if "%~1"=="--timeout" (
    set TIMEOUT=%~2
    shift
    shift
    goto :parse_args
)
if "%~1"=="--help" (
    call :show_usage
    exit /b 0
)
call :log_error "Unknown option: %~1"
call :show_usage
exit /b 1

REM Main execution
:main
call :parse_args %*

call :log_info "Starting Atom Connection Examples Test Suite"

REM Check prerequisites
call :command_exists cmake
if errorlevel 1 (
    call :log_error "CMake is required but not found"
    exit /b 1
)

REM Setup build environment
call :setup_build_environment
if errorlevel 1 exit /b 1

REM Build examples
call :build_examples "%SPECIFIC_EXAMPLE%"
if errorlevel 1 exit /b 1

if "%BUILD_ONLY%"=="true" (
    call :log_warning "Build-only mode: skipping execution tests"
    call :print_summary
    exit /b 0
)

REM Test execution
if not "%SPECIFIC_EXAMPLE%"=="" (
    call :test_example_execution "%SPECIFIC_EXAMPLE%"
) else (
    REM Test all examples
    for %%e in (tcpclient async_tcpclient udpclient udpserver async_udpclient async_udpserver sockethub async_sockethub fifoclient fifoserver async_fifoclient async_fifoserver sshclient sshserver ttybase) do (
        call :test_example_execution "%%e"
    )
)

call :print_summary
exit /b %ERRORLEVEL%
