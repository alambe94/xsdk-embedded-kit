@echo off
setlocal enabledelayedexpansion

:: Get the directory of the batch file
set "SCRIPT_DIR=%~dp0"
set "REPO_ROOT=%SCRIPT_DIR%.."

echo Exporting LikeC4 diagrams from %REPO_ROOT%\docs\architecture...

:: Ensure output directory exists
if not exist "%REPO_ROOT%\docs\images" (
    mkdir "%REPO_ROOT%\docs\images"
)

:: Run LikeC4 export
call npx -y @likec4/cli@latest export "%REPO_ROOT%\docs\architecture" -o "%REPO_ROOT%\docs\images"

if %ERRORLEVEL% neq 0 (
    echo [ERROR] LikeC4 export failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo [SUCCESS] LikeC4 diagrams exported successfully to docs/images/
