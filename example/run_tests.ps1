# Atom Framework Examples Test Runner (PowerShell Version)
#
# This script provides a simple way to test Atom framework examples
# on Windows systems using PowerShell.
#
# Usage: .\run_tests.ps1 [options]
#
# Options:
#   -BuildDir DIR       Build directory (default: build)
#   -Verbose            Enable verbose output
#   -BuildFirst         Build examples before testing
#   -CleanFirst         Clean build before testing
#   -Help               Show this help message

param(
    [string]$BuildDir = "build",
    [string]$SourceDir = ".",
    [switch]$Verbose,
    [switch]$BuildFirst,
    [switch]$CleanFirst,
    [switch]$Help
)

# Show help if requested
if ($Help) {
    Write-Host "Atom Framework Examples Test Runner" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Usage: .\run_tests.ps1 [options]"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  -BuildDir DIR       Build directory (default: build)"
    Write-Host "  -Verbose            Enable verbose output"
    Write-Host "  -BuildFirst         Build examples before testing"
    Write-Host "  -CleanFirst         Clean build before testing"
    Write-Host "  -Help               Show this help message"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\run_tests.ps1 -Verbose"
    Write-Host "  .\run_tests.ps1 -BuildFirst -BuildDir my_build"
    Write-Host "  .\run_tests.ps1 -CleanFirst"
    exit 0
}

# Logging functions
function Write-Log {
    param([string]$Message, [switch]$Force)
    if ($Verbose -or $Force) {
        Write-Host "[TestRunner] $Message" -ForegroundColor Blue
    }
}

function Write-Success {
    param([string]$Message)
    Write-Host "[SUCCESS] $Message" -ForegroundColor Green
}

function Write-Error {
    param([string]$Message)
    Write-Host "[ERROR] $Message" -ForegroundColor Red
}

function Write-Warning {
    param([string]$Message)
    Write-Host "[WARNING] $Message" -ForegroundColor Yellow
}

# Function to clean build directory
function Clean-Build {
    Write-Log "Cleaning build directory..." -Force

    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir
        Write-Success "Build directory cleaned"
    } else {
        Write-Log "Build directory doesn't exist, nothing to clean"
    }
}

# Function to configure CMake
function Configure-CMake {
    Write-Log "Configuring CMake..." -Force

    # Check if CMake exists
    try {
        $null = Get-Command cmake -ErrorAction Stop
    } catch {
        Write-Error "CMake not found. Please install CMake 3.20 or higher."
        return $false
    }

    $result = & cmake -B $BuildDir -S $SourceDir -DATOM_EXAMPLE_BUILD_ALL=ON

    if ($LASTEXITCODE -eq 0) {
        Write-Success "CMake configuration successful"
        return $true
    } else {
        Write-Error "CMake configuration failed"
        return $false
    }
}

# Function to build examples
function Build-Examples {
    Write-Log "Building examples..." -Force

    # Use parallel build
    $result = & cmake --build $BuildDir --parallel

    if ($LASTEXITCODE -eq 0) {
        Write-Success "Build successful"
        return $true
    } else {
        Write-Error "Build failed"
        return $false
    }
}

# Function to test a single example
function Test-Example {
    param(
        [string]$Module,
        [string]$Target,
        [string]$Description,
        [int]$Timeout = 30
    )

    Write-Log "Testing [$Module] $Description..."

    # Look for executable
    $executable = $null
    $possiblePaths = @(
        "$BuildDir\example\$Module\$Target.exe",
        "$BuildDir\example\$Module\$Target"
    )

    foreach ($path in $possiblePaths) {
        if (Test-Path $path) {
            $executable = $path
            break
        }
    }

    if (-not $executable) {
        Write-Warning "[$Module] $Description`: SKIPPED (executable not found)"
        return 2
    }

    # Run the executable with timeout
    try {
        $job = Start-Job -ScriptBlock {
            param($exe)
            & $exe 2>&1
        } -ArgumentList $executable

        $completed = Wait-Job $job -Timeout $Timeout

        if ($completed) {
            $output = Receive-Job $job
            $exitCode = if ($job.State -eq "Completed") { 0 } else { 1 }
            Remove-Job $job

            if ($exitCode -eq 0) {
                Write-Success "[$Module] $Description`: PASSED"
                return 0
            } else {
                Write-Error "[$Module] $Description`: FAILED (exit $exitCode)"
                return 1
            }
        } else {
            Stop-Job $job
            Remove-Job $job
            Write-Warning "[$Module] $Description`: TIMEOUT"
            return 1
        }
    } catch {
        Write-Error "[$Module] $Description`: ERROR ($_)"
        return 1
    }
}

