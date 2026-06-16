# xusbh

Portable USB Host Stack for embedded targets.

**CMake target:** `xsdk::xusbh`

---

## Design Principles

- **Static Allocation.** All device, interface, endpoint, and transfer descriptors are statically allocated using maximum configured limits. No dynamic heap allocation.
- **HCD Abstraction.** Hardware controller drivers (HCD) are fully decoupled. The stack operates via a callback ops table (`xUSBH_HCD_Ops_t`), allowing integration with any USB Host controller.
- **Debounced Enumeration.** Built-in root-port state debouncing and standard USB enumeration state machine (`xUSBH_Enumeration_State_t`).
- **Composite Support.** Class drivers can claim individual interfaces on composite devices.

---

## Integration

```cmake
add_subdirectory(path/to/xSDK)
target_link_libraries(my_app PRIVATE xsdk::xusbh)
```

Shared USB specification definitions are located in `src/components/xusb/include/xusb_std.h` (provided by target `xsdk::xusb_common`).

---

## Usage - Host Stack Initialization and Polling

```c
#include "xusbh_core.h"

static xUSBH_Context_t g_host;

// HCD driver options (provided by hardware porting layer)
extern const xUSBH_HCD_Ops_t g_my_hcd_ops;
extern void *g_my_hcd_ctx;

void usb_host_init(void)
{
    // Step 1 - Initialize the host stack context
    xUSBH_Init_Config_t init_cfg = {
        .root_port_count = 1U,
    };
    xUSBH_Init(&g_host, &init_cfg);

    // Step 2 - Register Class Drivers (e.g. MSC/HID) before start
    // xUSBH_MSC_Register(&g_host, &g_msc_ctx);

    // Step 3 - Start the host stack
    xUSBH_Start_Config_t start_cfg = {
        .hcd_ops = &g_my_hcd_ops,
        .hcd_ctx = g_my_hcd_ctx,
    };
    xUSBH_Start(&g_host, &start_cfg);
}

void usb_host_poll_loop(void)
{
    while (true) {
        // Poll and advance host state machine (port debounce, enumeration)
        xUSBH_Process(&g_host);
        
        // Wait or yield to RTOS task
        xRTOS_Task_Yield();
    }
}
```

---

## Key Public APIs

| Function | Description |
|---|---|
| `xUSBH_Init` | Initialize host context, set maximum ports, and clear descriptor slots |
| `xUSBH_Start` | Bind Host Controller Driver (HCD) and enable interrupts |
| `xUSBH_Process` | Poll and progress the port debounce and enumeration state machines |
| `xUSBH_Stop` | Stop the HCD and detach the host controller |
| `xUSBH_Device_Allocate` | Allocate a new device context slot |
| `xUSBH_Device_Release` | Release and clean up a device context slot |
| `xUSBH_Interface_Allocate` | Allocate an interface context slot within a device |
| `xUSBH_Endpoint_Allocate` | Allocate an endpoint context slot within an interface |
| `xUSBH_Transfer_Submit` | Submit an asynchronous control/bulk/interrupt transfer to HCD |
| `xUSBH_Transfer_Cancel` | Cancel a pending transfer |

---

## License

Apache 2.0 - see [LICENSE](../../../LICENSE).
