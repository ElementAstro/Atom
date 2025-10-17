@echo off
REM =============================================================================
REM Atom Test Runner Script (Windows)
REM =============================================================================
REM This script provides convenient access to the Atom testing infrastructure
REM with various options for running tests in different ways.
REM
REM Usage: scripts\run_tests.bat [OPTIONS] [TARGETS]
REM =============================================================================

setlocal enabledelayedexpansion

REM Script configuration
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."
set "BUILD_DIR=%PROJECT_ROOT%\build"

REM Default values
set "VERBOSE=false"
set "PARALLEL=false"
set "THREADS=%NUMBER_OF_PROCESSORS%"
set "OUTPUT_FORMAT="
set "OUTPUT_FILE="
set "MODULE="
set "CATEGORY="
set "FILTER="
set "HELP=false"
set "CLEAN=false"
set "BUILD_ONLY=false"
set "RUN_TESTS=true"
set "USE_CTEST=false"
set "COVERAGE=false"

REM Initialize targets array
set TARGET_COUNT=0

REM Print colored output (limited on Windows)
:print_status
echo [INFO] %~1
goto :eof

:print_success
echo [SUCCESS] %~1
goto :eof

:print_warning
echo [WARNING] %~1
goto :eof

:print_error
echo [ERROR] %~1
goto :eof

REM Show help information
:show_help
echo Atom Test Runner Script (Windows)
echo.
echo USAGE:
echo     %~nx0 [OPTIONS] [TARGETS]
echo.
echo OPTIONS:
echo     -h, --help              Show this help message
echo     -v, --verbose           Enable verbose output
echo     -p, --parallel [N]      Run tests in parallel (default: all CPU cores)
echo     -t, --threads N         Number of threads for parallel execution
echo     -m, --module NAME       Run tests from specific module only
echo     -c, --category NAME     Run tests from specific category only
echo     -f, --filter PATTERN    Run tests matching regex pattern
echo     --output-format FORMAT  Output format (json, xml, html, text)
echo     --output FILE           Output file for test results
echo     --clean                 Clean build directory before building
echo     --build-only            Build tests without running them
echo     --ctest                 Use CTest instead of unified test runner
echo     --coverage              Generate code coverage report
echo     --build-dir DIR         Specify build directory (default: build\)
echo.
echo TARGETS:
echo     all                     Run all tests (default)
echo     algorithm               Run algorithm module tests
echo     async                   Run async module tests
echo     components              Run components module tests
echo     connection              Run connection module tests
echo     containers              Run containers module tests
echo     error                   Run error module tests
echo     extra                   Run extra utilities tests
echo     image                   Run image module tests
echo     io                      Run IO module tests
echo     log                     Run log module tests
echo     memory                  Run memory module tests
echo     meta                    Run metaprogramming tests
echo     search                  Run search module tests
echo     secret                  Run cryptographic tests
echo     serial                  Run serial communication tests
echo     sysinfo                 Run system information tests
echo     system                  Run system integration tests
echo     type                    Run type system tests
echo     utils                   Run utilities tests
echo     web                     Run web utilities tests
echo     core                    Run core module tests (error, log, meta, type, utils)
echo     io_modules              Run IO module tests (io, image, serial)
echo     system_modules          Run system module tests (system, sysinfo)
echo     network_modules         Run network module tests (web, connection)
echo.
echo EXAMPLES:
echo     %~nx0                              # Run all tests
echo     %~nx0 --verbose --parallel         # Run all tests in parallel with verbose output
echo     %~nx0 --module error               # Run error module tests only
echo     %~nx0 --category unit              # Run unit tests only
echo     %~nx0 --filter ".*socket.*"        # Run tests containing "socket"
echo     %~nx0 --output-format=json --output=results.json  # Export results to JSON
echo.
goto :eof

REM Parse command line arguments
:parse_args
if "%~1"=="" goto :parse_args_done

if "%~1"=="-h" set "HELP=true" & shift & goto parse_args
if "%~1"=="--help" set "HELP=true" & shift & goto parse_args

if "%~1"=="-v" set "VERBOSE=true" & shift & goto parse_args
if "%~1"=="--verbose" set "VERBOSE=true" & shift & goto parse_args

if "%~1"=="-p" set "PARALLEL=true" & shift & goto parse_args
if "%~1"=="--parallel" set "PARALLEL=true" & shift & goto parse_args

if "%~1"=="-t" set "THREADS=%~2" & shift & shift & goto parse_args
if "%~1"=="--threads" set "THREADS=%~2" & shift & shift & goto parse_args

