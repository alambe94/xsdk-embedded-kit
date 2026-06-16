@echo off
setlocal enabledelayedexpansion

:: setup_cppcheck.bat -- Download and install cppcheck 2.14.2 (Windows x64, MSI).
:: Uses msiexec administrative install (/a) -- no elevation required.
:: The MSI is cached in tools\.cache\ and reused on subsequent runs.
::
:: Output:
::   tools\cppcheck\cppcheck.exe
::   tools\cppcheck_sha256.txt    (committed; verified on every run)
::
:: Usage:
::   tools\setup_cppcheck.bat           skip if already present
::   tools\setup_cppcheck.bat --force   force re-download (clears cache)

set "CPPCHECK_VERSION=2.14.2"
set "CPPCHECK_ARCHIVE=cppcheck-%CPPCHECK_VERSION%-x64-Setup.msi"
set "CPPCHECK_URL=https://github.com/danmar/cppcheck/releases/download/%CPPCHECK_VERSION%/%CPPCHECK_ARCHIVE%"

set "SCRIPT_DIR=%~dp0"
set "CPPCHECK_OUT=%SCRIPT_DIR%cppcheck"
set "CACHE_DIR=%SCRIPT_DIR%.cache"
set "INSTALLER_PATH=%CACHE_DIR%\%CPPCHECK_ARCHIVE%"
set "SHA256_FILE=%SCRIPT_DIR%sha256\cppcheck_sha256.txt"
set "EXTRACT_TMP=C:\xSDK_CPPCHECK_TMP"

if exist "%CPPCHECK_OUT%\cppcheck.exe" (
    if /i not "%~1"=="--force" (
        echo cppcheck.exe already present in tools\cppcheck\
        echo Run with --force to re-download.
        exit /b 0
    )
)

if /i "%~1"=="--force" ( if exist "%INSTALLER_PATH%" del /q "%INSTALLER_PATH%" )
if not exist "%CACHE_DIR%" mkdir "%CACHE_DIR%"

if exist "%INSTALLER_PATH%" (
    echo Using cached installer: %CPPCHECK_ARCHIVE%
) else (
    echo Downloading cppcheck %CPPCHECK_VERSION% ^(~30 MB^)...
    powershell -NoProfile -Command ^
        "$ProgressPreference='SilentlyContinue';" ^
        "Invoke-WebRequest -Uri '%CPPCHECK_URL%' -OutFile '%INSTALLER_PATH%' -UseBasicParsing"
    if errorlevel 1 ( echo ERROR: Download failed. & exit /b 1 )
)

echo Verifying...
for /f %%h in ('powershell -NoProfile -Command "(Get-FileHash -Path '%INSTALLER_PATH%' -Algorithm SHA256).Hash.ToLower()"') do set "ACTUAL_SHA256=%%h"
echo SHA256: !ACTUAL_SHA256!

if exist "%SHA256_FILE%" (
    set /p EXPECTED_SHA256=<"%SHA256_FILE%"
    if /i "!ACTUAL_SHA256!" neq "!EXPECTED_SHA256!" (
        echo ERROR: SHA256 mismatch -- deleting cached installer.
        echo   Expected: !EXPECTED_SHA256!
        echo   Actual:   !ACTUAL_SHA256!
        del /q "%INSTALLER_PATH%"
        exit /b 1
    )
    echo SHA256 verified.
) else (
    echo NOTE: tools\cppcheck_sha256.txt not found -- saving hash for future verification.
    echo       Commit this file so all developers verify against the same hash.
    echo !ACTUAL_SHA256!> "%SHA256_FILE%"
)

:: Administrative install -- extracts MSI contents without elevation.
:: MSI layout: PFiles\Cppcheck\ contains cppcheck.exe and all dependencies.
echo Extracting to %EXTRACT_TMP%...
if exist "%EXTRACT_TMP%" rmdir /s /q "%EXTRACT_TMP%"
msiexec /a "%INSTALLER_PATH%" TARGETDIR="%EXTRACT_TMP%" /quiet /norestart
if errorlevel 1 ( echo ERROR: msiexec extraction failed. & exit /b 1 )

if not exist "%EXTRACT_TMP%\PFiles\Cppcheck\cppcheck.exe" (
    echo ERROR: cppcheck.exe not found at expected path PFiles\Cppcheck\.
    rmdir /s /q "%EXTRACT_TMP%"
    exit /b 1
)

if exist "%CPPCHECK_OUT%" rmdir /s /q "%CPPCHECK_OUT%"
move "%EXTRACT_TMP%\PFiles\Cppcheck" "%CPPCHECK_OUT%" >nul
if errorlevel 1 ( echo ERROR: Move to tools\cppcheck\ failed. & exit /b 1 )
if exist "%EXTRACT_TMP%" rmdir /s /q "%EXTRACT_TMP%"

echo.
"%CPPCHECK_OUT%\cppcheck.exe" --version
echo.
echo Done. cppcheck %CPPCHECK_VERSION% installed to tools\cppcheck\
echo   Use via: xsdk.bat cppcheck

endlocal
