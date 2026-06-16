# xshell

Lightweight command shell and interactive line parser.

**CMake target:** `xsdk::xshell`

---

## Design Principles

- **No dynamic allocation.** The command registry (`xCMD_Context_t`), input buffers, and arguments are statically sized and owned by the caller.
- **Transport Abstraction.** Decoupled from physical I/O. Works over any byte-oriented stream interface (UART, USB CDC, virtual console) via the `xSHELL_Transport_t` interface.
- **Robust Parsing.** Parses arguments into standard `argc`/`argv` format and handles terminal line ending combinations (`\r`, `\n`, `\r\n`) and input overflow conditions gracefully.
- **Built-in Commands.** Includes optional compile-time built-in commands (like `help` and `echo`).

---

## Integration

```cmake
add_subdirectory(path/to/xSDK)
target_link_libraries(my_app PRIVATE xsdk::xshell)
```

---

## Usage - Console Initialization and Command Execution

```c
#include "xshell.h"
#include "xuart.h"

static xSHELL_Context_t g_shell_ctx;
static xCMD_Context_t   g_cmd_registry;

// Example transport adapter using UART driver
static xRETURN_t my_uart_read(void *ctx, uint8_t *buf, size_t len, size_t *read_len)
{
    // Wrap physical UART read
    xRETURN_t ret = xUART_Receive((xUART_Context_t *)ctx, buf, (uint32_t)len, 10U);
    if (ret == xRETURN_OK) {
        *read_len = len;
    } else {
        *read_len = 0;
    }
    return ret;
}

static xRETURN_t my_uart_write(void *ctx, const uint8_t *buf, size_t len, size_t *write_len)
{
    // Wrap physical UART write
    xRETURN_t ret = xUART_Transmit((xUART_Context_t *)ctx, buf, (uint32_t)len, 100U);
    if (ret == xRETURN_OK) {
        *write_len = len;
    } else {
        *write_len = 0;
    }
    return ret;
}

static xSHELL_Transport_t g_shell_transport = {
    .read  = my_uart_read,
    .write = my_uart_write,
    .flush = NULL,
};

// Example command callback
static xRETURN_t cmd_hello_callback(const xCMD_Request_t *req)
{
    xSHELL_Write_String((xSHELL_Context_t *)req->session_ctx, "Hello, World!\r\n");
    return xRETURN_OK;
}

void app_shell_init(xUART_Context_t *uart_device)
{
    // Step 1 - Initialize the command registry
    xCMD_Init(&g_cmd_registry);

    // Register a command
    static xCMD_Registration_t hello_cmd = {
        .command     = "hello",
        .description = "Print hello banner",
        .callback    = cmd_hello_callback,
    };
    xCMD_Register(&g_cmd_registry, &hello_cmd);

    // Step 2 - Configure the shell session
    xSHELL_Config_t cfg = {
        .cmd_ctx       = &g_cmd_registry,
        .transport     = &g_shell_transport,
        .transport_ctx = (void *)uart_device,
        .prompt        = "xsdk> ",
    };
    xSHELL_Init(&g_shell_ctx, &cfg);
    xSHELL_Register_Builtins(&g_shell_ctx);

    // Step 3 - Start the session
    xSHELL_Start(&g_shell_ctx);
}

void app_shell_poll(void)
{
    while (true) {
        // Polled within a main loop or thread task context
        xSHELL_Process(&g_shell_ctx);
        xRTOS_Task_Yield();
    }
}
```

---

## Key Public APIs

| Function | Description |
|---|---|
| `xSHELL_Init` | Initialize the shell session context and set the prompt |
| `xSHELL_Start` | Start the shell session, printing the initial prompt |
| `xSHELL_Process` | Poll the transport stream for new characters and execute matching commands on carriage return |
| `xSHELL_Stop` | Stop processing and disable interactive shell I/O |
| `xSHELL_Write` | Write raw data buffers directly to the active session transport |
| `xSHELL_Write_String` | Write a null-terminated string to the active session transport |
| `xSHELL_Register_Builtins` | Register standard console command helpers (`help`, `echo`) |

---

## License

Apache 2.0 - see [LICENSE](../../../LICENSE).
