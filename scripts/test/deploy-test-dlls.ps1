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
    "$BuildDir\atom\log\liblogurud.dll",
    "$BuildDir\atom\log\libatom-log.dll",
    "$BuildDir\atom\components\libatom-component.dll",
    "$BuildDir\atom\system\shortcut\libshortcut_detector.dll"
)

# Define MinGW/MSYS2 DLL locations (for GTest and runtime libraries)
# These are typically in D:\msys64\mingw64\bin but can be customized
$MinGWBinDir = "D:\msys64\mingw64\bin"
if ($env:MINGW_PREFIX) {
    $MinGWBinDir = "$env:MINGW_PREFIX\bin"
}

$MinGWDlls = @(
    "libgtest.dll",
    "libgtest_main.dll",
    "libgcc_s_seh-1.dll",
    "libwinpthread-1.dll",
    "libstdc++-6.dll"
)

# Define test directories that need DLLs (all test modules)
$TestDirectories = @(
    "$BuildDir\bin",  # Main test executable directory
    "$BuildDir\tests\algorithm",
    "$BuildDir\tests\async",
    "$BuildDir\tests\components",
    "$BuildDir\tests\connection",
    "$BuildDir\tests\error",
    "$BuildDir\tests\extra",
    "$BuildDir\tests\image",
    "$BuildDir\tests\io",
    "$BuildDir\tests\memory",
    "$BuildDir\tests\meta",
    "$BuildDir\tests\search",
    "$BuildDir\tests\secret",
    "$BuildDir\tests\serial",
    "$BuildDir\tests\sysinfo",
    "$BuildDir\tests\system",
    "$BuildDir\tests\type",
    "$BuildDir\tests\utils",
    "$BuildDir\tests\web"
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

        # Copy project DLLs
        foreach ($DllSource in $DllSources) {
            if (Copy-DllIfExists $DllSource $TestDir) {
                $TotalCopied++
            }
        }

        # Copy MinGW/GTest DLLs
        foreach ($DllName in $MinGWDlls) {
            $DllPath = Join-Path $MinGWBinDir $DllName
            if (Copy-DllIfExists $DllPath $TestDir) {
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
