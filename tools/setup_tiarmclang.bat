@echo off
setlocal enabledelayedexpansion

:: setup_tiarmclang.bat - Download and silently install TI ARM Clang 5.0.0.STS (Windows).
:: The installer is cached in tools\.cache\ and reused on subsequent runs.
:: Uses BitRock/InstallAnywhere unattended mode (--mode unattended --prefix <dir>).
::
:: Output:
::   tools\tiarmclang\bin\tiarmclang.exe  (and full toolchain)
::   tools\tiarmclang_sha256.txt          (committed; verified on every run)
::
:: Usage:
::   tools\setup_tiarmclang.bat           skip if already present
::   tools\setup_tiarmclang.bat --force   force re-download (clears cache)

set "TI_VERSION=5.0.0.STS"
set "TI_INSTALLER=ti_cgt_armllvm_%TI_VERSION%_windows-x64_installer.exe"
set "TI_URL=https://dr-download.ti.com/software-development/ide-configuration-compiler-or-debugger/MD-ayxs93eZNN/%TI_VERSION%/%TI_INSTALLER%"

set "SCRIPT_DIR=%~dp0"
set "TI_OUT=%SCRIPT_DIR%tiarmclang"
set "CACHE_DIR=%SCRIPT_DIR%.cache"
set "INSTALLER_PATH=%CACHE_DIR%\%TI_INSTALLER%"
set "SHA256_FILE=%SCRIPT_DIR%sha256\tiarmclang_sha256.txt"

if exist "%TI_OUT%\bin\tiarmclang.exe" (
    if /i not "%~1"=="--force" (
        echo tiarmclang.exe already present in tools\tiarmclang\bin\
        echo Run with --force to re-download.
        exit /b 0
    )
)

if /i "%~1"=="--force" ( if exist "%INSTALLER_PATH%" del /q "%INSTALLER_PATH%" )
if not exist "%CACHE_DIR%" mkdir "%CACHE_DIR%"

if exist "%INSTALLER_PATH%" (
    echo Using cached installer: %TI_INSTALLER%
) else (
    echo Downloading TI ARM Clang %TI_VERSION% ^(~500 MB^)...
    powershell -NoProfile -Command ^
        "$ProgressPreference='SilentlyContinue';" ^
        "Invoke-WebRequest -Uri '%TI_URL%' -OutFile '%INSTALLER_PATH%' -UseBasicParsing"
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
    echo NOTE: tools\tiarmclang_sha256.txt not found -- saving hash for future verification.
    echo       Commit this file so all developers verify against the same hash.
    echo !ACTUAL_SHA256!> "%SHA256_FILE%"
)

:: Install silently using BitRock/InstallAnywhere unattended mode
echo Installing to %TI_OUT%...
if exist "%TI_OUT%" rmdir /s /q "%TI_OUT%"
"%INSTALLER_PATH%" --mode unattended --unattendedmodeui none --prefix "%TI_OUT%"
if errorlevel 1 ( echo ERROR: Installation failed. & exit /b 1 )

:: Installer creates a versioned subdir (ti-cgt-armllvm_<ver>\) - flatten it up
set "TI_VERSIONED=%TI_OUT%\ti-cgt-armllvm_%TI_VERSION%"
if exist "%TI_VERSIONED%\bin\tiarmclang.exe" (
    robocopy "%TI_VERSIONED%" "%TI_OUT%" /e /move /nfl /ndl /njh /njs >nul
    if exist "%TI_VERSIONED%" rmdir /s /q "%TI_VERSIONED%"
)

if not exist "%TI_OUT%\bin\tiarmclang.exe" (
    echo ERROR: tiarmclang.exe not found after installation.
    echo        The installer may have used a different layout.
    exit /b 1
)

echo.
echo Done. TI ARM Clang %TI_VERSION% installed to tools\tiarmclang\
echo   Use via: xsdk.bat tiarmclang

endlocal
