@echo off
setlocal enabledelayedexpansion

:: setup_qemu.bat - Download xPack QEMU Arm 8.2.2-1 (Windows x64).
:: Provides qemu-system-arm and qemu-system-aarch64 (needed for xlnx-zynqmp
:: Cortex-R5F firmware validation - see xSIM plan, Section 12).
:: The archive is cached in tools\.cache\ and reused on subsequent runs.
:: Requires Windows 10+ (PowerShell Expand-Archive).
::
:: Output:
::   tools\qemu\bin\qemu-system-arm.exe
::   tools\qemu\bin\qemu-system-aarch64.exe
::   tools\qemu_sha256.txt    (committed; verified on every run)
::
:: Usage:
::   tools\setup_qemu.bat           skip if already present
::   tools\setup_qemu.bat --force   force re-download (clears cache)

set "QEMU_VERSION=8.2.2-1"
set "QEMU_ARCHIVE=xpack-qemu-arm-%QEMU_VERSION%-win32-x64.zip"
set "QEMU_URL=https://github.com/xpack-dev-tools/qemu-arm-xpack/releases/download/v%QEMU_VERSION%/%QEMU_ARCHIVE%"
set "QEMU_PREFIX=xpack-qemu-arm-%QEMU_VERSION%"

set "SCRIPT_DIR=%~dp0"
set "QEMU_OUT=%SCRIPT_DIR%qemu"
set "CACHE_DIR=%SCRIPT_DIR%.cache"
set "ARCHIVE_PATH=%CACHE_DIR%\%QEMU_ARCHIVE%"
set "SHA256_FILE=%SCRIPT_DIR%sha256\qemu_sha256.txt"

if exist "%QEMU_OUT%\bin\qemu-system-aarch64.exe" (
    if /i not "%~1"=="--force" (
        echo qemu-system-aarch64.exe already present in tools\qemu\bin\
        echo Run with --force to re-download.
        exit /b 0
    )
)

if /i "%~1"=="--force" ( if exist "%ARCHIVE_PATH%" del /q "%ARCHIVE_PATH%" )
if not exist "%CACHE_DIR%" mkdir "%CACHE_DIR%"

if exist "%ARCHIVE_PATH%" (
    echo Using cached archive: %QEMU_ARCHIVE%
) else (
    echo Downloading xPack QEMU Arm %QEMU_VERSION% ^(~60 MB^)...
    powershell -NoProfile -Command ^
        "$ProgressPreference='SilentlyContinue';" ^
        "Invoke-WebRequest -Uri '%QEMU_URL%' -OutFile '%ARCHIVE_PATH%' -UseBasicParsing"
    if errorlevel 1 ( echo ERROR: Download failed. & exit /b 1 )
)

echo Verifying...
for /f %%h in ('powershell -NoProfile -Command "(Get-FileHash -Path '%ARCHIVE_PATH%' -Algorithm SHA256).Hash.ToLower()"') do set "ACTUAL_SHA256=%%h"
echo SHA256: !ACTUAL_SHA256!

if exist "%SHA256_FILE%" (
    set /p EXPECTED_SHA256=<"%SHA256_FILE%"
    if /i "!ACTUAL_SHA256!" neq "!EXPECTED_SHA256!" (
        echo ERROR: SHA256 mismatch -- deleting cached archive.
        echo   Expected: !EXPECTED_SHA256!
        echo   Actual:   !ACTUAL_SHA256!
        del /q "%ARCHIVE_PATH%"
        exit /b 1
    )
    echo SHA256 verified.
) else (
    echo NOTE: tools\qemu_sha256.txt not found -- saving hash for future verification.
    echo       Commit this file so all developers verify against the same hash.
    echo !ACTUAL_SHA256!> "%SHA256_FILE%"
)

echo Extracting...
set "EXTRACT_TMP=%TEMP%\xsdk_qemu_extract"
if exist "%EXTRACT_TMP%" rmdir /s /q "%EXTRACT_TMP%"
powershell -NoProfile -Command ^
    "Expand-Archive -Path '%ARCHIVE_PATH%' -DestinationPath '%EXTRACT_TMP%' -Force"
if errorlevel 1 ( echo ERROR: Extraction failed. & exit /b 1 )

if exist "%QEMU_OUT%" rmdir /s /q "%QEMU_OUT%"
move "%EXTRACT_TMP%\%QEMU_PREFIX%" "%QEMU_OUT%" >nul
rmdir /s /q "%EXTRACT_TMP%" 2>nul

echo.
echo Done. QEMU Arm %QEMU_VERSION% installed to tools\qemu\
echo   qemu-system-arm.exe        -- 32-bit ARM machines
echo   qemu-system-aarch64.exe    -- xlnx-zynqmp Cortex-R5F firmware validation
echo.
echo Example R5F xRTOS smoke test:
echo   tools\qemu\bin\qemu-system-aarch64.exe -machine xlnx-zynqmp -cpu cortex-r5f ^
echo     -kernel build\qemu\xrtos_qemu.elf -nographic -serial mon:stdio

endlocal
