#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Enhanced OCR System - Windows Dependency Installer
.DESCRIPTION
    Installs all required dependencies for the Enhanced OCR system on Windows.
    Supports Visual Studio, vcpkg, Chocolatey, and MSYS2/MinGW64 environments.
.PARAMETER ModelsOnly
    Only download models without installing system dependencies
.PARAMETER UseVcpkg
    Use vcpkg for dependency management (default)
.PARAMETER UseMSYS2
    Use MSYS2/MinGW64 for dependency management
.PARAMETER VcpkgRoot
    Custom vcpkg installation directory
.PARAMETER MSYS2Root
    Custom MSYS2 installation directory
.EXAMPLE
    .\Install-OCRDependencies.ps1
.EXAMPLE
    .\Install-OCRDependencies.ps1 -ModelsOnly
.EXAMPLE
    .\Install-OCRDependencies.ps1 -UseMSYS2 -MSYS2Root "C:\msys64"
#>

[CmdletBinding()]
param(
    [switch]$ModelsOnly,
    [switch]$UseVcpkg,
    [switch]$UseMSYS2,
    [string]$VcpkgRoot = "C:\vcpkg",
    [string]$MSYS2Root = "C:\msys64"
)

# Configuration
$script:MODELS_DIR = ".\models"
$script:CACHE_DIR = ".\.ocr_cache"
$script:LOG_DIR = ".\logs"
$script:DICT_DIR = ".\dict"

# Model URLs and checksums
$script:Models = @{
    "east_text_detection.pb" = @{
        Url  = "https://github.com/oyyd/frozen_east_text_detection.pb/raw/master/frozen_east_text_detection.pb"
        Size = 96500000  # Approximate size in bytes
    }
    "ESPCN_x4.pb"            = @{
        Url  = "https://github.com/fannymonori/TF-ESPCN/raw/master/export/ESPCN_x4.pb"
        Size = 200000
    }
}

$script:DictionaryUrl = "https://raw.githubusercontent.com/dwyl/english-words/master/words.txt"

# Logging functions
function Write-Log {
    param([string]$Message, [string]$Level = "INFO")
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $color = switch ($Level) {
        "INFO" { "Cyan" }
        "SUCCESS" { "Green" }
        "WARNING" { "Yellow" }
        "ERROR" { "Red" }
        default { "White" }
    }
    Write-Host "[$timestamp] ${Level}: $Message" -ForegroundColor $color
}

function Write-Success { param([string]$Message) Write-Log $Message "SUCCESS" }
function Write-Warning { param([string]$Message) Write-Log $Message "WARNING" }
function Write-Error { param([string]$Message) Write-Log $Message "ERROR" }

# Create necessary directories
function Initialize-Directories {
    Write-Log "Creating necessary directories..."

    $directories = @($script:MODELS_DIR, $script:CACHE_DIR, $script:LOG_DIR, $script:DICT_DIR)
    foreach ($dir in $directories) {
        if (-not (Test-Path $dir)) {
            New-Item -ItemType Directory -Force -Path $dir | Out-Null
        }
    }

    Write-Success "Directories created successfully"
}

# Download file with retry and progress
function Get-FileWithRetry {
    param(
        [string]$Url,
        [string]$OutFile,
        [int]$MaxRetries = 3
    )

    $attempt = 0
    while ($attempt -lt $MaxRetries) {
        try {
            $attempt++
            Write-Log "Downloading: $Url (Attempt $attempt/$MaxRetries)"

            # Use WebClient for better progress reporting
            $webClient = New-Object System.Net.WebClient
            $webClient.DownloadFile($Url, $OutFile)

            if (Test-Path $OutFile) {
                $fileSize = (Get-Item $OutFile).Length
                Write-Log "Downloaded: $OutFile ($fileSize bytes)"
                return $true
            }
        }
        catch {
            Write-Warning "Download failed: $_"
            if ($attempt -lt $MaxRetries) {
                Write-Log "Retrying in 5 seconds..."
                Start-Sleep -Seconds 5
            }
        }
    }

    return $false
}

