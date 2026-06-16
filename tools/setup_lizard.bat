@echo off
setlocal enabledelayedexpansion

:: setup_lizard.bat - Install lizard 1.22.1 via pip.
:: lizard measures cyclomatic complexity (CCN) per function and enforces a
:: configurable threshold. Used by: xsdk.bat complexity
::
:: Requires Python 3 on PATH.
:: pip manages package integrity; no separate SHA256 file is used.
::
:: Usage:
::   tools\setup_lizard.bat           install if not already at 1.22.1
::   tools\setup_lizard.bat --force   reinstall even if already present

set "LIZARD_VERSION=1.22.1"

call "%~dp0resolve_python.bat"
if errorlevel 1 exit /b 1

if /i not "%~1"=="--force" (
    for /f "tokens=2" %%v in ('"%PYTHON_EXE%" %PYTHON_ARGS% -m pip show lizard 2^>nul ^| findstr /i "Version:"') do (
        if "%%v"=="%LIZARD_VERSION%" (
            echo lizard %LIZARD_VERSION% already installed.
            echo Run with --force to reinstall.
            exit /b 0
        )
    )
)

if /i "%~1"=="--force" (
    echo Reinstalling lizard %LIZARD_VERSION%...
    "%PYTHON_EXE%" %PYTHON_ARGS% -m pip install --force-reinstall lizard==%LIZARD_VERSION%
) else (
    echo Installing lizard %LIZARD_VERSION%...
    "%PYTHON_EXE%" %PYTHON_ARGS% -m pip install lizard==%LIZARD_VERSION%
)
if errorlevel 1 ( echo ERROR: pip install failed. & exit /b 1 )

echo.
"%PYTHON_EXE%" %PYTHON_ARGS% -m lizard --version
echo.
echo Done. lizard %LIZARD_VERSION% installed.
echo   Use via: xsdk.bat complexity

endlocal
