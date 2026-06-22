#include <stdbool.h>
#include <string.h>

#include "usb_dev.h"
#include "xusbd_core.h"
#include "xusbd_drv.h"
#include "xusbd_win.h"
#include "uart_console.h"

#define XSDK_USBSS_REG32(offset) (*(volatile uint32_t *)(0x40034000UL + (offset)))

#define USBSS_LST_STATE_MASK  0x00000F00U
#define USBSS_LST_STATE_SHIFT 8U
#ifndef XSDK_USB_VERBOSE_LOG
#define XSDK_USB_VERBOSE_LOG 0
#endif

// ── Loopback buffers ─────────────────────────────────────────────────────────
// SS bulk MPS = 1024.  The RX buffer must hold TWO descriptor slots so that
// usbss_arm_rx_chain can prime slot-0 at [0] and slot-1 at [MPS] with
// different addresses (matching the CH372Device reference double-buffer scheme).
// TX is always a single echo so one MPS is sufficient.
#define LOOPBACK_BUF_SIZE 1024U
static uint8_t loopback_rx_buf[LOOPBACK_BUF_SIZE * 2U] __attribute__((aligned(4)));
static uint8_t loopback_tx_buf[LOOPBACK_BUF_SIZE] __attribute__((aligned(4)));

static xUSBD_Device_Context_t xSDK_CH32H417_USB_Device;
static xUSBD_WIN_Context_t xSDK_CH32H417_USB_WIN;

// Set when the link reaches U0 and EP1 has been initialized; cleared on reset.
static volatile bool ep1_ready = false;
// Set once EP1 OUT is armed; re-cleared on reset so we re-arm after reconnect.
static volatile bool ep1_armed = false;

#if XSDK_USB_VERBOSE_LOG
static void xSDK_USB_Print_Reg(const char *name, uint32_t value)
{
    xSDK_Console_Write(" ");
    xSDK_Console_Write(name);
    xSDK_Console_Write("=0x");
    xSDK_Console_PrintHex32(value);
}

static const char *xSDK_USB_Link_State_Name(uint32_t lst)
{
    switch ((lst & USBSS_LST_STATE_MASK) >> USBSS_LST_STATE_SHIFT)
    {
    case 0x0: return "U0";
    case 0x1: return "U1";
    case 0x2: return "U2";
    case 0x3: return "U3";
    case 0x4: return "Disabled";
    case 0x5: return "RxDet";
    case 0x6: return "Inactive";
    case 0x7: return "Polling";
    case 0x8: return "Recovery";
    case 0x9: return "HotRst";
    case 0xA: return "Comply";
    case 0xB: return "Lpbk";
    default:  return "???";
    }
}
#endif

// ── WinUSB class callbacks ────────────────────────────────────────────────────

static xRETURN_t win_on_bus_event(xUSBD_Class_Context_t *ctx, USB_DCD_Event_t event)
{
    (void)ctx;
    if (event == USB_DCD_CONNECT_RECEIVED || event == USB_DCD_SPEED_CHANGE_RECEIVED)
    {
        // EP init happens in win_bus_event AFTER this user callback returns,
        // so defer the first arm to the main loop via ep1_ready.
        ep1_ready = true;
        ep1_armed = false;
    }
    else if (event == USB_DCD_DISCONNECT_RECEIVED || event == USB_DCD_RESET_RECEIVED)
    {
        ep1_ready = false;
        ep1_armed = false;
    }
    return xRETURN_OK;
}

static xRETURN_t win_on_data_received(xUSBD_Class_Context_t *ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    (void)ep_addr;

    // Copy and echo.  Re-arm is deferred to win_on_transmit_complete so the
    // next RX slot is only offered to the host after the echo TX is done.
    // Pass is_zlp=false: the host submits fixed-size reads and does not need
    // a ZLP to detect end-of-transfer.
    if (length > 0U && length <= LOOPBACK_BUF_SIZE)
    {
        (void)memcpy(loopback_tx_buf, data, length);
        (void)xUSBD_WIN_Transmit(ctx, loopback_tx_buf, length, false);
    }
    return xRETURN_OK;
}

static xRETURN_t win_on_transmit_complete(xUSBD_Class_Context_t *ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    (void)ep_addr;
    (void)data;
    (void)length;
    // Re-arm EP1 OUT now that the echo TX is done.
    (void)xUSBD_WIN_Prepare_To_Receive(ctx, loopback_rx_buf, LOOPBACK_BUF_SIZE);
    return xRETURN_OK;
}

static xUSBD_WIN_Callbacks_t s_win_callbacks = {
    .on_bus_event        = win_on_bus_event,
    .on_data_received    = win_on_data_received,
    .on_transmit_complete = win_on_transmit_complete,
};

// ── Service (called from main loop) ──────────────────────────────────────────

