@echo off
setlocal enabledelayedexpansion

:: setup_doxygen.bat - Download Doxygen 1.12.0 (Windows x64, standalone zip).
:: The archive is cached in tools\.cache\ and reused on subsequent runs.
:: Requires Windows 10+ (PowerShell Expand-Archive).
::
:: Output:
::   tools\doxygen\doxygen.exe
::   tools\doxygen_sha256.txt    (committed; verified on every run)
::
:: Optional - install Graphviz separately for call/include graphs:
::   winget install graphviz
::   (Doxygen works without it; graphs are skipped if dot.exe is not on PATH.)
::
:: Usage:
::   tools\setup_doxygen.bat           skip if already present
::   tools\setup_doxygen.bat --force   force re-download (clears cache)

set "DOXYGEN_VERSION=1.12.0"
set "DOXYGEN_ARCHIVE=doxygen-%DOXYGEN_VERSION%.windows.x64.bin.zip"
set "DOXYGEN_URL=https://github.com/doxygen/doxygen/releases/download/Release_%DOXYGEN_VERSION:.=_%/%DOXYGEN_ARCHIVE%"

set "SCRIPT_DIR=%~dp0"
set "DOXYGEN_OUT=%SCRIPT_DIR%doxygen"
set "CACHE_DIR=%SCRIPT_DIR%.cache"
set "ARCHIVE_PATH=%CACHE_DIR%\%DOXYGEN_ARCHIVE%"
set "SHA256_FILE=%SCRIPT_DIR%sha256\doxygen_sha256.txt"

if exist "%DOXYGEN_OUT%\doxygen.exe" (
    if /i not "%~1"=="--force" (
        echo doxygen.exe already present in tools\doxygen\
        echo Run with --force to re-download.
        exit /b 0
    )
)

if /i "%~1"=="--force" ( if exist "%ARCHIVE_PATH%" del /q "%ARCHIVE_PATH%" )
if not exist "%CACHE_DIR%" mkdir "%CACHE_DIR%"

if exist "%ARCHIVE_PATH%" (
    echo Using cached archive: %DOXYGEN_ARCHIVE%
) else (
    echo Downloading Doxygen %DOXYGEN_VERSION% ^(~50 MB^)...
    powershell -NoProfile -Command ^
        "$ProgressPreference='SilentlyContinue';" ^
        "Invoke-WebRequest -Uri '%DOXYGEN_URL%' -OutFile '%ARCHIVE_PATH%' -UseBasicParsing"
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
    echo NOTE: tools\doxygen_sha256.txt not found -- saving hash for future verification.
    echo       Commit this file so all developers verify against the same hash.
    echo !ACTUAL_SHA256!> "%SHA256_FILE%"
)

:: Doxygen zip extracts flat (no prefix subdirectory)
echo Extracting...
if exist "%DOXYGEN_OUT%" rmdir /s /q "%DOXYGEN_OUT%"
mkdir "%DOXYGEN_OUT%"
powershell -NoProfile -Command ^
    "Expand-Archive -Path '%ARCHIVE_PATH%' -DestinationPath '%DOXYGEN_OUT%' -Force"
if errorlevel 1 ( echo ERROR: Extraction failed. & exit /b 1 )

:: Check for Graphviz (dot.exe) on PATH - optional but recommended
where dot >nul 2>&1
if errorlevel 1 (
    echo.
    echo NOTE: Graphviz ^(dot.exe^) not found on PATH.
    echo       Call graphs in Doxygen output will be disabled.
    echo       Install with: winget install graphviz
)

echo.
echo Done. Doxygen %DOXYGEN_VERSION% installed to tools\doxygen\
echo   Use via: xsdk.bat docs

endlocal
exit /b 0
