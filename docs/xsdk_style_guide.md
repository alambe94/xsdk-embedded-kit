# xSDK Unified Style Guide

This document defines the global coding, naming, architecture, and API conventions for the entire xSDK.
All components (`xUSB`, `xFS`, and future stacks) must strictly adhere to these rules to ensure a clean,
predictable, and professional enterprise-grade embedded library.

---

## 1. Types and Primitives

**Rule:** Use standard C types from `<stdint.h>`, `<stdbool.h>`, and `<stddef.h>`.
Never define custom primitive wrappers.

* [OK] `uint8_t`, `uint16_t`, `uint32_t`, `size_t`, `bool`
* [NO] `xU08`, `xU32`, `xBOOL` - custom primitive wrappers are prohibited
* [NO] `int`, `char`, `short`, `unsigned int` - avoid raw C primitives where exact width matters

---

### 1a. Loop Variables and Counters

**Rule:** A loop variable or counter shall use the type of the domain it measures.

Use `size_t` when iterating over C arrays, object sizes, memory buffers, or values
returned by `sizeof`, `strlen`, and similar standard C interfaces. Use a
fixed-width type when the counter represents a protocol field, hardware value,
persisted format, or another contract with an explicit width.

```c
for (size_t i = 0U; i < buffer_size; i++)
{
    process_byte(buffer[i]);
}

for (uint32_t block = 0U; block < protocol_block_count; block++)
{
    process_protocol_block(block);
}
```

---

### 1b. Sizes, Counts, and Fixed-Width Boundaries

**Rule:** Use `size_t` for the size, capacity, count, offset, or index of a C
object or memory buffer. Use `uint32_t` only when 32-bit width is part of the
external or stored contract.

Typical `size_t` uses:

* Memory buffer lengths and capacities.
* Array element counts and indexes.
* Values compared with or derived from `sizeof`, `strlen`, or allocation APIs.
* Private parsing cursors that only address an in-memory object.

Typical fixed-width uses:

* Wire protocol fields and encoded trace records.
* Hardware registers, DMA descriptors, and peripheral transfer contracts.
* Flash offsets, persisted structures, and ABI fields whose width is specified.
* Timestamps and counters with an explicitly documented width or wrap behavior.

Do not narrow from `size_t` to `uint32_t` implicitly. Validate at the boundary,
then cast:

```c
if (buffer_size > UINT32_MAX)
{
    return xRETURN_INVALID_PARAMETER;
}

descriptor->length = (uint32_t)buffer_size;
```

Public headers that expose `size_t` shall include `<stddef.h>` directly. Use
`%zu` when formatting a `size_t`.

---

## 2. Global Prefixes

The leading prefix identifies which component and visibility level an identifier belongs to.
It is the most critical naming decision - choosing the wrong prefix makes code misleading.

| Prefix | Used for |
|---|---|
| `xUSBD_` | USB Device stack - all types, public APIs |
| `xUSBH_` | USB Host stack - all types, public APIs |
| `xUSB_` | Generic USB stack-wide types and constants |
| `USB_` | **Reserved** - constants and types defined literally by the USB Specification |
| `xFS_` | File System stack - all types, public APIs |
| `xRTOS_` | RTOS kernel - all types, public APIs |
| `xI2C_` | I2C driver - all types, public APIs |
| `xSPI_` | SPI driver - all types, public APIs |
| `xUART_` | UART driver - all types, public APIs |
| `xGPIO_` | GPIO driver - all types, public APIs |
| `xDMA_` | DMA driver - all types, public APIs |
| `xTIMER_` | Timer driver - all types, public APIs |
| `xADC_` | ADC driver - all types, public APIs |
| `xWDT_` | Watchdog timer driver - all types, public APIs |
| `xRTC_` | RTC driver - all types, public APIs |
| `xCAN_` | CAN driver - all types, public APIs |
| `xETH_` | Ethernet MAC driver - all types, public APIs |
| `xQSPI_` | QSPI driver - all types, public APIs |
| `xSDIO_` | SDIO driver - all types, public APIs |
| `xTRACE_` | xTrace event tracing component - all types, public APIs |
| `xFAULT_` | xFault fatal exception diagnostics component - all types, public APIs |
| `xCMD_` | Command framework - registry, metadata, permissions, and parser-independent dispatch |
| `xCLI_` | Text command parser frontend - argc/argv parsing, help formatting, text dispatch adapter |
| `xSHELL_` | Interactive shell sessions - prompt, line editing, history, completion, and transport UX |
| `xRETURN_` | SDK-wide return code infrastructure |

The leading lowercase `x` is always lowercase - even in macros. `XUSBD_` is never correct.