if "%~1"=="-m" set "MODULE=%~2" & shift & shift & goto parse_args
if "%~1"=="--module" set "MODULE=%~2" & shift & shift & goto parse_args

if "%~1"=="-c" set "CATEGORY=%~2" & shift & shift & goto parse_args
if "%~1"=="--category" set "CATEGORY=%~2" & shift & shift & goto parse_args

if "%~1"=="-f" set "FILTER=%~2" & shift & shift & goto parse_args
if "%~1"=="--filter" set "FILTER=%~2" & shift & shift & goto parse_args

if "%~1"=="--output-format" set "OUTPUT_FORMAT=%~2" & shift & shift & goto parse_args
if "%~1"=="--output" set "OUTPUT_FILE=%~2" & shift & shift & goto parse_args

if "%~1"=="--clean" set "CLEAN=true" & shift & goto parse_args
if "%~1"=="--build-only" set "BUILD_ONLY=true" & set "RUN_TESTS=false" & shift & goto parse_args
if "%~1"=="--ctest" set "USE_CTEST=true" & shift & goto parse_args
if "%~1"=="--coverage" set "COVERAGE=true" & shift & goto parse_args

if "%~1"=="--build-dir" set "BUILD_DIR=%~2" & shift & shift & goto parse_args

REM Handle targets
set "VALID_TARGET=0"
for %%T in (all algorithm async components connection containers error extra image io log memory meta search secret serial sysinfo system type utils web core io_modules system_modules network_modules) do (
    if "%~1"=="%%T" (
        set "VALID_TARGET=1"
        set /a TARGET_COUNT+=1
        set "TARGET_!TARGET_COUNT!=%~1"
        shift
        goto parse_args
    )
)

if "!VALID_TARGET!"=="0" (
    call :print_error "Unknown option: %~1"
    call :show_help
    exit /b 1
)

goto parse_args

:parse_args_done
goto :eof

REM Check if we need to build
:check_build_needed
if not exist "%BUILD_DIR%\run_all_tests.exe" (
    exit /b 0
)

REM Simple timestamp check would be complex in batch, assume we need to build if VERBOSE is true
if "%VERBOSE%"=="true" (
    exit /b 0
)

exit /b 1

REM Build the project
:build_project
call :print_status "Building project..."

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

REM Clean if requested
if "%CLEAN%"=="true" (
    call :print_status "Cleaning build directory..."
    del /Q /S * >nul 2>&1
)

REM Configure with CMake
set "CMAKE_ARGS=-DATOM_BUILD_TESTS=ON"

if "%COVERAGE%"=="true" (
    set "CMAKE_ARGS=!CMAKE_ARGS! -DCMAKE_BUILD_TYPE=Debug"
)

if "%VERBOSE%"=="true" (
    set "CMAKE_ARGS=!CMAKE_ARGS! -DCMAKE_VERBOSE_MAKEFILE=ON"
)

call :print_status "Configuring with CMake..."
cmake %CMAKE_ARGS% "%PROJECT_ROOT%"

if %ERRORLEVEL% neq 0 (
    call :print_error "CMake configuration failed"
    exit /b 1
)

REM Build
set "BUILD_ARGS=--parallel"
if "%VERBOSE%"=="true" (
    set "BUILD_ARGS=!BUILD_ARGS! --verbose"
)

call :print_status "Building..."
cmake --build . %BUILD_ARGS%

if %ERRORLEVEL% neq 0 (
    call :print_error "Build failed"
    exit /b 1
)

call :print_success "Build completed successfully"
goto :eof

REM Run tests using unified test runner
:run_unified_tests
cd /d "%BUILD_DIR%"

set "TEST_ARGS="

if "%VERBOSE%"=="true" (
    set "TEST_ARGS=!TEST_ARGS! --verbose"
)

if "%PARALLEL%"=="true" (
    set "TEST_ARGS=!TEST_ARGS! --parallel --threads=%THREADS%"
)

if not "%MODULE%"=="" (
    set "TEST_ARGS=!TEST_ARGS! --module=%MODULE%"
)

if not "%CATEGORY%"=="" (
    set "TEST_ARGS=!TEST_ARGS! --category=%CATEGORY%"
)

if not "%FILTER%"=="" (
    set "TEST_ARGS=!TEST_ARGS! --filter=%FILTER%"
)

if not "%OUTPUT_FORMAT%"=="" (
    set "TEST_ARGS=!TEST_ARGS! --output-format=%OUTPUT_FORMAT%"
)

if not "%OUTPUT_FILE%"=="" (
    set "TEST_ARGS=!TEST_ARGS! --output=%OUTPUT_FILE%"
)

