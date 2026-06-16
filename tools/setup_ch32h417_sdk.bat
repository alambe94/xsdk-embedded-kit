@echo off
setlocal

:: setup_ch32h417_sdk.bat - Initialize WCH CH32H417 SDK submodule (sparse checkout).
::
:: Source: https://github.com/openwch/ch32h417  (shallow, pinned by .gitmodules commit)
::
:: Sparse paths (minimal — device header + core only, no peripheral HAL library):
::   EVT/EXAM/SRC/Peripheral/inc/ch32h417.h  - device register definitions
::   EVT/EXAM/SRC/Core/                        - RISC-V core headers + core_riscv.c
::
:: Drivers use ch32h417.h directly (register-level access); the peripheral HAL
:: library files (ch32h417_usart.h, ch32h417_rcc.h, etc.) are not included.
::
:: Usage:
::   tools\setup_ch32h417_sdk.bat           skip if already present
::   tools\setup_ch32h417_sdk.bat --force   re-init (deinit + re-clone)

set "SCRIPT_DIR=%~dp0"
set "SDK_ROOT=%SCRIPT_DIR%.."
set "SUBMOD=src/third_party/wch/ch32h417"
set "SUBMOD_DIR=%SDK_ROOT%\src\third_party\wch\ch32h417"
set "SENTINEL=%SUBMOD_DIR%\EVT\EXAM\SRC\Peripheral\inc\ch32h417.h"

if exist "%SENTINEL%" (
    if /i not "%~1"=="--force" (
        echo CH32H417 SDK already initialized.
        echo Run with --force to re-initialize.
        exit /b 0
    )
    echo Re-initializing CH32H417 SDK...
    git -C "%SDK_ROOT%" submodule deinit -f %SUBMOD%
    if errorlevel 1 ( echo ERROR: submodule deinit failed. & exit /b 1 )
)

echo Initializing CH32H417 SDK submodule (shallow)...
git -C "%SDK_ROOT%" submodule update --init --depth 1 %SUBMOD%
if errorlevel 1 ( echo ERROR: submodule init failed. & exit /b 1 )

echo Applying sparse checkout (device header + core only)...
git -C "%SUBMOD_DIR%" sparse-checkout init --no-cone
git -C "%SUBMOD_DIR%" sparse-checkout set ^
    "/EVT/EXAM/SRC/Peripheral/inc/ch32h417.h" ^
    "/EVT/EXAM/SRC/Core/"
if errorlevel 1 ( echo ERROR: sparse-checkout failed. & exit /b 1 )
git -C "%SUBMOD_DIR%" sparse-checkout reapply

if not exist "%SENTINEL%" (
    echo ERROR: ch32h417.h not found after sparse checkout.
    exit /b 1
)

echo.
echo Done. CH32H417 SDK sparse checkout:
git -C "%SUBMOD_DIR%" sparse-checkout list
echo.
echo SDK layout:
echo   EVT\EXAM\SRC\Peripheral\inc\ch32h417.h  - device register definitions
echo   EVT\EXAM\SRC\Core\                        - core_riscv.h + core_riscv.c

endlocal
