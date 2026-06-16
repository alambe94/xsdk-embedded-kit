@echo off
setlocal enabledelayedexpansion

:: setup_llvm.bat - Download clang-format.exe and/or clang-tidy.exe (LLVM 18.1.8).
:: Both tools come from the same archive; one download covers either or both.
:: The archive is cached in tools\.cache\ and reused on subsequent runs.
:: Requires Windows 10 version 1803+ (built-in tar.exe with xz support).
::
:: Usage:
::   tools\setup_llvm.bat                    install clang-format + clang-tidy
::   tools\setup_llvm.bat clang-format       install clang-format only
::   tools\setup_llvm.bat clang-tidy         install clang-tidy only
::   tools\setup_llvm.bat [tool] --force     force re-download (clears cache)
::
:: Output:
::   tools\llvm\clang-format.exe    (gitignored, generated locally)
::   tools\llvm\clang-tidy.exe      (gitignored, generated locally)
::
:: SHA256 of the archive is pinned in tools\llvm_sha256.txt and verified on every run.
::
:: License:
::   LLVM is distributed under the Apache License v2.0 with LLVM Exceptions.
::   https://llvm.org/LICENSE.txt

set "LLVM_VERSION=18.1.8"
set "LLVM_TAG=llvmorg-18.1.8"
set "LLVM_ARCHIVE=clang+llvm-18.1.8-x86_64-pc-windows-msvc.tar.xz"
set "LLVM_URL=https://github.com/llvm/llvm-project/releases/download/%LLVM_TAG%/%LLVM_ARCHIVE%"
set "LLVM_PREFIX=clang+llvm-18.1.8-x86_64-pc-windows-msvc"

set "SCRIPT_DIR=%~dp0"
set "LLVM_OUT=%SCRIPT_DIR%llvm"
set "CACHE_DIR=%SCRIPT_DIR%.cache"
set "ARCHIVE_PATH=%CACHE_DIR%\%LLVM_ARCHIVE%"
set "SHA256_FILE=%SCRIPT_DIR%sha256\llvm_sha256.txt"

:: Parse arguments (order-independent)
set "TOOL="
set "FORCE_FLAG="
:parse_args
if "%~1"=="" goto :args_done
if /i "%~1"=="--force" ( set "FORCE_FLAG=--force" ) else ( set "TOOL=%~1" )
shift
goto :parse_args
:args_done

:: Determine which exe(s) to install
if /i "!TOOL!"=="clang-format" (
    set "DISPLAY=clang-format"
    set "CHECK_EXE=%LLVM_OUT%\clang-format.exe"
) else if /i "!TOOL!"=="clang-tidy" (
    set "DISPLAY=clang-tidy"
    set "CHECK_EXE=%LLVM_OUT%\clang-tidy.exe"
) else (
    set "DISPLAY=clang-format + clang-tidy"
    set "CHECK_EXE="
)

:: Skip if already installed (unless --force)
if not defined CHECK_EXE (
    if exist "%LLVM_OUT%\clang-format.exe" if exist "%LLVM_OUT%\clang-tidy.exe" (
        if not defined FORCE_FLAG (
            echo clang-format.exe and clang-tidy.exe already present in tools\llvm\
            echo Run with --force to re-download.
            exit /b 0
        )
    )
) else (
    if exist "!CHECK_EXE!" (
        if not defined FORCE_FLAG (
            echo !DISPLAY! already present in tools\llvm\
            echo Run with --force to re-download.
            exit /b 0
        )
    )
)

:: Require tar.exe (built in since Windows 10 1803)
where tar >nul 2>&1
if errorlevel 1 (
    echo ERROR: tar.exe not found. Requires Windows 10 version 1803 or later.
    exit /b 1
)

:: Download only if not already cached (or --force clears the cache)
if defined FORCE_FLAG (
    if exist "%ARCHIVE_PATH%" del /q "%ARCHIVE_PATH%"
)

if not exist "%CACHE_DIR%" mkdir "%CACHE_DIR%"

if exist "%ARCHIVE_PATH%" (
    echo Using cached archive: %LLVM_ARCHIVE%
) else (
    echo Downloading LLVM %LLVM_VERSION% ^(~260 MB^)...
    powershell -NoProfile -Command ^
        "$ProgressPreference='SilentlyContinue';" ^
        "Invoke-WebRequest -Uri '%LLVM_URL%' -OutFile '%ARCHIVE_PATH%' -UseBasicParsing"
    if errorlevel 1 ( echo ERROR: Download failed. & exit /b 1 )
)

:: Verify SHA256
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
    echo NOTE: tools\llvm_sha256.txt not found -- saving hash for future verification.
    echo       Commit this file so all developers verify against the same hash.
    echo !ACTUAL_SHA256!> "%SHA256_FILE%"
)

:: Extract the requested executable(s) - each tar call is explicit to avoid
:: variable-expansion splitting bugs when two paths are passed as one variable.
echo Extracting !DISPLAY!...
if not exist "%LLVM_OUT%" mkdir "%LLVM_OUT%"

if /i "!TOOL!"=="clang-format" (
    tar -xf "%ARCHIVE_PATH%" --strip-components=2 -C "%LLVM_OUT%" "%LLVM_PREFIX%/bin/clang-format.exe"
) else if /i "!TOOL!"=="clang-tidy" (
    tar -xf "%ARCHIVE_PATH%" --strip-components=2 -C "%LLVM_OUT%" "%LLVM_PREFIX%/bin/clang-tidy.exe"
) else (
    tar -xf "%ARCHIVE_PATH%" --strip-components=2 -C "%LLVM_OUT%" "%LLVM_PREFIX%/bin/clang-format.exe" "%LLVM_PREFIX%/bin/clang-tidy.exe"
)
if errorlevel 1 (
    echo ERROR: Extraction failed. tar may not support xz on this system.
    exit /b 1
)

echo.
echo Done. LLVM %LLVM_VERSION% -- !DISPLAY! installed to tools\llvm\
if /i "!TOOL!"=="clang-format" ( echo   clang-format.exe -- used by xsdk.bat format )
if /i "!TOOL!"=="clang-tidy"   ( echo   clang-tidy.exe   -- used by xsdk.bat clang-tidy )
if not defined TOOL (
    echo   clang-format.exe -- used by xsdk.bat format
    echo   clang-tidy.exe   -- used by xsdk.bat clang-tidy
)
echo.
echo Archive cached at: tools\.cache\%LLVM_ARCHIVE%
echo Run with --force to discard the cache and re-download.

endlocal
