# Atom Project vcpkg Dependency Installation Script
Write-Host "====================================" -ForegroundColor Cyan
Write-Host "Atom Project vcpkg Dependency Installation Script" -ForegroundColor Cyan
Write-Host "====================================" -ForegroundColor Cyan

# Define color functions for consistent output
function Write-Green($text) { Write-Host $text -ForegroundColor Green }
function Write-Yellow($text) { Write-Host $text -ForegroundColor Yellow }
function Write-Red($text) { Write-Host $text -ForegroundColor Red }

# Check if vcpkg is installed
Write-Green "Checking for vcpkg..."

$VcpkgPath = $null

# Check possible vcpkg locations by priority
if ($env:VCPKG_ROOT -and (Test-Path "$env:VCPKG_ROOT\vcpkg.exe")) {
    $VcpkgPath = $env:VCPKG_ROOT
    Write-Green "Found existing VCPKG_ROOT: $VcpkgPath"
}
elseif (Test-Path "C:\vcpkg\vcpkg.exe") {
    $VcpkgPath = "C:\vcpkg"
    Write-Green "Found vcpkg: $VcpkgPath"
}
elseif (Test-Path "$env:USERPROFILE\vcpkg\vcpkg.exe") {
    $VcpkgPath = "$env:USERPROFILE\vcpkg"
    Write-Green "Found vcpkg: $VcpkgPath"
}
elseif (Test-Path "$(Get-Location)\vcpkg\vcpkg.exe") {
    $VcpkgPath = "$(Get-Location)\vcpkg"
    Write-Green "Found vcpkg: $VcpkgPath"
}
else {
    # vcpkg not found, prompt to install
    Write-Yellow "vcpkg not found. Do you want to install it? (Y/N)"
    $installChoice = Read-Host "> "
    
    if ($installChoice -eq "Y" -or $installChoice -eq "y") {
        Write-Green "Installing vcpkg..."
        
        # Determine installation path
        Write-Yellow "Please select vcpkg installation location:"
        Write-Host "1. User home directory ($env:USERPROFILE\vcpkg)"
        Write-Host "2. C drive root (C:\vcpkg)"
        Write-Host "3. Current directory ($(Get-Location)\vcpkg)"
        $installLocation = Read-Host "> "
        
        switch ($installLocation) {
            "1" { $VcpkgPath = "$env:USERPROFILE\vcpkg" }
            "2" { $VcpkgPath = "C:\vcpkg" }
            "3" { $VcpkgPath = "$(Get-Location)\vcpkg" }
            default {
                Write-Red "Invalid choice. Using default location ($env:USERPROFILE\vcpkg)"
                $VcpkgPath = "$env:USERPROFILE\vcpkg"
            }
        }
        
        # Clone and bootstrap vcpkg
        if (Test-Path $VcpkgPath) {
            Write-Yellow "Directory $VcpkgPath already exists. Continue? (Y/N)"
            $continueChoice = Read-Host "> "
            if ($continueChoice -ne "Y" -and $continueChoice -ne "y") {
                exit
            }
        }
        
        Write-Green "Cloning vcpkg to $VcpkgPath..."
        git clone https://github.com/microsoft/vcpkg.git $VcpkgPath
        if ($LASTEXITCODE -ne 0) {
            Write-Red "Failed to clone vcpkg"
            exit
        }
        
        Write-Green "Bootstrapping vcpkg..."
        & "$VcpkgPath\bootstrap-vcpkg.bat" -disableMetrics
        if ($LASTEXITCODE -ne 0) {
            Write-Red "Failed to bootstrap vcpkg"
            exit
        }
        
        # Set VCPKG_ROOT environment variable
        Write-Green "Setting VCPKG_ROOT environment variable..."
        try {
            [Environment]::SetEnvironmentVariable("VCPKG_ROOT", $VcpkgPath, "User")
            $env:VCPKG_ROOT = $VcpkgPath
        }
        catch {
            Write-Yellow "Warning: Failed to set VCPKG_ROOT environment variable"
        }
    }
    else {
        Write-Red "Operation cancelled. vcpkg is required to continue."
        exit
    }
}

Write-Green "Using vcpkg: $VcpkgPath"

# Detect current system architecture
$Arch = "x64"
if (-not [Environment]::Is64BitOperatingSystem) {
    $Arch = "x86"
}

