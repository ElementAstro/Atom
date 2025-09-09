# PowerShell script to deploy DLLs to test directories
# This script copies required DLL files to test executable directories

param(
    [string]$BuildDir = "build",
    [switch]$Verbose
)

Write-Host "Deploying DLLs to test directories..." -ForegroundColor Green

# Define source DLL locations
$DllSources = @(
    "$BuildDir\atom\error\libatom-error.dll",
    "$BuildDir\atom\log\libloguru.dll",
    "$BuildDir\atom\log\liblogurud.dll"
)

# Define test directories that need DLLs
$TestDirectories = @(
    "$BuildDir\tests\memory",
    "$BuildDir\tests\search",
    "$BuildDir\tests\secret",
    "$BuildDir\tests\type",
    "$BuildDir\tests\extra",
    "$BuildDir\tests\meta"
)

# Function to copy DLL if it exists
function Copy-DllIfExists {
    param(
        [string]$SourcePath,
        [string]$DestinationDir
    )
    
    if (Test-Path $SourcePath) {
        $FileName = Split-Path $SourcePath -Leaf
        $DestPath = Join-Path $DestinationDir $FileName
        
        try {
            Copy-Item $SourcePath $DestPath -Force
            if ($Verbose) {
                Write-Host "  Copied: $FileName -> $DestinationDir" -ForegroundColor Cyan
            }
            return $true
        }
        catch {
            Write-Warning "Failed to copy $FileName to $DestinationDir : $_"
            return $false
        }
    }
    else {
        if ($Verbose) {
            Write-Warning "Source DLL not found: $SourcePath"
        }
        return $false
    }
}

# Deploy DLLs to each test directory
$TotalCopied = 0
foreach ($TestDir in $TestDirectories) {
    if (Test-Path $TestDir) {
        Write-Host "Deploying to: $TestDir" -ForegroundColor Yellow
        
        foreach ($DllSource in $DllSources) {
            if (Copy-DllIfExists $DllSource $TestDir) {
                $TotalCopied++
            }
        }
    }
    else {
        if ($Verbose) {
            Write-Warning "Test directory not found: $TestDir"
        }
    }
}

Write-Host "DLL deployment completed. Total files copied: $TotalCopied" -ForegroundColor Green

# Verify deployment by checking if key DLLs exist in test directories
Write-Host "Verifying deployment..." -ForegroundColor Yellow
$VerificationPassed = $true

foreach ($TestDir in $TestDirectories) {
    if (Test-Path $TestDir) {
        $KeyDll = Join-Path $TestDir "libatom-error.dll"
        if (-not (Test-Path $KeyDll)) {
            Write-Warning "Verification failed: libatom-error.dll not found in $TestDir"
            $VerificationPassed = $false
        }
    }
}

if ($VerificationPassed) {
    Write-Host "✓ DLL deployment verification passed!" -ForegroundColor Green
    exit 0
}
else {
    Write-Host "✗ DLL deployment verification failed!" -ForegroundColor Red
    exit 1
}
