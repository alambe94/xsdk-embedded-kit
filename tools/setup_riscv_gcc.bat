@echo off
setlocal enabledelayedexpansion

:: setup_riscv_gcc.bat - Download RISC-V GCC 15.2.0 (xPack, Windows x64).

set "GCC_VERSION=15.2.0-1"
set "GCC_ARCHIVE=xpack-riscv-none-elf-gcc-%GCC_VERSION%-win32-x64.zip"
set "GCC_URL=https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases/download/v%GCC_VERSION%/%GCC_ARCHIVE%"
set "GCC_PREFIX=xpack-riscv-none-elf-gcc-%GCC_VERSION%"

set "SCRIPT_DIR=%~dp0"
set "GCC_OUT=%SCRIPT_DIR%riscv_gcc"
set "CACHE_DIR=%SCRIPT_DIR%.cache"
set "ARCHIVE_PATH=%CACHE_DIR%\%GCC_ARCHIVE%"
set "SHA256_FILE=%SCRIPT_DIR%sha256\riscv_gcc_sha256.txt"

if exist "%GCC_OUT%\bin\riscv-none-elf-gcc.exe" (
    if /i not "%~1"=="--force" (
        echo riscv-none-elf-gcc.exe already present in tools\riscv_gcc\bin\
        echo Run with --force to re-download.
        exit /b 0
    )
)

if /i "%~1"=="--force" ( if exist "%ARCHIVE_PATH%" del /q "%ARCHIVE_PATH%" )
if not exist "%CACHE_DIR%" mkdir "%CACHE_DIR%"

if exist "%ARCHIVE_PATH%" (
    echo Using cached archive: %GCC_ARCHIVE%
) else (
    echo Downloading RISC-V GCC %GCC_VERSION%...
    powershell -NoProfile -Command ^
        "$ProgressPreference='SilentlyContinue';" ^
        "Invoke-WebRequest -Uri '%GCC_URL%' -OutFile '%ARCHIVE_PATH%' -UseBasicParsing"
    if errorlevel 1 ( echo ERROR: Download failed. & exit /b 1 )
)

echo Verifying...
for /f %%h in ('powershell -NoProfile -Command "(Get-FileHash -Path '%ARCHIVE_PATH%' -Algorithm SHA256).Hash.ToLower()"') do set "ACTUAL_SHA256=%%h"
set /p EXPECTED_SHA256=<"%SHA256_FILE%"
if /i "!ACTUAL_SHA256!" neq "!EXPECTED_SHA256!" (
    echo ERROR: SHA256 mismatch -- deleting cached archive.
    del /q "%ARCHIVE_PATH%"
    exit /b 1
)
echo SHA256 verified.

set "EXTRACT_TMP=%TEMP%\xsdk_riscv_gcc_extract"
if exist "%EXTRACT_TMP%" rmdir /s /q "%EXTRACT_TMP%"
powershell -NoProfile -Command ^
    "Expand-Archive -Path '%ARCHIVE_PATH%' -DestinationPath '%EXTRACT_TMP%' -Force"
if errorlevel 1 ( echo ERROR: Extraction failed. & exit /b 1 )

if exist "%GCC_OUT%" rmdir /s /q "%GCC_OUT%"
move "%EXTRACT_TMP%\%GCC_PREFIX%" "%GCC_OUT%" >nul
rmdir /s /q "%EXTRACT_TMP%" 2>nul

echo.
echo Done. RISC-V GCC %GCC_VERSION% installed to tools\riscv_gcc\
echo   Use via: xsdk.bat ch32h417-riscv-gcc

endlocal
