# rr32_hw_test.gdb - Automated GDB script for the AM243x 32-slot round-robin hardware test.
#
# Usage (from the xSDK root, after building am243x_rr32):
#
#   Step 1 - Start OpenOCD in a separate terminal:
#     tools\openocd\bin\openocd.exe -s tools\openocd\scripts \
#         -f src/port/am243x/debug/am243x.cfg
#
#   Step 2 - Run this script:
#     arm-none-eabi-gdb build\am243x-ticlang\src\applications\am243x_rr32\am243x_rr32.out \
#         --batch -x src/port/am243x/debug/rr32_hw_test.gdb
#
# Exit codes:
#   0 = PASS (g_rr32_done == 1)
#   1 = FAIL (g_rr32_done == 2 or timeout)

set pagination off
set confirm off

# 1. Connect to OpenOCD GDB server for main0_r5.0 (port 3336)
target remote :3336

# 2. Load the ELF
load
monitor reset halt

# 3. Watchpoint on the result sentinel
watch g_rr32_done

# 4. Run
continue

# 5. Evaluate result
set $result = g_rr32_done

if $result == 1
  printf "\n[GDB] RR32 HW TEST PASS  (g_rr32_done = %d)\n", $result
else
  printf "\n[GDB] RR32 HW TEST FAIL  (g_rr32_done = %d)\n", $result
end

printf "[GDB] Worker counters:\n"
set $i = 0
while $i < 29
  printf "  worker %2d: %u\n", $i + 1, s_worker_count[$i]
  set $i = $i + 1
end

# 6. Exit with pass/fail code
if $result == 1
  quit 0
else
  quit 1
end
