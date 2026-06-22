# ch32h417_inspect.gdb
# Reconnects to a running chip, halts it, dumps USBSS state, resumes.
# Run this AFTER the chip has been running for a few seconds.
#
# Usage:
#   tools\riscv_gcc\bin\riscv-none-elf-gdb.exe --batch \
#     -x src/port/ch32h417/gdb/ch32h417_inspect.gdb \
#     build/ch32h417-riscv-gcc-ram/src/applications/ch32h417_bringup/ch32h417_bringup.elf

define ss
  set $lst = *(unsigned int*)0x40034010
  set $state = ($lst & 0xF00) >> 8
  if $state == 0
    printf "link=U0      "
  end
  if $state == 5
    printf "link=RxDet   "
  end
  if $state == 6
    printf "link=Inactive"
  end
  if $state == 7
    printf "link=Polling "
  end
  if $state == 8
    printf "link=Recovery"
  end
  if $state == 9
    printf "link=HotRst  "
  end
  if $state == 4
    printf "link=Disabled"
  end
  if $state > 0
    if $state < 4
      printf "link=U%d      ", $state
    end
  end
  if $state > 9
    printf "link=s%d      ", $state
  end
  printf "\n"
  printf "  LCFG =0x%08X  LCTRL=0x%08X  LINTC=0x%08X\n", \
    *(unsigned int*)0x40034000, *(unsigned int*)0x40034004, \
    *(unsigned int*)0x40034008
  printf "  LIF  =0x%08X  LST  =0x%08X\n", \
    *(unsigned int*)0x4003400C, $lst
  printf "  LPCAP=0x%08X\n", *(unsigned int*)0x40034054
  printf "  RXD0 =0x%08X  TXD0 =0x%08X\n", \
    *(unsigned int*)0x40034058, *(unsigned int*)0x40034064
  printf "  UCTRL=0x%08X  UST  =0x%08X\n", \
    *(unsigned int*)0x40034070, *(unsigned int*)0x40034074
  printf "  TXEN =0x%04X       RXEN =0x%04X\n", \
    *(unsigned short*)0x40034080, *(unsigned short*)0x40034082
  printf "  EP0T =0x%08X  EP0R =0x%08X\n", \
    *(unsigned int*)0x40034084, *(unsigned int*)0x40034088
  printf "  EP0TD=0x%08X  EP0RD=0x%08X\n", \
    *(unsigned int*)0x4003408C, *(unsigned int*)0x40034090
end

define sspkt
  set $buf = &ep0_dma_buf
  printf "ep0_buf: %02X %02X %02X %02X %02X %02X %02X %02X\n", \
    ((unsigned char*)$buf)[0], ((unsigned char*)$buf)[1], \
    ((unsigned char*)$buf)[2], ((unsigned char*)$buf)[3], \
    ((unsigned char*)$buf)[4], ((unsigned char*)$buf)[5], \
    ((unsigned char*)$buf)[6], ((unsigned char*)$buf)[7]
  printf "  bmReqType=0x%02X bRequest=0x%02X wValue=0x%04X wLength=0x%04X\n", \
    ((unsigned char*)$buf)[0], ((unsigned char*)$buf)[1], \
    (((unsigned int)((unsigned char*)$buf)[3])<<8)|((unsigned char*)$buf)[2], \
    (((unsigned int)((unsigned char*)$buf)[7])<<8)|((unsigned char*)$buf)[6]
end

define ssctx
  set $ctx = &xUSBD_CH32H417_DCD_Context
  printf "DCD ctx: is_hw_init=%d is_connected=%d warm_reset=%d speed=%d\n", \
    $ctx->is_hardware_initialized, $ctx->is_connected, \
    $ctx->warm_reset_active, $ctx->speed
  printf "  EP1 IN: active=%d zlp=%d mps=%u length=%u\n", \
    $ctx->in_ep_handles[1].Is_Active, $ctx->in_ep_handles[1].Send_ZLP, \
    $ctx->in_ep_handles[1].MPS, $ctx->in_ep_handles[1].Transfer_Length
  printf "  EP1 OUT: active=%d primed=%d mps=%u length=%u\n", \
    $ctx->out_ep_handles[1].Is_Active, $ctx->out_ep_handles[1].Chain_Primed, \
    $ctx->out_ep_handles[1].MPS, $ctx->out_ep_handles[1].Transfer_Length
end

set architecture riscv:rv32
set remotetimeout 30
set confirm off

target extended-remote localhost:3333
monitor halt

printf "\n=== HALT SNAPSHOT ===\n"
info registers pc sp ra

printf "\n--- USBSS state ---\n"
ss

printf "\n--- EP0 DMA buffer ---\n"
sspkt

printf "\n--- DCD context ---\n"
ssctx

printf "\nResuming chip.\n"
monitor resume
disconnect
quit
