# xuart

Portable UART controller driver with hardware ports for TI AM243x and WCH CH32H417 SoCs.

**CMake target:** `xsdk::xuart`

---

## Design Principles

- **Driver Ops Interface.** Decoupled from specific registers or system libraries. Functions map to controller operations using the `xUART_Driver_Ops_t` structure.
- **Synchronous & Asynchronous support.** Supports both blocking (timeout-based) and non-blocking (callback-driven asynchronous) transfer routines.
- **Trace Integration.** Built-in optional telemetry hook integration using `xTRACE` contexts.
- **No static allocation limits.** Callers allocate and own the `xUART_Context_t` tracking structure.

---

## Integration

```cmake
add_subdirectory(path/to/xSDK)
target_link_libraries(my_app PRIVATE xsdk::xuart)
```

Include `<xuart.h>` to access the portable controller APIs.

---

## Usage - Synchronous (Blocking) Transmit & Receive

```c
#include "xuart.h"

static xUART_Context_t g_uart;

// Platform-specific driver context & ops provided by HAL port
extern const xUART_Driver_Ops_t g_my_uart_ops;
extern void *g_my_uart_driver_ctx;

void app_uart_init(void)
{
    // Step 1 - Define portable configuration
    xUART_Config_t cfg = {
        .baud_rate = 115200U,
        .data_bits = XUART_DATA_BITS_8,
        .stop_bits = XUART_STOP_BITS_1,
        .parity    = XUART_PARITY_NONE,
    };
    
    // Bind driver context
    g_uart.ops        = &g_my_uart_ops;
    g_uart.driver_ctx = g_my_uart_driver_ctx;
    g_uart.port       = 0U; // Port 0 / UART0

    // Step 2 - Initialize UART
    xUART_Init(&g_uart, &cfg);

    // Step 3 - Start the UART controller
    xUART_Start_Config_t start_cfg = {0}; // Port-specific clock settings
    xUART_Start(&g_uart, &start_cfg);
}

void send_and_receive(void)
{
    uint8_t tx_data[] = "Ping\r\n";
    uint8_t rx_data[8];

    // Transmit blocking with 100ms timeout
    xUART_Transmit(&g_uart, tx_data, sizeof(tx_data), 100U);

    // Receive blocking with 500ms timeout
    xUART_Receive(&g_uart, rx_data, sizeof(rx_data), 500U);
}
```

---

## Key Public APIs

| Function | Description |
|---|---|
| `xUART_Init` | Initialize portable driver tracking fields |
| `xUART_Deinit` | Clean up and clear tracking context |
| `xUART_Start` | Bind hardware peripheral clocks and configure baudrate/parity |
| `xUART_Stop` | Disable hardware transmitter/receiver and lower power state |
| `xUART_Transmit` | Blocking byte-array transmission with timeout |
| `xUART_Receive` | Blocking byte-array reception with timeout |
| `xUART_Transmit_Async` | Start non-blocking DMA/interrupt-driven transmission |
| `xUART_Receive_Async` | Start non-blocking DMA/interrupt-driven reception |
| `xUART_Abort_Tx` | Cancel a pending asynchronous transmission |
| `xUART_Abort_Rx` | Cancel a pending asynchronous reception |
| `xUART_Trace_Init` | Bind an optional telemetry context for debug logging |

---

## License

Apache 2.0 - see [LICENSE](../../../LICENSE).