---

## 3. Types and Enums

**Rule:** Use `Pascal_Snake_Case` with a `_t` suffix. All typedefs, structs, and enums must start with the appropriate component prefix.

* Acronyms (CDC, HID, MSC, DFU) remain fully capitalised.
* Avoid arbitrary abbreviations - use `Device` instead of `Dev`.

```c
// [OK]
typedef struct xUSBD_Device_Context_t { ... } xUSBD_Device_Context_t;
typedef enum   xUSBD_DFU_State_t      { ... } xUSBD_DFU_State_t;
typedef struct xFS_File_Context_t     { ... } xFS_File_Context_t;

// [NO]
typedef struct USBD_Dev_Context_t { ... } USBD_Dev_Context_t;  // wrong prefix, abbreviated
typedef struct xUSBD_DEV_Context_t { ... };                    // shouting prefix
```

---

## 4. Struct Members, Variables, and Pointers

**Rule:** All variables, struct members, and function parameters use `snake_case`.

**Pointer alignment:** `*` attaches to the variable name, not the type.

```c
uint8_t *buffer;          // [OK]
uint8_t* buffer;          // [NO]
```

**West-const:** `const` always precedes the type it qualifies.

```c
const uint8_t *buffer;    // [OK]  pointer to constant data
uint8_t const *buffer;    // [NO]  east-const - prohibited
uint8_t * const ptr;      // [OK]  constant pointer (rare, same rule applies)
```

**Exception - USB Specification wire-protocol fields:** Structs that map exactly to the USB wire-protocol
(Setup Packets, Standard Descriptors) must keep the official spec field names (`bRequestType`, `wValue`,
`bmAttributes`) to match the USB 2.0/3.0 documentation verbatim.

---

## 5. Function Naming

**Rule:** Public APIs use `Pascal_Snake_Case` with the component prefix. Private/static functions use `snake_case` with no prefix - the absence of a prefix is itself the signal that the function is private.

```c
// [OK] Public APIs
xRETURN_t xUSBD_Class_Register(xUSBD_Device_Context_t *device_ctx, ...);
xRETURN_t xFS_Mount(xFS_Volume_Context_t *volume_ctx, ...);

// [OK] Private/static - no prefix
static xRETURN_t build_config_descriptor(xUSBD_Device_Context_t *device_ctx);
static uint32_t  parse_bpb(const uint8_t *sector_data);

// [NO] Never prefix a static function with the component prefix
static xRETURN_t xUSBD_build_config_descriptor(...);
```

### 5a. Function Prototype Declarations

**Rule:** Function prototype declaration is mandatory for every function.

* Public functions must be declared in the owning public header.
* Private/static functions must be declared in the source file's `// FUNCTION PROTOTYPES` section.
* Do not rely on definition order instead of declaring a prototype.

```c
// Correct: private prototype declared before implementation
static xRETURN_t build_config_descriptor(xUSBD_Device_Context_t *device_ctx);

// ...

static xRETURN_t build_config_descriptor(xUSBD_Device_Context_t *device_ctx)
{
    return xRETURN_OK;
}
```

---
## 6. Context Objects

**Rule:** Use "Context" for all typed state-tracking structs. Never use "Handle" - a handle implies an opaque
pointer (`void *`). Since these are concrete typed structs, they are contexts.

```c
xUSBD_Device_Context_t *device_ctx;   // [OK]
xFS_File_Context_t     *file_ctx;     // [OK]
xUSBD_Device_Handle_t  *dev_handle;   // [NO]
```

### 6a. Embedded Class Contexts (Critical)

When a component-specific context struct embeds a base context as its first member for OOP-style
inheritance, the embedded member name must describe the base type being embedded.
For USB device classes, the embedded `xUSBD_Class_Context_t` member **must always be named
`class_ctx`** across every class.

Generic stack code that upcasts a class-specific context pointer to the base type relies on the
embedded context being the first struct member at offset zero.

```c
// [OK] Correct - class_ctx is always the first field for USB device classes
typedef struct xUSBD_CDC_Context_t {
    xUSBD_Class_Context_t class_ctx;
    uint8_t cmd_interface;
    uint8_t data_interface;
} xUSBD_CDC_Context_t;

// [OK] Upcast is always safe
xUSBD_Class_Context_t *class_ctx = (xUSBD_Class_Context_t *)&cdc_ctx;

// [NO] Prohibited names
base_ctx, super_ctx, core_ctx, parent_ctx
```

---

## 7. Enum Values

**Rule:** Enum values use `ALL_CAPS_WITH_UNDERSCORES` and must carry the same prefix as their enclosing
enum type, minus the `_t` suffix.