# Download OCR models
function Install-Models {
    Write-Log "Downloading OCR models and resources..."

    # Download detection and super-resolution models
    foreach ($modelName in $script:Models.Keys) {
        $modelInfo = $script:Models[$modelName]
        $outFile = Join-Path $script:MODELS_DIR $modelName

        if (Test-Path $outFile) {
            Write-Log "Model already exists: $modelName"
            continue
        }

        Write-Log "Downloading $modelName..."
        if (-not (Get-FileWithRetry -Url $modelInfo.Url -OutFile $outFile)) {
            Write-Error "Failed to download $modelName"
        }
    }

    # Download English dictionary
    $dictFile = Join-Path $script:DICT_DIR "english.txt"
    if (-not (Test-Path $dictFile)) {
        Write-Log "Downloading English dictionary..."
        if (-not (Get-FileWithRetry -Url $script:DictionaryUrl -OutFile $dictFile)) {
            Write-Error "Failed to download dictionary"
        }
    }

    # Verify downloads
    $allModelsPresent = $true
    foreach ($modelName in $script:Models.Keys) {
        $modelPath = Join-Path $script:MODELS_DIR $modelName
        if (-not (Test-Path $modelPath)) {
            Write-Error "Missing model: $modelName"
            $allModelsPresent = $false
        }
    }

    if ($allModelsPresent) {
        Write-Success "All models downloaded successfully"
    }
}

# Install Chocolatey
function Install-Chocolatey {
    if (Get-Command choco -ErrorAction SilentlyContinue) {
        Write-Log "Chocolatey is already installed"
        return
    }

    Write-Log "Installing Chocolatey..."
    Set-ExecutionPolicy Bypass -Scope Process -Force
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
    Invoke-Expression ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))

    # Refresh environment
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")

    Write-Success "Chocolatey installed successfully"
}

# Install vcpkg
function Install-Vcpkg {
    if (Test-Path (Join-Path $VcpkgRoot "vcpkg.exe")) {
        Write-Log "vcpkg is already installed at $VcpkgRoot"

        # Update vcpkg
        Push-Location $VcpkgRoot
        git pull 2>$null
        & ".\bootstrap-vcpkg.bat" -disableMetrics 2>$null
        Pop-Location
        return
    }

    Write-Log "Installing vcpkg to $VcpkgRoot..."

    # Clone vcpkg repository
    git clone https://github.com/Microsoft/vcpkg.git $VcpkgRoot

    # Bootstrap vcpkg
    Push-Location $VcpkgRoot
    & ".\bootstrap-vcpkg.bat" -disableMetrics
    Pop-Location

    # Add to PATH
    $env:Path += ";$VcpkgRoot"
    [Environment]::SetEnvironmentVariable("Path", $env:Path, [EnvironmentVariableTarget]::User)

    # Integrate with Visual Studio
    & "$VcpkgRoot\vcpkg.exe" integrate install

    Write-Success "vcpkg installed successfully"
}

# Install dependencies via vcpkg
function Install-VcpkgDependencies {
    Write-Log "Installing dependencies via vcpkg..."

    $vcpkgExe = Join-Path $VcpkgRoot "vcpkg.exe"
    if (-not (Test-Path $vcpkgExe)) {
        Write-Error "vcpkg not found. Please install vcpkg first."
        return
    }

    $packages = @(
        "opencv4[contrib,dnn]:x64-windows",
        "tesseract:x64-windows",
        "leptonica:x64-windows",
        "nlohmann-json:x64-windows"
    )

    foreach ($package in $packages) {
        Write-Log "Installing $package..."
        & $vcpkgExe install $package
    }

    Write-Success "vcpkg dependencies installed successfully"
}

# Install MSYS2
function Install-MSYS2 {
    if (Test-Path (Join-Path $MSYS2Root "msys2.exe")) {
        Write-Log "MSYS2 is already installed at $MSYS2Root"
        return
    }

    Write-Log "Installing MSYS2..."

    # Download MSYS2 installer
    $installerUrl = "https://github.com/msys2/msys2-installer/releases/download/2024-01-13/msys2-x86_64-20240113.exe"
    $installerPath = "$env:TEMP\msys2-installer.exe"

    if (-not (Get-FileWithRetry -Url $installerUrl -OutFile $installerPath)) {
        Write-Error "Failed to download MSYS2 installer"
        return
    }

    # Run installer silently
    Start-Process -FilePath $installerPath -ArgumentList "install", "--root", $MSYS2Root, "--confirm-command" -Wait

    Write-Success "MSYS2 installed successfully"
}

# Install dependencies via MSYS2/pacman
function Install-MSYS2Dependencies {
    Write-Log "Installing dependencies via MSYS2/pacman..."

    $pacman = Join-Path $MSYS2Root "usr\bin\pacman.exe"
    if (-not (Test-Path $pacman)) {
        Write-Error "pacman not found. Please install MSYS2 first."
        return
    }

    # Update package database
    & $pacman -Syu --noconfirm

    # Install MinGW64 toolchain and dependencies
    $packages = @(
        "mingw-w64-x86_64-gcc",
        "mingw-w64-x86_64-cmake",
        "mingw-w64-x86_64-make",
        "mingw-w64-x86_64-opencv",
        "mingw-w64-x86_64-tesseract-ocr",
        "mingw-w64-x86_64-leptonica",
        "mingw-w64-x86_64-nlohmann-json"
    )

    foreach ($package in $packages) {
        Write-Log "Installing $package..."
        & $pacman -S --noconfirm $package
    }

    Write-Success "MSYS2 dependencies installed successfully"
}

