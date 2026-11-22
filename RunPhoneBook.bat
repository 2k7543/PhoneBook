@echo off
setlocal

set "APP_DIR=%~dp0build-qt-runtime"
set "RUNTIME_DIR=C:\msys64\ucrt64\bin"

if not exist "%APP_DIR%\PhoneBookManagementQt.exe" (
    echo Khong tim thay chuong trinh trong thu muc build-qt-runtime.
    echo Hay build du an bang CMake truoc khi chay.
    pause
    exit /b 1
)

if not exist "%RUNTIME_DIR%\Qt6Widgets.dll" (
    echo Khong tim thay Qt runtime tai "%RUNTIME_DIR%".
    pause
    exit /b 1
)

set "PATH=%RUNTIME_DIR%;%PATH%"
start "" "%APP_DIR%\PhoneBookManagementQt.exe"
endlocal