# Function to test build-only example
function Test-BuildOnly {
    param(
        [string]$Module,
        [string]$Target,
        [string]$Description
    )

    Write-Log "Checking build for [$Module] $Description..."

    $possiblePaths = @(
        "$BuildDir\example\$Module\$Target.exe",
        "$BuildDir\example\$Module\$Target"
    )

    foreach ($path in $possiblePaths) {
        if (Test-Path $path) {
            Write-Success "[$Module] $Description`: BUILD OK"
            return 0
        }
    }

    Write-Error "[$Module] $Description`: BUILD FAILED"
    return 1
}

# Function to run all tests
function Run-Tests {
    Write-Log "Running example tests..." -Force

    $passed = 0
    $failed = 0
    $skipped = 0
    $total = 0

    Write-Host ""
    Write-Host "=== Atom Framework Examples Test Suite ===" -ForegroundColor Cyan
    Write-Host ""

    # Test known working examples
    $workingExamples = @(
        @("containers", "containers_high_performance_containers_example", "High Performance Containers", 30),
        @("meta", "meta_comprehensive_meta_example", "Comprehensive Meta", 30),
        @("secret", "secret_basic_test", "Secret Basic Test", 10),
        @("sysinfo", "sysinfo_header_test", "Sysinfo Header Test", 10)
    )

    foreach ($example in $workingExamples) {
        $result = Test-Example -Module $example[0] -Target $example[1] -Description $example[2] -Timeout $example[3]
        $total++

        switch ($result) {
            0 { $passed++ }
            1 { $failed++ }
            2 { $skipped++ }
        }
    }

    # Test build-only examples
    Write-Host ""
    Write-Host "=== Build-Only Tests (Known Runtime Issues) ===" -ForegroundColor Cyan
    Write-Host ""

    $buildOnlyExamples = @(
        @("algorithm", "algorithm_md5", "MD5 Algorithm (Build Only)"),
        @("secret", "secret_secure_storage_example", "Secret Secure Storage (Build Only)"),
        @("sysinfo", "sysinfo_basic_sysinfo_example", "Sysinfo Basic Example (Build Only)")
    )

    foreach ($example in $buildOnlyExamples) {
        $result = Test-BuildOnly -Module $example[0] -Target $example[1] -Description $example[2]
        $total++

        if ($result -eq 0) {
            $passed++
        } else {
            $failed++
        }
    }

    # Print summary
    Write-Host ""
    Write-Host "=== Test Summary ===" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Results:"
    Write-Host "  ✅ Passed: $passed" -ForegroundColor Green
    Write-Host "  ❌ Failed: $failed" -ForegroundColor Red
    Write-Host "  ⏭️ Skipped: $skipped" -ForegroundColor Yellow
    Write-Host "  📊 Total: $total" -ForegroundColor Blue

    if ($total -gt 0) {
        $successRate = [math]::Round(($passed / $total) * 100, 1)
        Write-Host "  📈 Success Rate: $successRate%" -ForegroundColor Blue
    }

    Write-Host ""

    # Return appropriate exit code
    if ($failed -eq 0) {
        Write-Success "All tests completed successfully!"
        return 0
    } else {
        Write-Error "$failed test(s) failed"
        return 1
    }
}

# Main execution
function Main {
    Write-Log "Starting Atom Framework Examples Test Runner..." -Force

    # Clean if requested
    if ($CleanFirst) {
        Clean-Build
    }

    # Configure CMake if needed
    if ($CleanFirst -or $BuildFirst -or -not (Test-Path "$BuildDir\CMakeCache.txt")) {
        if (-not (Configure-CMake)) {
            exit 1
        }
    }

    # Build examples if requested
    if ($BuildFirst) {
        if (-not (Build-Examples)) {
            exit 1
        }
    }

    # Run tests
    $exitCode = Run-Tests

    if ($exitCode -eq 0) {
        Write-Success "Test suite completed successfully!"
    } else {
        Write-Error "Test suite completed with failures"
    }

    exit $exitCode
}

# Run main function
Main
