@echo off
setlocal enabledelayedexpansion

:: setup_cmake.bat - Download CMake 4.3.2 + Ninja 1.13.2 (downloaded separately; CMake 4.x no longer bundles it).
:: The archive is cached in tools\.cache\ and reused on subsequent runs.
:: Requires Windows 10+ (PowerShell Expand-Archive).
::
:: Output:
::   tools\cmake\bin\cmake.exe
::   tools\cmake\bin\ctest.exe
::   tools\cmake\bin\ninja.exe
::   tools\cmake_sha256.txt    (committed; verified on every run)
::
:: Usage:
::   tools\setup_cmake.bat           skip if already present
::   tools\setup_cmake.bat --force   force re-download (clears cache)

set "CMAKE_VERSION=4.3.2"
set "CMAKE_ARCHIVE=cmake-%CMAKE_VERSION%-windows-x86_64.zip"
set "CMAKE_URL=https://github.com/Kitware/CMake/releases/download/v%CMAKE_VERSION%/%CMAKE_ARCHIVE%"
set "CMAKE_PREFIX=cmake-%CMAKE_VERSION%-windows-x86_64"

set "NINJA_VERSION=1.13.2"
set "NINJA_ARCHIVE=ninja-win.zip"
set "NINJA_URL=https://github.com/ninja-build/ninja/releases/download/v%NINJA_VERSION%/%NINJA_ARCHIVE%"

set "SCRIPT_DIR=%~dp0"
set "CMAKE_OUT=%SCRIPT_DIR%cmake"
set "CACHE_DIR=%SCRIPT_DIR%.cache"
set "ARCHIVE_PATH=%CACHE_DIR%\%CMAKE_ARCHIVE%"
set "SHA256_FILE=%SCRIPT_DIR%sha256\cmake_sha256.txt"

if exist "%CMAKE_OUT%\bin\cmake.exe" if exist "%CMAKE_OUT%\bin\ninja.exe" (
    if /i not "%~1"=="--force" (
        echo cmake.exe and ninja.exe already present in tools\cmake\bin\
        echo Run with --force to re-download.
        exit /b 0
    )
)

if /i "%~1"=="--force" ( if exist "%ARCHIVE_PATH%" del /q "%ARCHIVE_PATH%" )
if not exist "%CACHE_DIR%" mkdir "%CACHE_DIR%"

if exist "%ARCHIVE_PATH%" (
    echo Using cached archive: %CMAKE_ARCHIVE%
) else (
    echo Downloading CMake %CMAKE_VERSION% ^(~50 MB^)...
    powershell -NoProfile -Command ^
        "$ProgressPreference='SilentlyContinue';" ^
        "Invoke-WebRequest -Uri '%CMAKE_URL%' -OutFile '%ARCHIVE_PATH%' -UseBasicParsing"
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
    echo NOTE: tools\cmake_sha256.txt not found -- saving hash for future verification.
    echo       Commit this file so all developers verify against the same hash.
    echo !ACTUAL_SHA256!> "%SHA256_FILE%"
)

echo Extracting...
set "EXTRACT_TMP=%TEMP%\xsdk_cmake_extract"
if exist "%EXTRACT_TMP%" rmdir /s /q "%EXTRACT_TMP%"
powershell -NoProfile -Command ^
    "Expand-Archive -Path '%ARCHIVE_PATH%' -DestinationPath '%EXTRACT_TMP%' -Force"
if errorlevel 1 ( echo ERROR: Extraction failed. & exit /b 1 )

if exist "%CMAKE_OUT%" rmdir /s /q "%CMAKE_OUT%"
move "%EXTRACT_TMP%\%CMAKE_PREFIX%" "%CMAKE_OUT%" >nul
rmdir /s /q "%EXTRACT_TMP%" 2>nul

:: CMake 4.x no longer bundles Ninja -- download it separately
set "NINJA_ARCHIVE_PATH=%CACHE_DIR%\%NINJA_ARCHIVE%"
if exist "%NINJA_ARCHIVE_PATH%" (
    echo Using cached archive: %NINJA_ARCHIVE%
) else (
    echo Downloading Ninja %NINJA_VERSION% ^(~300 KB^)...
    powershell -NoProfile -Command ^
        "$ProgressPreference='SilentlyContinue';" ^
        "Invoke-WebRequest -Uri '%NINJA_URL%' -OutFile '%NINJA_ARCHIVE_PATH%' -UseBasicParsing"
    if errorlevel 1 ( echo ERROR: Ninja download failed. & exit /b 1 )
)

echo Extracting Ninja...
powershell -NoProfile -Command ^
    "Expand-Archive -Path '%NINJA_ARCHIVE_PATH%' -DestinationPath '%CMAKE_OUT%\bin' -Force"
if errorlevel 1 ( echo ERROR: Ninja extraction failed. & exit /b 1 )

echo.
echo Done. CMake %CMAKE_VERSION% + Ninja %NINJA_VERSION% installed to tools\cmake\
echo   cmake.exe  ctest.exe  ninja.exe  -- available via xsdk.bat

endlocal