if not exist "run_all_tests.exe" (
    call :print_error "Unified test runner not found. Did you build the project?"
    exit /b 1
)

call :print_status "Running tests with unified test runner..."
call :print_status "Command: run_all_tests.exe!TEST_ARGS!"

run_all_tests.exe!TEST_ARGS!
if %ERRORLEVEL% neq 0 (
    call :print_error "Some tests failed!"
    exit /b 1
)

call :print_success "All tests passed!"
goto :eof

REM Run tests using CTest
:run_ctest_tests
cd /d "%BUILD_DIR%"

set "CTEST_ARGS=--output-on-failure"

if "%PARALLEL%"=="true" (
    set "CTEST_ARGS=!CTEST_ARGS! --parallel %THREADS%"
)

if not "%MODULE%"=="" (
    set "CTEST_ARGS=!CTEST_ARGS! -L %MODULE%"
)

if not "%CATEGORY%"=="" (
    set "CTEST_ARGS=!CTEST_ARGS! -L %CATEGORY%"
)

if not "%FILTER%"=="" (
    set "CTEST_ARGS=!CTEST_ARGS! -R %FILTER%"
)

REM Handle specific targets
if %TARGET_COUNT% gtr 0 (
    for /L %%i in (1,1,!TARGET_COUNT!) do (
        if "!TARGET_%%i!"=="core" (
            set "CTEST_ARGS= -L error^|log^|meta^|type^|utils!CTEST_ARGS!"
        ) else if "!TARGET_%%i!"=="io_modules" (
            set "CTEST_ARGS= -L io^|image^|serial!CTEST_ARGS!"
        ) else if "!TARGET_%%i!"=="system_modules" (
            set "CTEST_ARGS= -L system^|sysinfo!CTEST_ARGS!"
        ) else if "!TARGET_%%i!"=="network_modules" (
            set "CTEST_ARGS= -L web^|connection!CTEST_ARGS!"
        ) else (
            set "CTEST_ARGS= -L !TARGET_%%i!!CTEST_ARGS!"
        )
    )
)

call :print_status "Running tests with CTest..."
call :print_status "Command: ctest!CTEST_ARGS!"

ctest !CTEST_ARGS!
if %ERRORLEVEL% neq 0 (
    call :print_error "Some tests failed!"
    exit /b 1
)

call :print_success "All tests passed!"
goto :eof

REM Generate coverage report
:generate_coverage
if "%COVERAGE%"=="false" goto :eof

cd /d "%BUILD_DIR%"

call :print_status "Generating code coverage report..."

REM Check if lcov is available (unlikely on Windows but we try)
lcov --version >nul 2>&1
if %ERRORLEVEL% neq 0 (
    call :print_warning "lcov not found. Skipping coverage report generation."
    goto :eof
)

REM Generate coverage data
lcov --directory . --capture --output-file coverage.info
lcov --remove coverage.info '/usr/*' --output-file coverage.info
lcov --remove coverage.info '*/tests/*' --output-file coverage.info

REM Generate HTML report if genhtml is available
genhtml --version >nul 2>&1
if %ERRORLEVEL% neq 0 (
    call :print_warning "genhtml not found. HTML report not generated."
    call :print_status "Coverage data available in %BUILD_DIR%\coverage.info"
    goto :eof
)

genhtml -o coverage_html coverage.info
call :print_success "Coverage report generated in %BUILD_DIR%\coverage_html\"
goto :eof

REM Main execution
:main
call :parse_args %*

if "%HELP%"=="true" (
    call :show_help
    exit /b 0
)

REM Validate arguments
if "%PARALLEL%"=="true" if %THREADS% leq 0 (
    call :print_error "Thread count must be positive"
    exit /b 1
)

REM Change to project root
cd /d "%PROJECT_ROOT%"

REM Set default target if none specified
if %TARGET_COUNT% equ 0 (
    set /a TARGET_COUNT+=1
    set "TARGET_1=all"
)

REM Build if needed
call :check_build_needed
if %ERRORLEVEL% equ 0 (
    call :build_project
    if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
) else if "%VERBOSE%"=="true" (
    call :print_status "Build is up to date"
)

REM Run tests
if "%RUN_TESTS%"=="true" (
    if "%USE_CTEST%"=="true" (
        call :run_ctest_tests
        if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
    ) else (
        call :run_unified_tests
        if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
    )

    REM Generate coverage report if requested
    call :generate_coverage
) else (
    call :print_success "Build completed. Use --run-tests to execute tests."
)

exit /b 0

REM Execute main function
call :main %*
