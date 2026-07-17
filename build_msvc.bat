@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "BUILD_LOG=%~dp0build.log"
> "%BUILD_LOG%" echo Storyland build started %DATE% %TIME%

echo Configuring Storyland...
cmake -S . -B build -A x64 >> "%BUILD_LOG%" 2>&1
if errorlevel 1 goto configure_failed

echo Building Storyland...
cmake --build build --config Release >> "%BUILD_LOG%" 2>&1
if errorlevel 1 goto build_failed

type "%BUILD_LOG%"
echo.
echo Build succeeded.
echo Executable: build\Release\Storyland.exe
echo.
pause
exit /b 0

:configure_failed
type "%BUILD_LOG%"
echo.
echo CMake configuration failed. The complete output is saved in:
echo %BUILD_LOG%
echo.
pause
exit /b 1

:build_failed
type "%BUILD_LOG%"
echo.
echo Storyland compilation failed. The complete output is saved in:
echo %BUILD_LOG%
echo.
pause
exit /b 2
