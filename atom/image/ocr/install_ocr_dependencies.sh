#!/usr/bin/env bash
# Enhanced OCR System - Multi-platform Dependency Installer
# Usage: ./install_ocr_dependencies.sh [--models-only]

set -e  # Exit on error

# Configuration
MODELS_DIR="./models"
CACHE_DIR="./.ocr_cache"
LOG_DIR="./logs"
DICT_DIR="./dict"

# Text colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Log message with timestamp
log() {
    echo -e "[$(date '+%Y-%m-%d %H:%M:%S')] ${BLUE}INFO:${NC} $1"
}

# Warning message
warn() {
    echo -e "[$(date '+%Y-%m-%d %H:%M:%S')] ${YELLOW}WARNING:${NC} $1"
}

# Error message
error() {
    echo -e "[$(date '+%Y-%m-%d %H:%M:%S')] ${RED}ERROR:${NC} $1"
}

# Success message
success() {
    echo -e "[$(date '+%Y-%m-%d %H:%M:%S')] ${GREEN}SUCCESS:${NC} $1"
}

# Detect operating system
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        # Detect Linux distribution
        if command -v apt-get &> /dev/null; then
            OS="debian"
        elif command -v dnf &> /dev/null; then
            OS="fedora"
        elif command -v yum &> /dev/null; then
            OS="rhel"
        elif command -v pacman &> /dev/null; then
            OS="arch"
        elif command -v zypper &> /dev/null; then
            OS="suse"
        else
            OS="linux-unknown"
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        OS="macos"
    elif [[ "$OSTYPE" == "cygwin" ]] || [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
        OS="windows"
    else
        OS="unknown"
    fi

    log "Detected operating system: $OS"
}

# Create necessary directories
create_directories() {
    log "Creating necessary directories..."
    mkdir -p "$MODELS_DIR" "$CACHE_DIR" "$LOG_DIR" "$DICT_DIR"
    success "Directories created"
}

# Download models
download_models() {
    log "Downloading OCR models and resources..."

    # Create models directory if it doesn't exist
    mkdir -p "$MODELS_DIR"

    # Download EAST text detection model
    log "Downloading EAST text detection model..."
    if command -v wget &> /dev/null; then
        wget -O "$MODELS_DIR/east_text_detection.pb" \
             "https://github.com/oyyd/frozen_east_text_detection.pb/raw/master/frozen_east_text_detection.pb" || {
            error "Failed to download EAST model with wget. Trying with curl..."
            if command -v curl &> /dev/null; then
                curl -L -o "$MODELS_DIR/east_text_detection.pb" \
                     "https://github.com/oyyd/frozen_east_text_detection.pb/raw/master/frozen_east_text_detection.pb"
            else
                error "Both wget and curl failed. Please download manually."
            fi
        }
    elif command -v curl &> /dev/null; then
        curl -L -o "$MODELS_DIR/east_text_detection.pb" \
             "https://github.com/oyyd/frozen_east_text_detection.pb/raw/master/frozen_east_text_detection.pb"
    else
        error "Neither wget nor curl is installed. Please install one of them or download the file manually."
        error "Download URL: https://github.com/oyyd/frozen_east_text_detection.pb/raw/master/frozen_east_text_detection.pb"
        error "Save to: $MODELS_DIR/east_text_detection.pb"
    fi

    # Download super resolution model
    log "Downloading ESPCN super resolution model..."
    if command -v wget &> /dev/null; then
        wget -O "$MODELS_DIR/ESPCN_x4.pb" \
             "https://github.com/fannymonori/TF-ESPCN/raw/master/export/ESPCN_x4.pb" || {
            error "Failed to download ESPCN model with wget. Trying with curl..."
            if command -v curl &> /dev/null; then
                curl -L -o "$MODELS_DIR/ESPCN_x4.pb" \
                     "https://github.com/fannymonori/TF-ESPCN/raw/master/export/ESPCN_x4.pb"
            else
                error "Both wget and curl failed. Please download manually."
            fi
        }
    elif command -v curl &> /dev/null; then
        curl -L -o "$MODELS_DIR/ESPCN_x4.pb" \
             "https://github.com/fannymonori/TF-ESPCN/raw/master/export/ESPCN_x4.pb"
    else
        error "Neither wget nor curl is installed. Please install one of them or download the file manually."
        error "Download URL: https://github.com/fannymonori/TF-ESPCN/raw/master/export/ESPCN_x4.pb"
        error "Save to: $MODELS_DIR/ESPCN_x4.pb"
    fi

    # Download English dictionary for spell checking
    log "Downloading English dictionary for spell checking..."
    if command -v wget &> /dev/null; then
        wget -O "$DICT_DIR/english.txt.gz" \
             "https://raw.githubusercontent.com/dwyl/english-words/master/words.txt" || {
            error "Failed to download dictionary with wget. Trying with curl..."
            if command -v curl &> /dev/null; then
                curl -L -o "$DICT_DIR/english.txt" \
                     "https://raw.githubusercontent.com/dwyl/english-words/master/words.txt"
            else
                error "Both wget and curl failed. Please download manually."
            fi
        }
    elif command -v curl &> /dev/null; then
        curl -L -o "$DICT_DIR/english.txt" \
             "https://raw.githubusercontent.com/dwyl/english-words/master/words.txt"
    else
        error "Neither wget nor curl is installed. Please install one of them or download the file manually."
        error "Download URL: https://raw.githubusercontent.com/dwyl/english-words/master/words.txt"
        error "Save to: $DICT_DIR/english.txt"
    fi

    # Check if files were downloaded successfully
    if [ -f "$MODELS_DIR/east_text_detection.pb" ] && [ -f "$MODELS_DIR/ESPCN_x4.pb" ]; then
        success "Models downloaded successfully"
    else
        error "Failed to download some models"
    fi
}

