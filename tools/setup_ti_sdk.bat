@echo off
setlocal

:: setup_ti_sdk.bat - Initialize TI mcupsdk_core submodule with sparse checkout.
::
:: Sparse paths (only what xSDK build files actually reference):
::   source/drivers/sciclient.h     - Sciclient top-level header
::   source/drivers/sciclient/      - Sciclient implementation
::   source/drivers/hw_include/     - HW register definitions (xspi)
::   source/usb/                    - USB CDN driver (xUSBH AM64x MCU+ SDK binding)
::   examples/drivers/boot/sbl_null/ - SBL null main.c (AM243x SBL port)
::
:: Full checkout: ~662 MB / 51 K files.  Sparse checkout: ~59 MB / 1.3 K files.
::
:: Usage:
::   tools\setup_ti_sdk.bat           skip if already present
::   tools\setup_ti_sdk.bat --force   re-init (deinit + re-clone)

set "SCRIPT_DIR=%~dp0"
set "SDK_ROOT=%SCRIPT_DIR%.."
set "SUBMOD=src/third_party/ti/mcupsdk_core"
set "SUBMOD_DIR=%SDK_ROOT%\src\third_party\ti\mcupsdk_core"
set "SENTINEL=%SUBMOD_DIR%\source\drivers\sciclient.h"

if exist "%SENTINEL%" (
    if /i not "%~1"=="--force" (
        echo TI mcupsdk_core already initialized.
        echo Run with --force to re-initialize.
        exit /b 0
    )
    echo Re-initializing TI mcupsdk_core...
    git -C "%SDK_ROOT%" submodule deinit -f %SUBMOD%
    if errorlevel 1 ( echo ERROR: submodule deinit failed. & exit /b 1 )
)

echo Initializing TI mcupsdk_core submodule (shallow)...
git -C "%SDK_ROOT%" submodule update --init --depth 1 %SUBMOD%
if errorlevel 1 ( echo ERROR: submodule init failed. & exit /b 1 )

echo Applying sparse checkout...
git -C "%SUBMOD_DIR%" sparse-checkout init --no-cone
git -C "%SUBMOD_DIR%" sparse-checkout set ^
    "/source/drivers/sciclient.h" ^
    "/source/drivers/sciclient/" ^
    "/source/drivers/hw_include/" ^
    "/source/usb/" ^
    "/examples/drivers/boot/sbl_null/"
if errorlevel 1 ( echo ERROR: sparse-checkout failed. & exit /b 1 )
git -C "%SUBMOD_DIR%" sparse-checkout reapply

if not exist "%SENTINEL%" (
    echo ERROR: source/drivers/sciclient.h not found after sparse checkout.
    exit /b 1
)

echo.
echo Done. TI mcupsdk_core sparse checkout:
git -C "%SUBMOD_DIR%" sparse-checkout list
echo.
echo Full checkout ~662 MB / 51K files. Sparse checkout active.

endlocal
