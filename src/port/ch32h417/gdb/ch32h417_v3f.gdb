# GDB startup script for CH32H417 V3F (QingKeV3F, 150 MHz) via WCH OpenOCD
#
# Usage:
#   # Terminal 1:
#   tools\openocd_wch\bin\openocd.exe -f src/port/ch32h417/gdb/ch32h417_v3f_wch.cfg
#   # Terminal 2:
#   tools\riscv_gcc\bin\riscv-none-elf-gdb.exe ^
#     -x src/port/ch32h417/gdb/ch32h417_v3f.gdb ^
#     <path-to-v3f-elf>
#
# OpenOCD must already be running before invoking GDB.

set architecture riscv:rv32
set remotetimeout 30

target extended-remote localhost:3333

load
compare-sections

flushregs

info registers pc sp ra mstatus mepc mcause

# hbreak main
# break Reset_Handler

echo \nCH32H417 V3F connected. Type 'continue' to run.\n
