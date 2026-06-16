@echo off
setlocal enabledelayedexpansion

:: setup_gcc.bat - Download WinLibs MinGW-w64 GCC 14.2.0 (host builds only).
:: The archive is cached in tools\.cache\ and reused on subsequent runs.
:: Requires Windows 10+ (PowerShell Expand-Archive).
::
:: Output:
::   tools\gcc\bin\gcc.exe   (and full MinGW-w64 toolchain)
::
:: SHA256 of the archive is pinned in tools\gcc_sha256.txt and verified on every run.
::
:: License:
::   GCC - GPL v3. MinGW-w64 runtime - varies (mostly public domain / LGPL).
::   See tools\gcc\licenses\ for full details after extraction.
::
:: Usage:
::   tools\setup_gcc.bat           skip if already present
::   tools\setup_gcc.bat --force   force re-download (clears cache)

set "GCC_VERSION=14.2.0"
set "MINGW_VERSION=12.0.0"
set "GCC_REV=r1"
set "GCC_ARCHIVE=winlibs-x86_64-posix-seh-gcc-%GCC_VERSION%-mingw-w64ucrt-%MINGW_VERSION%-%GCC_REV%.zip"
set "GCC_URL=https://github.com/brechtsanders/winlibs_mingw/releases/download/%GCC_VERSION%posix-18.1.8-%MINGW_VERSION%-ucrt-%GCC_REV%/%GCC_ARCHIVE%"
set "GCC_PREFIX=mingw64"

set "SCRIPT_DIR=%~dp0"
set "GCC_OUT=%SCRIPT_DIR%gcc"
set "CACHE_DIR=%SCRIPT_DIR%.cache"
set "ARCHIVE_PATH=%CACHE_DIR%\%GCC_ARCHIVE%"
set "SHA256_FILE=%SCRIPT_DIR%sha256\gcc_sha256.txt"

if exist "%GCC_OUT%\bin\gcc.exe" (
    if /i not "%~1"=="--force" (
        echo gcc.exe already present in tools\gcc\bin\
        echo Run with --force to re-download.
        exit /b 0
    )
)

if /i "%~1"=="--force" ( if exist "%ARCHIVE_PATH%" del /q "%ARCHIVE_PATH%" )
if not exist "%CACHE_DIR%" mkdir "%CACHE_DIR%"

if exist "%ARCHIVE_PATH%" (
    echo Using cached archive: %GCC_ARCHIVE%
) else (
    echo Downloading MinGW-w64 GCC %GCC_VERSION% ^(~150 MB^)...
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
    echo NOTE: tools\gcc_sha256.txt not found -- saving hash for future verification.
    echo       Commit this file so all developers verify against the same hash.
    echo !ACTUAL_SHA256!> "%SHA256_FILE%"
)

echo Extracting ^(~500 MB uncompressed^)...
set "EXTRACT_TMP=%TEMP%\xsdk_gcc_extract"
if exist "%EXTRACT_TMP%" rmdir /s /q "%EXTRACT_TMP%"
powershell -NoProfile -Command ^
    "Expand-Archive -Path '%ARCHIVE_PATH%' -DestinationPath '%EXTRACT_TMP%' -Force"
if errorlevel 1 ( echo ERROR: Extraction failed. & exit /b 1 )

if exist "%GCC_OUT%" rmdir /s /q "%GCC_OUT%"
move "%EXTRACT_TMP%\%GCC_PREFIX%" "%GCC_OUT%" >nul
rmdir /s /q "%EXTRACT_TMP%" 2>nul

echo.
echo Done. GCC %GCC_VERSION% installed to tools\gcc\
echo   Use via: xsdk.bat  (host build and tests)

endlocal
