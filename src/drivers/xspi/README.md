# xspi

Portable SPI controller driver.

**CMake target:** `xsdk::xspi`

---

## Design Principles

- **Controller-level Abstraction.** Handles transactions using a generic `xSPI_Transaction_t` structure that supports full-duplex, half-duplex, transmit-only, and receive-only modes.
- **Ops Interface.** Hardware-specific register programming is isolated inside a custom `xSPI_Driver_Ops_t` driver table.
- **Client Ownership.** Callers allocate and own the `xSPI_Context_t` session structures.

---

## Integration

```cmake
add_subdirectory(path/to/xSDK)
target_link_libraries(my_app PRIVATE xsdk::xspi)
```

Include `<xspi.h>` to access the portable controller APIs.

---

## Usage - Synchronous Transfer

```c
#include "xspi.h"

static xSPI_Context_t g_spi;

// Driver context & ops provided by HAL port
extern const xSPI_Driver_Ops_t g_my_spi_ops;
extern void *g_my_spi_driver_ctx;

void app_spi_init(void)
{
    xSPI_Instance_t instance = {
        .channel = 0U,
    };
    xSPI_Config_t cfg = {
        .mode       = XSPI_MODE_0,
        .bit_rate   = 1000000U, // 1 MHz
        .word_width = 8U,
    };

    g_spi.ops        = &g_my_spi_ops;
    g_spi.driver_ctx = g_my_spi_driver_ctx;

    // Initialize and start the SPI controller
    xSPI_Init(&g_spi, &instance, &cfg);
    xSPI_Start(&g_spi);
}

void perform_spi_transaction(void)
{
    uint8_t tx_buf[] = {0x9F, 0x00, 0x00, 0x00}; // JEDEC ID Command
    uint8_t rx_buf[4];

    xSPI_Device_t flash_device = {
        .spi_ctx = &g_spi,
        .cs_pin  = 0U, // Chip Select 0
    };

    xSPI_Transaction_t trans = {
        .tx_buffer = tx_buf,
        .rx_buffer = rx_buf,
        .length    = sizeof(tx_buf),
    };

    // Perform full-duplex blocking transfer
    xRETURN_t ret = xSPI_Transfer(&flash_device, &trans);
    if (ret == xRETURN_OK) {
        // ID read successfully
    }
}
```

---

## Key Public APIs

| Function | Description |
|---|---|
| `xSPI_Init` | Initialize portable controller tracking context and config parameters |
| `xSPI_Deinit` | Clean up and release tracking context |
| `xSPI_Start` | Configure clock polarity, phase, chip select, and enable the hardware block |
| `xSPI_Stop` | Disable hardware clocks and place SPI block in reset/low-power mode |
| `xSPI_Transfer` | Perform a blocking SPI transaction (read, write, or full-duplex exchange) |

---

## License

Apache 2.0 - see [LICENSE](../../../LICENSE).
