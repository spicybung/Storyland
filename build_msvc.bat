@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "BUILD_LOG=%~dp0build.log"
set "BUILD_DIR=build-portable"
set "STORYLAND_EXE=%~dp0%BUILD_DIR%\Release\Storyland.exe"
set "DIST_DIR=%~dp0Release"
set "DIST_EXE=%DIST_DIR%\Storyland.exe"
> "%BUILD_LOG%" echo Storyland build started %DATE% %TIME%

set "VCPKG_TOOLCHAIN="
set "VCPKG_EXE="
if defined VCPKG_ROOT if exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" set "VCPKG_TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
if not defined VCPKG_TOOLCHAIN if exist "C:\Projects\vcpkg\scripts\buildsystems\vcpkg.cmake" set "VCPKG_TOOLCHAIN=C:\Projects\vcpkg\scripts\buildsystems\vcpkg.cmake"
if not defined VCPKG_TOOLCHAIN goto find_vcpkg_on_path
goto vcpkg_found

:find_vcpkg_on_path
for /f "delims=" %%I in ('where vcpkg.exe 2^>nul') do if not defined VCPKG_EXE set "VCPKG_EXE=%%~fI"
if not defined VCPKG_EXE goto vcpkg_missing
for %%I in ("%VCPKG_EXE%") do set "VCPKG_TOOLCHAIN=%%~dpIscripts\buildsystems\vcpkg.cmake"
if not exist "%VCPKG_TOOLCHAIN%" goto vcpkg_missing

:vcpkg_found
>> "%BUILD_LOG%" echo vcpkg toolchain: %VCPKG_TOOLCHAIN%

echo Configuring Storyland...
cmake --fresh -S . -B "%BUILD_DIR%" -A x64 "-DCMAKE_TOOLCHAIN_FILE=%VCPKG_TOOLCHAIN%" -DVCPKG_TARGET_TRIPLET=x64-windows-static >> "%BUILD_LOG%" 2>&1
if errorlevel 1 goto configure_failed

echo Building Storyland...
cmake --build "%BUILD_DIR%" --config Release --clean-first >> "%BUILD_LOG%" 2>&1
if errorlevel 1 goto build_failed

if not exist "%STORYLAND_EXE%" goto missing_exe
where dumpbin >nul 2>&1
if errorlevel 1 goto missing_dumpbin
dumpbin /DEPENDENTS "%STORYLAND_EXE%" >> "%BUILD_LOG%" 2>&1
dumpbin /DEPENDENTS "%STORYLAND_EXE%" | findstr /I /C:"zlib1.dll" /C:"zlibd1.dll" /C:"MSVCP140.dll" /C:"VCRUNTIME140.dll" /C:"VCRUNTIME140_1.dll" >nul
if not errorlevel 1 goto dependency_failed

if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"
copy /Y "%STORYLAND_EXE%" "%DIST_EXE%" >> "%BUILD_LOG%" 2>&1
if errorlevel 1 goto copy_failed

type "%BUILD_LOG%"
echo.
echo Build succeeded.
echo Executable: Release\Storyland.exe
echo Storyland.exe contains its shaders and zlib; no shaders folder or zlib1.dll is required.
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

:vcpkg_missing
type "%BUILD_LOG%"
echo.
echo Could not locate vcpkg. Set VCPKG_ROOT to your vcpkg folder or add vcpkg.exe to PATH.
echo Expected toolchain: %%VCPKG_ROOT%%\scripts\buildsystems\vcpkg.cmake
echo.
pause
exit /b 6

:build_failed
type "%BUILD_LOG%"
echo.
echo Storyland compilation failed. The complete output is saved in:
echo %BUILD_LOG%
echo.
pause
exit /b 2

:missing_exe
type "%BUILD_LOG%"
echo.
echo Build finished without producing %STORYLAND_EXE%.
echo.
pause
exit /b 3

:missing_dumpbin
type "%BUILD_LOG%"
echo.
echo Storyland was built, but dumpbin was unavailable to verify that it is portable.
echo Run build_msvc.bat from a Visual Studio Developer Command Prompt.
echo.
pause
exit /b 4

:dependency_failed
type "%BUILD_LOG%"
echo.
echo Portable build verification failed: Storyland.exe still imports a bundled runtime DLL.
echo Do not distribute this executable. See the dependency list in build.log.
echo.
pause
exit /b 5

:copy_failed
type "%BUILD_LOG%"
echo.
echo Storyland built successfully, but the portable executable could not be copied to:
echo %DIST_EXE%
echo Close Storyland.exe if it is currently running, then build again.
echo.
pause
exit /b 7
