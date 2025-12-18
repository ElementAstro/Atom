@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "TARGET=%SCRIPT_DIR%test\run_tests.bat"

if not exist "%TARGET%" (
    echo Error: Script not found: %TARGET%
    exit /b 1
)

call "%TARGET%" %*
