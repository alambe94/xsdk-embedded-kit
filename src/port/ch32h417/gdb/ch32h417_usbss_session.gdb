# USBSS bring-up debug session — sourced after ch32h417_v5f_ram.gdb
# (all define commands: ss / sslnk / ssep0 / sspkt / sslmp / ssrcc are already defined)
# Execution starts here with the target stopped at main().

set confirm off

# thbreak main in ch32h417_v5f_ram.gdb auto-removed itself — no stale triggers.
printf "=== at main() -- initial USBSS state ===\n"
ssrcc
ss

# ── LINK layer monitor ──────────────────────────────────────────────────────
# Capture up to 30 LINK IRQs with decoded state on every hit.
set $link_cnt = 0

hbreak USBSS_LINK_IRQHandler
commands
  set $link_cnt = $link_cnt + 1
  printf "\n--- LINK-IRQ #%d ---\n", $link_cnt
  sslnk
  if $link_cnt < 30
    continue
  end
end

# ── USBSS EP0 / transfer monitor ───────────────────────────────────────────
# Capture up to 8 USBSS EP0 IRQs (SETUP, STATUS, TRANSFER events).
set $ep0_cnt = 0

hbreak USBSS_IRQHandler
commands
  set $ep0_cnt = $ep0_cnt + 1
  printf "\n--- USBSS-IRQ #%d ---\n", $ep0_cnt
  set $ust = *(unsigned int*)0x40034074
  printf "  USB_STATUS=0x%08X\n", $ust
  if ($ust & 2)
    printf "  -> SETUP flag set\n"
    sspkt
  end
  if ($ust & 4)
    printf "  -> STATUS flag set\n"
  end
  if ($ust & 1)
    set $ep = ($ust & 0x700) >> 8
    set $dir = ($ust & 0x1000) >> 12
    printf "  -> TRANSFER ep=%d dir=%d\n", $ep, $dir
  end
  ssep0
  if $ep0_cnt < 8
    continue
  end
end

printf "Running — capturing up to 30 LINK-IRQs and 8 USBSS-IRQs...\n"
continue

# ── Session complete (reached on 30th LINK-IRQ or 8th USBSS-IRQ) ─────────
printf "\n========== Session capture complete ==========\n"
printf "LINK-IRQs captured : %d\n", $link_cnt
printf "USBSS-IRQs captured: %d\n", $ep0_cnt
printf "\n--- Final register snapshot ---\n"
ss
ssep0
printf "\n--- ep0_dma_buf (last SETUP or descriptor fragment) ---\n"
sspkt
printf "\n--- LMP registers ---\n"
sslmp
