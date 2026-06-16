@echo off
setlocal enabledelayedexpansion

:: setup_misra.bat - Download and pin the cppcheck MISRA C:2012 addon scripts.
::
:: Pins three Python files from the cppcheck 2.4.1 source tree that match
:: the system cppcheck.exe version. Files are written to tools\misra\.
::
:: SHA256 of misra.py is pinned below and verified on every run.
::
:: Usage:
::   tools\setup_misra.bat          install if not present
::   tools\setup_misra.bat --force  re-download even if present

set "EXPECTED_SHA256=4e495f0d74099f15c5a27e35d10275023c79c997e7b2d0a862492301c97c7978"

set "SCRIPT_DIR=%~dp0"
set "FORCE="
if /i "%~1"=="--force" set "FORCE=1"

set "BASE_URL=https://raw.githubusercontent.com/danmar/cppcheck/2.4.1/addons"
set "FILES=misra.py cppcheckdata.py misra_9.py"
set "MISRA_DIR=%SCRIPT_DIR%misra"

:: Check if all files already exist (skip unless --force)
if not defined FORCE (
    set "ALL_PRESENT=1"
    for %%f in (%FILES%) do (
        if not exist "%MISRA_DIR%\%%f" set "ALL_PRESENT=0"
    )
    if "!ALL_PRESENT!"=="1" (
        echo [misra] Already installed. Use --force to re-download.
        exit /b 0
    )
)

if not exist "%MISRA_DIR%" mkdir "%MISRA_DIR%"

echo [misra] Downloading MISRA C:2012 addon (cppcheck 2.4.1)...

for %%f in (%FILES%) do (
    echo   Downloading %%f...
    powershell -NoProfile -Command ^
        "Invoke-WebRequest -Uri '%BASE_URL%/%%f' -OutFile '%MISRA_DIR%\%%f' -UseBasicParsing" ^
        2>nul
    if not exist "%MISRA_DIR%\%%f" (
        echo ERROR: Failed to download %%f
        exit /b 1
    )
)

:: Verify SHA256 of misra.py against the pinned hash
for /f %%h in ('powershell -NoProfile -Command ^
    "(Get-FileHash '%MISRA_DIR%\misra.py' -Algorithm SHA256).Hash.ToLower()"') do (
    set "ACTUAL_SHA256=%%h"
)
if /i "!ACTUAL_SHA256!" neq "!EXPECTED_SHA256!" (
    echo ERROR: SHA256 mismatch for misra.py
    echo   Expected: !EXPECTED_SHA256!
    echo   Actual:   !ACTUAL_SHA256!
    exit /b 1
)
echo [misra] SHA256 verified.

echo [misra] MISRA addon installed.
echo   tools\misra\misra.py
echo   tools\misra\cppcheckdata.py
echo   tools\misra\misra_9.py
echo.
echo Run: xsdk.bat misra

endlocal
exit /b 0