```c
// [OK] xUSBD_DFU_State_t  -> prefix is xUSBD_DFU_STATE_
xUSBD_DFU_STATE_APP_IDLE
xUSBD_DFU_STATE_DFU_IDLE

// [OK] xFS_Return_Code_t -> prefix is xFS_RETURN_CODE_
xFS_RETURN_CODE_FILE_NOT_FOUND

// [NO] No module prefix
DFU_STATE_APP_IDLE
```

---

## 8. Macros

The component prefix keeps its lowercase `x` even in macros. Only the words **after** the prefix change case:

* **Constant / configuration macros** - prefix + `ALL_CAPS_WORDS`:

  ```c
  xUSBD_MAX_EP0_DATA_SIZE     // [OK]
  xFS_MAX_PATH_LENGTH         // [OK]
  XUSBD_MAX_EP0_DATA_SIZE     // [NO] - uppercase X is always wrong
  ```

* **Function-like / helper macros** - prefix + `Pascal_Snake_Case`:

  ```c
  xUSBD_MOS_Property(name, data, length)   // [OK]
  xUSBD_MOS_Props(...)                     // [OK]
  ```

* Function-like macros must not contain a trailing semicolon inside the macro body.

---

## 9. Application Callback Structs

**Rule:** Application-facing callback dispatch tables are named `x<COMPONENT>_<Class>_Callbacks_t`.
Callback fields use `snake_case` with an `on_` prefix (event notification - "this happened") and return
`xRETURN_t`.

```c
typedef struct xUSBD_CDC_Callbacks_t {
    xRETURN_t (*on_bus_event)       (xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);
    xRETURN_t (*on_data_received)   (xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
    xRETURN_t (*on_transmit_complete)(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
} xUSBD_CDC_Callbacks_t;
```

---

## 10. Driver Dispatch Table Fields

**Rule:** Fields inside internal class driver structs are commands from the stack, not event notifications.
They use imperative verbs and `snake_case` with **no** `on_` prefix.

* **Stack -> App (Rule 9):** `on_*` - event notification ("this happened")
* **Stack -> Class (Rule 10):** imperative verb - command ("do this")

```c
// [OK] Dispatch table fields
init_instance, build_descriptor, bus_event, control_in_request, data_received

// [NO] Wrong prefix for dispatch fields
on_init_instance, on_build_descriptor
```

---

## 11. Abbreviations

Only the abbreviations in the table below may be used as identifier segments.

| Abbreviation | Meaning | Allowed in |
|---|---|---|
| `EP` | Endpoint | Any context |
| `MPS` | Max Packet Size | Any context |
| `HCD` | Host Controller Driver | USB host controller interfaces and port drivers |
| `Ops` | Operations table | Driver and class dispatch-table type names |
| `MOS` | Microsoft OS descriptor | Any context |
| `BOS` | Binary Object Store | Any context |
| `CDC`, `HID`, `MSC`, `DFU`, `UAC`, `UVC`, `WIN` | USB class acronyms | Any context |
| `IO` | Input/Output | Any context |
| `ISR` | Interrupt Service Routine | Any context |
| `ctx` | Context | Variables and struct fields only (e.g., `class_ctx`, `device_ctx`) |
| `dev`, `Dev`, `DEV` | Device | Any context |
| `intf`, `Intf`, `INTF` | Interface | Any context |
| `desc`, `Desc`, `DESC` | Descriptor | Any context |
| `cfg`, `Cfg`, `CFG` | Config | Any context |
| `len`, `Len`, `LEN` | Length | Any context |
| `addr`, `Addr`, `ADDR` | Address | Any context - `adr` is not a valid variant |
| `str`, `Str`, `STR` | String | Any context |
| `idx`, `Idx`, `IDX` | Index | Any context |
| `buf`/`buff`, `Buf`, `BUF` | Buffer | Any context - both single and double `f` accepted |
| `req`, `Req`, `REQ` | Request | Any context |

**One prohibition remains:** `Ctx` and `CTX` are prohibited in type names and macros.
Use `Context` in type names (e.g., `xUSBD_Device_Context_t`, not `xUSBD_Device_Ctx_t`).
Lowercase `ctx` is still allowed for variables and struct fields.

---

## 12. File Naming

**Rule:** Source and header files use `snake_case` with the appropriate component prefix.

| Component | Pattern |
|---|---|
| USB Device stack | `xusbd_<module>.c` / `xusbd_<module>.h` |
| USB Host stack | `xusbh_<module>.c` / `xusbh_<module>.h` |
| Generic USB | `xusb_<module>.h` |
| File System | `xfs_<module>.c` / `xfs_<module>.h` |
| Shared utilities | `x<util>.h` (e.g., `xreturn.h`, `xtype.h`) |

