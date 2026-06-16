# xboot

Lightweight secondary bootloader and update framework.

**CMake target:** `xsdk::xboot`

---

## Design Principles

- **No dynamic allocation.** Statically allocated bootloader context and data structures.
- **Hardware Abstraction.** Decoupled storage access (`xBOOT_Storage_Ops_t`) and platform-specific control (`xBOOT_Port_Ops_t`). Plug in any flash (QSPI, SPI, OSPI, eMMC) and platform (cortex-R5, RISC-V, etc.).
- **Safe A/B Updates.** Employs redundant primary and secondary partition logic to prevent bricking during firmware upgrades.
- **Verification & Authentication.** Built-in support for secure boot header parsing, CRC/checksum checks, and optional cryptographic signature verification.
- **Recovery Mode.** Allows forcing recovery or entering low-level serial update mode based on configuration or GPIO straps.

---

## Integration

```cmake
add_subdirectory(path/to/xSDK)
target_link_libraries(my_bootloader PRIVATE xsdk::xboot)
```

The `xboot` module depends on `xsdk::xutil` for core assertions, basic string helpers, and debug log outputs.

---

## Usage - Bootloader Execution

```c
#include "xboot_core.h"

static xBOOT_Context_t g_boot_ctx;

// Abstract operations provided by platform porting layer
extern const xBOOT_Storage_Ops_t g_my_storage_ops;
extern const xBOOT_Port_Ops_t g_my_port_ops;

int main(void)
{
    // Step 1 - Define boot config and assign platform ops
    xBOOT_Config_t cfg = {
        .storage_ops    = &g_my_storage_ops,
        .storage_ctx    = NULL,
        .port_ops       = &g_my_port_ops,
        .port_ctx       = NULL,
        .force_recovery = false,
    };

    // Step 2 - Initialize bootloader context
    if (xBOOT_Init(&g_boot_ctx, &cfg) == xRETURN_OK) {
        // Step 3 - Run the boot decision and handoff logic
        // This will locate, verify, and jump to the chosen application image.
        // On success, this function does not return.
        xBOOT_Run(&g_boot_ctx);
    }

    // Fallback on boot failure - enter infinite loop or reset
    while (true);
}
```

---

## Key Public APIs

| Function | Description |
|---|---|
| `xBOOT_Init` | Initialize the bootloader context, validate platform operations |
| `xBOOT_Run` | Main entry point that parses headers, makes boot decisions, verifies the image, and invokes handoff |
| `xBOOT_Image_Verify` | Verify checksum/hash/signatures of a given partition offset |
| `xBOOT_Partition_Find_Active` | Query the storage offsets to locate the active firmware image |
| `xBOOT_Handoff` | Platform-specific CPU registers and stack teardown before branching to app main |

---

## License

Apache 2.0 - see [LICENSE](../../../LICENSE).
