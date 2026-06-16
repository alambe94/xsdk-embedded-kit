/*
 * CH32H417 QingKe V5F reset path and interrupt vector table.
 *
 * Reset executes from flash, copies the loader into RAM_LOAD, and transfers
 * control to it. The loader initializes ITCM, data, BSS, and the QingKe CSRs
 * before entering main in user mode with machine interrupts enabled.
 */

    .macro VECTOR handler
    .word \handler
    .weak \handler
    .set \handler, Default_Handler
    .endm

    .macro RESERVED
    .word 0
    .endm

    /*
     * RAM_LOAD executes through a flash alias on QingKeV5F. PC-relative `la`
     * resolves from the physical flash PC there, so use absolute relocations.
     */
    .macro ABS_ADDR reg, symbol
    lui \reg, %hi(\symbol)
    addi \reg, \reg, %lo(\symbol)
    .endm

    .section .init, "ax", @progbits
    .global _start
    .type _start, @function
    .align 2
_start:
    j handle_reset
    .size _start, . - _start

    .section .vector, "ax", @progbits
    .global _vector_base
    .align 1
_vector_base:
    .option push
    .option norvc
    .word _start
    RESERVED
    VECTOR NMI_Handler
    VECTOR HardFault_Handler
    RESERVED
    VECTOR Ecall_M_Mode_Handler
    RESERVED
    RESERVED
    VECTOR Ecall_U_Mode_Handler
    VECTOR Break_Point_Handler
    RESERVED
    RESERVED
    VECTOR SysTick0_Handler
    VECTOR SysTick1_Handler
    VECTOR SW_Handler
    RESERVED
    VECTOR IPC_CH0_Handler
    VECTOR IPC_CH1_Handler
    VECTOR IPC_CH2_Handler
    VECTOR IPC_CH3_Handler
    RESERVED
    RESERVED
    RESERVED
    RESERVED
    RESERVED
    RESERVED
    RESERVED
    RESERVED
    VECTOR HSEM_Handler
    RESERVED
    RESERVED
    RESERVED

    VECTOR WWDG_IRQHandler
    VECTOR EXTI15_8_IRQHandler
    VECTOR FLASH_IRQHandler
    VECTOR RCC_IRQHandler
    VECTOR EXTI7_0_IRQHandler
    VECTOR SPI1_IRQHandler
    VECTOR DMA1_Channel2_IRQHandler
    VECTOR DMA1_Channel3_IRQHandler
    VECTOR DMA1_Channel4_IRQHandler
    VECTOR DMA1_Channel5_IRQHandler
    VECTOR DMA1_Channel6_IRQHandler
    VECTOR DMA1_Channel7_IRQHandler
    VECTOR DMA1_Channel8_IRQHandler
    VECTOR USART2_IRQHandler
    VECTOR I2C1_EV_IRQHandler
    VECTOR I2C1_ER_IRQHandler
    VECTOR USART1_IRQHandler
    VECTOR SPI2_IRQHandler
    VECTOR SPI3_IRQHandler
    VECTOR SPI4_IRQHandler
    VECTOR I2C2_EV_IRQHandler
    VECTOR I2C2_ER_IRQHandler
    VECTOR USBPD_IRQHandler
    VECTOR USBPDWakeUp_IRQHandler
    VECTOR USBHS_IRQHandler
    VECTOR DMA1_Channel1_IRQHandler
    VECTOR CAN1_SCE_IRQHandler
    VECTOR CAN1_TX_IRQHandler
    VECTOR CAN1_RX0_IRQHandler
    VECTOR CAN1_RX1_IRQHandler
    VECTOR USBSS_IRQHandler
    VECTOR USBSS_LINK_IRQHandler
    VECTOR USBHSWakeup_IRQHandler
    VECTOR USBSSWakeup_IRQHandler
    VECTOR RTCAlarm_IRQHandler
    VECTOR USBFS_IRQHandler
    VECTOR USBFSWakeup_IRQHandler
    VECTOR ADC1_2_IRQHandler
    VECTOR TIM1_BRK_IRQHandler
    VECTOR TIM1_UP_IRQHandler
    VECTOR TIM1_TRG_COM_IRQHandler
    VECTOR TIM1_CC_IRQHandler
    VECTOR TIM2_IRQHandler
    VECTOR TIM3_IRQHandler
    VECTOR TIM4_IRQHandler
    VECTOR TIM5_IRQHandler
    VECTOR I2C3_EV_IRQHandler
    VECTOR I2C3_ER_IRQHandler
    VECTOR I2C4_EV_IRQHandler
    VECTOR I2C4_ER_IRQHandler
    VECTOR QSPI1_IRQHandler
    VECTOR SERDES_IRQHandler
    VECTOR USART3_IRQHandler
    VECTOR USART4_IRQHandler
    VECTOR TIM8_BRK_IRQHandler
    VECTOR TIM8_UP_IRQHandler
    VECTOR TIM8_TRG_COM_IRQHandler
    VECTOR TIM8_CC_IRQHandler
    VECTOR TIM9_IRQHandler
    VECTOR TIM10_IRQHandler
    VECTOR TIM11_IRQHandler
    VECTOR TIM12_IRQHandler
    VECTOR FMC_IRQHandler
    VECTOR SDMMC_IRQHandler
    VECTOR LPTIM1_IRQHandler
    VECTOR LPTIM2_IRQHandler
    VECTOR USART5_IRQHandler
    VECTOR USART6_IRQHandler
    VECTOR TIM6_IRQHandler
    VECTOR TIM7_IRQHandler
    VECTOR DMA2_Channel1_IRQHandler
    VECTOR DMA2_Channel2_IRQHandler
    VECTOR DMA2_Channel3_IRQHandler
    VECTOR DMA2_Channel4_IRQHandler
    VECTOR DMA2_Channel5_IRQHandler
    VECTOR DMA2_Channel6_IRQHandler
    VECTOR DMA2_Channel7_IRQHandler
    VECTOR DMA2_Channel8_IRQHandler
    VECTOR ETH_IRQHandler
    VECTOR ETH_WKUP_IRQHandler
    VECTOR CAN2_SCE_IRQHandler
    VECTOR CAN2_TX_IRQHandler
    VECTOR CAN2_RX0_IRQHandler
    VECTOR CAN2_RX1_IRQHandler
    VECTOR USART7_IRQHandler
    VECTOR USART8_IRQHandler
    VECTOR I3C_EV_IRQHandler
    VECTOR I3C_ER_IRQHandler
    VECTOR DVP_IRQHandler
    VECTOR ECDC_IRQHandler
    VECTOR PIOC_IRQHandler
    VECTOR SAI_IRQHandler
    VECTOR LTDC_IRQHandler
    VECTOR GPHA_IRQHandler
    RESERVED
    VECTOR DFSDM0_IRQHandler
    VECTOR DFSDM1_IRQHandler
    RESERVED
    RESERVED
    VECTOR SWPMI_IRQHandler
    RESERVED
    RESERVED
    VECTOR QSPI2_IRQHandler
    VECTOR SWPMI_WKUP_IRQHandler
    VECTOR CAN3_SCE_IRQHandler
    VECTOR CAN3_TX_IRQHandler
    VECTOR CAN3_RX0_IRQHandler
    VECTOR CAN3_RX1_IRQHandler
    VECTOR LPTIM2_WKUP_IRQHandler
    VECTOR LPTIM1_WKUP_IRQHandler
    VECTOR I3C_WKUP_IRQHandler
    VECTOR RTC_IRQHandler
    VECTOR HSADC_IRQHandler
    VECTOR UHSIF_IRQHandler
    VECTOR RNG_IRQHandler
    VECTOR SDIO_IRQHandler
    VECTOR USART_WKUP_IRQHandler
    .option pop

    .section .text.vector_handler, "ax", @progbits
    .type Default_Handler, @function
    .align 2
