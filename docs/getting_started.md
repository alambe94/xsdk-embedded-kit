# Getting Started with xSDK

Welcome to xSDK, the eXtensible Embedded SDK. This guide covers local tool setup,
host tests, target builds, QEMU smoke tests, and AM243x hardware debug using
OpenOCD, GNU GDB, and the open-source Cortex-Debug VS Code extension.

## 1. One-Time Developer Setup on Windows

xSDK installs pinned, repo-local developer tools into `tools/`. This keeps your
system environment clean and makes builds reproducible.

From the `sub_modules/xsdk` directory, run:

```cmd
xsdk.bat setup
```

To install or refresh specific tools:

```cmd
xsdk.bat setup <tool_name>

xsdk.bat setup cmake          :: CMake + Ninja
xsdk.bat setup openocd        :: OpenOCD (for Cortex-R5 debug)
xsdk.bat setup openocd-wch    :: WCH-patched OpenOCD (for CH32H417 RISC-V debug)
xsdk.bat setup arm_gcc        :: arm-none-eabi-gcc toolchain
xsdk.bat setup riscv-gcc      :: riscv-none-elf-gcc toolchain
xsdk.bat setup tiarmclang     :: TI ARM Clang toolchain
xsdk.bat setup ti-sdk         :: TI mcupsdk_core sparse checkout (~59 MB)
xsdk.bat setup ch32h417-sdk   :: WCH CH32H417 SDK sparse checkout (~2 MB)
xsdk.bat setup qemu           :: QEMU system emulators for Cortex-R5
```

## 2. Build and Test

### Host Build and Unit Tests

```cmd
xsdk.bat test
```

This is the fastest path for OS-agnostic logic.

### Hardware Target Builds

```cmd
xsdk.bat r5-gcc
xsdk.bat r5-ticlang
xsdk.bat am243x-ticlang
xsdk.bat ch32h417-riscv-gcc
```

`xsdk.bat am243x-ticlang` builds the AM243x standalone bring-up application:

```text
build/am243x-ticlang/src/applications/am243x_bringup/am243x_bringup.out
```

When the TI boot image tools are present, the build also emits:

```text
build/am243x-ticlang/src/applications/am243x_bringup/am243x_bringup.rprc
build/am243x-ticlang/src/applications/am243x_bringup/am243x_bringup.appimage
build/am243x-ticlang/src/applications/am243x_bringup/am243x_bringup.appimage.hs_fs
```

### Generated VS Code Launch Configuration

After a successful debuggable target build, `tools/xsdk.py` invokes
`tools/update_launch.py` to replace `.vscode/launch.json` with configurations
for that target. This keeps the Run and Debug list focused on the last target
built:

- `am243x-ticlang` generates the AM243x load and attach configurations.
- `ch32h417-riscv-gcc` generates the CH32H417 V5F load configuration.
- Host, analysis, QEMU, unsupported-target, and failed builds preserve the last
  generated debug configuration.

Treat `.vscode/launch.json` as generated output. Add or change persistent target
configurations in `tools/update_launch.py`, then rebuild the target or run:

```cmd
python tools\update_launch.py <preset>
```

### CH32H417 V5F Bring-Up

`xsdk.bat ch32h417-riscv-gcc` builds the CH32H417 V5F bring-up ELF plus flash
images. Install its pinned compiler once before the first build:

```bat
xsdk.bat setup riscv-gcc
xsdk.bat ch32h417-riscv-gcc
```

The bring-up image uses repository-owned startup, linker, and polling USART1
code. At the reset 25 MHz HSI clock, PA9 transmits the boot banner and xRTOS
task heartbeat messages at `115200 8N1`.

```text
build/ch32h417-riscv-gcc/src/port/ch32h417/ch32h417_bringup.elf
build/ch32h417-riscv-gcc/src/port/ch32h417/ch32h417_bringup.hex
build/ch32h417-riscv-gcc/src/port/ch32h417/ch32h417_bringup.bin
```

## 3. AM243x Hardware Debug with OpenOCD and GDB

The LP-AM243 workspace supports debugging using the onboard XDS110 debug probe:

- OpenOCD from `tools/openocd/bin/openocd.exe`
- GNU GDB from `tools/arm_gcc/bin/arm-none-eabi-gdb.exe`
- Cortex-Debug VS Code extension (`marus25.cortex-debug`)