void xSDK_USB_Service(void)
{
    xUSBD_CH32H417_DCD_Poll();

    // Arm EP1 OUT once after connect (ep_init runs synchronously inside the
    // LINK IRQ before the main loop resumes, so the EP is ready by here).
    if (ep1_ready && !ep1_armed)
    {
        if (xUSBD_WIN_Prepare_To_Receive(&xSDK_CH32H417_USB_WIN.class_ctx,
                                         loopback_rx_buf, LOOPBACK_BUF_SIZE) == xRETURN_OK)
        {
            ep1_armed = true;
#if XSDK_USB_VERBOSE_LOG
            xSDK_Console_Write("EP1 OUT armed for loopback\r\n");
#endif
        }
    }
}

void xSDK_USB_Dump_Status(const char *tag)
{
#if XSDK_USB_VERBOSE_LOG
    static bool have_last;
    static uint32_t last_lcfg;
    static uint32_t last_lctrl;
    static uint32_t last_lst;
    static uint32_t last_uctrl;
    static uint32_t last_ust;
    static uint32_t last_ep0t;
    static uint32_t last_ep0r;

    uint32_t lcfg = XSDK_USBSS_REG32(0x00U);
    uint32_t lctrl = XSDK_USBSS_REG32(0x04U);
    uint32_t lif = XSDK_USBSS_REG32(0x0CU);
    uint32_t lst = XSDK_USBSS_REG32(0x10U);
    uint32_t uctrl = XSDK_USBSS_REG32(0x70U);
    uint32_t ust = XSDK_USBSS_REG32(0x74U);
    uint32_t ep0t = XSDK_USBSS_REG32(0x84U);
    uint32_t ep0r = XSDK_USBSS_REG32(0x88U);
    bool force = (tag != NULL) && (tag[0] != 'm');

    if (have_last &&
        !force &&
        lcfg == last_lcfg &&
        lctrl == last_lctrl &&
        ((lst & USBSS_LST_STATE_MASK) == (last_lst & USBSS_LST_STATE_MASK)) &&
        uctrl == last_uctrl &&
        ust == last_ust &&
        ep0t == last_ep0t &&
        ep0r == last_ep0r)
    {
        return;
    }

    have_last = true;
    last_lcfg = lcfg;
    last_lctrl = lctrl;
    last_lst = lst;
    last_uctrl = uctrl;
    last_ust = ust;
    last_ep0t = ep0t;
    last_ep0r = ep0r;

    xSDK_Console_Write("USBSS");
    if (tag != NULL)
    {
        xSDK_Console_Write("[");
        xSDK_Console_Write(tag);
        xSDK_Console_Write("]");
    }

    xSDK_Console_Write(" link=");
    xSDK_Console_Write(xSDK_USB_Link_State_Name(lst));

    xSDK_USB_Print_Reg("LCFG", lcfg);
    xSDK_USB_Print_Reg("LCTRL", lctrl);
    xSDK_USB_Print_Reg("LIF", lif);
    xSDK_USB_Print_Reg("LST", lst);
    xSDK_USB_Print_Reg("UCTRL", uctrl);
    xSDK_USB_Print_Reg("UST", ust);
    xSDK_USB_Print_Reg("EP0T", ep0t);
    xSDK_USB_Print_Reg("EP0R", ep0r);
    xSDK_Console_Write("\r\n");
#else
    (void)tag;
#endif
}

void xSDK_USB_Init(void)
{
    // Initialize USB device stack
    xUSBD_Init_Config_t usb_init_config = {
        .speed = USB_SPEED_SUPER,
        .vendor_string = (const uint8_t *)"alambe94",
        .product_string = (const uint8_t *)"xSDK CH32H417 USBSS WinUSB",
        // Bumped during USBSS bring-up so Windows creates a fresh PnP instance
        // after MS OS descriptor fixes instead of reusing a failed-install cache.
        .serial_number_string = (const uint8_t *)"123463",
        .vendor_id = 0x1209,
        .product_id = 0x0001,
    };

    if (xUSBD_Init(&xSDK_CH32H417_USB_Device, &usb_init_config) == xRETURN_OK)
    {
        if (xUSBD_Class_Register(&xSDK_CH32H417_USB_Device, &xSDK_CH32H417_USB_WIN.class_ctx, xUSBD_WIN_Class()) == xRETURN_OK)
        {
            (void)xUSBD_WIN_Set_Callbacks(&xSDK_CH32H417_USB_WIN.class_ctx, &s_win_callbacks);
            xUSBD_Start_Config_t usb_start_config = {
                .port = 0,
                .dcd_ops = &xUSBD_CH32H417_DCD_Ops,
                .dcd_ctx = &xUSBD_CH32H417_DCD_Context,
            };
            if (xUSBD_Start(&xSDK_CH32H417_USB_Device, &usb_start_config) == xRETURN_OK)
            {
                xSDK_Console_Write("USBSS started\r\n");
                xSDK_USB_Dump_Status("start");
            }
            else
            {
                xSDK_Console_Write("Failed to start USBSS DCD\r\n");
            }
        }
        else
        {
            xSDK_Console_Write("Failed to register WinUSB class\r\n");
        }
    }
    else
    {
        xSDK_Console_Write("Failed to initialize USB stack\r\n");
    }
}