# Detect if in MSYS2 environment
$IsMsys2 = $false
if ($env:MSYSTEM) {
    $IsMsys2 = $true
    Write-Green "MSYS2 environment detected: $($env:MSYSTEM)"
}

# Determine triplet
$Triplet = "$Arch-windows"
if ($IsMsys2) {
    $Triplet = "$Arch-mingw-dynamic"
    Write-Green "MSYS2: Using triplet $Triplet"
    
    # Check if MinGW triplet needs to be created
    $TripletFile = "$VcpkgPath\triplets\community\$Triplet.cmake"
    if (-not (Test-Path $TripletFile)) {
        Write-Yellow "Need to create MinGW triplet file: $Triplet"
        
        # Create directory if it doesn't exist
        $TripletDir = "$VcpkgPath\triplets\community"
        if (-not (Test-Path $TripletDir)) {
            New-Item -Path $TripletDir -ItemType Directory -Force | Out-Null
        }
        
        # Create the triplet file
        @"
set(VCPKG_TARGET_ARCHITECTURE $Arch)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_CMAKE_SYSTEM_NAME MinGW)
"@ | Set-Content -Path $TripletFile
        
        Write-Green "Triplet file created: $Triplet"
    }
}

# Install dependencies
Write-Green "Preparing to install Atom dependencies..."
Write-Green "Using triplet: $Triplet"

Write-Green "Installing core dependencies..."
& "$VcpkgPath\vcpkg.exe" install openssl zlib sqlite3 fmt readline pybind11 boost[container] --triplet=$Triplet
if ($LASTEXITCODE -ne 0) {
    Write-Red "Failed to install core dependencies"
    exit
}

Write-Yellow "Install optional dependencies? (Y/N)"
$optionalDeps = Read-Host "> "
if ($optionalDeps -eq "Y" -or $optionalDeps -eq "y") {
    Write-Green "Installing optional Boost components..."
    & "$VcpkgPath\vcpkg.exe" install boost[atomic, thread, graph] --triplet=$Triplet
    if ($LASTEXITCODE -ne 0) {
        Write-Yellow "Warning: Failed to install optional Boost components"
    }
    
    Write-Green "Installing test components..."
    & "$VcpkgPath\vcpkg.exe" install gtest --triplet=$Triplet
    if ($LASTEXITCODE -ne 0) {
        Write-Yellow "Warning: Failed to install test components"
    }
}

Write-Green "Integrating vcpkg with the system..."
& "$VcpkgPath\vcpkg.exe" integrate install
if ($LASTEXITCODE -ne 0) {
    Write-Yellow "Warning: vcpkg integration failed"
}

Write-Host "====================================" -ForegroundColor Cyan
Write-Green "vcpkg setup complete!"
Write-Host "====================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "You can now configure the project using the following command:" -ForegroundColor Cyan
Write-Host "cmake -B build -G `"Ninja`" -DCMAKE_TOOLCHAIN_FILE=`"$VcpkgPath/scripts/buildsystems/vcpkg.cmake`" -DVCPKG_TARGET_TRIPLET=$Triplet" -ForegroundColor Yellow
Write-Host ""
Write-Host "Then build:" -ForegroundColor Cyan
Write-Host "cmake --build build" -ForegroundColor Yellow
Write-Host "====================================" -ForegroundColor Cyan

Write-Yellow "Configure the project now? (Y/N)"
$configNow = Read-Host "> "
if ($configNow -eq "Y" -or $configNow -eq "y") {
    Write-Green "Configuring project..."
    & cmake -B build -G "Ninja" -DCMAKE_TOOLCHAIN_FILE="$VcpkgPath/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=$Triplet
    
    if ($LASTEXITCODE -ne 0) {
        Write-Red "Project configuration failed"
    }
    else {
        Write-Green "Project configured successfully!"
        
        Write-Yellow "Start build now? (Y/N)"
        $buildNow = Read-Host "> "
        if ($buildNow -eq "Y" -or $buildNow -eq "y") {
            Write-Green "Building project..."
            & cmake --build build
            
            if ($LASTEXITCODE -ne 0) {
                Write-Red "Project build failed"
            }
            else {
                Write-Green "Project built successfully!"
            }
        }
    }
}

Write-Host "Press any key to continue..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")