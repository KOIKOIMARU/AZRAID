@echo off
setlocal
set "debugExe=%~dp0..\..\generated\outputs\Debug\CG2.exe"
if not exist "%debugExe%" (
    echo Build CG2 as Debug x64 first.
    pause
    exit /b 1
)

echo 1. Stage opening
echo 2. Wave 2
echo 3. Wave 3
echo 4. Sniper wing
echo 5. Boss
echo Q. Quit
choice /C 12345Q /N /M "Choose a scene: "
set "selection=%errorlevel%"
if "%selection%"=="1" set "CG2_DEBUG_START_PHASE=Opening"
if "%selection%"=="2" set "CG2_DEBUG_START_PHASE=Wave2"
if "%selection%"=="3" set "CG2_DEBUG_START_PHASE=Wave3"
if "%selection%"=="4" set "CG2_DEBUG_START_PHASE=SniperWing"
if "%selection%"=="5" set "CG2_DEBUG_START_PHASE=Boss"
if %selection% LSS 1 exit /b
if %selection% GTR 5 exit /b
start "" /D "%~dp0.." "%debugExe%" --debug-direct-start
