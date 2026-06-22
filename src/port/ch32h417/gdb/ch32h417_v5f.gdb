# GDB startup script for CH32H417 V5F (QingKeV5F, 400 MHz) via WCH OpenOCD
#
# Usage:
#   # Terminal 1:
#   tools\openocd_wch\bin\openocd.exe -f src/port/ch32h417/gdb/ch32h417_v5f_wch.cfg
#   # Terminal 2:
#   tools\riscv_gcc\bin\riscv-none-elf-gdb.exe ^
#     -x src/port/ch32h417/gdb/ch32h417_v5f.gdb ^
#     build/ch32h417-riscv-gcc/src/port/ch32h417/ch32h417_bringup.elf
#
# OpenOCD must already be running before invoking GDB.

set architecture riscv:rv32
set remotetimeout 30

# Connect to OpenOCD GDB server (default port 3333)
target extended-remote localhost:3333

# Load and verify the ELF (only if launched with an ELF argument)
# Comment out 'load' when attaching to a chip that already has firmware flashed.
load
compare-sections

# Flush register cache after load
flushregs

# Show key RISC-V registers on attach
info registers pc sp ra mstatus mepc mcause

# Useful breakpoints for bring-up - uncomment as needed:
hbreak main
# break Reset_Handler
# break HardFault_Handler

continue

echo \nCH32H417 V5F connected and stopped at main().\n