# Install dependencies on Debian/Ubuntu
install_debian() {
    log "Installing dependencies on Debian/Ubuntu..."

    # Update package lists
    sudo apt-get update

    # Install build tools and basic dependencies
    sudo apt-get install -y build-essential cmake git pkg-config wget curl

    # Install OpenCV dependencies
    sudo apt-get install -y \
        libopencv-dev \
        python3-opencv \
        libgtk-3-dev \
        libavcodec-dev \
        libavformat-dev \
        libswscale-dev \
        libv4l-dev \
        libxvidcore-dev \
        libx264-dev \
        libjpeg-dev \
        libpng-dev \
        libtiff-dev \
        gfortran \
        openexr \
        libatlas-base-dev

    # Install Tesseract OCR and language data
    sudo apt-get install -y \
        tesseract-ocr \
        libtesseract-dev \
        libleptonica-dev \
        tesseract-ocr-eng \
        tesseract-ocr-osd

    # Optional: Install additional language packs
    sudo apt-get install -y \
        tesseract-ocr-fra \
        tesseract-ocr-deu \
        tesseract-ocr-spa

    success "Dependencies installed successfully on Debian/Ubuntu"
}

# Install dependencies on Fedora
install_fedora() {
    log "Installing dependencies on Fedora..."

    # Update package lists
    sudo dnf update -y

    # Install build tools and basic dependencies
    sudo dnf install -y gcc-c++ cmake git pkgconfig wget curl

    # Install OpenCV and its dependencies
    sudo dnf install -y \
        opencv \
        opencv-devel \
        gtk3-devel \
        ffmpeg-devel \
        libv4l-devel \
        libpng-devel \
        libjpeg-turbo-devel \
        libtiff-devel \
        blas-devel \
        lapack-devel \
        atlas-devel \
        openexr-devel

    # Install Tesseract OCR and language data
    sudo dnf install -y \
        tesseract \
        tesseract-devel \
        tesseract-langpack-eng \
        leptonica-devel

    # Optional: Install additional language packs
    sudo dnf install -y \
        tesseract-langpack-fra \
        tesseract-langpack-deu \
        tesseract-langpack-spa

    success "Dependencies installed successfully on Fedora"
}

