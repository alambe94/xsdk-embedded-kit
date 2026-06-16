# xusb

Portable USB device stack for embedded targets.

**CMake target:** `xsdk::xusbd`

---

## Class Drivers

| Class | Header | Description |
|---|---|---|
| CDC | `xusbd_cdc.h` | USB Communications Device - virtual serial port |
| HID | `xusbd_hid.h` | Human Interface Device - keyboard, mouse, gamepad |
| MSC | `xusbd_msc.h` | Mass Storage Class - USB drive (BOT/SCSI) |
| DFU | `xusbd_dfu.h` | Device Firmware Upgrade |
| WinUSB | `xusbd_win.h` | WinUSB / Microsoft OS 2.0 - driverless access on Windows |

Multiple class drivers can be registered on a single device (composite device).

---

## Design

- **No dynamic allocation.** All device and class contexts are statically
  allocated by the application.
- **DCD abstraction.** The stack depends on a `xUSBD_DCD_Ops_t` ops table,
  not on any specific hardware. The hardware driver is provided externally
  (for example, by `xDRIVERS`).
- **Composite-friendly.** Each class driver allocates its own interfaces,
  endpoints, and string indices during registration. The stack builds the
  configuration descriptor automatically.

---

## Integration

```cmake
add_subdirectory(path/to/xSDK)
target_link_libraries(my_app PRIVATE xsdk::xusbd)
```

Use the role-specific `xsdk::xusbd` target for device-stack applications.

Public device headers are in `device/include/`. Shared USB specification
headers are in `include/`.

---

## Usage - CDC (virtual serial port)

```c
#include "xusbd_std.h"
#include "xusbd_cdc.h"

// Static allocation - no heap required
static xUSBD_Device_Context_t g_device;
static xUSBD_CDC_Context_t    g_cdc;

// Application callbacks - implement these in your application
static xUSBD_CDC_Callbacks_t cdc_callbacks = {
    .on_bus_event         = my_cdc_bus_event,
    .on_receive           = my_cdc_receive,
    .on_transmit_complete = my_cdc_transmit_complete,
};

// dcd_ops and dcd_ctx come from your hardware driver (e.g., xDRIVERS)
void usb_start(const xUSBD_DCD_Ops_t *dcd_ops, void *dcd_ctx)
{
    // Step 1 - initialize the device descriptor
    xUSBD_Init_Config_t init_cfg = {
        .speed                = USB_SPEED_HIGH,
        .vendor_id            = 0x1209U,
        .product_id           = 0x0001U,
        .vendor_string        = (const uint8_t *)"My Company",
        .product_string       = (const uint8_t *)"CDC Device",
        .serial_number_string = (const uint8_t *)"0001",
    };
    xUSBD_Init(&g_device, &init_cfg);

    // Step 2 - register the CDC class driver
    xUSBD_Class_Register_Config_t reg_cfg = {
        .class_ctx    = &g_cdc.class_ctx,
        .class_driver = xUSBD_CDC_Class(),
        .class_config = NULL,
    };
    xUSBD_Class_Register(&g_device, &reg_cfg);
    xUSBD_CDC_Set_Callbacks(&g_cdc.class_ctx, &cdc_callbacks);

    // Step 3 - start the device
    xUSBD_Start_Config_t start_cfg = {
        .port    = 0,
        .dcd_ops = dcd_ops,
        .dcd_ctx = dcd_ctx,
    };
    xUSBD_Start(&g_device, &start_cfg);
}
```

To transmit data once the device is configured:

```c
xUSBD_CDC_Transmit(&g_cdc.class_ctx, data, length);
```

To receive data, arm the receive buffer inside `on_bus_event` when
`USB_DCD_CONNECT_RECEIVED` fires:

```c
xUSBD_CDC_Prepare_To_Receive(&g_cdc.class_ctx, rx_buf, sizeof(rx_buf));
```

---

## Usage - Composite Device (CDC + HID)

Register multiple class drivers before calling `xUSBD_Start`. The stack
allocates interface and endpoint numbers automatically:

```c
xUSBD_Init(&g_device, &init_cfg);

xUSBD_Class_Register(&g_device, &cdc_reg_cfg);   // interfaces 0-1
xUSBD_Class_Register(&g_device, &hid_reg_cfg);   // interface 2

xUSBD_Start(&g_device, &start_cfg);
```

---

## Key Public APIs

| Function | Description |
|---|---|
| `xUSBD_Init` | Initialize device context and descriptor |
| `xUSBD_Start` | Attach to the bus via DCD ops table |
| `xUSBD_Stop` | Detach from the bus and release DCD |
| `xUSBD_Class_Register` | Register a class driver before start |
| `xUSBD_Get_Lifecycle_State` | Query current device lifecycle state |
| `xUSBD_Is_Configured` | True once the host completes enumeration |
| `xUSBD_CDC_Class` | Returns the CDC class driver ops table |
| `xUSBD_CDC_Transmit` | Send data to the host |
| `xUSBD_CDC_Prepare_To_Receive` | Arm the receive buffer |

See `device/include/xusbd_std.h` and `device/include/xusbd_cdc.h` (and the
equivalent headers for each class) for the full API reference.

---

## Stopping the Device

```c
xUSBD_Stop(&g_device);
```

After `xUSBD_Stop` the device can be re-initialized and restarted by
repeating the three-step sequence above.

---

## License

Apache 2.0 - see [LICENSE](../../../LICENSE).
