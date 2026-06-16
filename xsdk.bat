@echo off
setlocal enabledelayedexpansion

:: xsdk.bat - configure and build the xSDK host targets.
:: Run from anywhere; paths are relative to the script location.
::
:: Source analysis commands accept an optional [module] argument to scope to
:: one module (e.g. xnet). Module is resolved across src/components,
:: src/applications, and src/drivers.
::
:: Quick reference:
::   xsdk.bat [test]              host build (+ run tests)
::   xsdk.bat r5-gcc / r5-ticlang Cortex-R5 cross-compile
::   xsdk.bat ch32h417-riscv-gcc CH32H417 V5F minimal bring-up
::   xsdk.bat qemu [comp] [--trace] QEMU smoke tests
::   xsdk.bat format [module]     run clang-format
::   xsdk.bat format-check [mod]  check clang-format compliance
::   xsdk.bat cppcheck [module]   cppcheck static analysis
::   xsdk.bat clang-tidy [module] clang-tidy static analysis
::   xsdk.bat misra [module]      MISRA C:2012 check
::   xsdk.bat complexity [module] cyclomatic complexity report
::   xsdk.bat codespell [module]  spell check
::   xsdk.bat policy-check [mod]  xSDK policy checks
::   xsdk.bat check [module]      full quality gate (or scoped)
::   xsdk.bat coverage [module]   coverage + HTML report
::   xsdk.bat setup [tool]        install pinned tools
::   xsdk.bat help                full command list

set "SDK_ROOT=%~dp0"
set "BUILD_DIR=%SDK_ROOT%build\host"

:: Resolve cmake/ninja - prefer tools\cmake\ (pinned), fall back to system install
set "CMAKE_EXE=%SDK_ROOT%tools\cmake\bin\cmake.exe"
set "CTEST_EXE=%SDK_ROOT%tools\cmake\bin\ctest.exe"
if not exist "%CMAKE_EXE%" (
    set "CMAKE_EXE=C:\Program Files\CMake\bin\cmake.exe"
    set "CTEST_EXE=C:\Program Files\CMake\bin\ctest.exe"
)

:: Resolve host GCC - prefer tools\gcc\ (pinned), fall back to system MinGW
set "GCC_DIR=%SDK_ROOT%tools\gcc\bin"
if not exist "%GCC_DIR%\gcc.exe" set "GCC_DIR=C:\MinGW\bin"

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
if defined PYTHON_EXE set "PYTHONDONTWRITEBYTECODE=1"

:: Add tools to PATH
set "PATH=%GCC_DIR%;%SDK_ROOT%tools\cmake\bin;C:\Program Files\CMake\bin;%PATH%"

:: Help
if "%~1"=="" goto :show_help
if /i "%~1"=="help" goto :show_help
if /i "%~1"=="--help" goto :show_help
if /i "%~1"=="-h" goto :show_help


:: Developer tool setup - must be checked before any tool existence guards
if /i "%~1"=="setup" (
    call "%SDK_ROOT%tools\setup.bat" %~2 %~3
    exit /b !errorlevel!
)

:: Backward-compatible alias - forwards to the generic cross-compile dispatch.
if /i "%~1"=="tiarmclang" ( call "%~f0" r5-ticlang & exit /b !errorlevel! )

if not exist "%CMAKE_EXE%" (
    echo ERROR: cmake not found at %CMAKE_EXE%
    echo        Run: xsdk.bat setup cmake
    exit /b 1
)