# Install dependencies on RHEL/CentOS
install_rhel() {
    log "Installing dependencies on RHEL/CentOS..."

    # Enable EPEL repository
    sudo yum install -y epel-release

    # Update package lists
    sudo yum update -y

    # Install build tools and basic dependencies
    sudo yum groupinstall -y "Development Tools"
    sudo yum install -y cmake3 git pkgconfig wget curl

    # Create link for cmake if needed
    if ! command -v cmake &> /dev/null && command -v cmake3 &> /dev/null; then
        sudo ln -s /usr/bin/cmake3 /usr/bin/cmake
    fi

    # Install OpenCV dependencies
    sudo yum install -y \
        opencv \
        opencv-devel \
        gtk3-devel \
        ffmpeg-devel \
        libv4l-devel \
        libpng-devel \
        libjpeg-turbo-devel \
        libtiff-devel \
        atlas-devel \
        openexr-devel

    # Install Tesseract OCR and language data
    sudo yum install -y \
        tesseract \
        tesseract-devel \
        leptonica-devel

    # Download and install English language data
    if [ ! -d "/usr/share/tesseract/tessdata" ]; then
        sudo mkdir -p /usr/share/tesseract/tessdata
    fi

    wget -O /tmp/eng.traineddata https://github.com/tesseract-ocr/tessdata/raw/4.0.0/eng.traineddata
    sudo mv /tmp/eng.traineddata /usr/share/tesseract/tessdata/

    success "Dependencies installed successfully on RHEL/CentOS"
}

# Install dependencies on Arch Linux
install_arch() {
    log "Installing dependencies on Arch Linux..."

    # Update package database
    sudo pacman -Syu --noconfirm

    # Install build tools and basic dependencies
    sudo pacman -S --noconfirm base-devel cmake git pkgconf wget curl

    # Install OpenCV and its dependencies
    sudo pacman -S --noconfirm \
        opencv \
        gtk3 \
        ffmpeg \
        v4l-utils \
        libpng \
        libjpeg-turbo \
        libtiff \
        openblas \
        lapack \
        openexr

    # Install Tesseract OCR and language data
    sudo pacman -S --noconfirm \
        tesseract \
        tesseract-data-eng \
        leptonica

    # Optional: Install additional language data
    sudo pacman -S --noconfirm \
        tesseract-data-fra \
        tesseract-data-deu \
        tesseract-data-spa

    success "Dependencies installed successfully on Arch Linux"
}

# Install dependencies on openSUSE
install_suse() {
    log "Installing dependencies on openSUSE..."

    # Update package database
    sudo zypper refresh

    # Install build tools and basic dependencies
    sudo zypper install -y -t pattern devel_basis
    sudo zypper install -y cmake git pkgconfig wget curl

    # Install OpenCV and its dependencies
    sudo zypper install -y \
        opencv \
        opencv-devel \
        gtk3-devel \
        ffmpeg-devel \
        libv4l-devel \
        libpng16-devel \
        libjpeg8-devel \
        libtiff-devel \
        blas-devel \
        lapack-devel \
        OpenEXR-devel

    # Install Tesseract OCR and language data
    sudo zypper install -y \
        tesseract-ocr \
        tesseract-ocr-devel \
        tesseract-ocr-traineddata-english \
        leptonica-devel

    # Optional: Install additional language data
    sudo zypper install -y \
        tesseract-ocr-traineddata-french \
        tesseract-ocr-traineddata-german \
        tesseract-ocr-traineddata-spanish

    success "Dependencies installed successfully on openSUSE"
}

# Install dependencies on macOS using Homebrew
install_macos() {
    log "Installing dependencies on macOS..."

    # Check if Homebrew is installed, install if not
    if ! command -v brew &> /dev/null; then
        log "Installing Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    else
        log "Homebrew already installed, updating..."
        brew update
    fi

    # Install build tools and basic dependencies
    brew install cmake git wget curl

    # Install OpenCV and its dependencies
    brew install opencv

    # Install Tesseract OCR and language data
    brew install tesseract

    # Optional: Install additional language data
    brew install tesseract-lang

    success "Dependencies installed successfully on macOS"
}

