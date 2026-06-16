@echo off
setlocal enabledelayedexpansion

:: setup.bat - Download and install all xSDK developer tools.
:: Run once when setting up a new machine. Each tool is gitignored locally;
:: only the SHA256 files are committed to track expected hashes.
::
:: Usage:
::   tools\setup.bat                 install all tools
::   tools\setup.bat llvm            install clang-format + clang-tidy
::   tools\setup.bat clang-format   install clang-format only
::   tools\setup.bat clang-tidy     install clang-tidy only
::   tools\setup.bat cmake           install cmake + ninja only
::   tools\setup.bat gcc             install host GCC (MinGW-w64) only
::   tools\setup.bat arm-gcc         install arm-none-eabi-gcc only
::   tools\setup.bat riscv-gcc       install riscv-none-elf-gcc only
::   tools\setup.bat tiarmclang      install TI ARM Clang only
::   tools\setup.bat openocd         install OpenOCD only
::   tools\setup.bat openocd-wch    install WCH-patched OpenOCD (CH32H417 / RVSWD) only
::   tools\setup.bat ti-sdk          init TI mcupsdk_core submodule (sparse, ~59 MB)
::   tools\setup.bat ch32h417-sdk   init WCH CH32H417 SDK submodule (sparse, ~2 MB)
::   tools\setup.bat qemu            install QEMU (qemu-system-arm + qemu-system-aarch64) only
::   tools\setup.bat doxygen         install Doxygen only
::   tools\setup.bat gcovr           install gcovr (pip) only
::   tools\setup.bat lizard          install lizard (pip) only
::   tools\setup.bat codespell       install codespell (pip) only
::   tools\setup.bat cppcheck        install cppcheck only
::   tools\setup.bat --force         re-download all tools
::
:: After running, use xsdk.bat as normal - it prefers tools\ over system installs.

set "SCRIPT_DIR=%~dp0"
set "FORCE_FLAG="
if /i "%~2"=="--force" set "FORCE_FLAG=--force"
if "%~1"=="" (
    set "TARGET=all"
) else if /i "%~1"=="--force" (
    set "FORCE_FLAG=--force"
    set "TARGET=all"
) else (
    set "TARGET=%~1"
)

:: Resolve Python - prefer python, fall back to py -3, then typical LocalAppData paths
set "PYTHON_EXE="
set "PYTHON_ARGS="
python -c "import sys" >nul 2>&1 && set "PYTHON_EXE=python"
if not defined PYTHON_EXE (
    py -3 -c "import sys" >nul 2>&1 && set "PYTHON_EXE=py" && set "PYTHON_ARGS=-3"
)
if not defined PYTHON_EXE (
    if exist "%LocalAppData%\Python\bin\python.exe" set "PYTHON_EXE=%LocalAppData%\Python\bin\python.exe"
)
if not defined PYTHON_EXE (
    if exist "%LocalAppData%\Programs\Python\Python310\python.exe" set "PYTHON_EXE=%LocalAppData%\Programs\Python\Python310\python.exe"
)

echo xSDK Tool Setup
echo =================
echo.

set "FAILED=0"

if /i "%TARGET%"=="all"          goto :install_all
if /i "%TARGET%"=="llvm"         goto :install_llvm
if /i "%TARGET%"=="clang-format" goto :install_clang_format
if /i "%TARGET%"=="clang-tidy"   goto :install_clang_tidy
if /i "%TARGET%"=="cmake"        goto :install_cmake
if /i "%TARGET%"=="gcc"          goto :install_gcc
if /i "%TARGET%"=="arm-gcc"      goto :install_arm_gcc
if /i "%TARGET%"=="riscv-gcc"    goto :install_riscv_gcc
if /i "%TARGET%"=="tiarmclang"   goto :install_tiarmclang
if /i "%TARGET%"=="openocd"      goto :install_openocd
if /i "%TARGET%"=="openocd-wch"  goto :install_openocd_wch
if /i "%TARGET%"=="ti-sdk"       goto :install_ti_sdk
if /i "%TARGET%"=="ch32h417-sdk" goto :install_ch32h417_sdk
if /i "%TARGET%"=="qemu"         goto :install_qemu
if /i "%TARGET%"=="doxygen"      goto :install_doxygen
if /i "%TARGET%"=="gcovr"        goto :install_gcovr
if /i "%TARGET%"=="lizard"       goto :install_lizard
if /i "%TARGET%"=="codespell"    goto :install_codespell
if /i "%TARGET%"=="cppcheck"     goto :install_cppcheck
if /i "%TARGET%"=="misra"        goto :install_misra

