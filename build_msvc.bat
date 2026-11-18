@echo off
setlocal
set "STORYLAND_BUILD_EXIT=1"
where powershell.exe >nul 2>nul
if errorlevel 1 goto missing_powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_msvc.ps1"
set "STORYLAND_BUILD_EXIT=%ERRORLEVEL%"
goto finished

:missing_powershell
echo ERROR: Windows PowerShell is required to run the build launcher.

:finished
echo.
if not "%STORYLAND_BUILD_EXIT%"=="0" echo Build failed. Read the error above and build-msvc.log beside this launcher.
if /I "%~1"=="--no-pause" goto return_result
pause
:return_result
exit /b %STORYLAND_BUILD_EXIT%
