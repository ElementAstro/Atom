@echo off
echo Setting up environment for DirectoryStack tests...

REM Add MSYS2 bin directory to PATH for this session
set PATH=D:\msys64\mingw64\bin;%PATH%

echo Running DirectoryStack tests...
echo ================================

REM Change to the directory containing the test executable
cd /d "%~dp0build\tests\io"

REM Run only the DirectoryStack tests
atom_io.test.exe --gtest_filter=DirectoryStackTest* --gtest_brief=1

echo.
echo Test execution completed.
echo Exit code: %ERRORLEVEL%

pause
