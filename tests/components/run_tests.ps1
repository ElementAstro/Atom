# PowerShell script for running Atom Components tests
param(
    [string]$Filter = "*",
    [switch]$Verbose,
    [switch]$Coverage,
    [switch]$Performance,
    [switch]$Clean,
    [string]$Output = "console"
)

$ErrorActionPreference = "Stop"

Write-Host "Atom Components Test Runner" -ForegroundColor Green
Write-Host "===========================" -ForegroundColor Green

# Clean build directory if requested
if ($Clean) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    if (Test-Path "build") {
        Remove-Item -Recurse -Force "build"
    }
}

# Create build directory
if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

# Configure build
Write-Host "Configuring build..." -ForegroundColor Yellow
$cmakeArgs = @("-B", "build", "-S", ".")

if ($Coverage) {
    $cmakeArgs += @("-DCMAKE_BUILD_TYPE=Debug", "-DENABLE_COVERAGE=ON")
} else {
    $cmakeArgs += @("-DCMAKE_BUILD_TYPE=Release")
}

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed"
    exit 1
}

# Build tests
Write-Host "Building tests..." -ForegroundColor Yellow
& cmake --build build
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed"
    exit 1
}

# Prepare test arguments
$testArgs = @()

if ($Filter -ne "*") {
    $testArgs += "--gtest_filter=$Filter"
}

if ($Verbose) {
    $testArgs += "--gtest_verbose"
}

if ($Output -eq "xml") {
    $testArgs += "--gtest_output=xml:test_results.xml"
} elseif ($Output -eq "json") {
    $testArgs += "--gtest_output=json:test_results.json"
}

if ($Performance) {
    $testArgs += "--gtest_filter=*Performance*:*Benchmark*"
}

# Run tests
Write-Host "Running tests..." -ForegroundColor Yellow
Write-Host "Test arguments: $($testArgs -join ' ')" -ForegroundColor Gray

$testExecutable = ".\build\atom_iocomponent.test.exe"
if (Test-Path $testExecutable) {
    & $testExecutable @testArgs
    $testResult = $LASTEXITCODE
} else {
    Write-Error "Test executable not found: $testExecutable"
    exit 1
}

# Generate coverage report if requested
if ($Coverage -and $testResult -eq 0) {
    Write-Host "Generating coverage report..." -ForegroundColor Yellow

    # Check if coverage tools are available
    $gcovAvailable = Get-Command gcov -ErrorAction SilentlyContinue
    $lcovAvailable = Get-Command lcov -ErrorAction SilentlyContinue

    if ($gcovAvailable -and $lcovAvailable) {
        # Generate coverage data
        & gcov build/CMakeFiles/atom_iocomponent.test.dir/*.gcno

        # Create coverage report
        & lcov --capture --directory . --output-file coverage.info
        & lcov --remove coverage.info '/usr/*' --output-file coverage.info
        & lcov --remove coverage.info '*/gtest/*' --output-file coverage.info

        # Generate HTML report
        if (Get-Command genhtml -ErrorAction SilentlyContinue) {
            & genhtml coverage.info --output-directory coverage_html
            Write-Host "Coverage report generated in coverage_html/" -ForegroundColor Green
        }

        # Display coverage summary
        & lcov --list coverage.info
    } else {
        Write-Warning "Coverage tools (gcov, lcov) not found. Install them to generate coverage reports."
    }
}

# Display results
Write-Host ""
if ($testResult -eq 0) {
    Write-Host "All tests passed!" -ForegroundColor Green
} else {
    Write-Host "Some tests failed!" -ForegroundColor Red
}

# Display usage examples
if ($Filter -eq "*" -and -not $Verbose -and -not $Coverage -and -not $Performance) {
    Write-Host ""
    Write-Host "Usage examples:" -ForegroundColor Cyan
    Write-Host "  .\run_tests.ps1                          # Run all tests"
    Write-Host "  .\run_tests.ps1 -Filter 'ComponentPool*' # Run specific test suite"
    Write-Host "  .\run_tests.ps1 -Verbose                 # Run with verbose output"
    Write-Host "  .\run_tests.ps1 -Coverage                # Run with coverage analysis"
    Write-Host "  .\run_tests.ps1 -Performance             # Run only performance tests"
    Write-Host "  .\run_tests.ps1 -Clean                   # Clean build before running"
    Write-Host "  .\run_tests.ps1 -Output xml              # Output results to XML"
}

exit $testResult
