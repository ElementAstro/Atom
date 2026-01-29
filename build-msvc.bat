@echo off
setlocal

set SCRIPT_DIR=%~dp0
set ACTUAL_SCRIPT=%SCRIPT_DIR%scripts\build-msvc.bat

if not exist "%ACTUAL_SCRIPT%" (
    echo Error: Build script not found at %ACTUAL_SCRIPT%
    exit /b 1
)

call "%ACTUAL_SCRIPT%" %*
