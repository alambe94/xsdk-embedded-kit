# xnet

Lightweight, static-allocation IPv4/Ethernet networking stack.

**CMake target:** `xsdk::xnet`

---

## Design Principles

- **No dynamic allocation.** The packet buffer pool (`xNET_Packet_Buffer_t`) and interface records are statically allocated at compile-time/initialization.
- **Embedded-friendly.** Streamlined memory layout designed specifically for small microcontrollers.
- **Protocol support.** Core IPv4 protocols: Ethernet (ARP, ICMP/Ping, IPv4), transport (UDP), and application utilities (DHCP, DNS).
- **Interface Abstraction.** Direct coupling with custom hardware Ethernet MACs or Wi-Fi network interfaces via `xNET_Interface_Context_t`.

---

## Integration

```cmake
add_subdirectory(path/to/xSDK)
target_link_libraries(my_app PRIVATE xsdk::xnet)
```

---

## Usage - Stack Initialization and Network Interface Addition

```c
#include "xnet_core.h"
#include "xnet_interface.h"

static xNET_Context_t g_net;
static xNET_Interface_Context_t g_eth_interface;

// MAC driver frame transmission helper (provided by MAC driver)
static xRETURN_t my_mac_tx(void *driver_ctx, const uint8_t *frame, uint32_t length)
{
    // Send raw Ethernet frame
    return xRETURN_OK;
}

void net_stack_init(void)
{
    // Step 1 - Initialize xNET stack context
    xNET_Config_t cfg = {
        .packet_pool_buffer      = NULL, // Default compile-time pool size is used
        .packet_pool_buffer_size = 0,
    };
    xNET_Init(&g_net, &cfg);

    // Step 2 - Configure Ethernet interface
    xNET_Interface_Config_t intf_cfg = {
        .mac_addr    = {0x02, 0x00, 0x00, 0x11, 0x22, 0x33},
        .ip_addr     = {192, 168, 1, 100},
        .netmask     = {255, 255, 255, 0},
        .gateway     = {192, 168, 1, 1},
        .tx_frame    = my_mac_tx,
        .driver_ctx  = NULL,
    };
    xNET_Interface_Init(&g_eth_interface, &intf_cfg);

    // Step 3 - Add interface to the stack
    xNET_Interface_Add(&g_net, &g_eth_interface);
}

void net_stack_poll(void)
{
    // Feed the stack with system time ticks and parse incoming frames
    while (true) {
        g_net.system_ticks++;
        
        // Pull received packets from MAC, pass to xNET_Interface_Receive
        // ...
        
        xNET_Process(&g_net);
        
        xRTOS_Task_Yield();
    }
}
```

---

## Key Public APIs

| Function | Description |
|---|---|
| `xNET_Init` | Initialize networking stack context and packet pool |
| `xNET_Interface_Add` | Register a new network interface |
| `xNET_Process` | Run periodic protocol timeouts (ARP caches, DHCP lease, UDP timeouts) |
| `xNET_Interface_Receive` | Input a raw Ethernet frame received from the MAC driver into the stack |
| `xNET_UDP_Bind` | Bind a UDP context to a local port and register a receive callback |
| `xNET_UDP_Send` | Send a UDP packet from a bound context |
| `xNET_DHCP_Start` | Initiate DHCP IP address acquisition on an interface |
| `xNET_DNS_Resolve` | Resolve a domain name to an IP address |

---

## License

Apache 2.0 - see [LICENSE](../../../LICENSE).
