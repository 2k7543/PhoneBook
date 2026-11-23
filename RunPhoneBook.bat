@echo off
setlocal
cd /d "%~dp0"

set "APP_DIR=%~dp0build-qt-runtime"
set "SERVER_EXE=%APP_DIR%\PhoneBookWebServer.exe"
set "RUNTIME_DIR=C:\msys64\ucrt64\bin"

if not exist "%SERVER_EXE%" (
    echo Khong tim thay PhoneBookWebServer.exe.
    echo Hay build target PhoneBookWebServer bang CMake truoc khi chay.
    pause
    exit /b 1
)

if exist "%RUNTIME_DIR%" set "PATH=%RUNTIME_DIR%;%PATH%"
start "" "%SERVER_EXE%"
timeout /t 2 /nobreak >nul
start "" "http://localhost:8080"
endlocal