### VS Code Debugging

1. Connect the LP-AM243 USB debug port to your PC.
2. Connect a serial terminal to the board's UART at `115200 8N1`.
3. Compile the target binary:

   ```cmd
   xsdk.bat am243x-ticlang
   ```

4. Ensure the Cortex-Debug extension is active in VS Code.
5. Launch the debug session: **AM243x R5FSS0-0: OpenOCD load at reset**.

The debug profile automatically halts at `_vectors` in SVC ARM mode. Step once to jump to `xRTOS_Port_AM243x_Reset_Handler`, then run or step through the application.

### Command Line Debugging

To debug from outside VS Code:

**Terminal 1 (OpenOCD):**
```cmd
tools\openocd\bin\openocd.exe ^
  -s tools\openocd\openocd\scripts ^
  -f tools\gdb\xsdk_openocd_am243x.cfg
```

**Terminal 2 (GDB):**
```cmd
tools\arm_gcc\bin\arm-none-eabi-gdb.exe ^
  build\am243x-ticlang\src\applications\am243x_bringup\am243x_bringup.out
```

**Inside GDB:**
```gdb
target extended-remote localhost:3333
monitor targets
monitor halt
load
set $cpsr = 0x1d3
set $pc = _vectors
stepi
```

---

## 4. CH32H417 Hardware Debug with OpenOCD and GDB

For WCH CH32H417 debugging, the repository leverages the WCH-patched OpenOCD and RISC-V GCC toolchain over WCH-LinkE:

- OpenOCD from `tools/openocd_wch/bin/openocd.exe`
- GNU GDB from `tools/riscv_gcc/bin/riscv-none-elf-gdb.exe`
- Cortex-Debug VS Code extension (`marus25.cortex-debug`)

### VS Code Debugging

1. Connect the WCH-LinkE probe to your target's RVSWD pins (SWDIO/SWCLK/GND/3V3).
2. Connect a serial terminal to USART1 TX/RX (PA9/PA10) at `115200 8N1`.
3. Compile the target binary:

   ```cmd
   xsdk.bat ch32h417-riscv-gcc
   ```

4. Launch the debug session: **CH32H417 Hart 1 (V5F): OpenOCD load**.

### Command Line Debugging

To debug from outside VS Code:

**Terminal 1 (OpenOCD):**
```cmd
tools\openocd_wch\bin\openocd.exe ^
  -f src\port\ch32h417\gdb\ch32h417_v5f_wch.cfg
```

**Terminal 2 (GDB):**
```cmd
tools\riscv_gcc\bin\riscv-none-elf-gdb.exe ^
  build\ch32h417-riscv-gcc\src\port\ch32h417\ch32h417_bringup.elf
```

**Inside GDB:**
```gdb
target extended-remote localhost:3333
monitor halt
monitor reg dcsr 0x400003
monitor reg pc 0x00010000
load
compare-sections
tbreak main
continue
```

---

## 5. QEMU Smoke Tests

xSDK can run selected R5-oriented integration tests on QEMU without hardware.

```cmd
xsdk.bat setup qemu
xsdk.bat qemu
xsdk.bat qemu xrtos
xsdk.bat qemu xfs
```

---

## 6. Quality Checks

Run the local quality gate:

```cmd
xsdk.bat check
```

Format code:

```cmd
xsdk.bat format
```

Run individual analyzers:

```cmd
xsdk.bat cppcheck
xsdk.bat misra
xsdk.bat clang-tidy
xsdk.bat codespell
xsdk.bat markdownlint
xsdk.bat policy-check
```

---

## 7. Integrating xSDK into a Project

Add xSDK to your CMake tree:

```cmake
add_subdirectory(path/to/xsdk)
```

Link the modules you need:

```cmake
target_link_libraries(my_firmware PRIVATE
    xsdk::xrtos          # Bitmap-scheduler RTOS
    xsdk::xusbd          # USB device stack
    xsdk::xusbh          # USB host stack
    xsdk::xfs            # FAT32 file system
    xsdk::xnet           # Lightweight IPv4 stack
    xsdk::xboot          # Bootloader update framework
    xsdk::xshell         # Interactive shell console
    xsdk::xutil          # Utilities (assertions, faults, trace)
)
```

The exported targets carry their include paths and dependencies.
