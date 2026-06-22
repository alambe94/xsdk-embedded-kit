# GDB startup script for CH32H417 V5F IRAM debug via WCH OpenOCD
#
# ALL executable code lives in ITRAM (RAM_CODE: 0x200A0000-0x200BFFFF).
# BSS/data live in SRAM (RAM: 0x200C0300+).  ep0_dma_buf at 0x200C1218.
#
# Usage:
#   Terminal 1:  tools\openocd_wch\bin\openocd.exe -f src/port/ch32h417/gdb/ch32h417_v5f_wch_ram.cfg
#   Terminal 2:  tools\riscv_gcc\bin\riscv-none-elf-gdb.exe \
#                  -x src/port/ch32h417/gdb/ch32h417_v5f_ram.gdb \
#                  build/ch32h417-riscv-gcc-ram/src/applications/ch32h417_bringup/ch32h417_bringup.elf

# ── Defines first — available even if load fails ──────────────────────────────

# USBSS_BASE = 0x40034000
# Offsets: LINK_CFG+0 LINK_CTRL+4 LINK_INT_CTRL+8 LINK_INT_FLAG+C LINK_STATUS+10
#          LINK_LPM_CR+50  LINK_LMP_PORT_CAP+54
#          LINK_LMP_RX_DATA0+58  LINK_LMP_TX_DATA0+64
#          USB_CONTROL+70  USB_STATUS+74  USB_ITP+78
#          UEP_TX_EN+80(h) UEP_RX_EN+82(h)
#          UEP0_TX_CTRL+84 UEP0_RX_CTRL+88 UEP0_TX_DMA+8C UEP0_RX_DMA+90

define ss
  set $lcfg  = *(unsigned int*)0x40034000
  set $lctrl = *(unsigned int*)0x40034004
  set $lintc = *(unsigned int*)0x40034008
  set $lif   = *(unsigned int*)0x4003400C
  set $lst   = *(unsigned int*)0x40034010
  set $uctrl = *(unsigned int*)0x40034070
  set $ust   = *(unsigned int*)0x40034074
  set $txen  = *(unsigned short*)0x40034080
  set $rxen  = *(unsigned short*)0x40034082
  set $ep0t  = *(unsigned int*)0x40034084
  set $ep0r  = *(unsigned int*)0x40034088
  set $ep0td = *(unsigned int*)0x4003408C
  set $ep0rd = *(unsigned int*)0x40034090
  set $state = ($lst & 0xF00) >> 8
  if $state == 0
    printf "link=U0       "
  end
  if $state == 4
    printf "link=Disabled "
  end
  if $state == 5
    printf "link=RxDet    "
  end
  if $state == 6
    printf "link=Inactive "
  end
  if $state == 7
    printf "link=Polling  "
  end
  if $state == 8
    printf "link=Recovery "
  end
  if $state == 9
    printf "link=HotReset "
  end
  if $state > 0
    if $state < 4
      printf "link=U%d       ", $state
    end
  end
  if $state > 9
    printf "link=s%d       ", $state
  end
  printf " LCFG=0x%08X LCTRL=0x%08X\n", $lcfg, $lctrl
  printf "  LINTC=0x%08X LIF=0x%08X LST=0x%08X\n", $lintc, $lif, $lst
  printf "  UCTRL=0x%08X UST=0x%08X\n", $uctrl, $ust
  printf "  EP_TX_EN=0x%04X EP_RX_EN=0x%04X\n", $txen, $rxen
  printf "  EP0_TX_CTRL=0x%08X EP0_RX_CTRL=0x%08X\n", $ep0t, $ep0r
  printf "  EP0_TX_DMA =0x%08X EP0_RX_DMA =0x%08X\n", $ep0td, $ep0rd
end
document ss
Print all key USBSS registers + decoded link state.
end

define sslnk
  set $lif  = *(unsigned int*)0x4003400C
  set $lst  = *(unsigned int*)0x40034010
  printf "LIF=0x%08X  LST=0x%08X  state=%d\n", $lif, $lst, ($lst&0xF00)>>8
end
document sslnk
Print LINK_INT_FLAG and LINK_STATUS (state bits [11:8]).
end