# Install dependencies on Windows using Chocolatey and vcpkg
create_windows_script() {
    log "Creating Windows installation script..."

    cat > Install-OCRDependencies.ps1 << 'EOF'
# Enhanced OCR System - Windows Dependency Installer
# Run this script with administrator privileges

# Configuration
$MODELS_DIR = ".\models"
$CACHE_DIR = ".\.ocr_cache"
$LOG_DIR = ".\logs"
$DICT_DIR = ".\dict"
$VCPKG_DIR = "C:\vcpkg"

# Create directories
function Create-Directories {
    Write-Host "Creating necessary directories..."

    if (-not (Test-Path $MODELS_DIR)) { New-Item -ItemType Directory -Force -Path $MODELS_DIR | Out-Null }
    if (-not (Test-Path $CACHE_DIR)) { New-Item -ItemType Directory -Force -Path $CACHE_DIR | Out-Null }
    if (-not (Test-Path $LOG_DIR)) { New-Item -ItemType Directory -Force -Path $LOG_DIR | Out-Null }
    if (-not (Test-Path $DICT_DIR)) { New-Item -ItemType Directory -Force -Path $DICT_DIR | Out-Null }

    Write-Host "Directories created successfully" -ForegroundColor Green
}

# Download models
function Download-Models {
    Write-Host "Downloading OCR models and resources..."

    # Download EAST text detection model
    Write-Host "Downloading EAST text detection model..."
    Invoke-WebRequest -Uri "https://github.com/oyyd/frozen_east_text_detection.pb/raw/master/frozen_east_text_detection.pb" -OutFile "$MODELS_DIR\east_text_detection.pb"

    # Download super resolution model
    Write-Host "Downloading ESPCN super resolution model..."
    Invoke-WebRequest -Uri "https://github.com/fannymonori/TF-ESPCN/raw/master/export/ESPCN_x4.pb" -OutFile "$MODELS_DIR\ESPCN_x4.pb"

    # Download English dictionary for spell checking
    Write-Host "Downloading English dictionary for spell checking..."
    Invoke-WebRequest -Uri "https://raw.githubusercontent.com/dwyl/english-words/master/words.txt" -OutFile "$DICT_DIR\english.txt"

    if ((Test-Path "$MODELS_DIR\east_text_detection.pb") -and (Test-Path "$MODELS_DIR\ESPCN_x4.pb")) {
        Write-Host "Models downloaded successfully" -ForegroundColor Green
    } else {
        Write-Host "Failed to download some models" -ForegroundColor Red
    }
}

# Install Chocolatey if not already installed
function Install-Chocolatey {
    if (-not (Get-Command choco -ErrorAction SilentlyContinue)) {
        Write-Host "Installing Chocolatey..."
        Set-ExecutionPolicy Bypass -Scope Process -Force
        [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
        Invoke-Expression ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))
    } else {
        Write-Host "Chocolatey is already installed"
    }
}

# Install vcpkg if not already installed
function Install-Vcpkg {
    if (-not (Test-Path $VCPKG_DIR)) {
        Write-Host "Installing vcpkg..."

        # Clone vcpkg repository
        git clone https://github.com/Microsoft/vcpkg.git $VCPKG_DIR

        # Run bootstrap script
        & "$VCPKG_DIR\bootstrap-vcpkg.bat" -disableMetrics

        # Add vcpkg to PATH
        $env:Path += ";$VCPKG_DIR"
        [Environment]::SetEnvironmentVariable("Path", $env:Path, [EnvironmentVariableTarget]::User)

        # Integrate vcpkg with Visual Studio
        & "$VCPKG_DIR\vcpkg" integrate install
    } else {
        Write-Host "vcpkg is already installed"

        # Update vcpkg
        Push-Location $VCPKG_DIR
        git pull
        & ".\bootstrap-vcpkg.bat" -disableMetrics
        Pop-Location
    }
}

# Install Visual Studio Build Tools if not already installed
function Install-BuildTools {
    if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
        Write-Host "Installing Visual Studio Build Tools..."
        choco install visualstudio2022buildtools -y
        choco install visualstudio2022-workload-vctools -y
    } else {
        Write-Host "Visual Studio Build Tools are already installed"
    }
}

# Install dependencies using vcpkg
function Install-Dependencies {
    Write-Host "Installing dependencies using vcpkg..."

    # Install OpenCV
    & "$VCPKG_DIR\vcpkg" install opencv:x64-windows

    # Install Tesseract OCR
    & "$VCPKG_DIR\vcpkg" install tesseract:x64-windows

    # Install additional dependencies
    & "$VCPKG_DIR\vcpkg" install leptonica:x64-windows

    Write-Host "Dependencies installed successfully" -ForegroundColor Green
}