echo ERROR: Unknown target '%TARGET%'
echo Valid targets: llvm, clang-format, clang-tidy, cmake, gcc, arm-gcc, riscv-gcc, tiarmclang, openocd, openocd-wch, ti-sdk, ch32h417-sdk, qemu, doxygen, gcovr, lizard, codespell, cppcheck, misra, or omit for all.
exit /b 1

:install_all
call :run_setup llvm
call :run_setup cmake
call :run_setup gcc
call :run_setup arm-gcc
call :run_setup riscv-gcc
call :run_setup tiarmclang
call :run_setup openocd
call :run_setup openocd-wch
call :run_setup ti-sdk
call :run_setup ch32h417-sdk
call :run_setup qemu
call :run_setup doxygen
call :run_setup gcovr
call :run_setup lizard
call :run_setup codespell
call :run_setup cppcheck
call :run_setup misra
goto :summary

:install_llvm
call :run_setup llvm
goto :summary

:install_clang_format
call "%SCRIPT_DIR%setup_llvm.bat" clang-format %FORCE_FLAG%
goto :summary

:install_clang_tidy
call "%SCRIPT_DIR%setup_llvm.bat" clang-tidy %FORCE_FLAG%
goto :summary

:install_cmake
call :run_setup cmake
goto :summary

:install_gcc
call :run_setup gcc
goto :summary

:install_arm_gcc
call :run_setup arm-gcc
goto :summary

:install_riscv_gcc
call :run_setup riscv-gcc
goto :summary

:install_tiarmclang
call :run_setup tiarmclang
goto :summary

:install_openocd
call :run_setup openocd
goto :summary

:install_openocd_wch
call :run_setup openocd-wch
goto :summary

:install_ti_sdk
call :run_setup ti-sdk
goto :summary

:install_ch32h417_sdk
call :run_setup ch32h417-sdk
goto :summary

:install_qemu
call :run_setup qemu
goto :summary

:install_doxygen
call :run_setup doxygen
goto :summary

:install_gcovr
call :run_setup gcovr
goto :summary

:install_lizard
call :run_setup lizard
goto :summary

:install_codespell
call :run_setup codespell
goto :summary

:install_cppcheck
call :run_setup cppcheck
goto :summary

:install_misra
call :run_setup misra
goto :summary

:: -----------------------------------------------------------------------
:run_setup
set "TOOL=%~1"
set "TOOL_FILE=%TOOL:-=_%"
set "TOOL_FILE=%TOOL_FILE: =_%"
echo [%TOOL%] Setting up...
call "%SCRIPT_DIR%setup_%TOOL_FILE%.bat" %FORCE_FLAG%
if errorlevel 1 (
    echo [%TOOL%] FAILED.
    set "FAILED=1"
) else (
    echo [%TOOL%] OK.
)
echo.
exit /b 0

:: -----------------------------------------------------------------------
:summary
echo =================
if !FAILED! neq 0 (
    echo One or more tools failed to install. See output above.
    exit /b 1
)
echo All tools installed successfully.
echo.
echo.
echo Build commands:
echo   xsdk.bat              -- host build
echo   xsdk.bat test         -- host build + tests
echo   xsdk.bat coverage     -- coverage build + HTML report (requires gcovr)
echo   xsdk.bat r5           -- Cortex-R5 GCC cross-compile
echo   xsdk.bat tiarmclang   -- Cortex-R5 TI ARM Clang cross-compile
echo   xsdk.bat ch32h417-riscv-gcc -- CH32H417 V5F minimal bring-up
echo   xsdk.bat docs         -- build Doxygen documentation
echo   xsdk.bat clang-tidy   -- static analysis
echo   xsdk.bat cppcheck     -- static analysis
echo   xsdk.bat format [comp] -- clang-format
echo.
echo Coverage tool:
echo   tools\setup.bat gcovr  -- install gcovr 7.2 (pip)
echo.
echo Complexity tool:
echo   tools\setup.bat lizard -- install lizard 1.22.1 (pip)
echo.
echo Spell check tool:
echo   tools\setup.bat codespell -- install codespell 2.3.0 (pip)
echo.
echo Static analysis tool:
echo   tools\setup.bat cppcheck  -- install cppcheck 2.14.2
echo.
echo QEMU firmware simulator:
echo   tools\setup.bat qemu      -- install xPack QEMU Arm 8.2.2-1
echo   qemu-system-aarch64.exe   -- xlnx-zynqmp Cortex-R5F firmware validation

endlocal
