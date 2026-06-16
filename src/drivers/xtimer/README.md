# xtimer

Portable hardware Timer driver.

**CMake target:** `xsdk::xtimer`

---

## Design Principles

- **Simple Interface.** Designed primarily to drive RTOS system ticks and software timer periodic interrupts.
- **Hardware Agnostic.** Abstract interface functions accept base addresses, period in microseconds, and input clocks.

---

## Integration

```cmake
add_subdirectory(path/to/xSDK)
target_link_libraries(my_app PRIVATE xsdk::xtimer)
```

Include `<xtimer.h>` to access the portable timer APIs.

---

## Usage - Periodic Tick Timer Setup

```c
#include "xtimer.h"

#define TIM_BASE_ADDR    0x02400000U // Example DMTimer peripheral base address
#define TIM_PERIOD_US    1000U       // 1 millisecond tick period (1000 microseconds)
#define TIM_INPUT_CLK_HZ 25000000U   // 25 MHz input clock

void app_timer_setup(void)
{
    // Step 1 - Configure timer for periodic overflow interrupts
    // Note: clocks and pinmux must be enabled externally
    xtimer_init_periodic(TIM_BASE_ADDR, TIM_PERIOD_US, TIM_INPUT_CLK_HZ);

    // Step 2 - Start the timer
    xtimer_start(TIM_BASE_ADDR);
}

// Timer interrupt service routine (ISR) handler
void timer_isr_handler(void)
{
    // Step 3 - Clear peripheral pending interrupt flags
    xtimer_clear_irq(TIM_BASE_ADDR);

    // Perform periodic RTOS/system tick processing
    // ...
}
```

---

## Key Public APIs

| Function | Description |
|---|---|
| `xtimer_init_periodic` | Configure hardware registers (e.g. DMTimer) for automatic periodic overflow interrupts |
| `xtimer_start` | Enable and start count operation |
| `xtimer_stop` | Stop count operation |
| `xtimer_clear_irq` | Clear peripheral interrupt status flags inside the ISR |

---

## License

Apache 2.0 - see [LICENSE](../../../LICENSE).
