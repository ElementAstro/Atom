# coverage_badge.ps1 - PowerShell wrapper for coverage_badge.py
# This project is licensed under the terms of the GPL3 license.
#
# Author: Max Qian
# License: GPL3

[CmdletBinding()]
param(
    [string]$CoverageFile = "coverage\unified\coverage.json",
    [string]$ReadmeFile = "README.md",
    [ValidateSet("markdown", "urls", "update-readme")]
    [string]$Output = "update-readme",
    [switch]$Help
)

# Set UTF-8 encoding for console output
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

function Show-Help {
    Write-Host "Coverage Badge Generator for Windows PowerShell" -ForegroundColor Cyan
    Write-Host "===============================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Usage: .\coverage_badge.ps1 [OPTIONS]" -ForegroundColor White
    Write-Host ""
    Write-Host "Parameters:" -ForegroundColor Yellow
    Write-Host "  -CoverageFile PATH    Path to coverage JSON file" -ForegroundColor White
    Write-Host "  -ReadmeFile PATH      Path to README.md file" -ForegroundColor White
    Write-Host "  -Output FORMAT        Output format: markdown, urls, update-readme" -ForegroundColor White
    Write-Host "  -Help                 Show this help message" -ForegroundColor White
    Write-Host ""
    Write-Host "Examples:" -ForegroundColor Yellow
    Write-Host "  .\coverage_badge.ps1" -ForegroundColor Green
    Write-Host "  .\coverage_badge.ps1 -Output markdown" -ForegroundColor Green
    Write-Host "  .\coverage_badge.ps1 -CoverageFile 'coverage\unified\coverage.json'" -ForegroundColor Green
    Write-Host "  .\coverage_badge.ps1 -ReadmeFile 'README.md' -Output update-readme" -ForegroundColor Green
    Write-Host ""
    Write-Host "Default paths:" -ForegroundColor Yellow
    Write-Host "  Coverage file: coverage\unified\coverage.json" -ForegroundColor White
    Write-Host "  README file:   README.md" -ForegroundColor White
    Write-Host ""
    Write-Host "For more information, see docs\COVERAGE_GUIDE.md" -ForegroundColor Cyan
}

function Test-Prerequisites {
    # Check if Python is available
    try {
        $pythonVersion = python --version 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "Python not found"
        }
        Write-Host "✅ Python found: $pythonVersion" -ForegroundColor Green
    }
    catch {
        Write-Host "❌ Python is not installed or not in PATH" -ForegroundColor Red
        Write-Host "💡 Install Python from https://python.org or Microsoft Store" -ForegroundColor Yellow
        return $false
    }

    # Check if the Python script exists
    $scriptPath = Join-Path $PSScriptRoot "coverage_badge.py"
    if (-not (Test-Path $scriptPath)) {
        Write-Host "❌ coverage_badge.py not found in $PSScriptRoot" -ForegroundColor Red
        return $false
    }
    Write-Host "✅ coverage_badge.py found" -ForegroundColor Green

    return $true
}

function Invoke-CoverageBadge {
    param(
        [string]$CoverageFile,
        [string]$ReadmeFile,
        [string]$Output
    )

    $scriptPath = Join-Path $PSScriptRoot "coverage_badge.py"
    $projectRoot = Split-Path $PSScriptRoot -Parent

    # Change to project root directory
    Push-Location $projectRoot

    try {
        # Build arguments
        $arguments = @(
            $scriptPath,
            "--coverage-file", $CoverageFile,
            "--readme-file", $ReadmeFile,
            "--output", $Output
        )

        Write-Host "🚀 Running Coverage Badge Generator..." -ForegroundColor Cyan
        Write-Host ""

        # Run the Python script
        & python @arguments

        $exitCode = $LASTEXITCODE

        Write-Host ""
        if ($exitCode -eq 0) {
            Write-Host "✅ Coverage badge generation completed successfully!" -ForegroundColor Green
        } else {
            Write-Host "❌ Coverage badge generation failed with exit code $exitCode" -ForegroundColor Red
        }

        return $exitCode
    }
    finally {
        Pop-Location
    }
}

# Main execution
if ($Help) {
    Show-Help
    exit 0
}

Write-Host "Coverage Badge Generator for Windows PowerShell" -ForegroundColor Cyan
Write-Host "Platform: $($PSVersionTable.Platform) PowerShell $($PSVersionTable.PSVersion)" -ForegroundColor Gray
Write-Host ""

# Check prerequisites
if (-not (Test-Prerequisites)) {
    Read-Host "Press Enter to continue"
    exit 1
}

# Run the coverage badge generator
$exitCode = Invoke-CoverageBadge -CoverageFile $CoverageFile -ReadmeFile $ReadmeFile -Output $Output

# Pause if running interactively
if ($Host.Name -eq "ConsoleHost" -and -not [Environment]::GetCommandLineArgs().Contains("-NonInteractive")) {
    Write-Host ""
    Read-Host "Press Enter to continue"
}

exit $exitCode
