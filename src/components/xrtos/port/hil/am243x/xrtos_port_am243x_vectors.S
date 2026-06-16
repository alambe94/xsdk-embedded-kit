/*
 * xRTOS AM243x Cortex-R5 vector table and fault handlers.
 *
 * Only the vector table and fault stub handlers live here.
 * The reset handler and startup sequence live in
 * src/port/am243x/startup_am243x.S.
 */

        .text
        .global _vectors
        .type   _vectors,%function
        .section ".vectors","ax",%progbits
        .syntax unified
        .arm
        .align 2

_vectors:
        LDR pc, reset_addr          /* Reset                */
        LDR pc, undefined_addr      /* Undefined Instruction */
        LDR pc, svc_addr            /* Software interrupt    */
        LDR pc, prefetch_abort_addr /* Abort (prefetch)     */
        LDR pc, data_abort_addr     /* Abort (data)         */
        LDR pc, reserved_addr       /* Reserved             */
        LDR pc, irq_addr            /* IRQ                  */
        LDR pc, fiq_addr            /* FIQ                  */

reset_addr:          .long xRTOS_Port_AM243x_Reset_Handler
undefined_addr:      .long xRTOS_Port_AM243x_Undefined_Handler
svc_addr:            .long xRTOS_Port_ARM_R5_Context_Switch
prefetch_abort_addr: .long xRTOS_Port_AM243x_Prefetch_Abort_Handler
data_abort_addr:     .long xRTOS_Port_AM243x_Data_Abort_Handler
reserved_addr:       .long xRTOS_Port_AM243x_Reserved_Handler
irq_addr:            .long xRTOS_Port_ARM_R5_IRQ_Handler
fiq_addr:            .long xRTOS_Port_AM243x_FIQ_Handler

        .section ".text.hwi","ax",%progbits
        .arm
        .align 2

xRTOS_Port_AM243x_Undefined_Handler:
        B       xRTOS_Port_AM243x_Undefined_Handler

xRTOS_Port_AM243x_Prefetch_Abort_Handler:
        B       xRTOS_Port_AM243x_Prefetch_Abort_Handler

xRTOS_Port_AM243x_Data_Abort_Handler:
        B       xRTOS_Port_AM243x_Data_Abort_Handler

xRTOS_Port_AM243x_Reserved_Handler:
        B       xRTOS_Port_AM243x_Reserved_Handler

xRTOS_Port_AM243x_FIQ_Handler:
        B       xRTOS_Port_AM243x_FIQ_Handler

        .global xRTOS_Port_AM243x_Reset_Handler
        .global xRTOS_Port_ARM_R5_Context_Switch
        .global xRTOS_Port_ARM_R5_IRQ_Handler

        .end
