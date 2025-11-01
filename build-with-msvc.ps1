#Requires -Version 5.1

<#
.SYNOPSIS
    Build Atom project with MSVC compiler using vcpkg dependencies
.DESCRIPTION
    This script configures and builds the Atom project using MSVC toolchain.
    It automatically fixes vcpkg Ninja RC compiler rule issues caused by MSYS2 CMake.
.PARAMETER Clean
    Remove build directory before building
.PARAMETER ConfigureOnly
    Only run CMake configuration, don't build
#>

param(
    [switch]$Clean,
    [switch]$ConfigureOnly
)

$ErrorActionPreference = "Stop"
$ProjectRoot = "D:\Project\Atom"
$VcpkgRoot = "D:\vcpkg"
$BuildDir = "$ProjectRoot\build-msvc"

# Ensure MSYS2 cmake is in PATH (needed for vcpkg)
$env:PATH = "D:\msys64\mingw64\bin;" + $env:PATH

# Function to fix Ninja RC rules
function Fix-NinjaRCRules {
    param([string]$BuildTreesPath)

    Write-Host "Fixing Ninja RC compiler rules..." -ForegroundColor Cyan
    $fixedCount = 0

    Get-ChildItem -Path $BuildTreesPath -Filter "rules.ninja" -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
        $filePath = $_.FullName
        try {
            $content = Get-Content $filePath -Raw -Encoding UTF8 -ErrorAction Stop

            # Check for broken RC rule
            if ($content -match 'command\s*=\s*RC\s+') {
                Write-Host "  Fixing: $($_.Directory.Parent.Name)/$($_.Directory.Name)" -ForegroundColor Yellow

                # Pattern to match the broken RC rule
                $pattern = 'command\s*=\s*RC\s+[^\r\n]*?"(D:\\Windows Kits[^"]+\\rc\.exe)"[^\r\n]*?/fo\s+\$out\s+\$in'

                # Replace with correct RC rule
                $replacement = 'command = ${LAUNCHER}${CODE_CHECK}"$1" $DEFINES $FLAGS $INCLUDES /fo $out $in'

                $newContent = $content -replace $pattern, $replacement

                if ($newContent -ne $content) {
                    Set-Content -Path $filePath -Value $newContent -Encoding UTF8 -NoNewline
                    $fixedCount++
                    Write-Host "    √ Fixed" -ForegroundColor Green
                }
            }
        } catch {
            Write-Warning "Failed to process $filePath : $_"
        }
    }

    if ($fixedCount -gt 0) {
        Write-Host "Fixed $fixedCount Ninja build file(s)" -ForegroundColor Green
    } else {
        Write-Host "No Ninja files needed fixing" -ForegroundColor Green
    }
}

# Clean build directory if requested
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Cyan
    Remove-Item -Recurse -Force $BuildDir
}

# Create build directory
if (!(Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

Write-Host "`n==================================" -ForegroundColor Cyan
Write-Host "Configuring Atom with MSVC + vcpkg" -ForegroundColor Cyan
Write-Host "==================================`n" -ForegroundColor Cyan

# Start background job to monitor and fix Ninja files during vcpkg installation
$fixerJob = Start-Job -ScriptBlock {
    param($vcpkgRoot)
    while ($true) {
        Start-Sleep -Seconds 2
        Get-ChildItem -Path "$vcpkgRoot\buildtrees" -Filter "rules.ninja" -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
            $filePath = $_.FullName
            $content = Get-Content $filePath -Raw -Encoding UTF8 -ErrorAction SilentlyContinue

            if ($content -and ($content -match 'command\s*=\s*RC\s+.*?包含文件')) {
                $pattern = 'command\s*=\s*RC\s+[^\r\n]*?"(D:\\Windows Kits[^"]+\\rc\.exe)"[^\r\n]*?/fo\s+\$out\s+\$in'
                $replacement = 'command = ${LAUNCHER}${CODE_CHECK}"$1" $DEFINES $FLAGS $INCLUDES /fo $out $in'
                $newContent = $content -replace $pattern, $replacement

                if ($newContent -ne $content) {
                    Set-Content -Path $filePath -Value $newContent -Encoding UTF8 -NoNewline -ErrorAction SilentlyContinue
                }
            }
        }
    }
} -ArgumentList $VcpkgRoot

try {
    # Configure with CMake
    $cmakeArgs = @(
        "-B", $BuildDir,
        "-S", $ProjectRoot,
        "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot\scripts\buildsystems\vcpkg.cmake",
        "-DUSE_VCPKG=ON",
        "-DCMAKE_BUILD_TYPE=Release",
        "-G", "Visual Studio 17 2022",
        "-A", "x64"
    )

    Write-Host "Running: cmake $($cmakeArgs -join ' ')`n" -ForegroundColor Gray

    & cmake $cmakeArgs

    if ($LASTEXITCODE -ne 0) {
        Write-Host "`nConfiguration failed. Attempting to fix Ninja RC rules and retry..." -ForegroundColor Yellow
        Fix-NinjaRCRules -BuildTreesPath "$VcpkgRoot\buildtrees"

        Write-Host "`nRetrying configuration..." -ForegroundColor Cyan
        & cmake $cmakeArgs

        if ($LASTEXITCODE -ne 0) {
            throw "CMake configuration failed with exit code $LASTEXITCODE"
        }
    }

    Write-Host "`n✓ Configuration successful!" -ForegroundColor Green

    if (!$ConfigureOnly) {
        Write-Host "`n==================================" -ForegroundColor Cyan
        Write-Host "Building Atom" -ForegroundColor Cyan
        Write-Host "==================================`n" -ForegroundColor Cyan

        & cmake --build $BuildDir --config Release --parallel

        if ($LASTEXITCODE -ne 0) {
            throw "Build failed with exit code $LASTEXITCODE"
        }

        Write-Host "`n✓ Build successful!" -ForegroundColor Green
    }

} finally {
    # Stop the background fixer job
    if ($fixerJob) {
        Stop-Job -Job $fixerJob
        Remove-Job -Job $fixerJob
    }
}

Write-Host "`n==================================" -ForegroundColor Cyan
Write-Host "Build process completed!" -ForegroundColor Cyan
Write-Host "==================================`n" -ForegroundColor Cyan
Write-Host "Build directory: $BuildDir" -ForegroundColor Gray
