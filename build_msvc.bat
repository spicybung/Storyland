@echo off
setlocal
cmake -S . -B build -A x64
if errorlevel 1 exit /b 1
cmake --build build --config Release