:: Shared cross-platform tasks. Keep these aliases while xsdk.bat is reduced.
set "_SHARED_TASK=0"
for %%c in (format format-check cppcheck clang-tidy misra complexity docs policy-check codespell host-tools-test trace-dict trace-dict-check check coverage qemu release) do (
    if /i "%~1"=="%%c" set "_SHARED_TASK=1"
)
if "!_SHARED_TASK!"=="1" if not defined PYTHON_EXE (
    echo ERROR: Python was not found. Install Python or add it to PATH.
    exit /b 1
)
if /i "%~1"=="release" (
    "!PYTHON_EXE!" !PYTHON_ARGS! "%SDK_ROOT%tools\release_public.py"
    exit /b !errorlevel!
)
if /i "%~1"=="format" (
    "!PYTHON_EXE!" !PYTHON_ARGS! "%SDK_ROOT%tools\xsdk.py" format apply %~2
    exit /b !errorlevel!
)
if /i "%~1"=="format-check" (
    "!PYTHON_EXE!" !PYTHON_ARGS! "%SDK_ROOT%tools\xsdk.py" format check %~2
    exit /b !errorlevel!
)
if /i "%~1"=="cppcheck" (
    "!PYTHON_EXE!" !PYTHON_ARGS! "%SDK_ROOT%tools\xsdk.py" cppcheck %~2
    exit /b !errorlevel!
)
if /i "%~1"=="clang-tidy" (
    "!PYTHON_EXE!" !PYTHON_ARGS! "%SDK_ROOT%tools\xsdk.py" clang-tidy %~2
    exit /b !errorlevel!
)
if /i "%~1"=="misra" (
    "!PYTHON_EXE!" !PYTHON_ARGS! "%SDK_ROOT%tools\xsdk.py" misra %~2
    exit /b !errorlevel!
)
if /i "%~1"=="complexity" (
    "!PYTHON_EXE!" !PYTHON_ARGS! "%SDK_ROOT%tools\xsdk.py" complexity %~2
    exit /b !errorlevel!
)
if /i "%~1"=="docs" (
    "!PYTHON_EXE!" !PYTHON_ARGS! "%SDK_ROOT%tools\xsdk.py" docs
    exit /b !errorlevel!
)
if /i "%~1"=="check" (
    "!PYTHON_EXE!" !PYTHON_ARGS! "%SDK_ROOT%tools\xsdk.py" check %~2
    exit /b !errorlevel!
)
if /i "%~1"=="coverage" (
    "!PYTHON_EXE!" !PYTHON_ARGS! "%SDK_ROOT%tools\xsdk.py" coverage %~2
    exit /b !errorlevel!
)
if /i "%~1"=="policy-check" (
    "!PYTHON_EXE!" !PYTHON_ARGS! "%SDK_ROOT%tools\xsdk.py" policy %~2
    exit /b !errorlevel!
)
if /i "%~1"=="codespell" (
    "!PYTHON_EXE!" !PYTHON_ARGS! "%SDK_ROOT%tools\xsdk.py" spell %~2
    exit /b !errorlevel!
)
if /i "%~1"=="host-tools-test" (
    "!PYTHON_EXE!" !PYTHON_ARGS! "%SDK_ROOT%tools\xsdk.py" host-tools-test
    exit /b !errorlevel!
)
if /i "%~1"=="trace-dict" (
    "!PYTHON_EXE!" !PYTHON_ARGS! "%SDK_ROOT%tools\xsdk.py" trace-dict generate
    exit /b !errorlevel!
)
if /i "%~1"=="trace-dict-check" (
    "!PYTHON_EXE!" !PYTHON_ARGS! "%SDK_ROOT%tools\xsdk.py" trace-dict check
    exit /b !errorlevel!
)
if /i "%~1"=="qemu" (
    set "_QEMU_SHARED_ARGS="
    if /i "%~2"=="--trace" (
        set "_QEMU_SHARED_ARGS=--trace"
    ) else (
        if not "%~2"=="" set "_QEMU_SHARED_ARGS=--regex %~2"
        if /i "%~3"=="--trace" set "_QEMU_SHARED_ARGS=--trace --regex %~2"
    )
    "!PYTHON_EXE!" !PYTHON_ARGS! "%SDK_ROOT%tools\xsdk.py" qemu !_QEMU_SHARED_ARGS!
    exit /b !errorlevel!
)

