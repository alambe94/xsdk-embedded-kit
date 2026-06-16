@echo off
setlocal enabledelayedexpansion

:: setup_codespell.bat - Install codespell 2.3.0 via pip.
:: codespell checks source files, comments, and documentation for typos.
:: Used by: xsdk.bat codespell
::
:: Requires Python 3 on PATH.
::
:: Usage:
::   tools\setup_codespell.bat           install if not already at 2.3.0
::   tools\setup_codespell.bat --force   reinstall even if already present

set "CODESPELL_VERSION=2.3.0"

call "%~dp0resolve_python.bat"
if errorlevel 1 exit /b 1

if /i not "%~1"=="--force" (
    for /f "tokens=2" %%v in ('call "%PYTHON_EXE%" %PYTHON_ARGS% -m pip show codespell 2^>nul ^| findstr /i "Version:"') do (
        if "%%v"=="%CODESPELL_VERSION%" (
            echo codespell %CODESPELL_VERSION% already installed.
            "%PYTHON_EXE%" %PYTHON_ARGS% -m codespell_lib --version
            if errorlevel 1 ( echo ERROR: codespell verification failed. & exit /b 1 )
            echo Run with --force to reinstall.
            exit /b 0
        )
    )
)

if /i "%~1"=="--force" (
    echo Reinstalling codespell %CODESPELL_VERSION%...
    "%PYTHON_EXE%" %PYTHON_ARGS% -m pip install --force-reinstall codespell==%CODESPELL_VERSION%
) else (
    echo Installing codespell %CODESPELL_VERSION%...
    "%PYTHON_EXE%" %PYTHON_ARGS% -m pip install codespell==%CODESPELL_VERSION%
)
if errorlevel 1 ( echo ERROR: pip install failed. & exit /b 1 )

echo.
"%PYTHON_EXE%" %PYTHON_ARGS% -m codespell_lib --version
if errorlevel 1 ( echo ERROR: codespell verification failed. & exit /b 1 )
echo.
echo Done. codespell %CODESPELL_VERSION% installed.
echo   Use via: xsdk.bat codespell

endlocal
exit /b 0
