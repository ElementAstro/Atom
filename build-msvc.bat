@echo off
REM Wrapper script for backward compatibility
REM This script has been moved to scripts/build-msvc.bat
REM This wrapper maintains backward compatibility for existing workflows

REM Get the directory where this script is located
set SCRIPT_DIR=%~dp0

REM Check if the actual build script exists
set ACTUAL_SCRIPT=%SCRIPT_DIR%scripts\build-msvc.bat

if not exist "%ACTUAL_SCRIPT%" (
    echo Error: Build script not found at %ACTUAL_SCRIPT%
    echo Please ensure the scripts directory contains build-msvc.bat
    exit /b 1
)

REM Forward all arguments to the actual build script
call "%ACTUAL_SCRIPT%" %*
