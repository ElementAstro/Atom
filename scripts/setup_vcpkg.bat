@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "TARGET=%SCRIPT_DIR%deps\setup_vcpkg.bat"

if not exist "%TARGET%" (
    echo Error: Script not found: %TARGET%
    exit /b 1
)

call "%TARGET%" %*
