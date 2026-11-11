@echo off
setlocal
set "AZRAID_BLUR_REVIEW=1"
set "AZRAID_FLIGHT_BLUR=camera"
if not exist "%~dp0..\..\generated\outputs\Release\CG2.exe" (
    echo Release build is missing.
    pause
    exit /b 1
)
start "" /D "%~dp0.." "%~dp0..\..\generated\outputs\Release\CG2.exe"
