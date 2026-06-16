@echo off
setlocal enabledelayedexpansion

:: setup_arm_gcc.bat - Download arm-none-eabi-gcc 13.3.1 (xPack, Windows x64).
:: The archive is cached in tools\.cache\ and reused on subsequent runs.
:: Requires Windows 10+ (PowerShell Expand-Archive).
::
:: Output:
::   tools\arm_gcc\bin\arm-none-eabi-gcc.exe  (and full toolchain)
::   tools\arm_gcc_sha256.txt                 (committed; verified on every run)
::
:: Usage:
::   tools\setup_arm_gcc.bat           skip if already present
::   tools\setup_arm_gcc.bat --force   force re-download (clears cache)

set "GCC_VERSION=13.3.1-1.1"
set "GCC_ARCHIVE=xpack-arm-none-eabi-gcc-%GCC_VERSION%-win32-x64.zip"
set "GCC_URL=https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v%GCC_VERSION%/%GCC_ARCHIVE%"
set "GCC_PREFIX=xpack-arm-none-eabi-gcc-%GCC_VERSION%"

set "SCRIPT_DIR=%~dp0"
set "GCC_OUT=%SCRIPT_DIR%arm_gcc"
set "CACHE_DIR=%SCRIPT_DIR%.cache"
set "ARCHIVE_PATH=%CACHE_DIR%\%GCC_ARCHIVE%"
set "SHA256_FILE=%SCRIPT_DIR%sha256\arm_gcc_sha256.txt"

if exist "%GCC_OUT%\bin\arm-none-eabi-gcc.exe" (
    if /i not "%~1"=="--force" (
        echo arm-none-eabi-gcc.exe already present in tools\arm_gcc\bin\
        echo Run with --force to re-download.
        exit /b 0
    )
)

if /i "%~1"=="--force" ( if exist "%ARCHIVE_PATH%" del /q "%ARCHIVE_PATH%" )
if not exist "%CACHE_DIR%" mkdir "%CACHE_DIR%"

if exist "%ARCHIVE_PATH%" (
    echo Using cached archive: %GCC_ARCHIVE%
) else (
    echo Downloading arm-none-eabi-gcc %GCC_VERSION% ^(~130 MB^)...
    powershell -NoProfile -Command ^
        "$ProgressPreference='SilentlyContinue';" ^
        "Invoke-WebRequest -Uri '%GCC_URL%' -OutFile '%ARCHIVE_PATH%' -UseBasicParsing"
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
    echo NOTE: tools\arm_gcc_sha256.txt not found -- saving hash for future verification.
    echo       Commit this file so all developers verify against the same hash.
    echo !ACTUAL_SHA256!> "%SHA256_FILE%"
)

echo Extracting ^(~400 MB uncompressed^)...
set "EXTRACT_TMP=%TEMP%\xsdk_arm_gcc_extract"
if exist "%EXTRACT_TMP%" rmdir /s /q "%EXTRACT_TMP%"
powershell -NoProfile -Command ^
    "Expand-Archive -Path '%ARCHIVE_PATH%' -DestinationPath '%EXTRACT_TMP%' -Force"
if errorlevel 1 ( echo ERROR: Extraction failed. & exit /b 1 )

if exist "%GCC_OUT%" rmdir /s /q "%GCC_OUT%"
move "%EXTRACT_TMP%\%GCC_PREFIX%" "%GCC_OUT%" >nul
rmdir /s /q "%EXTRACT_TMP%" 2>nul

echo.
echo Done. arm-none-eabi-gcc %GCC_VERSION% installed to tools\arm_gcc\
echo   Use via: xsdk.bat r5

endlocal
