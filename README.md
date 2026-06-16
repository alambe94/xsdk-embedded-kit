# xSDK

eXtensible Embedded SDK - portable, modular firmware components for embedded targets.

---

## Modules & Drivers

| Module | CMake target | Description |
|---|---|---|
| [`xrtos`](src/components/xrtos/README.md) | `xsdk::xrtos` | Deterministic bitmap-scheduler RTOS with semaphore, mutex (PI), queue, event, notify, and timer |
| [`xusbd`](src/components/xusbd/README.md) | `xsdk::xusbd` | Portable USB device stack - CDC, HID, MSC, DFU, WinUSB |
| [`xusbh`](src/components/xusbh/README.md) | `xsdk::xusbh` | Portable USB host stack (HID, MSC, etc.) |
| [`xfs`](src/components/xfs/README.md) | `xsdk::xfs` | FAT32 filesystem for embedded block devices |
| [`xnet`](src/components/xnet/README.md) | `xsdk::xnet` | Lightweight IPv4/Ethernet networking stack with ARP, ICMP, UDP, DHCP, and DNS |
| [`xboot`](src/components/xboot/README.md) | `xsdk::xboot` | Lightweight secondary bootloader and update framework supporting A/B updates |
| [`xshell`](src/components/xshell/README.md) | `xsdk::xshell` | Lightweight command shell and line parser |
| [`xutil`](src/components/xutil/README.md) | `xsdk::xutil` | Trace, fault, and logging utilities |
| [`xusb_common`](src/components/xusb/README.md) | `xsdk::xusb_common` | Common USB specification definitions and helpers shared by USB device and host stacks |
| [`xuart`](src/drivers/xuart/README.md) | `xsdk::xuart` | Portable UART driver with AM243x and CH32H417 hardware ports |
| [`xspi`](src/drivers/xspi/README.md) | `xsdk::xspi` | Portable SPI driver |
| [`xtimer`](src/drivers/xtimer/README.md) | `xsdk::xtimer` | Portable Timer driver with AM243x hardware port |
| [`xble`](src/components/xble/xble_implementation_plan.md) *(planned)* | `xsdk::xble` | Low Energy Bluetooth stack |
| [`xnor`](src/components/xnor/xnor_implementation_plan.md) *(planned)* | `xsdk::xnor` | NOR Flash driver and translation layer |

Each module has its own README with API details and usage examples.

---

## Target Applications

| Application | Description |
|---|---|
| [`xbridge`](src/applications/xbridge/) | USB-to-peripheral protocol bridge (UART, I2C, SPI, CAN, QSPI, CMSIS-DAP, GPIO, PWM, ADC) |
| [`xlogic`](src/applications/xlogic/) | PRU-based logic analyzer with USB 3 SuperSpeed streaming and PulseView/sigrok host integration |
| [`xusbip`](src/applications/xusbip/) *(planned)* | USB/IP server to expose USB host devices over TCP/IP to remote clients |

---

## Integration

Add xSDK as a subdirectory and link the modules you need:

```cmake
add_subdirectory(path/to/xSDK)

target_link_libraries(my_app PRIVATE
    xsdk::xusbd
    xsdk::xutil
)
```

No additional include directories or compiler flags are required - each
namespaced target carries its own.

---

## Quick Start

**Prerequisites:** CMake 3.20+, a supported C11 compiler (see below).

```bat
:: Windows - one-time developer tool setup
xsdk.bat setup

:: Build all host-testable targets and run the test suite
xsdk.bat test

:: Cross-compile for Cortex-R5 (arm-none-eabi-gcc)
xsdk.bat r5-gcc

:: Cross-compile for Cortex-R5 (TI ARM Clang)
xsdk.bat r5-ticlang

:: Cross-compile for AM243x R5FSS0-0 (TI ARM Clang)
xsdk.bat am243x-ticlang

:: Cross-compile for WCH CH32H417 RISC-V SoC (QingKe V5F)
xsdk.bat ch32h417-riscv-gcc

:: Future combinations follow the same pattern - no xsdk.bat changes needed
:: xsdk.bat m33-gcc       (add cmake/toolchains/m33-gcc.cmake)

:: Run QEMU smoke tests on a real Cortex-R5 CPU model
xsdk.bat qemu            :: all components (xrtos + xfs)
xsdk.bat qemu xrtos      :: xrtos primitives only
xsdk.bat qemu xfs        :: xfs file-system only
```

Run `xsdk.bat help` for the full command reference.

---

## Local Quality Checks

```bat
:: Run the full local quality gate
xsdk.bat check

:: Run individual checks
xsdk.bat format-check
xsdk.bat clang-tidy
xsdk.bat cppcheck
xsdk.bat misra
xsdk.bat codespell
xsdk.bat markdownlint
xsdk.bat policy-check
```

`xsdk.bat lint` is deprecated and redirects to `xsdk.bat check`. Use
`xsdk.bat format` for the only mutating formatting path.

---

## QEMU Hardware Validation

xRTOS and xFS include smoke tests that run on a real ARM Cortex-R5 CPU model
via QEMU (`realview-pb-a8 / -cpu cortex-r5`), covering every public primitive
end-to-end without requiring physical hardware.

```bat
xsdk.bat setup qemu    :: install QEMU (one-time)
xsdk.bat qemu          :: build ELFs + run all QEMU tests
```

The shared QEMU infrastructure (`src/port/qemu_r5/`) and the
`xsdk_add_qemu_test()` CMake macro (`cmake/qemu.cmake`) let each component
register its own QEMU test with a single call in its own `CMakeLists.txt`.

---

## Supported Toolchains

| Toolchain | Version | Purpose |
|---|---|---|
| arm-none-eabi-gcc | 13.3.1 (xPack) | Cortex-R5 firmware + QEMU tests |
| riscv-none-elf-gcc | 13.3.1 (xPack) | CH32H417 RISC-V firmware |
| TI ARM Clang | 5.0.0.STS | Cortex-R5 firmware (TI toolchain) |
| Host GCC (MinGW-w64) | 14.2.0 | Host builds and unit tests - Windows |
| Host GCC | system | Host builds and unit tests - Linux / CI |
| QEMU xPack 8.2.2 | arm + aarch64 | Cortex-R5 hardware smoke tests |

---

## Design Principles

- **No hidden dynamic allocation.** All contexts are statically allocated by
  the application. No `malloc` in any core runtime path.
- **Explicit hardware abstraction.** Hardware access goes through an ops-table
  interface. The SDK core has no dependency on any specific MCU or RTOS.
- **Host-testable.** Core logic compiles and runs on a host machine without
  target hardware. CI runs a full unit test suite on every commit.
- **Modular.** Link only the components you use. Each module is an independent
  CMake target.

---

## License

Apache 2.0 - see [LICENSE](LICENSE).
