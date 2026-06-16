@echo off
setlocal enabledelayedexpansion

:: setup_openocd.bat - Download xPack OpenOCD 0.12.0-4 (Windows x64).
:: The archive is cached in tools\.cache\ and reused on subsequent runs.
:: Requires Windows 10+ (PowerShell Expand-Archive).
::
:: Output:
::   tools\openocd\bin\openocd.exe
::   tools\openocd\scripts\          (target/interface configs)
::   tools\openocd_sha256.txt        (committed; verified on every run)
::
:: Usage:
::   tools\setup_openocd.bat           skip if already present
::   tools\setup_openocd.bat --force   force re-download (clears cache)

set "OPENOCD_VERSION=0.12.0-4"
set "OPENOCD_ARCHIVE=xpack-openocd-%OPENOCD_VERSION%-win32-x64.zip"
set "OPENOCD_URL=https://github.com/xpack-dev-tools/openocd-xpack/releases/download/v%OPENOCD_VERSION%/%OPENOCD_ARCHIVE%"
set "OPENOCD_PREFIX=xpack-openocd-%OPENOCD_VERSION%"

set "SCRIPT_DIR=%~dp0"
set "OPENOCD_OUT=%SCRIPT_DIR%openocd"
set "CACHE_DIR=%SCRIPT_DIR%.cache"
set "ARCHIVE_PATH=%CACHE_DIR%\%OPENOCD_ARCHIVE%"
set "SHA256_FILE=%SCRIPT_DIR%sha256\openocd_sha256.txt"

if exist "%OPENOCD_OUT%\bin\openocd.exe" (
    if /i not "%~1"=="--force" (
        echo openocd.exe already present in tools\openocd\bin\
        echo Run with --force to re-download.
        exit /b 0
    )
)

if /i "%~1"=="--force" ( if exist "%ARCHIVE_PATH%" del /q "%ARCHIVE_PATH%" )
if not exist "%CACHE_DIR%" mkdir "%CACHE_DIR%"

if exist "%ARCHIVE_PATH%" (
    echo Using cached archive: %OPENOCD_ARCHIVE%
) else (
    echo Downloading xPack OpenOCD %OPENOCD_VERSION% ^(~15 MB^)...
    powershell -NoProfile -Command ^
        "$ProgressPreference='SilentlyContinue';" ^
        "Invoke-WebRequest -Uri '%OPENOCD_URL%' -OutFile '%ARCHIVE_PATH%' -UseBasicParsing"
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
    echo NOTE: tools\openocd_sha256.txt not found -- saving hash for future verification.
    echo       Commit this file so all developers verify against the same hash.
    echo !ACTUAL_SHA256!> "%SHA256_FILE%"
)

echo Extracting...
set "EXTRACT_TMP=%TEMP%\xsdk_openocd_extract"
if exist "%EXTRACT_TMP%" rmdir /s /q "%EXTRACT_TMP%"
powershell -NoProfile -Command ^
    "Expand-Archive -Path '%ARCHIVE_PATH%' -DestinationPath '%EXTRACT_TMP%' -Force"
if errorlevel 1 ( echo ERROR: Extraction failed. & exit /b 1 )

if exist "%OPENOCD_OUT%" rmdir /s /q "%OPENOCD_OUT%"
move "%EXTRACT_TMP%\%OPENOCD_PREFIX%" "%OPENOCD_OUT%" >nul
rmdir /s /q "%EXTRACT_TMP%" 2>nul

echo.
echo Done. OpenOCD %OPENOCD_VERSION% installed to tools\openocd\
echo   openocd.exe   -- tools\openocd\bin\openocd.exe
echo   scripts/      -- tools\openocd\scripts\  ^(target and interface configs^)
echo.
echo Example usage:
echo   tools\openocd\bin\openocd.exe -f scripts\interface\xds110.cfg -f scripts\target\am64x.cfg

endlocal
