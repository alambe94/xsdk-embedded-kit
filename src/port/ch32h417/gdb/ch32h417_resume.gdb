# ch32h417_resume.gdb
# Step past a stale hardware trigger at main then resume freely.
# Run when the chip is already halted at main (0x200a0402) from a prior load.

set architecture riscv:rv32
set remotetimeout 30
set confirm off

target extended-remote localhost:3333
monitor halt

printf "Chip halted at:\n"
info registers pc sp mstatus mcause

# Single-step past the stale trigger at main's entry without re-arming it.
stepi
stepi

printf "PC after step:\n"
info registers pc

# Free-run — USB init and IRQ handlers will now execute.
# GDB will disconnect; the chip keeps running.
printf "Resuming -- watch COM7 for UART trace.\n"
continue &
disconnect
quit