---

### 12a. File Organization Threshold

**Rule:** A logical feature may remain as a flat file pair only while it is
small. Once it owns more than three production `.c` / `.h` files, it shall be
promoted into a dedicated directory.

Production files count toward the threshold:

```text
<feature>.h
<feature>.c
<feature>_config.h
<feature>_return.h
```

Support files do not count toward the threshold:

```text
CMakeLists.txt
README.md
<feature>_implementation_plan.md
docs/misra_waivers.md
tests/test_<feature>.c
```

Required shape after promotion:

```text
src/components/<component>/<feature>/
|-- include/
|-- src/
|-- tests/
`-- docs/
```

The same rule applies below umbrella components such as `xutil`. For example,
`xtrace`, `xreturn`, `xassert`, and `xlog` are separate logical utility
features even though they live under the `xutil` component.

Flat include rules still apply after promotion: source files include headers by
filename only, and the module `CMakeLists.txt` owns include-path exposure.

---

## 13. Header Guards

**Rule:** Guards use `X<COMPONENT>_<FILENAME>_H`, all caps, no leading or trailing underscore.
Leading underscores followed by an uppercase letter are reserved identifiers in C.

```c
#ifndef XUSBD_CLASS_H    // [OK]
#ifndef XUSB_DEFS_H      // [OK]
#ifndef XRETURN_H        // [OK] (shared utility)
#ifndef TEMPLATE_H       // [OK] (non-component file - use <FILENAME>_H, no prefix)
#ifndef _USBD_CLASS_H_   // [NO] leading underscore
#ifndef XUSBD_CLASS_H_   // [NO] trailing underscore
```

The closing `#endif` must always carry a comment identifying the guard:

```c
#endif // XUSBD_CLASS_H
```

---

## 14. Return Type Convention

**Rule:** All public functions that can fail must return `xRETURN_t`. Use `void` only when a function is
guaranteed never to fail. Module-specific enums (e.g., `xUSB_Return_Code_t`) define named constant sets
for comparison - they are not used as function return types.

| Function kind | Return type |
|---|---|
| Public API (`xUSBD_` / `xFS_`) | `xRETURN_t` |
| Dispatch table function that returns a value | Appropriate domain type (e.g., `size_t` for a memory byte count) |
| Private static helper | Whatever is appropriate - `void`, `size_t`, a fixed-width integer, or a typed value |

```c
xRETURN_t xUSBD_Class_Register(xUSBD_Device_Context_t *device_ctx, ...);   // [OK] Public API
xRETURN_t xFS_Mount(xFS_Volume_Context_t *volume_ctx, ...);                 // [OK] Public API

size_t (*build_descriptor)(xUSBD_Class_Context_t *class_ctx,                // [OK] Returns an in-memory size
                           uint8_t *buffer, size_t buffer_capacity,
                           USB_Speed_t speed);

static void append_mos_property(uint8_t *buffer,                            // [OK] Cannot fail
                                 const xUSBD_MOS_Property_t *property);
```

---

## 15. Boolean Field Naming

**Rule:** Struct members that represent a boolean state must use a descriptive prefix.
Never use bare names, negated names, or `flag_` prefix.

| Prefix | Use for |
|---|---|
| `is_` | Current state (e.g., `is_configured`, `is_mounted`) |
| `has_` | Capability or ownership (e.g., `has_remote_wakeup`, `has_write_protect`) |
| `can_` | Permission or ability (e.g., `can_write`, `can_upload`) |

```c
bool is_configured;         // [OK]
bool has_remote_wakeup;     // [OK]
bool can_write;             // [OK]
bool configured;            // [NO] ambiguous
bool not_configured;        // [NO] negated logic inverts every read site
bool flag_configured;       // [NO] verbose and redundant
```

---

## 16. Boolean Field Type

**Rule:** Boolean struct members must use `bool` from `<stdbool.h>`. Never use `uint8_t`, `int`, or
`unsigned char` as a substitute.

```c
// [OK]
#include <stdbool.h>
typedef struct {
    bool is_configured;
    bool has_remote_wakeup;
} xUSBD_Device_Context_t;

// [NO]
typedef struct {
    uint8_t is_configured;   // integer masquerading as bool
} xUSBD_Device_Context_t;
```

---

## 17. File Structure and Templates

**Rule:** All new `.c` and `.h` files must be created from `template.c` and `template.h`.