:: Cross-compile dispatch - pattern: {mcu}-{compiler}
:: The target name is also the CMake toolchain filename: cmake/toolchains/{target}.cmake
:: and the build output directory: build/{target}/
::
:: Compiler registry - add one entry per supported compiler:
::   gcc       arm-none-eabi-gcc  (tools\arm_gcc\)
::   ticlang   TI ARM Clang       (tools\tiarmclang\)
::
:: To add a new combination (e.g. m33-gcc):
::   1. Add cmake/toolchains/m33-gcc.cmake
::   2. Add matching configure/build presets to CMakePresets.json
::   3. No changes to xsdk.bat required.
::
:: Examples: xsdk.bat r5-gcc   xsdk.bat r5-ticlang   xsdk.bat m33-gcc
set "_CROSS_TC=%SDK_ROOT%cmake\toolchains\%~1.cmake"
if exist "!_CROSS_TC!" (
    if not defined PYTHON_EXE (
        echo ERROR: Python was not found. Install Python or add it to PATH.
        exit /b 1
    )
    "!PYTHON_EXE!" !PYTHON_ARGS! "%SDK_ROOT%tools\xsdk.py" cross-build %~1
    exit /b !errorlevel!
)

:: Markdown lint
if /i "%~1"=="markdownlint" (
    echo Markdown lint temporarily disabled.
    echo TODO: Re-enable markdownlint-cli2 after the current Markdown/link issues are resolved.
    exit /b 0
)

:: Deprecated xSDK style linter
if /i "%~1"=="lint" (
    echo DEPRECATED: xsdk.bat lint no longer runs the legacy custom linter.
    echo             Use xsdk.bat check for the local quality gate.
    if /i "%~2"=="--fix" (
        echo             Auto-fix mode was retired; use xsdk.bat format for formatting.
        exit /b 1
    )
    call "%~f0" check
    exit /b !errorlevel!
)

if not exist "%GCC_DIR%\gcc.exe" (
    echo ERROR: gcc not found at %GCC_DIR%\gcc.exe
    echo        Run: xsdk.bat setup gcc
    exit /b 1
)

:: Handle clean
if /i "%~1"=="clean" (
    echo Removing %BUILD_DIR%...
    powershell -NoProfile -Command "if (Test-Path '%BUILD_DIR%') { Remove-Item '%BUILD_DIR%' -Recurse -Force }"
    echo Done.
    exit /b 0
)

:: Handle rebuild
if /i "%~1"=="rebuild" (
    echo Removing %BUILD_DIR%...
    powershell -NoProfile -Command "if (Test-Path '%BUILD_DIR%') { Remove-Item '%BUILD_DIR%' -Recurse -Force }"
    shift
)

:: Configure host. CMake refreshes an existing build directory when needed.
echo [1/2] Configuring...
"%CMAKE_EXE%" --preset host-dev -S "%SDK_ROOT%."
if errorlevel 1 ( echo ERROR: Configure failed. & exit /b 1 )

:: Build
echo [2/2] Building...
"%CMAKE_EXE%" --build "%BUILD_DIR%"
if errorlevel 1 ( echo ERROR: Build failed. & exit /b 1 )

if /i not "%~1"=="test" (
    echo Build complete.
    exit /b 0
)

:: Test
echo [3/3] Running tests...
"%CTEST_EXE%" --test-dir "%BUILD_DIR%" --output-on-failure
if errorlevel 1 ( echo FAILED: one or more tests failed. & exit /b 1 )

echo All tests passed.
goto :eof

