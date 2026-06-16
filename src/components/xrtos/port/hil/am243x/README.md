# xRTOS AM243x Port

This target layer implements the self-contained AM243x Cortex-R5 port.

## Source Files

Add these xRTOS sources to the AM243x CCS project:

```text
src/components/xrtos/port/arm_r5/xrtos_port_arm_r5.c
src/components/xrtos/port/arm_r5/xrtos_port_arm_r5_asm.S
src/components/xrtos/port/hil/am243x/xrtos_port_am243x.c
src/components/xrtos/port/hil/am243x/xrtos_port_am243x_vectors.S
```

Use `linker.cmd` as the xRTOS linker ownership reference for the first
standalone R5FSS0-0 bring-up.

## Interrupt Ownership

`xrtos_port_am243x_vectors.S` owns reset and exception vectors:

- Reset to `xRTOS_Port_AM243x_Reset_Handler`
- SVC to `xRTOS_Port_ARM_R5_Context_Switch`
- IRQ to `xRTOS_Port_ARM_R5_IRQ_Handler`
- abort/FIQ exceptions to local halt loops

`xrtos_port_am243x.c` provides a strong override for
`xrtos_port_arm_r5_irq_handler`, which dispatches the active AM243x VIM IRQ
through a small local ISR table.

## Tick Source

Call `xRTOS_Port_AM243x_Init`, then register the hardware timer IRQ with
`xRTOS_Port_AM243x_Register_IRQ` and `xRTOS_Port_AM243x_Tick_ISR`.

Example:

```c
xRTOS_Port_AM243x_Init();
xRTOS_Port_AM243x_Register_IRQ(160U,
                               xRTOS_Port_AM243x_Tick_ISR,
                               NULL,
                               15U,
                               true);
```

The callback increments the xRTOS tick only. The ARM R5 IRQ wrapper performs
any required context switch after the local AM243x dispatcher returns.

Keep the tick interrupt disabled until the xRTOS scheduler is ready to start.
The current ARM R5 IRQ wrapper saves an interrupted task context, so it expects
a valid current task.