# Install additional tools
function Install-AdditionalTools {
    Write-Host "Installing additional tools..."

    # Install Git if not already installed
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        choco install git -y
    }

    # Install CMake if not already installed
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        choco install cmake --installargs 'ADD_CMAKE_TO_PATH=System' -y
    }

    Write-Host "Additional tools installed successfully" -ForegroundColor Green
}

# Configure environment
function Configure-Environment {
    Write-Host "Configuring environment..."

    # Create a sample config file
    $configJson = @"
{
    "language": "eng",
    "enableDeskew": true,
    "enablePerspectiveCorrection": true,
    "enableNoiseRemoval": true,
    "enableTextDetection": true,
    "enableSpellCheck": true,
    "enableSuperResolution": false,
    "cacheResults": true,
    "maxThreads": 8,
    "preprocessing": {
        "applyGaussianBlur": true,
        "gaussianKernelSize": 3,
        "applyThreshold": true,
        "useAdaptiveThreshold": true,
        "blockSize": 11,
        "constantC": 2,
        "medianBlurSize": 3,
        "applyClahe": false,
        "clipLimit": 2.0,
        "binarizationMethod": 0
    },
    "superResolution": {
        "modelPath": "models/ESPCN_x4.pb",
        "modelName": "espcn",
        "scale": 4
    },
    "textDetection": {
        "confThreshold": 0.5,
        "nmsThreshold": 0.4,
        "detectionSize": 320,
        "modelPath": "models/east_text_detection.pb"
    },
    "cache": {
        "maxCacheSize": 104857600,
        "cacheDir": ".ocr_cache"
    }
}
"@

    Set-Content -Path "ocr_config.json" -Value $configJson

    Write-Host "Environment configured successfully" -ForegroundColor Green
}

# Create example compilation script
function Create-CompilationScript {
    Write-Host "Creating compilation script..."

    $compileBat = @"
@echo off
REM Compile Enhanced OCR system

REM Check if build directory exists, create if not
if not exist build mkdir build

REM Change to build directory
cd build

REM Run CMake to generate build files
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_PREFIX_PATH=C:/vcpkg/installed/x64-windows

REM Build the project
cmake --build . --config Release

REM Return to original directory
cd ..

echo Build completed. Check the 'build' directory for output.
"@

    Set-Content -Path "compile.bat" -Value $compileBat

    Write-Host "Compilation script created successfully" -ForegroundColor Green
}

# Main function
function Main {
    Write-Host "Starting OCR dependencies installation for Windows..." -ForegroundColor Cyan

    # Create directories
    Create-Directories

    # Check if only downloading models
    if ($args[0] -eq "--models-only") {
        Download-Models
        return
    }

    # Install Chocolatey
    Install-Chocolatey

    # Install additional tools
    Install-AdditionalTools

    # Install Visual Studio Build Tools
    Install-BuildTools

    # Install vcpkg
    Install-Vcpkg

    # Install dependencies
    Install-Dependencies

    # Download models
    Download-Models

    # Configure environment
    Configure-Environment

    # Create compilation script
    Create-CompilationScript

    Write-Host "Installation completed successfully!" -ForegroundColor Green
    Write-Host "You can now build the Enhanced OCR system using the generated compile.bat script."
}

# Run as administrator
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "This script requires administrator privileges. Please run as administrator." -ForegroundColor Red
    exit
}

# Run main function with passed arguments
Main $args
EOF

    success "Windows installation script created: Install-OCRDependencies.ps1"
    log "Please run this script on Windows with administrator privileges."
}

