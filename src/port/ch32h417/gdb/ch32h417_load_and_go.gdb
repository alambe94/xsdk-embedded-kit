# ch32h417_load_and_go.gdb
#
# Loads the IRAM image, sets the entry point, and disconnects.
# The OpenOCD gdb-detach event resumes the chip so it runs freely.
# NO hardware breakpoints — previous sessions showed that any hw breakpoint
# left in trigger registers across a disconnect causes an infinite trap loop.
#
# Usage:
#   tools\riscv_gcc\bin\riscv-none-elf-gdb.exe --batch \
#     -x src/port/ch32h417/gdb/ch32h417_load_and_go.gdb \
#     build/ch32h417-riscv-gcc-ram/src/applications/ch32h417_bringup/ch32h417_bringup.elf

set architecture riscv:rv32
set remotetimeout 120
set confirm off

target extended-remote localhost:3333
monitor halt

# ── Clear all RISC-V hardware triggers ────────────────────────────────────────
# WCH hardware triggers SURVIVE physical reset and persist across GDB sessions.
# tdata1=0 (TYPE=0) disables a trigger regardless of tdata2's address.
# We clear slots 0-3 and read back to confirm the write reached the CSR.
set $ts = 0
while $ts < 4
  set $tselect = $ts
  printf "slot %d  tdata1=0x%08X tdata2=0x%08X\n", $ts, $tdata1, $tdata2
  set $tdata1 = 0
  set $tdata2 = 0
  printf "slot %d  tdata1 after=0x%08X  (0=cleared OK, non-zero=CSR write failed)\n", $ts, $tdata1
  set $ts = $ts + 1
end
printf "Trigger clear done.\n"

printf "Loading IRAM image...\n"
load

printf "Setting entry point to xSDK_RAM_Entry...\n"
set $pc = xSDK_RAM_Entry
maintenance flush register-cache

printf "Entry PC: "
info registers pc sp mstatus

printf "Resuming chip, then disconnecting.\n"
printf "Monitor COM7 (115200 8N1) for firmware trace.\n"

# Resume the chip BEFORE disconnecting so it keeps running independently.
# 'monitor resume' sends the resume command to OpenOCD directly; GDB then
# disconnects without needing the gdb-detach Tcl event to fire.
monitor resume
disconnect
quit