1. **Header block:** Every file starts with the MIT License notice, followed by `// @file` and `// @brief` tags.
2. **C++ compatibility:** Headers always wrap their public content in `#ifdef __cplusplus` / `extern "C"`.
3. **Section dividers:** Both file types use the same section names. Do not invent new sections.

   **Both file types:**
   * `// INCLUDES` (sub-sections in order: `// COMPILER INCLUDES`, `// SYSTEM INCLUDES`, `// MODULE INCLUDES`, `// DEBUG`)
   * `// MACROS`
   * `// TYPES`
   * `// VARIABLES`
   * `// FUNCTION PROTOTYPES`
   * `// INLINE FUNCTIONS`

   **Source files only (additional sections):**
   * `// EXTERN VARIABLES`
   * `// MODULE FUNCTIONS IMPLEMENTATION` (static/private)
   * `// PUBLIC FUNCTIONS IMPLEMENTATION` (global/public)

   **`// DEBUG` sub-section:** Source files that use logging must define `MODULE_LOG_LEVEL`
   immediately before `#include "xlog.h"`. Place this pair as the last sub-section under
   `// INCLUDES`, separated by a blank line from the rest of `// MODULE INCLUDES`. This keeps the
   `#define` visually grouped with its header rather than floating inline between regular includes.

   Use `xlog.h` and `xassert.h` from `xutil` directly - they are SDK-wide and work in any
   component without modification.

   ```c
   // MODULE INCLUDES
   #include "xusbd_return.h"
   #include "xusbd_class.h"
   #include "xusbd_cdc.h"
   #include "xusbd_log.h"
   ```

4. **EOF footer:** Every file ends with:
   ```
   // EOF /////////////////////////////////////////////////////////////////////////////
   ```

---

### 17a. Include Path Style

**Rule:** Use flat includes - the header file name only. Never navigate with relative path segments.

```c
// [OK] Flat - works regardless of where the .c file lives
#include "xfs_file.h"
#include "xusbd_class.h"

// [NO] Relative path - leaks directory layout into source code
#include "../include/xfs_file.h"
#include "../../xutil/xassert.h"
```

Flat includes work because every module's `CMakeLists.txt` exposes its `include/` directory
via `target_include_directories` with `PUBLIC` scope. The build system owns the search path;
the source file owns only the name:

```cmake
target_include_directories(xfs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
```

This rule applies uniformly across all `src/`, `port/`, and `tests/` subdirectories within
a module - the search path is the same for all of them.

**Include section order** within a file (matches the `// INCLUDES` section layout):

```c
// COMPILER INCLUDES  - standard C/C++ headers only
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// SYSTEM INCLUDES    - RTOS, OS, or platform headers
#include "FreeRTOS.h"

// MODULE INCLUDES    - SDK module headers, flat names, no path prefix
#include "xfs_file.h"
#include "xfs_defs.h"
```

Do not sort includes. Manual grouping is intentional and must be preserved.

---

## 18. Comments

**Rule:** Use single-line `//` comments exclusively. `/* */` block comments are not recommended and
should be avoided. Single-line `//` works for both one-liners and multi-line blocks and avoids nested
comment syntax errors during debugging.

```c
// [OK] Multi-line comment block
// This function builds the configuration descriptor for the given speed.
// It must be called after all classes have been registered.

// [NO] Not recommended
/* This function builds the configuration descriptor. */
```

---

## 19. Code Formatting

All source and header files must be formatted with **clang-format** using the `.clang-format` file at
the repository root. Run `format_xusb.bat` to apply formatting to the xUSB component.

| Rule | Setting | Rationale |
|---|---|---|
| Indentation | 4 spaces, no tabs | Consistent across all editors |
| Brace style | Allman - opening brace on its own line | Matches existing codebase |
| Braces required | All control structures must have braces | Prevents single-statement bugs |
| Column limit | 140 characters | Fits two files side-by-side on a wide monitor |
| Pointer alignment | Right - `uint8_t *ptr` | Type describes the variable, not the pointer |
| `const` placement | West-const - `const uint8_t *` | Enforced automatically by `QualifierAlignment: Left` |
| `#define` alignment | Consecutive groups aligned | Readable constant blocks |
| Include order | Preserved - never sorted | Manual grouping is intentional |

---

## 20. Return Code and Error Handling

### 20a. Universal Return Type

All SDK functions that can fail return `xRETURN_t` (`uint32_t`), defined in `xreturn.h`.
`xRETURN_OK = 0` is the universal success value regardless of module.

### 20b. Return Code Layout

```
bits [31:16]  module ID   - which SDK module produced this value
bits [15:14]  severity    - 0=OK, 1=error, 2=warning, 3=message
bits [13:0]   code        - per-module, per-severity specific code
```

Use the macros from `xreturn.h`:

```c
// Construct
xRETURN_t ret = xRETURN_MAKE(xRETURN_xUSBD_MODULE, xRETURN_SEVERITY_ERROR, 0x001);

// Classify
if (xRETURN_IS_OK(ret))      { /* success          */ }
if (xRETURN_IS_ERROR(ret))   { /* fatal - abort    */ }
if (xRETURN_IS_WARNING(ret)) { /* non-fatal - log  */ }
if (xRETURN_IS_MESSAGE(ret)) { /* informational    */ }

// Decode
uint32_t module   = xRETURN_GET_MODULE(ret);
uint32_t severity = xRETURN_GET_SEVERITY(ret);
uint32_t code     = xRETURN_GET_CODE(ret);
```

### 20c. Module ID Registry

Every SDK module gets a unique ID constant in `xreturn.h`. Never reuse an ID.

```c
#define xRETURN_xUSBD_MODULE    0x0001U
#define xRETURN_xFS_MODULE      0x0002U
// Add new modules here.
```

### 20d. Per-Module Return Code Enums

Each module defines its return codes in its own header. Use `xRETURN_MAKE` for every non-zero value.
Errors, warnings, and messages are separated by the severity field - no manual sub-range bases needed.

```c
// xusbd_return.h
typedef enum {
    // Errors [severity=ERROR, 0x001 - 0x0FF]
    xRETURN_xERR_xUSBD_NULL_POINTER  = xRETURN_MAKE(xRETURN_xUSBD_MODULE, xRETURN_SEVERITY_ERROR, 0x001),
    xRETURN_xERR_xUSBD_INVALID_PORT,

    // Warnings [severity=WARNING]
    xRETURN_xWRN_xUSBD_STRING_TRUNCATED = xRETURN_MAKE(xRETURN_xUSBD_MODULE, xRETURN_SEVERITY_WARNING, 0x001),

    // Messages [severity=MESSAGE]
    xRETURN_xMSG_xUSBD_REQ_SET_ADDRESS  = xRETURN_MAKE(xRETURN_xUSBD_MODULE, xRETURN_SEVERITY_MESSAGE, 0x001),
} xRETURN_xUSBD_t;

// xfs_return.h
typedef enum {
    xRETURN_xFS_OK = 0,
    xRETURN_xERR_xFS_FILE_NOT_FOUND = xRETURN_MAKE(xRETURN_xFS_MODULE, xRETURN_SEVERITY_ERROR, 0x001),
    xRETURN_xERR_xFS_DISK_FULL,
} xRETURN_xFS_t;
```

### 20e. Application-Side Interpretation

```c
xRETURN_t ret = xUSBD_Class_Register(&device_ctx, &cdc_ctx);

if (ret != xRETURN_OK)                         { /* any failure    */ }
if (xRETURN_IS_ERROR(ret))                     { /* abort          */ }
if (ret == xRETURN_xERR_xUSBD_NULL_POINTER)   { /* exact match    */ }
```

### 20f. Mandatory Return Code Checking

**Rule:** Callers shall check the return code of every function that returns `xRETURN_t` or another status/error code.

Ignoring a return code is prohibited. A caller may continue only after it has explicitly handled success,
warning, and error cases according to the function contract.

Do not introduce an extra lexical block solely for return-code checking; place the status assignment and check directly in the current scope.

```c
// Correct: check and propagate
xRETURN_t ret = xUSBD_DCD_EP_Send(device_ctx->dcd_ops, device_ctx->dcd_ctx, ep_addr, data, length, false);
if (ret != xRETURN_OK)
{
    return ret;
}

// Prohibited: ignored return code
xUSBD_DCD_EP_Send(device_ctx->dcd_ops, device_ctx->dcd_ctx, ep_addr, data, length, false);
```

---
## 21. Hardware Abstraction

**Rule:** Components must never directly access hardware peripherals. All hardware interactions must
occur through interface structs (function pointer tables) provided at initialisation. This keeps
the stack portable and testable.

```c
// [OK] xFS uses a driver interface - it never touches an SPI/SDMMC register directly
typedef struct xFS_Block_Driver_t {
    xRETURN_t (*read) (uint32_t block, uint8_t *buffer, uint32_t count);
    xRETURN_t (*write)(uint32_t block, const uint8_t *buffer, uint32_t count);
} xFS_Block_Driver_t;
```

---

## 22. Debug, Logging, and Tracing

Diagnostic feedback and event tracing use shared infrastructure from `xutil` designed to maximize troubleshooting visibility while keeping runtime noise, CPU overhead, and flash footprint to a minimum.

### 22a. xASSERT (Invariant Enforcement)

