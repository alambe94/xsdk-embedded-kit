@echo off
setlocal enabledelayedexpansion

:: setup_gcovr.bat - Install gcovr 7.2 via pip.
:: gcovr reads gcov coverage data produced by GCC --coverage builds and
:: generates HTML, Cobertura XML, and summary reports.
:: Used by: xsdk.bat coverage
::
:: Requires Python 3 on PATH (any CPython 3.x installation is sufficient).
:: pip manages package integrity; no separate SHA256 file is used.
::
:: Usage:
::   tools\setup_gcovr.bat           install if not already at 7.2
::   tools\setup_gcovr.bat --force   reinstall even if already present

set "GCOVR_VERSION=7.2"

call "%~dp0resolve_python.bat"
if errorlevel 1 exit /b 1

:: Skip if already at the required version (unless --force)
if /i not "%~1"=="--force" (
    for /f "tokens=2" %%v in ('"%PYTHON_EXE%" %PYTHON_ARGS% -m pip show gcovr 2^>nul ^| findstr /i "Version:"') do (
        if "%%v"=="%GCOVR_VERSION%" (
            echo gcovr %GCOVR_VERSION% already installed.
            echo Run with --force to reinstall.
            exit /b 0
        )
    )
)

if /i "%~1"=="--force" (
    echo Reinstalling gcovr %GCOVR_VERSION%...
    "%PYTHON_EXE%" %PYTHON_ARGS% -m pip install --force-reinstall gcovr==%GCOVR_VERSION%
) else (
    echo Installing gcovr %GCOVR_VERSION%...
    "%PYTHON_EXE%" %PYTHON_ARGS% -m pip install gcovr==%GCOVR_VERSION%
)
if errorlevel 1 ( echo ERROR: pip install failed. & exit /b 1 )

echo.
"%PYTHON_EXE%" %PYTHON_ARGS% -m gcovr --version
echo.
echo Done. gcovr %GCOVR_VERSION% installed.
echo   Use via: xsdk.bat coverage

endlocal