# Main function
main() {
    log "Starting OCR dependencies installation..."

    # Create directories
    create_directories

    # Check if only downloading models
    if [[ "$1" == "--models-only" ]]; then
        download_models
        success "Models downloaded successfully. Exiting."
        exit 0
    fi

    # Detect OS
    detect_os

    # Install dependencies based on OS
    case $OS in
        debian)
            install_debian
            ;;
        fedora)
            install_fedora
            ;;
        rhel)
            install_rhel
            ;;
        arch)
            install_arch
            ;;
        suse)
            install_suse
            ;;
        macos)
            install_macos
            ;;
        windows)
            create_windows_script
            log "For Windows, please use the generated PowerShell script."
            exit 0
            ;;
        linux-unknown)
            error "Unsupported Linux distribution. Please install dependencies manually."
            cat << EOF
Required dependencies:
- OpenCV (>= 4.5.0)
- Tesseract OCR (>= 4.1.1)
- Leptonica
- Build tools (gcc/g++, cmake)
- Git

Please refer to your distribution's documentation for installation instructions.
EOF
            ;;
        unknown)
            error "Unsupported operating system. Please install dependencies manually."
            exit 1
            ;;
    esac

    # Download models
    download_models

    # Create sample config file
    log "Creating sample configuration file..."
    cat > ocr_config.json << EOF
{
    "language": "eng",
    "enableDeskew": true,
    "enablePerspectiveCorrection": true,
    "enableNoiseRemoval": true,
    "enableTextDetection": true,
    "enableSpellCheck": true,
    "enableSuperResolution": false,
    "cacheResults": true,
    "maxThreads": $(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4),
    "preprocessing": {
        "applyGaussianBlur": true,
        "gaussianKernelSize": 3,
        "applyThreshold": true,
        "useAdaptiveThreshold": true,
        "blockSize": 11,
        "constantC": 2,
        "medianBlurSize": 3,
        "applyClahe": false,
        "clipLimit": 2.0,
        "binarizationMethod": 0
    },
    "superResolution": {
        "modelPath": "models/ESPCN_x4.pb",
        "modelName": "espcn",
        "scale": 4
    },
    "textDetection": {
        "confThreshold": 0.5,
        "nmsThreshold": 0.4,
        "detectionSize": 320,
        "modelPath": "models/east_text_detection.pb"
    },
    "cache": {
        "maxCacheSize": 104857600,
        "cacheDir": ".ocr_cache"
    }
}
EOF

    # Create CMakeLists.txt file
    log "Creating CMakeLists.txt file..."
    cat > CMakeLists.txt << EOF
cmake_minimum_required(VERSION 3.10)
project(EnhancedOCR VERSION 1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compiler options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "\${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -march=native")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS "\${CMAKE_CXX_FLAGS} /W4 /O2")
endif()

# Find packages
find_package(OpenCV REQUIRED)
find_package(Tesseract REQUIRED)

# Include directories
include_directories(
    \${OpenCV_INCLUDE_DIRS}
    \${Tesseract_INCLUDE_DIRS}
)

# Source files
set(SOURCES
    enhanced_ocr.cpp
)

# Add executable
add_executable(enhanced_ocr \${SOURCES})

# Link libraries
target_link_libraries(enhanced_ocr
    \${OpenCV_LIBS}
    \${Tesseract_LIBRARIES}
)

# Install target
install(TARGETS enhanced_ocr DESTINATION bin)

# Copy models directory to build directory
file(COPY \${CMAKE_SOURCE_DIR}/models DESTINATION \${CMAKE_BINARY_DIR})

# Copy dictionary directory to build directory
file(COPY \${CMAKE_SOURCE_DIR}/dict DESTINATION \${CMAKE_BINARY_DIR})

# Create cache directory in build directory
file(MAKE_DIRECTORY \${CMAKE_BINARY_DIR}/.ocr_cache)

# Create logs directory in build directory
file(MAKE_DIRECTORY \${CMAKE_BINARY_DIR}/logs)
EOF

    # Create compilation script
    log "Creating compilation script..."
    cat > compile.sh << EOF
#!/bin/bash
# Compile Enhanced OCR system

# Create build directory if it doesn't exist
mkdir -p build

# Change to build directory
cd build

# Configure with CMake
cmake ..

# Build
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Return to original directory
cd ..

echo "Build completed. Check the 'build' directory for output."
EOF
    chmod +x compile.sh

    success "Installation completed successfully!"
    log "You can now build the Enhanced OCR system using the generated compile.sh script."
}

# Run main function with all arguments
main "$@"