# Install Visual Studio Build Tools
function Install-BuildTools {
    if (Get-Command cl -ErrorAction SilentlyContinue) {
        Write-Log "Visual Studio Build Tools are already installed"
        return
    }

    Write-Log "Installing Visual Studio Build Tools..."
    choco install visualstudio2022buildtools -y
    choco install visualstudio2022-workload-vctools -y

    Write-Success "Visual Studio Build Tools installed"
}

# Install additional tools
function Install-CommonTools {
    Write-Log "Installing common tools..."

    # Install Git if not present
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        choco install git -y
    }

    # Install CMake if not present
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        choco install cmake --installargs 'ADD_CMAKE_TO_PATH=System' -y
    }

    # Refresh environment
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")

    Write-Success "Common tools installed"
}

# Create OCR configuration file
function New-OCRConfig {
    Write-Log "Creating OCR configuration file..."

    $config = @{
        language                    = "eng"
        enableDeskew                = $true
        enablePerspectiveCorrection = $true
        enableNoiseRemoval          = $true
        enableTextDetection         = $true
        enableSpellCheck            = $true
        enableSuperResolution       = $false
        cacheResults                = $true
        maxThreads                  = [Environment]::ProcessorCount
        preprocessing               = @{
            applyGaussianBlur    = $true
            gaussianKernelSize   = 3
            applyThreshold       = $true
            useAdaptiveThreshold = $true
            blockSize            = 11
            constantC            = 2
            medianBlurSize       = 3
            applyClahe           = $false
            clipLimit            = 2.0
            binarizationMethod   = 0
        }
        superResolution             = @{
            modelPath = "models/ESPCN_x4.pb"
            modelName = "espcn"
            scale     = 4
        }
        textDetection               = @{
            confThreshold = 0.5
            nmsThreshold  = 0.4
            detectionSize = 320
            modelPath     = "models/east_text_detection.pb"
        }
        cache                       = @{
            maxCacheSize = 104857600
            cacheDir     = ".ocr_cache"
        }
    }

    $config | ConvertTo-Json -Depth 4 | Set-Content -Path "ocr_config.json" -Encoding UTF8

    Write-Success "Configuration file created: ocr_config.json"
}

# Create build script
function New-BuildScript {
    Write-Log "Creating build script..."

    if ($UseMSYS2) {
        $buildScript = @"
@echo off
REM Build Enhanced OCR with MSYS2/MinGW64

set MSYS2_ROOT=$MSYS2Root
set PATH=%MSYS2_ROOT%\mingw64\bin;%PATH%

if not exist build mkdir build
cd build

cmake .. -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DATOM_IMAGE_HAS_OCR=ON

mingw32-make -j%NUMBER_OF_PROCESSORS%

cd ..
echo Build completed.
"@
    }
    else {
        $buildScript = @"
@echo off
REM Build Enhanced OCR with vcpkg

if not exist build mkdir build
cd build

cmake .. -DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot\scripts\buildsystems\vcpkg.cmake ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DATOM_IMAGE_HAS_OCR=ON

cmake --build . --config Release

cd ..
echo Build completed.
"@
    }

    Set-Content -Path "build_ocr.bat" -Value $buildScript -Encoding ASCII

    Write-Success "Build script created: build_ocr.bat"
}

# Main installation function
function Main {
    Write-Log "Starting OCR dependencies installation for Windows..."
    Write-Log "Mode: $(if ($UseMSYS2) { 'MSYS2/MinGW64' } else { 'vcpkg/MSVC' })"

    # Initialize directories
    Initialize-Directories

    # Models only mode
    if ($ModelsOnly) {
        Install-Models
        Write-Success "Models installation completed!"
        return
    }

    # Install Chocolatey for common tools
    Install-Chocolatey

    # Install common tools
    Install-CommonTools

    if ($UseMSYS2) {
        # MSYS2/MinGW64 path
        Install-MSYS2
        Install-MSYS2Dependencies
    }
    else {
        # vcpkg/MSVC path (default)
        Install-BuildTools
        Install-Vcpkg
        Install-VcpkgDependencies
    }

    # Download models
    Install-Models

    # Create configuration
    New-OCRConfig

    # Create build script
    New-BuildScript

    Write-Success "Installation completed successfully!"
    Write-Log "You can now build the Enhanced OCR system using the generated build_ocr.bat script."
}

# Check for admin privileges
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Error "This script requires administrator privileges. Please run as administrator."
    exit 1
}

# Run main function
Main