Default_Handler:
    j Default_Handler
    .size Default_Handler, . - Default_Handler

    .section .load, "ax", @progbits
    .type _load_base, @function
    .align 1
_load_base:
    ABS_ADDR a0, _highcode_lma
    ABS_ADDR a1, _highcode_vma_start
    ABS_ADDR a2, _highcode_vma_end
    bgeu a1, a2, 2f
1:
    lw t0, 0(a0)
    sw t0, 0(a1)
    addi a0, a0, 4
    addi a1, a1, 4
    bltu a1, a2, 1b

2:
    ABS_ADDR a0, _highcode_lma1
    ABS_ADDR a1, _highcode_vma_start1
    ABS_ADDR a2, _highcode_vma_end1
    bgeu a1, a2, 2f
1:
    lw t0, 0(a0)
    sw t0, 0(a1)
    addi a0, a0, 4
    addi a1, a1, 4
    bltu a1, a2, 1b

2:
    ABS_ADDR a0, _data_lma
    ABS_ADDR a1, _data_vma
    ABS_ADDR a2, _edata
    bgeu a1, a2, 2f
1:
    lw t0, 0(a0)
    sw t0, 0(a1)
    addi a0, a0, 4
    addi a1, a1, 4
    bltu a1, a2, 1b

2:
    ABS_ADDR a0, _sbss
    ABS_ADDR a1, _ebss
    bgeu a0, a1, 2f
1:
    sw zero, 0(a0)
    addi a0, a0, 4
    bltu a0, a1, 1b
2:
    li t0, 0x1237B3E0
    csrw 0xBC0, t0

    li t0, 0x07
    csrw 0xBC1, t0

    li t0, 0x0F
    csrw 0x804, t0

    li t0, 0x6088
    csrw mstatus, t0

    ABS_ADDR t0, _vector_base
    ori t0, t0, 3
    csrw mtvec, t0

    ABS_ADDR t0, main
    csrw mepc, t0
    fence.i
    mret
    .size _load_base, . - _load_base

    .section .handle_reset, "ax", @progbits
    .global handle_reset
    .type handle_reset, @function
    .align 2
handle_reset:
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop

    la sp, _eusrstack

    /*
     * Temporarily enable both flash banks while the RAM loader is copied.
     * Restore the original register value before entering RAM_LOAD.
     */
    lui a3, 0x40022
    lw a4, 0(a3)
    ori a5, a4, 3
    sw a5, 0(a3)

    la a0, _loadcode_lma
    la a1, _loadcode_vma_start
    la a2, _loadcode_vma_end
    bgeu a1, a2, 2f
1:
    lw t0, 0(a0)
    sw t0, 0(a1)
    addi a0, a0, 4
    addi a1, a1, 4
    bltu a1, a2, 1b
2:
    sw a4, 0(a3)
    fence.i
    call _load_base
    .size handle_reset, . - handle_reset
