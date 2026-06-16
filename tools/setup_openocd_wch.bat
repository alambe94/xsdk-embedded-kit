@echo off
setlocal

:: setup_openocd_wch.bat - Initialize WCH-patched OpenOCD submodule (tools/openocd_wch).
:: Source: https://github.com/openwch/openocd_wch  (shallow, pinned by .gitmodules commit)
::
:: WCH-LinkE must be in native mode (VID=1A86 PID=8010) to use the wlinke driver.
:: If the device shows as 8012 (CMSIS-DAP), long-press the MODE button ~3s to switch back.
::
:: Usage:
::   tools\setup_openocd_wch.bat           skip if already present
::   tools\setup_openocd_wch.bat --force   re-init (removes and re-clones)
::
:: After setup, connect:
::   tools\openocd_wch\bin\openocd.exe -f src/port/ch32h417/debug/ch32h417_v5f_wch.cfg

set "SCRIPT_DIR=%~dp0"
set "SDK_ROOT=%SCRIPT_DIR%.."
set "OCD_EXE=%SCRIPT_DIR%openocd_wch\bin\openocd.exe"

if exist "%OCD_EXE%" (
    if /i not "%~1"=="--force" (
        echo WCH OpenOCD already present in tools\openocd_wch\
        echo Run with --force to re-initialize.
        exit /b 0
    )
    echo Removing existing tools\openocd_wch\ for re-init...
    git -C "%SDK_ROOT%" submodule deinit -f tools/openocd_wch
    if errorlevel 1 ( echo ERROR: submodule deinit failed. & exit /b 1 )
)

echo Initializing WCH OpenOCD submodule (shallow)...
git -C "%SDK_ROOT%" submodule update --init --depth 1 tools/openocd_wch
if errorlevel 1 ( echo ERROR: submodule init failed. & exit /b 1 )

if not exist "%OCD_EXE%" (
    echo ERROR: openocd.exe not found after submodule init.
    exit /b 1
)

echo.
echo Done. WCH OpenOCD installed to tools\openocd_wch\
echo.
echo Usage:
echo   tools\openocd_wch\bin\openocd.exe -f src/port/ch32h417/debug/ch32h417_v5f_wch.cfg
echo.
echo WCH-LinkE must be in native mode (VID=1A86 PID=8010).
echo If it shows as 8012 (CMSIS-DAP), long-press MODE button ~3s to switch back.

endlocal