*   **Definition File:** [xassert.h](file:///c:/Users/lambe/Documents/Embedded_Swiss_Knife/sub_modules/xsdk/src/components/xutil/xassert.h)
*   **Usage Rule:** Used exclusively to catch programming bugs (e.g., null pointers, out-of-bound arguments, division by zero) at development time. Never use `xASSERT` to handle runtime/environmental errors (such as disk full or communication timeouts); these must be handled via `xRETURN_t` checks.
*   **SNR Optimization:**
    *   **Compile-Time Gated:** Controlled globally by the build option `-DxSDK_ENABLE_ASSERT=1`.
    *   **Zero-cost in Production:** When disabled in release builds (`xSDK_ENABLE_ASSERT=0`), `xASSERT` compiles away to a no-op `((void)0)`, leaving no code or literal string footprint in the final production binary.
    *   **Scope Safe:** Safe for use in both `.c` files and `static inline` functions inside headers.
    *   **Customizable Halt:** Includes a customizable `xASSERT_HOOK()` to safely halt execution (e.g., `__BKPT(0)`) or trigger a diagnostics dump/watchdog reset.

```c
#include "xassert.h"

// Usage: halts in debug/test builds, compiled to no-op in release
xASSERT(ptr != NULL, "ptr is NULL");
```

---

### 22b. xLOG (Diagnostic Logging)

*   **Definition File:** [xlog.h](file:///c:/Users/lambe/Documents/Embedded_Swiss_Knife/sub_modules/xsdk/src/components/xutil/xlog.h)
*   **Usage Rule:** Intended for runtime diagnostics, warnings, and errors. It must be included exactly once per `.c` file *after* defining the local `MODULE_LOG_LEVEL`.
*   **SNR Optimization:**
    *   **Local Scope:** Rather than using a global verbose log level, logging is configured individually per source file using `MODULE_LOG_LEVEL`. Developers can enable verbose debugging on a single file under review while keeping other system files silent.
    *   **Predefined Log Levels:**
        *   `0U` (Silent): Code and formatting strings are completely compiled away.
        *   `1U` (Status Only): Outputs only the hex value of the diagnostic code (e.g., `[00018001]`), keeping communication/print bandwidth extremely low.
        *   `2U` (Verbose): Outputs the status code plus the human-readable formatting string.
    *   **No Magic Numbers:** Every code passed to `xLOG` must be a named SDK return constant (e.g., `xRETURN_xERR_xUSBD_NULL_POINTER`), ensuring logs are decodeable offline without reading target source code.

```c
// MODULE INCLUDES - include the module log header; level is set in <module>_config.h
#include "xusbd_log.h"

// Usage: logs a named status constant with an optional formatting message
xUSBD_LOG(xRETURN_xERR_xUSBD_NULL_POINTER, "null pointer at entry");
```

---

### 22c. xTRACE (Execution Recording)

*   **Definition File:** [xtrace.h](file:///c:/Users/lambe/Documents/Embedded_Swiss_Knife/sub_modules/xsdk/src/components/xutil/xtrace/include/xtrace.h)
*   **Usage Rule:** Real-time software tracing for time-sensitive events (ISR entries, context switching, USB packet handling) where execution determinism is critical.
*   **SNR Optimization:**
    *   **No Target-Side Formatting:** Core design rule: *"Keep the MCU-side protocol small. Make the host-side decoder smart."* The target only packs binary data (event ID, timestamp delta, arguments) into a ring buffer. No string parsing, formatting, or printing takes place on the CPU.
    *   **LEB128 Compression:** Event IDs and parameters are compressed using variable-length integer encoding. Core components (xRTOS, xFS, xUSB) are pre-allocated Event IDs `0x00 - 0x7F` to guarantee they only occupy a single byte on the wire.
    *   **Non-Blocking & Zero Allocation:** Record writes are strictly non-blocking. If the circular buffer overflows, new events are dropped, and a counter tracks the gap. Transport output (`xTRACE_Flush`) is deferred out of ISR contexts to avoid timing noise.
    *   **Compile-Time Gated:** If `xTRACE_ENABLE` is set to `0`, all tracing macros expand to no-ops.

```c
#include "xtrace.h"

// Usage: emits a high-frequency event with optional parameter (a)
xTRACE_E1(trace_ctx, xUSB_TRACE_EVENT_RESET, port_id);
```

---

## 23. Safety-Critical Coding Rules

To comply with safety standards (ISO 26262, IEC 61508, DO-178C) for deterministic execution and stack/heap safety:

### 23a. Prohibit Dynamic Memory Allocation After Init
*   **Rule:** Dynamic memory allocation (`malloc`, `free`, `realloc`, or custom dynamic heaps) is strictly prohibited after the initial boot/initialization phase of the system.
*   **Guideline:** All context structures, buffers, and queues must be statically declared or allocated once during boot initialization. All runtime resources must have static, deterministic lifecycles.

### 23b. Prohibit Recursion
*   **Rule:** Functions must not call themselves, either directly or indirectly (mutual recursion).
*   **Guideline:** The stack depth of all tasks and execution paths must be statically determinable at compile-time. Use iterative state-machines instead of recursive algorithms.

### 23c. Bounded Loops and Execution
*   **Rule:** Every loop (`for`, `while`, `do-while`) must have a statically or dynamically verifiable constant upper bound.
*   **Guideline:** Loops waiting on hardware state or register flags must utilize a timeout counter or retry limit. Infinite loops are prohibited except for the top-level main loop or designated task entry loops in the RTOS.

### 23d. Array Bounds Verification
*   **Rule:** Every array index access must be checked against the array boundaries before dereferencing, unless the index is mathematically guaranteed to be within bounds.
*   **Guideline:** Use assertions (`xASSERT`) to validate indices at boundary limits during debug builds, and return safety-safe fallbacks (or trigger diagnostic faults) in production.

---

## 24. Secure Coding Rules

To mitigate security vulnerabilities (CERT C, CWE, ISO/SAE 21434) and secure the device against exploits:

### 24a. Safe Buffer and String Operations
*   **Rule:** Unbounded string and memory copying functions (`strcpy`, `strcat`, `sprintf`, `vsprintf`, `gets`) are strictly prohibited.
*   **Guideline:** Always use bounded alternatives (`strncpy`, `strncat`, `snprintf`, `vsnprintf`) and explicitly force null-termination on the destination buffer.

### 24b. Secure Memory Scrubbing
*   **Rule:** Sensitive data (such as cryptographic keys, plaintext payloads, or initialization vectors) must be cleared from memory immediately after use.
*   **Guideline:** Standard `memset` calls can be optimized away by compiler dead-code elimination if the compiler determines the buffer is no longer read. Use secure scrubbing functions (such as custom volatile-write loops or memory barriers) to guarantee memory clearance.

### 24c. Integer Overflow and Underflow Prevention
*   **Rule:** Arithmetic operations on variables used for memory allocations, array indices, or loop counters must be checked for overflows, underflows, and wrap-arounds.
*   **Guideline:** Prior to performing additions, multiplications, or shifts on sizes or index boundaries, validate that the result will not exceed the storage type's range.

### 24d. Minimizing Attack Surface in Production
*   **Rule:** Test ports, debug commands, verbose debugging strings, and developer diagnostics must be compiled out or disabled in production builds.
*   **Guideline:** Use compiler flags (e.g. `xSDK_ENABLE_ASSERT=0` and `MODULE_LOG_LEVEL=0`) to strip diagnostic hooks and symbols before generating the production binary.

---

## 25. Character Set and Encoding Rules

To prevent compile-time warnings, diagnostic errors, and garbled text (mojibake) across different compilers, terminals, and localization settings:

### 25a. File Encoding
*   **Rule:** All source code files (`.c`, `.h`, `.cpp`, `.hpp`, `.py`), configurations (`.json`, `.yml`), and documentation (`.md`) must be encoded in **UTF-8 without Byte Order Mark (BOM)**.
*   **Guideline:** Strip any BOM (`\ufeff`) from files before committing. Most modern code editors default to UTF-8 without BOM.

### 25b. Repository Character Set
*   **Rule:** Repository text files are strictly limited to printable **7-bit ASCII**, plus the required tab and line-ending control characters. Non-ASCII characters are prohibited in code, comments, configuration, scripts, and documentation.
*   **Guideline:** Use ASCII words and operators such as `->`, `-`, `*`, `u`, `deg`, `+/-`, `>=`, `<=`, `!=`, and `Delta`. Use standard ASCII quotes and simple `-`, `|`, and `+` diagram borders.

### 25c. File and Identifier Names
*   **Rule:** File names, directory names, symbols, trace names, log tags, test names, and other repository-defined identifiers must use ASCII characters only.
*   **Guideline:** Transliterate a non-ASCII proper name into an ASCII form before using it in a repository-defined name or text file.

### 25d. Automated Verification
*   **Rule:** The repository CI pipeline enforces the absence of mojibake and illegal non-ASCII character sequences.
*   **Guideline:**
    *   To scan the repository and check for violations, run:
        ```bash
        python tools/xsdk_mojibake_check.py
        ```
    *   To automatically correct non-ASCII characters and strip BOMs, run:
        ```bash
        python tools/xsdk_fix_all_non_ascii.py --fix
        ```
