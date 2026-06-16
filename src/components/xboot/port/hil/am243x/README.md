# xBOOT AM243x Port

This directory contains target-specific startup, linker, and port operations implementation for the TI AM243x SoC (Cortex-R5FSS0-0).

## Memory Layout Constraints

To prevent memory collision between the bootloader and the application:
1. **xBOOT Boundary:** MSRAM is partitioned at `0x70040000`. The lower 256 KB (`0x70000000` to `0x70040000`) is strictly reserved for xBOOT vectors, code, data, BSS, and stacks.
2. **Linker Enforced:** The linker script [linker_am243x.cmd](file:///c:/Users/lambe/Documents/Embedded_Swiss_Knife/sub_modules/xsdk/src/components/xboot/port/am243x/linker_am243x.cmd) restricts memory sections to `MSRAM_CODE` and `MSRAM_DATA` within this 256 KB region.
3. **Application Staging:** Target applications must be linked above `0x70040000`.

## SYSFW / DMSC Boot Notifications

When booting on AM243x:
1. The ROM bootloader loads SYSFW (to `0x44000`) and BoardCfg (to `0x7B000`) and the signed xBOOT image.
2. xBOOT startup code runs `__mpu_init` and branches to `xBOOT_Main`.
3. `xBOOT_Main` calls `Bootloader_socWaitForFWBoot()` to synchronize with the DMSC firmware before requesting system configs.
4. If initialization succeeds, xBOOT boots the target application via `Bootloader_bootSelfCpu()` or Sciclient proc-boot sequences.