define ssep0
  printf "UEP0_TX_CTRL=0x%08X  UEP0_RX_CTRL=0x%08X\n", \
    *(unsigned int*)0x40034084, *(unsigned int*)0x40034088
  printf "UEP0_TX_DMA =0x%08X  UEP0_RX_DMA =0x%08X\n", \
    *(unsigned int*)0x4003408C, *(unsigned int*)0x40034090
  printf "UEP_TX_EN=0x%04X  UEP_RX_EN=0x%04X\n", \
    *(unsigned short*)0x40034080, *(unsigned short*)0x40034082
end
document ssep0
Print USBSS EP0 TX/RX control and DMA registers.
end

define sspkt
  set $buf = &ep0_dma_buf
  printf "EP0 buf: %02X %02X %02X %02X %02X %02X %02X %02X\n", \
    ((unsigned char*)$buf)[0], ((unsigned char*)$buf)[1], \
    ((unsigned char*)$buf)[2], ((unsigned char*)$buf)[3], \
    ((unsigned char*)$buf)[4], ((unsigned char*)$buf)[5], \
    ((unsigned char*)$buf)[6], ((unsigned char*)$buf)[7]
  printf "  bmRequestType=0x%02X bRequest=0x%02X wValue=0x%04X wLength=0x%04X\n", \
    ((unsigned char*)$buf)[0], ((unsigned char*)$buf)[1], \
    (((unsigned int)((unsigned char*)$buf)[3])<<8)|((unsigned char*)$buf)[2], \
    (((unsigned int)((unsigned char*)$buf)[7])<<8)|((unsigned char*)$buf)[6]
end
document sspkt
Decode ep0_dma_buf[0..7] as a USB SETUP packet.
end

define sslmp
  printf "LMP_PORT_CAP=0x%08X\n", *(unsigned int*)0x40034054
  printf "LMP_RX_DATA0=0x%08X LMP_RX_DATA1=0x%08X LMP_RX_DATA2=0x%08X\n", \
    *(unsigned int*)0x40034058, *(unsigned int*)0x4003405C, *(unsigned int*)0x40034060
  printf "LMP_TX_DATA0=0x%08X LMP_TX_DATA1=0x%08X LMP_TX_DATA2=0x%08X\n", \
    *(unsigned int*)0x40034064, *(unsigned int*)0x40034068, *(unsigned int*)0x4003406C
end
document sslmp
Print USBSS LMP port-capability and data registers.
end

define ssrcc
  set $ctlr = *(unsigned int*)0x40021000
  set $hbpc = *(unsigned int*)0x40021050
  set $cfg0 = *(unsigned int*)0x40021008
  printf "RCC_CTLR=0x%08X  USBSS_PLL on=%d rdy=%d\n", \
    $ctlr, ($ctlr>>22)&1, ($ctlr>>23)&1
  printf "RCC_HBPCENR=0x%08X  USBSSEN=%d\n", $hbpc, ($hbpc>>12)&1
  printf "RCC_CFGR0=0x%08X\n", $cfg0
end
document ssrcc
Show USBSS PLL lock and clock-enable bits in RCC.
end

# ── Connection and IRAM load ──────────────────────────────────────────────────

set architecture riscv:rv32
# Large timeout: the ITRAM load runs at ~4 KB/s (62 KB → ~15 s); GDB must
# not treat a single slow packet as a timeout during that window.
set remotetimeout 120
set print pretty on
set confirm off

target extended-remote localhost:3333

# Halt wherever the chip is (no flash reset — chip stays in RAM).
monitor halt

# Load ELF sections into ITRAM + SRAM.
load

# Set PC to IRAM entry stub (zeroes BSS, configures CSRs, jumps to main).
set $pc = xSDK_RAM_Entry
flushregs

info registers pc sp ra mstatus mepc mcause

# ── Run to main ───────────────────────────────────────────────────────────────
# Use a TEMPORARY hardware breakpoint: it auto-removes after one hit so the
# trigger register is clean before the next continue.
thbreak main
continue

echo \nCH32H417 V5F stopped at main() -- IRAM image loaded.\n
echo Commands: ss  sslnk  ssep0  sspkt  sslmp  ssrcc\n
