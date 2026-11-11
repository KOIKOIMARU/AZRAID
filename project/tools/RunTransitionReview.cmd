@echo off
setlocal
set "AZRAID_TRANSITION_REVIEW=1"
set "AZRAID_TITLE_FADE=1"
if "%~1"=="0" set "AZRAID_TITLE_FADE=0"
if not exist "%~dp0..\..\generated\outputs\Release\CG2.exe" (
    echo Release build is missing.
    pause
    exit /b 1
)
start "" /D "%~dp0.." "%~dp0..\..\generated\outputs\Release\CG2.exe"
