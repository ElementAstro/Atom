@echo off
setlocal enabledelayedexpansion

REM Initialize Visual Studio environment
call "D:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64

REM Remove MinGW/MSYS2 from PATH
set "NEWPATH="
for %%i in ("%PATH:;=" "%") do (
    set "ITEM=%%~i"
    echo !ITEM! | findstr /i /c:"msys64" /c:"mingw" >nul
    if errorlevel 1 (
        if defined NEWPATH (
            set "NEWPATH=!NEWPATH!;!ITEM!"
        ) else (
            set "NEWPATH=!ITEM!"
        )
    )
)
set "PATH=!NEWPATH!"

REM Set vcpkg root
set "VCPKG_ROOT=C:\vcpkg"

REM Show which cmake we're using
echo Using cmake from:
where cmake

REM Clean build directory
if exist build rmdir /s /q build
mkdir build

REM Configure with CMake
cmake --preset msvc-vcpkg-release

endlocal