:show_help
echo xSDK build script
echo.
echo Usage:
echo   xsdk.bat                        show this help
echo   xsdk.bat test                   host build + run all CTest tests
echo   xsdk.bat host-tools-test        run Python host-tool tests
echo   xsdk.bat r5-gcc                 Cortex-R5 cross-compile with arm-none-eabi-gcc + size report
echo   xsdk.bat r5-ticlang             Cortex-R5 cross-compile with TI ARM Clang + size report
echo   xsdk.bat am243x-ticlang         AM243x R5FSS0-0 cross-compile with TI ARM Clang
echo   xsdk.bat ch32h417-riscv-gcc     CH32H417 V5F minimal bring-up with RISC-V GCC
echo   xsdk.bat ^<mcu^>-^<compiler^>       Generic: any cmake/toolchains/^<mcu^>-^<compiler^>.cmake
echo   xsdk.bat qemu [comp]            QEMU smoke tests ^(comp: xrtos xfs xutil; default: all^)
echo   xsdk.bat qemu [comp] --trace    same, with xTRACE decode + Chrome Trace JSON + Perfetto proto
echo   xsdk.bat clean                  delete the host build directory
echo   xsdk.bat rebuild [test]         clean + reconfigure + build ^(+ run tests if 'test' given^)
echo.
echo   Source analysis - all accept an optional [module] to scope to one module ^(e.g. xnet^):
echo   xsdk.bat format [module]        run clang-format
echo   xsdk.bat format-check [module]  check clang-format compliance without edits
echo   xsdk.bat cppcheck [module]      run cppcheck static analysis
echo   xsdk.bat clang-tidy [module]    run clang-tidy static analysis
echo   xsdk.bat misra [module]         run MISRA C:2012 check ^(Mandatory + Required gated^)
echo   xsdk.bat complexity [module]    cyclomatic complexity analysis + HTML report
echo   xsdk.bat codespell [module]     spell check source, comments, and docs
echo   xsdk.bat policy-check [module]  run xSDK repository policy checks
echo   xsdk.bat check [module]         run full local quality gate, or scoped to one module
echo   xsdk.bat coverage [module]      coverage build + run tests + HTML report
echo.
echo   Other:
echo   xsdk.bat trace-dict             generate JSON trace dictionaries from header annotations
echo   xsdk.bat trace-dict-check       verify JSON trace dictionaries are up-to-date with headers
echo   xsdk.bat docs                   generate Doxygen HTML
echo   xsdk.bat markdownlint           Markdown lint ^(temporarily skipped^)
echo   xsdk.bat lint                   deprecated alias for xsdk.bat check
echo   xsdk.bat release                push clean release to public remote
echo   xsdk.bat help                   show this help
echo.
echo Tool setup ^(downloads and pins tools to tools\^):
echo   xsdk.bat setup                  install all tools
echo   xsdk.bat setup cmake            CMake + Ninja
echo   xsdk.bat setup llvm             clang-format + clang-tidy
echo   xsdk.bat setup gcc              host GCC ^(MinGW-w64^)
echo   xsdk.bat setup arm-gcc          arm-none-eabi-gcc ^(xPack^) - used by r5-gcc targets
echo   xsdk.bat setup riscv-gcc        riscv-none-elf-gcc ^(xPack^) - used by CH32H417
echo   xsdk.bat setup tiarmclang       TI ARM Clang
echo   xsdk.bat setup openocd          OpenOCD (xPack 0.12.0, standard JTAG/SWD)
echo   xsdk.bat setup openocd-wch     WCH-patched OpenOCD (CH32H417 / RVSWD via WCH-LinkE)
echo   xsdk.bat setup ti-sdk          TI mcupsdk_core sparse checkout (~59 MB, sciclient + USB + hw_include)
echo   xsdk.bat setup ch32h417-sdk   WCH CH32H417 SDK sparse checkout (~2 MB, Peripheral + Startup + Core)
echo   xsdk.bat setup qemu             QEMU Arm ^(qemu-system-arm + qemu-system-aarch64^)
echo   xsdk.bat setup doxygen          Doxygen
echo   xsdk.bat setup gcovr            gcovr ^(pip^) - coverage reports
echo   xsdk.bat setup lizard           lizard ^(pip^) - complexity analysis
echo   xsdk.bat setup cppcheck         cppcheck - static analysis
echo   xsdk.bat setup ^<tool^> --force   re-download a specific tool
exit /b 0

endlocal
