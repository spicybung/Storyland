@echo off
setlocal EnableExtensions DisableDelayedExpansion
set "ROOT=%~dp0"
set "STORYLAND_RELEASE_VERSION="
if not exist "%ROOT%STORYLAND_VERSION.txt" (
    echo ERROR: STORYLAND_VERSION.txt is missing. Extract the full source ZIP.
    goto :failed
)
set /p STORYLAND_RELEASE_VERSION=<"%ROOT%STORYLAND_VERSION.txt"
if not defined STORYLAND_RELEASE_VERSION (
    echo ERROR: STORYLAND_VERSION.txt is empty.
    goto :failed
)
title Storyland Android %STORYLAND_RELEASE_VERSION% - APK Build

pushd "%ROOT%" 2>nul
if errorlevel 1 (
    echo ERROR: Cannot open the Storyland source directory.
    echo Directory: %ROOT%
    goto :failed
)

echo ============================================================
echo                   STORYLAND ANDROID %STORYLAND_RELEASE_VERSION%
 echo                       APK BUILD
 echo ============================================================
echo Source: %ROOT%
echo.

if not exist "%ROOT%android\build_apk.py" (
    echo ERROR: android\build_apk.py was not found.
    echo Extract the full source ZIP, keeping its folders intact.
    goto :failed
)

if not exist "%ROOT%android\build_android_windows.ps1" (
    echo ERROR: android\build_android_windows.ps1 was not found.
    echo Install the repaired build files in the correct directories.
    goto :failed
)

where powershell.exe >nul 2>&1
if errorlevel 1 (
    echo ERROR: Windows PowerShell is missing.
    goto :failed
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%ROOT%android\build_android_windows.ps1" %*
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" goto :failed

echo.
echo ============================================================
echo SUCCESS: Storyland Android APK build finished.
echo The APK is in the build-android folder.
echo ============================================================
popd
echo.
echo Press any key to close this window.
pause >nul
exit /b 0

:failed
if not defined RESULT set "RESULT=1"
echo.
echo ============================================================
echo FAILED: Storyland Android APK build stopped with error %RESULT%.
echo See build-android\build_android.log for diagnostic output.
echo ============================================================
popd 2>nul
echo.
echo Press any key to close this window.
pause >nul
exit /b %RESULT%
