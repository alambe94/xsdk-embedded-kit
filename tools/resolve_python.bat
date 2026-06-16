@echo off
:: resolve_python.bat - Locate Python 3 and set PYTHON_EXE / PYTHON_ARGS.
::
:: Call with 'call' from any setup_*.bat that needs Python.
:: Does nothing if PYTHON_EXE is already defined.
::
:: On return:
::   PYTHON_EXE  - path or command name for Python 3
::   PYTHON_ARGS - extra arguments (e.g. "-3" for the py launcher; empty otherwise)
::
:: Exit code is 0 on success, 1 if Python was not found.
::
:: No setlocal - intentionally writes PYTHON_EXE/PYTHON_ARGS into the caller's scope.

if defined PYTHON_EXE goto :eof

python -c "import sys" >nul 2>&1
if not errorlevel 1 ( set "PYTHON_EXE=python" & goto :eof )

py -3 -c "import sys" >nul 2>&1
if not errorlevel 1 ( set "PYTHON_EXE=py" & set "PYTHON_ARGS=-3" & goto :eof )

if exist "%LocalAppData%\Python\bin\python.exe" (
    set "PYTHON_EXE=%LocalAppData%\Python\bin\python.exe"
    goto :eof
)
if exist "%LocalAppData%\Programs\Python\Python310\python.exe" (
    set "PYTHON_EXE=%LocalAppData%\Programs\Python\Python310\python.exe"
    goto :eof
)

echo ERROR: python not found on PATH.
echo        Install Python 3 from https://www.python.org/ and ensure it is on PATH.
exit /b 1
