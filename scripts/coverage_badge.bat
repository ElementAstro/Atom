@echo off
REM coverage_badge.bat - Windows batch wrapper for coverage_badge.py
REM This project is licensed under the terms of the GPL3 license.
REM
REM Author: Max Qian
REM License: GPL3

setlocal enabledelayedexpansion

REM Set UTF-8 code page for Unicode support
chcp 65001 >nul 2>&1

REM Get the directory where this batch file is located
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."

REM Change to project root directory
cd /d "%PROJECT_ROOT%"

REM Check if Python is available
python --version >nul 2>&1
if errorlevel 1 (
    echo ❌ Python is not installed or not in PATH
    echo 💡 Install Python from https://python.org or Microsoft Store
    pause
    exit /b 1
)

REM Check if the Python script exists
if not exist "%SCRIPT_DIR%coverage_badge.py" (
    echo ❌ coverage_badge.py not found in %SCRIPT_DIR%
    pause
    exit /b 1
)

REM Display help if requested
if "%1"=="--help" goto :show_help
if "%1"=="-h" goto :show_help
if "%1"=="/?" goto :show_help

REM Run the Python script with all arguments
echo 🚀 Running Coverage Badge Generator...
echo.
python "%SCRIPT_DIR%coverage_badge.py" %*
set "EXIT_CODE=%ERRORLEVEL%"

REM Show completion message
echo.
if %EXIT_CODE%==0 (
    echo ✅ Coverage badge generation completed successfully!
) else (
    echo ❌ Coverage badge generation failed with exit code %EXIT_CODE%
)

REM Pause if running interactively (not from command line with arguments)
if "%1"=="" (
    echo.
    echo Press any key to continue...
    pause >nul
)

exit /b %EXIT_CODE%

:show_help
echo Coverage Badge Generator for Windows
echo ===================================
echo.
echo Usage: %~nx0 [OPTIONS]
echo.
echo Options:
echo   --coverage-file PATH    Path to coverage JSON file
echo   --readme-file PATH      Path to README.md file  
echo   --output FORMAT         Output format: markdown, urls, update-readme
echo   --help, -h, /?          Show this help message
echo.
echo Examples:
echo   %~nx0
echo   %~nx0 --output markdown
echo   %~nx0 --coverage-file coverage\unified\coverage.json
echo   %~nx0 --readme-file README.md --output update-readme
echo.
echo Default paths:
echo   Coverage file: coverage\unified\coverage.json
echo   README file:   README.md
echo.
echo For more information, see docs\COVERAGE_GUIDE.md
pause
exit /b 0
