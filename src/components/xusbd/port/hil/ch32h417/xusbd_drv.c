// Copyright 2026 alambe94
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// @file xusbd_drv.c
// @brief CH32H417 USBSS (USB 3.0) Device Controller Driver (single instance)

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

// SYSTEM INCLUDES
#ifndef asm
#define asm __asm__
#endif
#include "ch32h417.h"

#include "xusb_defs.h"
#include "xusbd_return.h"
#include "xusbd_drv.h"
#include "xusbd_log.h"

// MACROS //////////////////////////////////////////////////////////////////////////

#define DEF_USBSSD_UEP0_SIZE      512U
#define DEF_ENDP_BURST_LEVEL      16U
#define USBSS_DISCONNECT_HOLD_US  50000U
#define USBSS_DELAY_LOOPS_PER_US  100U
#define USBSS_PHY_CFG_CR          (*((volatile uint32_t *)0x400341F8U))
#define USBSS_PHY_CFG_DAT         (*((volatile uint32_t *)0x400341FCU))
#define USBSS_PHY_CFG_MOD_REG     (*((volatile uint32_t *)0x5003C018U))

#define USBSS_LMP_HP              0U
#define USBSS_LMP_SUBTYPE_MASK    (0xFU << 5U)
#define USBSS_LMP_SET_LINK_FUNC   (0x1U << 5U)
#define USBSS_LMP_U2_INACT_TOUT   (0x2U << 5U)
#define USBSS_LMP_PORT_CAP        (0x4U << 5U)
#define USBSS_LMP_PORT_CFG        (0x5U << 5U)
#define USBSS_LMP_PORT_CFG_RES    (0x6U << 5U)
#define USBSS_LMP_LINK_SPEED      (1U << 9U)
#define USBSS_LMP_NUM_HP_BUF      (4U << 0U)
#define USBSS_LMP_UP_STREAM       (2U << 16U)

// WCH USBSS Controller register bit fields and masks (originally from ch32h417_usb.h)
#define USBSS_USB_CLR_ALL           0x00000002U
#define USBSS_FORCE_RST             0x00000004U
#define USBSS_DMA_EN                0x00000001U
#define USBSS_SETUP_FLOW            0x00000020U

#define USBSS_UIE_TRANSFER          0x00010000U
#define USBSS_UDIE_SETUP            0x00020000U
#define USBSS_UDIE_STATUS           0x00040000U

#define USBSS_UIF_TRANSFER          0x00000001U
#define USBSS_UDIF_SETUP            0x00000002U
#define USBSS_UDIF_STATUS           0x00000004U

#define USBSS_DEV_ADDR_MASK         0x7F000000U
#define USBSS_ITP_INTERVAL_MASK     0x00003FFFU

#define USBSS_EP0_TX_ERDY           0x00800000U
#define USBSS_EP0_TX_DPH            0x00200000U
#define USBSS_EP0_TX_STALL          0x00400000U
#define USBSS_EP0_TX_LEN_MASK       0x000007FFU

#define USBSS_EP0_RX_ERDY           0x00800000U
#define USBSS_EP0_RX_ACK            0x00200000U
#define USBSS_EP0_RX_STALL          0x00400000U
#define USBSS_EP0_RX_LEN_MASK       0x000007FFU

#define USBSS_EP_TX_CHAIN_AUTO      0x80U
#define USBSS_EP_TX_FIFO_MODE       0x40U
#define USBSS_EP_TX_ERDY_AUTO       0x04U
#define USBSS_EP_TX_SEQ_AUTO        0x02U
#define USBSS_EP_TX_HALT            0x80U
#define USBSS_EP_TX_CLR             0x40U
#define USBSS_EP_TX_CHAIN_CLR       0x20U
#define USBSS_EP_TX_CHAIN_IF        0x40U

#define USBSS_EP_RX_CHAIN_AUTO      0x80U
#define USBSS_EP_RX_FIFO_MODE       0x40U
#define USBSS_EP_RX_ERDY_AUTO       0x04U
#define USBSS_EP_RX_SEQ_AUTO        0x02U
#define USBSS_EP_RX_HALT            0x80U
#define USBSS_EP_RX_CLR             0x40U
#define USBSS_EP_RX_CHAIN_CLR       0x20U
#define USBSS_EP_RX_CHAIN_IF        0x40U

#define USBSS_EP_DIR_MASK           0x00001000U
#define USBSS_EP_ID_MASK            0x00000700U

#define U3_LINK_RESET               0x80000000U
#define LINK_FORCE_RXTERM           0x00800000U
#define LINK_TOUT_MODE              0x00200000U
#define LINK_U2_ALLOW               0x00020000U
#define LINK_U1_ALLOW               0x00010000U
#define LINK_LTSSM_MODE             0x00008000U
#define LINK_TX_DEEMPH_MASK         0x00000300U
#define LINK_RX_EQ_EN               0x00000040U
#define LINK_PHY_RESET              0x00000008U
#define LINK_SS_PLR_SWAP            0x00000004U
#define LINK_RX_TERM_EN             0x00000002U

#define USBSS_LINK_CFG_BOARD_FLAGS  0U

#ifndef XUSBD_CH32H417_DEBUG_COUNTERS
#define XUSBD_CH32H417_DEBUG_COUNTERS 0
#endif

#define LINK_HOT_RESET              0x00010000U
#define LINK_GO_RX_DET              0x00000080U
#define LINK_GO_DISABLED            0x00000010U
#define LINK_P2_MODE                0x00000002U

#define LINK_IE_STATE_CHG           0x80000000U
#define LINK_IE_RX_LMP_TOUT         0x00800000U
#define LINK_IE_TX_LMP              0x00400000U
#define LINK_IE_RX_LMP              0x00200000U
#define LINK_IE_RX_SET_FC           0x00020000U
#define LINK_IE_WARM_RST            0x00002000U
#define LINK_IE_TERM_PRES           0x00000400U

#define LINK_IF_STATE_CHG           0x80000000U
#define LINK_IF_RX_LMP_TOUT         0x00800000U
#define LINK_IF_TX_LMP              0x00400000U
#define LINK_IF_RX_LMP              0x00200000U
#define LINK_IF_RX_SET_FC           0x00020000U
#define LINK_IF_WARM_RST            0x00002000U
#define LINK_IF_TERM_PRES           0x00000400U

#define LINK_RX_WARM_RST            0x00000002U
#define LINK_LPM_EN                 0x00000200U
#define LINK_LMP_TX_CAP_VLD         0x40000000U
#define FORCE_PM                    0x20000000U

#define LINK_STATE_MASK             0x00000F00U
#define LINK_STATE_U0               0x00000000U
#define LINK_STATE_U1               0x00000100U
#define LINK_STATE_U2               0x00000200U
#define LINK_STATE_U3               0x00000300U
#define LINK_STATE_DISABLE          0x00000400U
#define LINK_STATE_RXDET            0x00000500U
#define LINK_STATE_INACTIVE         0x00000600U
#define LINK_STATE_POLLING          0x00000700U
#define LINK_STATE_RECOVERY         0x00000800U
#define LINK_STATE_HOTRST           0x00000900U
#define LINK_STATE_COMPLIANCE       0x00000A00U
#define LINK_STATE_LOOPBACK         0x00000B00U

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// Static EP0 buffer aligned for DMA access (shared for TX and RX control phases)
static uint8_t ep0_dma_buf[DEF_USBSSD_UEP0_SIZE] __attribute__((aligned(4)));

// Single DCD instance singleton
xUSBD_CH32H417_DCD_Context_t xUSBD_CH32H417_DCD_Context = {0};
#if XUSBD_CH32H417_DEBUG_COUNTERS
volatile uint32_t xUSBD_CH32H417_Debug_RX_Arm_Count;
volatile uint32_t xUSBD_CH32H417_Debug_OUT_Done_Count;
volatile uint32_t xUSBD_CH32H417_Debug_IN_Submit_Count;
volatile uint32_t xUSBD_CH32H417_Debug_IN_Done_Count;
#endif

// Tracks the active EP0 request direction until the USBSS status-stage IRQ fires.
static uint8_t setup_req_type;
static bool usbss_link_irq_enabled;

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static inline USBSS_EP_TX_TypeDef* get_ep_tx(uint8_t ep_num);
static inline USBSS_EP_RX_TypeDef* get_ep_rx(uint8_t ep_num);
static bool usbss_endpoint_valid(uint8_t ep_addr, bool allow_ep0);
static uint8_t usbss_chain_nump(uint32_t length, uint16_t mps);
static uint32_t usbss_rx_chain_length(const USBSS_EP_RX_TypeDef *ep_rx, uint32_t requested_length);
static void usbss_arm_rx_chain(USBSS_EP_RX_TypeDef *ep_rx, uint8_t *data, uint16_t mps, uint8_t max_nump);
static void usbss_start_tx_chain(USBSS_EP_TX_TypeDef *ep_tx, uint8_t *data, uint32_t length, uint16_t mps);
static void usbss_delay_us(uint32_t delay_us);
static void usbss_reset_endpoint_state(xUSBD_CH32H417_DCD_Context_t *ctx);
static void usbss_pll_init(bool enable);
static void usbss_rcc_init(bool enable);
static void usbss_phy_cfg(uint8_t addr, uint16_t data);
static void usbss_cfg_mod(void);
static void usbss_endpoint_reset(void);
static void usbss_endpoint_init(xUSBD_CH32H417_DCD_Context_t *ctx);
static void usbss_controller_reset_init(xUSBD_CH32H417_DCD_Context_t *ctx, bool configure_phy);
static void usbss_emit_event(xUSBD_CH32H417_DCD_Context_t *ctx, USB_DCD_Event_t event, uint8_t ep_addr, uint8_t *data, uint32_t length);
static USB_DCD_Link_State_t usbss_map_link_state(uint32_t link_state);
static void usbss_notify_link_state(xUSBD_CH32H417_DCD_Context_t *ctx, uint32_t link_state);
static void usbss_notify_connect_once(xUSBD_CH32H417_DCD_Context_t *ctx);
static void usbss_handle_link_lmp(xUSBD_CH32H417_DCD_Context_t *ctx, uint32_t link_int);
static void usbss_service_link_interrupts(xUSBD_CH32H417_DCD_Context_t *ctx);
static void usbss_service_device_interrupts(xUSBD_CH32H417_DCD_Context_t *ctx);
static void usbss_reset_ep0_control_state(void);

static xRETURN_t ch32h417_dcd_init(void *dcd_ctx, USB_Speed_t speed, void *device_ctx);
static xRETURN_t ch32h417_dcd_deinit(void *dcd_ctx);
static xRETURN_t ch32h417_dcd_set_event_callback(void *dcd_ctx, xUSBD_DCD_Event_Callback_t callback);
static xRETURN_t ch32h417_dcd_connect(void *dcd_ctx);
static xRETURN_t ch32h417_dcd_disconnect(void *dcd_ctx);
static xRETURN_t ch32h417_dcd_enable_interrupts(void *dcd_ctx);
static xRETURN_t ch32h417_dcd_disable_interrupts(void *dcd_ctx);
static xRETURN_t ch32h417_dcd_set_address(void *dcd_ctx, uint8_t address);
static xRETURN_t ch32h417_dcd_set_remote_wakeup(void *dcd_ctx, bool enable);
static xRETURN_t ch32h417_dcd_set_test_mode(void *dcd_ctx, uint8_t mode);
static uint32_t ch32h417_dcd_get_frame_number(void *dcd_ctx);
static USB_Speed_t ch32h417_dcd_get_speed(void *dcd_ctx);
static xRETURN_t ch32h417_dcd_ep_init(void *dcd_ctx, uint8_t ep_addr, uint8_t ep_type, uint16_t mps);
static xRETURN_t ch32h417_dcd_ep_deinit(void *dcd_ctx, uint8_t ep_addr);
static xRETURN_t ch32h417_dcd_ep_receive(void *dcd_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
static xRETURN_t ch32h417_dcd_ep_send(void *dcd_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length, bool is_zlp_required);
static xRETURN_t ch32h417_dcd_ep_stall(void *dcd_ctx, uint8_t ep_addr);
static xRETURN_t ch32h417_dcd_ep_clear_stall(void *dcd_ctx, uint8_t ep_addr);
static bool ch32h417_dcd_ep_is_stalled(void *dcd_ctx, uint8_t ep_addr);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static inline USBSS_EP_TX_TypeDef* get_ep_tx(uint8_t ep_num)
{
    return (USBSS_EP_TX_TypeDef*)((uintptr_t)&USBSSD->EP1_TX + (ep_num - 1) * 32);
}

static inline USBSS_EP_RX_TypeDef* get_ep_rx(uint8_t ep_num)
{
    return (USBSS_EP_RX_TypeDef*)((uintptr_t)&USBSSD->EP1_RX + (ep_num - 1) * 32);
}

static bool usbss_endpoint_valid(uint8_t ep_addr, bool allow_ep0)
{
    uint8_t ep_num = ep_addr & 0x7FU;
    uint8_t max_ep = (ep_addr & 0x80U) ? xUSBD_DEVICE_MAX_IN_ENDPOINT : xUSBD_DEVICE_MAX_OUT_ENDPOINT;
    return (ep_num <= max_ep) && (allow_ep0 || (ep_num != 0U));
}

static uint8_t usbss_chain_nump(uint32_t length, uint16_t mps)
{
    if (length == 0U)
    {
        return 1U;
    }
    return (uint8_t)((length + mps - 1U) / mps);
}

static uint32_t usbss_rx_chain_length(const USBSS_EP_RX_TypeDef *ep_rx, uint32_t requested_length)
{
    uint32_t nump = ep_rx->UEP_RX_CHAIN_NUMP;
    uint32_t length = ep_rx->UEP_RX_CHAIN_LEN;

    if (nump > 1U)
    {
        length += (nump - 1U) * ep_rx->UEP_RX_DMA_OFS;
    }
    return (length < requested_length) ? length : requested_length;
}

static void usbss_arm_rx_chain(USBSS_EP_RX_TypeDef *ep_rx, uint8_t *data, uint16_t mps, uint8_t max_nump)
{
    // Only slot-0 is armed.  The WCH chain has two hardware entries; by
    // writing DMA+MAX_NUMP once we prime entry-0 only.  After entry-0
    // completes the hardware sends NRDY (no entry-1 descriptor), which holds
    // off the host until the application calls ep_receive again.  This gives
    // the application single-outstanding-transfer ownership: it processes
    // the received buffer before the next packet can arrive.
    //
    // True pipelining (entry-1 active while entry-0 is being processed)
    // requires the caller to pre-submit a second ep_receive, which the DCD
    // would need to queue.  That queue API is not yet implemented; until it
    // is, single-slot is the correct and safe choice.
    (void)mps;  // kept in signature for future pipelined path
    ep_rx->UEP_RX_DMA = (uint32_t)data;
    ep_rx->UEP_RX_CHAIN_MAX_NUMP = max_nump;
}

static void usbss_start_tx_chain(USBSS_EP_TX_TypeDef *ep_tx, uint8_t *data, uint32_t length, uint16_t mps)
{
    uint8_t nump = usbss_chain_nump(length, mps);
    uint16_t final_packet_length = (uint16_t)(length % mps);

    if ((length > 0U) && (final_packet_length == 0U))
    {
        final_packet_length = mps;
    }

    ep_tx->UEP_TX_DMA = (uint32_t)((data != NULL) ? data : ep0_dma_buf);
    ep_tx->UEP_TX_CHAIN_LEN = final_packet_length;
    ep_tx->UEP_TX_CHAIN_EXP_NUMP = nump;
    ep_tx->UEP_TX_CR = nump;
}

static void usbss_delay_us(uint32_t delay_us)
{
    volatile uint32_t loops = delay_us * USBSS_DELAY_LOOPS_PER_US;
    while (loops-- > 0U)
    {
        __NOP();
    }
}

static void usbss_reset_endpoint_state(xUSBD_CH32H417_DCD_Context_t *ctx)
{
    memset(ctx->in_ep_handles, 0, sizeof(ctx->in_ep_handles));
    memset(ctx->out_ep_handles, 0, sizeof(ctx->out_ep_handles));
    setup_req_type = 0U;
}

static void usbss_pll_init(bool enable)
{
    if (enable)
    {
        // Enable HSE if not already enabled and wait for ready state
        if ((RCC->CTLR & RCC_HSERDY) == 0)
        {
            RCC->CTLR |= RCC_HSEON;
            while ((RCC->CTLR & RCC_HSERDY) == 0)
            {
            }
        }

        // Set reference clock for USBSS PLL to 25 MHz HSE
        RCC->PLLCFGR2 &= ~RCC_USBSSPLL_REFSEL;
        RCC->PLLCFGR2 |= RCC_USBSSPLL_REFSEL_25MHz;

        // Turn on the USBSS PLL
        RCC->CTLR |= RCC_USBSS_PLLON;

        // Wait until USBSS PLL locks
        while ((RCC->CTLR & RCC_USBSS_PLLRDY) == 0)
        {
        }
    }
    else
    {
        RCC->CTLR &= ~RCC_USBSS_PLLON;
    }
}

static void usbss_rcc_init(bool enable)
{
    if (enable)
    {
        // Initialize and lock USBSS PLL first
        usbss_pll_init(true);

        // Enable USBSS and SerDes clocks in RCC HBPCENR
        RCC->HBPCENR |= RCC_USBSSEN;
        RCC->HBPCENR |= RCC_SERDESEN;

        // Enable PIPE and UTMI clocks in RCC CFGR0
        RCC->CFGR0 |= RCC_PIPEON;
        RCC->CFGR0 |= RCC_UTMION;
    }
    else
    {
        // Disable peripheral clocks
        RCC->HBPCENR &= ~RCC_USBSSEN;
        RCC->CFGR0 &= ~RCC_UTMION;
        RCC->CFGR0 &= ~RCC_PIPEON;

        // If system PLL source is not USBSS, turn off USBSS PLL
        if ((RCC->PLLCFGR & RCC_SYSPLL_SEL) != RCC_SYSPLL_USBSS)
        {
            usbss_pll_init(false);
        }
    }
}

static void usbss_phy_cfg(uint8_t addr, uint16_t data)
{
    USBSS_PHY_CFG_CR = (1UL << 23U) | ((uint32_t)addr << 16U) | data;
    USBSS_PHY_CFG_DAT = 0x01U;
}

static void usbss_cfg_mod(void)
{
    usbss_phy_cfg(0x03U, 0x7C12U);
    usbss_phy_cfg(0x0DU, 0x79AAU);
    usbss_phy_cfg(0x15U, 0x4430U);
    usbss_phy_cfg(0x13U, 0x0010U);
    USBSS_PHY_CFG_MOD_REG = 0xB0054000U;
}

static void usbss_endpoint_reset(void)
{
    USBSSD->USB_CONTROL |= USBSS_USB_CLR_ALL;
    USBSSD->USB_CONTROL &= ~USBSS_USB_CLR_ALL;
}

static void usbss_endpoint_init(xUSBD_CH32H417_DCD_Context_t *ctx)
{
    usbss_endpoint_reset();
    usbss_reset_endpoint_state(ctx);

    USBSSD->UEP_TX_EN = 0U;
    USBSSD->UEP_RX_EN = 0U;
    USBSSD->UEP0_TX_CTRL = 0;
    USBSSD->UEP0_RX_CTRL = 0;
    USBSSD->UEP0_TX_DMA = (uint32_t)&ep0_dma_buf[0];
    USBSSD->UEP0_RX_DMA = (uint32_t)&ep0_dma_buf[0];
}

static void usbss_controller_reset_init(xUSBD_CH32H417_DCD_Context_t *ctx, bool configure_phy)
{
    USBSSD->USB_CONTROL |= USBSS_FORCE_RST;
    USBSSD->USB_STATUS = USBSS_UIF_TRANSFER;
    USBSSD->USB_CONTROL = USBSS_UIE_TRANSFER | USBSS_UDIE_SETUP | USBSS_UDIE_STATUS | USBSS_DMA_EN | USBSS_SETUP_FLOW;

    if (configure_phy)
    {
        usbss_cfg_mod();
    }

    usbss_endpoint_init(ctx);
}

static void usbss_emit_event(xUSBD_CH32H417_DCD_Context_t *ctx, USB_DCD_Event_t event, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    if ((ctx->event_callback != NULL) && (ctx->device_ctx != NULL))
    {
        ctx->event_callback(ctx->device_ctx, event, ep_addr, data, length);
    }
}

static void usbss_reset_ep0_control_state(void)
{
    USBSSD->UEP0_TX_CTRL = 0;
    USBSSD->UEP0_RX_CTRL = 0;
}

static USB_DCD_Link_State_t usbss_map_link_state(uint32_t link_state)
{
    switch (link_state & LINK_STATE_MASK)
    {
    case LINK_STATE_RXDET:
        return USB_DCD_LINK_STATE_RX_DETECT;
    case LINK_STATE_POLLING:
        return USB_DCD_LINK_STATE_POLLING;
    case LINK_STATE_U0:
        return USB_DCD_LINK_STATE_U0;
    case LINK_STATE_U1:
        return USB_DCD_LINK_STATE_U1;
    case LINK_STATE_U2:
        return USB_DCD_LINK_STATE_U2;
    case LINK_STATE_U3:
        return USB_DCD_LINK_STATE_U3;
    case LINK_STATE_RECOVERY:
        return USB_DCD_LINK_STATE_RECOVERY;
    case LINK_STATE_HOTRST:
        return USB_DCD_LINK_STATE_HOT_RESET;
    case LINK_STATE_COMPLIANCE:
        return USB_DCD_LINK_STATE_COMPLIANCE;
    case LINK_STATE_LOOPBACK:
        return USB_DCD_LINK_STATE_LOOPBACK;
    case LINK_STATE_DISABLE:
        return USB_DCD_LINK_STATE_DISABLED;
    default:
        return USB_DCD_LINK_STATE_UNKNOWN;
    }
}

static void usbss_notify_link_state(xUSBD_CH32H417_DCD_Context_t *ctx, uint32_t link_state)
{
    xUSBD_DCD_Link_State_Event_t event = {
        .link_state = usbss_map_link_state(link_state),
    };
    usbss_emit_event(ctx, USB_DCD_LINK_STATE_CHANGE_RECEIVED, 0, (uint8_t *)&event, sizeof(event));
}

static void usbss_notify_connect_once(xUSBD_CH32H417_DCD_Context_t *ctx)
{
    if ((ctx->event_callback == NULL) || ctx->is_connected)
    {
        return;
    }

    ctx->is_connected = true;
    ctx->speed = USB_SPEED_SUPER;
    usbss_emit_event(ctx, USB_DCD_CONNECT_RECEIVED, 0, NULL, 0);
}


static void usbss_handle_link_lmp(xUSBD_CH32H417_DCD_Context_t *ctx, uint32_t link_int)
{
    if (link_int & LINK_IF_RX_SET_FC)
    {
        USBSSD->LINK_INT_FLAG = LINK_IF_RX_SET_FC;
        if (USBSSD->LINK_LMP_PORT_CAP & FORCE_PM)
        {
            USBSSD->LINK_CFG |= LINK_U1_ALLOW | LINK_U2_ALLOW;
        }
        else
        {
            USBSSD->LINK_CFG &= ~(LINK_U1_ALLOW | LINK_U2_ALLOW);
        }
    }

    else if (link_int & LINK_IF_RX_LMP_TOUT)
    {
        USBSSD->LINK_INT_FLAG = LINK_IF_RX_LMP_TOUT;
        USBSSD->LINK_CTRL |= LINK_GO_DISABLED;
        USBSSD->LINK_CTRL |= LINK_GO_RX_DET;
    }

    else if (link_int & LINK_IF_TX_LMP)
    {
        USBSSD->LINK_INT_FLAG = LINK_IF_TX_LMP;
        USBSSD->LINK_LMP_TX_DATA0 = USBSS_LMP_LINK_SPEED | USBSS_LMP_PORT_CAP | USBSS_LMP_HP;
        USBSSD->LINK_LMP_TX_DATA1 = USBSS_LMP_UP_STREAM | USBSS_LMP_NUM_HP_BUF;
        USBSSD->LINK_LMP_TX_DATA2 = 0x0U;
    }

    else if (link_int & LINK_IF_RX_LMP)
    {
        USBSSD->LINK_INT_FLAG = LINK_IF_RX_LMP;
        uint32_t lmp_data0 = USBSSD->LINK_LMP_RX_DATA0;
        uint32_t subtype = lmp_data0 & USBSS_LMP_SUBTYPE_MASK;

        if (subtype == USBSS_LMP_PORT_CFG)
        {
            USBSSD->LINK_LMP_TX_DATA0 = USBSS_LMP_LINK_SPEED | USBSS_LMP_PORT_CFG_RES | USBSS_LMP_HP;
            USBSSD->LINK_LMP_TX_DATA1 = 0x0U;
            USBSSD->LINK_LMP_TX_DATA2 = 0x0U;
            USBSSD->LINK_LMP_PORT_CAP |= LINK_LMP_TX_CAP_VLD;
            ctx->speed = USB_SPEED_SUPER;
        }
        else if (subtype == USBSS_LMP_U2_INACT_TOUT)
        {
            USBSSD->LINK_U2_INACT_TIMER = (lmp_data0 >> 9U) & 0xFFU;
        }
        else if (subtype == USBSS_LMP_SET_LINK_FUNC)
        {
            if (lmp_data0 & (0x02U << 9U))
            {
                USBSSD->LINK_CFG |= LINK_U1_ALLOW | LINK_U2_ALLOW;
            }
            else
            {
                USBSSD->LINK_CFG &= ~(LINK_U1_ALLOW | LINK_U2_ALLOW);
            }
        }
    }
}

// ============================================================================
//  * Interrupt Handling
// ============================================================================

static void usbss_service_link_interrupts(xUSBD_CH32H417_DCD_Context_t *ctx)
{
    uint32_t link_int = USBSSD->LINK_INT_FLAG;
    uint32_t link_state = USBSSD->LINK_STATUS & LINK_STATE_MASK;

    if (link_int & LINK_IF_RX_SET_FC)
    {
        usbss_handle_link_lmp(ctx, LINK_IF_RX_SET_FC);
    }

    if (link_int & LINK_IF_STATE_CHG)
    {
        USBSSD->LINK_INT_FLAG = LINK_IF_STATE_CHG;
#if !defined(xSDK_CH32H417_RAM)
        usbss_notify_link_state(ctx, link_state);
#endif

        switch (link_state)
        {
        case LINK_STATE_U0:
            NVIC_EnableIRQ(USBSS_IRQn);
            usbss_notify_connect_once(ctx);
            break;
        case LINK_STATE_RXDET:
            USBSSD->LINK_CFG &= ~(LINK_U1_ALLOW | LINK_U2_ALLOW);
            break;
        case LINK_STATE_POLLING:
            break;
        case LINK_STATE_DISABLE:
            // Clear GO_DISABLED so the LTSSM can proceed to RxDetect/Polling.
            USBSSD->LINK_CTRL &= ~LINK_GO_DISABLED;
            break;
        case LINK_STATE_INACTIVE:
            // SS.Inactive on cable unplug: force the LTSSM cleanly to Disabled
            // so it can proceed to RxDetect on replug.  Clear is_connected so
            // usbss_notify_connect_once fires again on the next U0.
            //
            // Emit DISCONNECT_RECEIVED (not RESET_RECEIVED): DISCONNECT only
            // notifies class callbacks so they clear ep1_ready/ep1_armed, without
            // calling xUSBD_EP0_Configure.  RESET_RECEIVED would call EP0_Configure
            // here, setting ep0_rx.Is_Active=true; on replug CONNECT calls
            // EP0_Configure again, sees Is_Active=true, returns EP_BUSY, and
            // dcd_speed_transition bails out before calling the class callbacks —
            // win_on_bus_event(CONNECT) never fires and EP1 is never re-armed.
            ctx->is_connected = false;
            usbss_controller_reset_init(ctx, false);
            USBSSD->LINK_CTRL |= LINK_GO_DISABLED;
            usbss_emit_event(ctx, USB_DCD_DISCONNECT_RECEIVED, 0, NULL, 0);
            break;
        case LINK_STATE_HOTRST:
            USBSSD->LINK_CFG &= ~(LINK_U1_ALLOW | LINK_U2_ALLOW);
            ctx->is_connected = false;
            usbss_controller_reset_init(ctx, false);
            USBSSD->LINK_CTRL &= ~LINK_HOT_RESET;
            usbss_emit_event(ctx, USB_DCD_RESET_RECEIVED, 0, NULL, 0);
            break;
        case LINK_STATE_U1:
        case LINK_STATE_U2:
        case LINK_STATE_U3:
            usbss_phy_cfg(0x12U, 0x67C8U & ~(1U << 9U));
            break;
        case LINK_STATE_RECOVERY:
            usbss_phy_cfg(0x12U, 0x67C8U);
            break;
        default:
            break;
        }
        return;
    }

    link_int = USBSSD->LINK_INT_FLAG;

    if (link_int & LINK_IF_TERM_PRES)
    {
        USBSSD->LINK_INT_FLAG = LINK_IF_TERM_PRES;
        return;
    }

    if (link_int & LINK_IF_RX_LMP_TOUT)
    {
        usbss_handle_link_lmp(ctx, LINK_IF_RX_LMP_TOUT);
        return;
    }

    if (link_int & LINK_IF_TX_LMP)
    {
        usbss_handle_link_lmp(ctx, LINK_IF_TX_LMP);
        return;
    }

    if (link_int & LINK_IF_RX_LMP)
    {
        usbss_handle_link_lmp(ctx, LINK_IF_RX_LMP);
        return;
    }

    if (link_int & LINK_IF_WARM_RST)
    {
        USBSSD->LINK_INT_FLAG = LINK_IF_WARM_RST;
        if (USBSSD->LINK_STATUS & LINK_RX_WARM_RST)
        {
            if (!ctx->warm_reset_active)
            {
                ctx->warm_reset_active = true;
                ctx->is_connected = false;
                usbss_controller_reset_init(ctx, false);
                USBSSD->LINK_CTRL |= LINK_GO_DISABLED;
                __NOP();
                __NOP();
                __NOP();
                __NOP();
                USBSSD->LINK_CTRL &= ~LINK_GO_DISABLED;
                usbss_emit_event(ctx, USB_DCD_RESET_RECEIVED, 0, NULL, 0);
            }
        }
        else
        {
            ctx->warm_reset_active = false;
        }
        return;
    }
    else if ((USBSSD->LINK_STATUS & LINK_RX_WARM_RST) == 0U)
    {
        ctx->warm_reset_active = false;
    }
}

void xUSBD_CH32H417_DCD_Poll(void)
{
    if (!xUSBD_CH32H417_DCD_Context.is_hardware_initialized)
    {
        return;
    }

    usbss_service_device_interrupts(&xUSBD_CH32H417_DCD_Context);

#if defined(xSDK_CH32H417_RAM)
    if (usbss_link_irq_enabled)
    {
        return;
    }
#endif
    usbss_service_link_interrupts(&xUSBD_CH32H417_DCD_Context);
}

void USBSS_LINK_IRQHandler(void)
{
    usbss_service_link_interrupts(&xUSBD_CH32H417_DCD_Context);
}

void USBSS_IRQHandler(void)
{
    usbss_service_device_interrupts(&xUSBD_CH32H417_DCD_Context);
}

static void usbss_service_device_interrupts(xUSBD_CH32H417_DCD_Context_t *ctx)
{
    uint32_t status = USBSSD->USB_STATUS;

    // 1. SETUP packet received on EP0
    if ((status & USBSS_UDIF_SETUP) && !(status & USBSS_UDIF_STATUS))
    {
        // Parse Setup packet values directly from EP0 buffer
        setup_req_type = ep0_dma_buf[0];
        // Clear Setup interrupt flag
        USBSSD->USB_STATUS = USBSS_UDIF_SETUP;

        // Dispatch setup packet to core callback
        usbss_emit_event(ctx, USB_DCD_SETUP_RECEIVED, 0, ep0_dma_buf, 8);
    }
    // 2. Status Stage Complete interrupt
    else if (status & USBSS_UDIF_STATUS)
    {
        // Clear flag
        USBSSD->USB_STATUS = USBSS_UDIF_STATUS;

        if (setup_req_type & USB_REQ_TYPE_IN)
        {
            usbss_emit_event(ctx, USB_DCD_DATA_RECEIVED, 0x00, ep0_dma_buf, 0);
        }
        else
        {
            usbss_emit_event(ctx, USB_DCD_DATA_SENT, 0x80, NULL, 0);
        }

        // Reset control states
        usbss_reset_ep0_control_state();
    }
    // 3. Endpoint Transfer Done interrupt
    else if (status & USBSS_UIF_TRANSFER)
    {
        uint8_t ep_num = (status & USBSS_EP_ID_MASK) >> 8;
        uint32_t ep_dir = status & USBSS_EP_DIR_MASK; // 1: IN, 0: OUT

        // Clear Transfer interrupt flag
        USBSSD->USB_STATUS = USBSS_UIF_TRANSFER;

        if (ep_dir) // IN transfer complete (Transmit done)
        {
            if (ep_num == 0)
            {
                // Advance the EP0 TX sequence counter and clear ERDY.
                USBSSD->UEP0_TX_CTRL = USBSS_EP0_TX_DPH | ((((USBSSD->UEP0_TX_CTRL >> 16) & 0x1F) + 1) << 16);
                // Arm EP0 RX so the status OUT ZLP (or next SETUP) is accepted and
                // UDIF_STATUS fires to complete the control transaction.  Matches the
                // WCH reference TRANSFER-IN ep0 handler that always sets ERDY|ACK here.
                USBSSD->UEP0_RX_CTRL = USBSS_EP0_RX_ERDY | USBSS_EP0_RX_ACK;

                usbss_emit_event(ctx, USB_DCD_DATA_SENT, 0x80, NULL, 0);
            }
            else if (ep_num <= xUSBD_DEVICE_MAX_IN_ENDPOINT)
            {
                USBSS_EP_TX_TypeDef* ep_tx = get_ep_tx(ep_num);
                // Clear endpoint transmit interface flag
                ep_tx->UEP_TX_CHAIN_ST |= USBSS_EP_TX_CHAIN_IF;
#if XUSBD_CH32H417_DEBUG_COUNTERS
                xUSBD_CH32H417_Debug_IN_Done_Count++;
#endif

                xUSBD_CH32H417_EP_Handle_t *ep_handle = &ctx->in_ep_handles[ep_num];
                if (ep_handle->Is_Active)
                {
                    if (ep_handle->Send_ZLP)
                    {
                        ep_handle->Send_ZLP = false;
                        usbss_start_tx_chain(ep_tx, ep_handle->Data, 0U, ep_handle->MPS);
                    }
                    else
                    {
                        ep_handle->Is_Active = false;
                        usbss_emit_event(ctx, USB_DCD_DATA_SENT, ep_num | 0x80U, ep_handle->Data, ep_handle->Transfer_Length);
                    }
                }
            }
        }
        else // OUT transfer complete (Receive done)
        {
            if (ep_num == 0)
            {
                uint32_t rx_len = USBSSD->UEP0_RX_CTRL & USBSS_EP0_RX_LEN_MASK;

                // Re-enable EP0 OUT for setup/data stage
                USBSSD->UEP0_RX_CTRL = USBSS_EP0_RX_ERDY | USBSS_EP0_RX_ACK;

                usbss_emit_event(ctx, USB_DCD_DATA_RECEIVED, 0x00, ep0_dma_buf, rx_len);
            }
            else if (ep_num <= xUSBD_DEVICE_MAX_OUT_ENDPOINT)
            {
                USBSS_EP_RX_TypeDef* ep_rx = get_ep_rx(ep_num);
#if XUSBD_CH32H417_DEBUG_COUNTERS
                xUSBD_CH32H417_Debug_OUT_Done_Count++;
#endif
                // Read DMA registers BEFORE clearing CHAIN_IF.  Clearing CHAIN_IF
                // advances the hardware chain to the next slot; registers read after
                // that point reflect the NEW (empty) slot, not the completed one.
                // The reference (CH372Device USBSS_IRQHandler) also reads first,
                // then clears CHAIN_ST.
                xUSBD_CH32H417_EP_Handle_t *ep_handle = &ctx->out_ep_handles[ep_num];

                // Capture completed-slot registers BEFORE clearing CHAIN_IF.
                // Clearing CHAIN_IF advances the chain ring to the next slot, after
                // which DMA registers reflect the *next* slot (no data yet).
                // Reference CH372Device reads EP1_RX registers first, then clears.
                uint32_t rx_len      = usbss_rx_chain_length(ep_rx, ep_handle->Transfer_Length);
                uint8_t *actual_data = (uint8_t *)(uintptr_t)(
                    ep_rx->UEP_RX_DMA -
                    ((uint32_t)ep_rx->UEP_RX_CHAIN_NUMP * (uint32_t)ep_rx->UEP_RX_DMA_OFS));

                // Advance the hardware chain; next slot can now receive.
                ep_rx->UEP_RX_CHAIN_ST |= USBSS_EP_RX_CHAIN_IF;

                if (ep_handle->Is_Active)
                {
                    ep_handle->Is_Active = false;
                    usbss_emit_event(ctx, USB_DCD_DATA_RECEIVED, ep_num, actual_data, rx_len);
                }
            }
        }
    }
}

// ============================================================================
//  * DCD Ops Implementations
// ============================================================================

static xRETURN_t ch32h417_dcd_init(void *dcd_ctx, USB_Speed_t speed, void *device_ctx)
{
    xUSBD_CH32H417_DCD_Context_t *ctx = (xUSBD_CH32H417_DCD_Context_t *)dcd_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    xUSBD_LOG(0x1001, "USB_DCD_Init: speed=%d", speed);

    if (ctx->is_hardware_initialized)
    {
        return xRETURN_OK;
    }

    ctx->device_ctx = device_ctx;
    ctx->speed = speed;
    ctx->is_connected = false;
    ctx->warm_reset_active = false;
    ctx->event_callback = NULL;
    usbss_reset_endpoint_state(ctx);

    // Disable USBSS IRQs before touching hardware.  After a GDB RAM reload the
    // NVIC retains the previous session's enable state; if USBSS_LINK_IRQn fires
    // during the 50 ms PHY-reset hold it would clear LINK_GO_DISABLED prematurely
    // and corrupt the init sequence while the PHY is still in reset.
    // dcd_enable_interrupts() re-enables them after init completes.
    NVIC_DisableIRQ(USBSS_LINK_IRQn);
    NVIC_DisableIRQ(USBSS_IRQn);

    // Enable clocks and PLL configuration
    usbss_rcc_init(true);

    // PHY and Link setup sequence.
    // Assert PHY_RESET + GO_DISABLED so the host sees the SS Rx.Detect
    // terminations drop. Hold for 50 ms so Windows registers the disconnect and
    // clears the previous device state before the link retrains. A 1 ms hold was
    // too short and left Windows showing the previous session's error state.
    USBSSD->LINK_CFG = LINK_RX_EQ_EN | LINK_TX_DEEMPH_MASK | USBSS_LINK_CFG_BOARD_FLAGS | LINK_PHY_RESET;
    USBSSD->LINK_CTRL = LINK_P2_MODE | LINK_GO_DISABLED;
    usbss_delay_us(USBSS_DISCONNECT_HOLD_US);
    USBSSD->LINK_CFG = LINK_RX_EQ_EN | LINK_TX_DEEMPH_MASK | USBSS_LINK_CFG_BOARD_FLAGS | LINK_LTSSM_MODE | LINK_TOUT_MODE;
    USBSSD->LINK_LPM_CR |= LINK_LPM_EN;

    // Enable terminations
    USBSSD->LINK_CFG |= LINK_RX_TERM_EN;

    // RAM-debug reloads do not reset USBSS hardware. Drop any stale link flags
    // before enabling interrupts and starting LTSSM training.
    USBSSD->LINK_INT_FLAG = 0xFFFFFFFFU;

    // Enable Link interrupts
    USBSSD->LINK_INT_CTRL = LINK_IE_TX_LMP | LINK_IE_RX_LMP | LINK_IE_RX_LMP_TOUT |
                            LINK_IE_STATE_CHG | LINK_IE_WARM_RST | LINK_IE_TERM_PRES | LINK_IE_RX_SET_FC;

    USBSSD->LINK_CTRL = LINK_P2_MODE;
    USBSSD->LINK_U1_WKUP_TMR = 120;
    USBSSD->LINK_U1_WKUP_FILTER = 50;
    USBSSD->LINK_U2_WKUP_FILTER = 0;
    USBSSD->LINK_U3_WKUP_FILTER = 0;

    usbss_controller_reset_init(ctx, true);

    ctx->is_hardware_initialized = true;
    xUSBD_LOG(0x10E0, "USB_DCD: CH32H417 initialization complete\r\n");

    return xRETURN_OK;
}

static xRETURN_t ch32h417_dcd_deinit(void *dcd_ctx)
{
    xUSBD_CH32H417_DCD_Context_t *ctx = (xUSBD_CH32H417_DCD_Context_t *)dcd_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    xUSBD_LOG(0x1002, "USB_DCD_Deinit");

    // Disable interrupts
    NVIC_DisableIRQ(USBSS_LINK_IRQn);
    NVIC_DisableIRQ(USBSS_IRQn);

    // Reset link and controller
    USBSSD->USB_CONTROL = USBSS_FORCE_RST;
    USBSSD->LINK_CFG |= LINK_PHY_RESET | U3_LINK_RESET;
    usbss_delay_us(100U);
    USBSSD->USB_CONTROL &= ~USBSS_FORCE_RST;
    USBSSD->LINK_CFG &= ~(LINK_PHY_RESET | U3_LINK_RESET);

    // Disable clocks
    usbss_rcc_init(false);

    usbss_reset_endpoint_state(ctx);
    ctx->is_hardware_initialized = false;
    ctx->is_connected = false;
    ctx->warm_reset_active = false;
    ctx->event_callback = NULL;
    ctx->device_ctx = NULL;
    usbss_link_irq_enabled = false;
    return xRETURN_OK;
}

static xRETURN_t ch32h417_dcd_set_event_callback(void *dcd_ctx, xUSBD_DCD_Event_Callback_t callback)
{
    xUSBD_CH32H417_DCD_Context_t *ctx = (xUSBD_CH32H417_DCD_Context_t *)dcd_ctx;
    if (ctx == NULL || callback == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    ctx->event_callback = callback;
    return xRETURN_OK;
}

static xRETURN_t ch32h417_dcd_connect(void *dcd_ctx)
{
    xUSBD_CH32H417_DCD_Context_t *ctx = (xUSBD_CH32H417_DCD_Context_t *)dcd_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    xUSBD_LOG(0x1002, "USB_DCD_Connect");

    // Connect pull-up/terminations to begin link training
    USBSSD->LINK_CFG |= LINK_RX_TERM_EN;
    return xRETURN_OK;
}

static xRETURN_t ch32h417_dcd_disconnect(void *dcd_ctx)
{
    xUSBD_CH32H417_DCD_Context_t *ctx = (xUSBD_CH32H417_DCD_Context_t *)dcd_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    xUSBD_LOG(0x1003, "USB_DCD_Disconnect");

    // Disconnect pull-up/terminations
    USBSSD->LINK_CFG &= ~LINK_RX_TERM_EN;
    return xRETURN_OK;
}

static xRETURN_t ch32h417_dcd_enable_interrupts(void *dcd_ctx)
{
    xUSBD_CH32H417_DCD_Context_t *ctx = (xUSBD_CH32H417_DCD_Context_t *)dcd_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

#if defined(xSDK_CH32H417_RAM)
    // RAM bring-up still uses the link IRQ because SS training/LMP handling is
    // timing-sensitive. Keep the handler UART-free so debugger halts stay sane.
    NVIC_EnableIRQ(USBSS_LINK_IRQn);
    NVIC_DisableIRQ(USBSS_IRQn);
    usbss_link_irq_enabled = true;
#else
    // Link IRQ handles LTSSM/LMP timing. USBSS IRQ handles EP0 setup/status and
    // transfer completion once the link reaches U0.
    NVIC_EnableIRQ(USBSS_LINK_IRQn);
    NVIC_EnableIRQ(USBSS_IRQn);
    usbss_link_irq_enabled = true;
#endif
    return xRETURN_OK;
}

static xRETURN_t ch32h417_dcd_disable_interrupts(void *dcd_ctx)
{
    xUSBD_CH32H417_DCD_Context_t *ctx = (xUSBD_CH32H417_DCD_Context_t *)dcd_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    NVIC_DisableIRQ(USBSS_LINK_IRQn);
    NVIC_DisableIRQ(USBSS_IRQn);
    usbss_link_irq_enabled = false;
    return xRETURN_OK;
}

static xRETURN_t ch32h417_dcd_set_address(void *dcd_ctx, uint8_t address)
{
    xUSBD_CH32H417_DCD_Context_t *ctx = (xUSBD_CH32H417_DCD_Context_t *)dcd_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    // Set device address in USB_CONTROL register
    USBSSD->USB_CONTROL = (USBSSD->USB_CONTROL & ~USBSS_DEV_ADDR_MASK) | ((uint32_t)address << 24);
    return xRETURN_OK;
}

static xRETURN_t ch32h417_dcd_set_remote_wakeup(void *dcd_ctx, bool enable)
{
    (void)dcd_ctx;
    (void)enable;
    return xRETURN_OK;
}

static xRETURN_t ch32h417_dcd_set_test_mode(void *dcd_ctx, uint8_t mode)
{
    (void)dcd_ctx;
    (void)mode;
    return xRETURN_OK;
}

static uint32_t ch32h417_dcd_get_frame_number(void *dcd_ctx)
{
    (void)dcd_ctx;
    return USBSSD->USB_ITP & USBSS_ITP_INTERVAL_MASK;
}

static USB_Speed_t ch32h417_dcd_get_speed(void *dcd_ctx)
{
    (void)dcd_ctx;
    return USB_SPEED_SUPER;
}

static xRETURN_t ch32h417_dcd_ep_init(void *dcd_ctx, uint8_t ep_addr, uint8_t ep_type, uint16_t mps)
{
    xUSBD_CH32H417_DCD_Context_t *ctx = (xUSBD_CH32H417_DCD_Context_t *)dcd_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    uint8_t ep_num = ep_addr & 0x7FU;

    // EP0 is managed by the hardware and usbss_endpoint_init(); no additional
    // configuration is needed here.
    if (ep_num == 0U)
    {
        return xRETURN_OK;
    }

    if (!usbss_endpoint_valid(ep_addr, false) || (mps == 0U))
    {
        return xRETURN_xERR_xUSBD_DCD_INVALID_ENDPOINT;
    }

    if (ep_addr & 0x80U) // IN endpoint
    {
        xUSBD_CH32H417_EP_Handle_t *ep_handle = &ctx->in_ep_handles[ep_num];
        memset(ep_handle, 0, sizeof(*ep_handle));
        ep_handle->EP_Type = ep_type;
        ep_handle->MPS = mps;

        // Enable TX endpoint
        USBSSD->UEP_TX_EN |= (1UL << ep_num);

        USBSS_EP_TX_TypeDef* ep_tx = get_ep_tx(ep_num);
        // ERDY_AUTO required for TX: without it, writing TX DMA + CHAIN_EXP_NUMP
        // does not send ERDY to the host, so the host never fetches IN data and
        // ReadPipe times out.  With single-slot RX and deferred re-arm
        // (re-arm in transmit_complete) the TX-ERDY_AUTO race is eliminated:
        // only one TX descriptor is ever outstanding at a time.
        ep_tx->UEP_TX_CFG = USBSS_EP_TX_CHAIN_AUTO | USBSS_EP_TX_ERDY_AUTO | USBSS_EP_TX_SEQ_AUTO | USBSS_EP_TX_FIFO_MODE;
        ep_tx->UEP_TX_CR = USBSS_EP_TX_CLR | USBSS_EP_TX_CHAIN_CLR;
    }
    else // OUT endpoint
    {
        xUSBD_CH32H417_EP_Handle_t *ep_handle = &ctx->out_ep_handles[ep_num];
        memset(ep_handle, 0, sizeof(*ep_handle));
        ep_handle->EP_Type = ep_type;
        ep_handle->MPS = mps;

        // Enable RX endpoint
        USBSSD->UEP_RX_EN |= (1UL << ep_num);

        USBSS_EP_RX_TypeDef* ep_rx = get_ep_rx(ep_num);
        // ERDY_AUTO re-added for RX: without it, writing UEP_RX_DMA + MAX_NUMP
        // does not send ERDY to the host (no ERDY = host write times out).
        // The TX-busy race that prompted removing ERDY_AUTO was a TX-only
        // problem; for RX with single-slot priming ERDY_AUTO is safe.
        ep_rx->UEP_RX_CFG = USBSS_EP_RX_CHAIN_AUTO | USBSS_EP_RX_ERDY_AUTO | USBSS_EP_RX_SEQ_AUTO | USBSS_EP_RX_FIFO_MODE;
        ep_rx->UEP_RX_CR = USBSS_EP_RX_CLR | USBSS_EP_RX_CHAIN_CLR;
    }

    return xRETURN_OK;
}

static xRETURN_t ch32h417_dcd_ep_deinit(void *dcd_ctx, uint8_t ep_addr)
{
    xUSBD_CH32H417_DCD_Context_t *ctx = (xUSBD_CH32H417_DCD_Context_t *)dcd_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    uint8_t ep_num = ep_addr & 0x7FU;
    if (ep_num == 0U)
    {
        return xRETURN_OK;  // EP0 is managed by hardware; nothing to deinit here.
    }

    if (!usbss_endpoint_valid(ep_addr, false))
    {
        return xRETURN_xERR_xUSBD_DCD_INVALID_ENDPOINT;
    }

    if (ep_addr & 0x80U)
    {
        USBSSD->UEP_TX_EN &= ~(1UL << ep_num);
        USBSS_EP_TX_TypeDef* ep_tx = get_ep_tx(ep_num);
        ep_tx->UEP_TX_CR = USBSS_EP_TX_CLR | USBSS_EP_TX_CHAIN_CLR;
        memset(&ctx->in_ep_handles[ep_num], 0, sizeof(ctx->in_ep_handles[ep_num]));
    }
    else
    {
        USBSSD->UEP_RX_EN &= ~(1UL << ep_num);
        USBSS_EP_RX_TypeDef* ep_rx = get_ep_rx(ep_num);
        ep_rx->UEP_RX_CR = USBSS_EP_RX_CLR | USBSS_EP_RX_CHAIN_CLR;
        memset(&ctx->out_ep_handles[ep_num], 0, sizeof(ctx->out_ep_handles[ep_num]));
    }

    return xRETURN_OK;
}

static xRETURN_t ch32h417_dcd_ep_receive(void *dcd_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    xUSBD_CH32H417_DCD_Context_t *ctx = (xUSBD_CH32H417_DCD_Context_t *)dcd_ctx;
    if (ctx == NULL || data == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    if ((ep_addr & 0x80U) || !usbss_endpoint_valid(ep_addr, true))
    {
        return xRETURN_xERR_xUSBD_DCD_INVALID_ENDPOINT;
    }
    uint8_t ep_num = ep_addr & 0x7FU;

    if (ep_num == 0)
    {
        // For EP0, receive expects setup or control OUT data to go to ep0_dma_buf
        USBSSD->UEP0_RX_DMA = (uint32_t)&ep0_dma_buf[0];
        USBSSD->UEP0_RX_CTRL = USBSS_EP0_RX_ERDY | USBSS_EP0_RX_ACK;
        return xRETURN_OK;
    }

    xUSBD_CH32H417_EP_Handle_t *ep_handle = &ctx->out_ep_handles[ep_num];
    if ((ep_handle->MPS == 0U) || (length == 0U) || (length > ((uint32_t)ep_handle->MPS * DEF_ENDP_BURST_LEVEL)))
    {
        return xRETURN_xERR_xUSBD_INSUFFICIENT_BUFFER_SIZE;
    }
    if (ep_handle->Is_Active)
    {
        return xRETURN_xWRN_xUSBD_DCD_EP_BUSY;
    }

    ep_handle->Data = data;
    ep_handle->Transfer_Length = length;
    ep_handle->Is_Active = true;
#if XUSBD_CH32H417_DEBUG_COUNTERS
    xUSBD_CH32H417_Debug_RX_Arm_Count++;
#endif

    // Always prime a single slot.  Priming two slots causes a TX-busy race in
    // a single-buffer loopback: slot-1 can fire while slot-0's echo TX is still
    // in flight, making WIN_Transmit return EP_BUSY and silently dropping the
    // echo.  With single-slot + deferred re-arm (re-arm in transmit_complete)
    // the pipeline is: receive -> echo -> re-arm -> receive, with no overlap.
    usbss_arm_rx_chain(get_ep_rx(ep_num), data, ep_handle->MPS, usbss_chain_nump(length, ep_handle->MPS));
    ep_handle->Chain_Primed = true;

    return xRETURN_OK;
}

static xRETURN_t ch32h417_dcd_ep_send(void *dcd_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length, bool is_zlp_required)
{
    xUSBD_CH32H417_DCD_Context_t *ctx = (xUSBD_CH32H417_DCD_Context_t *)dcd_ctx;
    if ((ctx == NULL) || ((data == NULL) && (length > 0U)))
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    if (((ep_addr & 0x80U) == 0U) || !usbss_endpoint_valid(ep_addr, true))
    {
        return xRETURN_xERR_xUSBD_DCD_INVALID_ENDPOINT;
    }
    uint8_t ep_num = ep_addr & 0x7FU;

    if (ep_num == 0)
    {
        if (length == 0)
        {
            // OUT-type status stage: device sends IN ZLP, host ACKs.
            // WCH hardware requires UEP0_RX_CTRL = ERDY|ACK to be armed alongside
            // the TX so that UDIF_STATUS fires when the host acknowledges the ZLP.
            USBSSD->UEP0_TX_CTRL = USBSS_EP0_TX_DPH;
            USBSSD->UEP0_TX_CTRL |= USBSS_EP0_TX_ERDY;
            USBSSD->UEP0_RX_CTRL = USBSS_EP0_RX_ERDY | USBSS_EP0_RX_ACK;
        }
        else
        {
            uint32_t chunk = (length > DEF_USBSSD_UEP0_SIZE) ? DEF_USBSSD_UEP0_SIZE : length;
            memcpy(&ep0_dma_buf[0], data, chunk);

            USBSSD->UEP0_TX_DMA = (uint32_t)&ep0_dma_buf[0];
            USBSSD->UEP0_TX_CTRL = USBSS_EP0_TX_DPH | chunk;
            USBSSD->UEP0_TX_CTRL |= USBSS_EP0_TX_ERDY;
        }
        return xRETURN_OK;
    }

    xUSBD_CH32H417_EP_Handle_t *ep_handle = &ctx->in_ep_handles[ep_num];
    if ((ep_handle->MPS == 0U) || (length > ((uint32_t)ep_handle->MPS * DEF_ENDP_BURST_LEVEL)))
    {
        return xRETURN_xERR_xUSBD_INSUFFICIENT_BUFFER_SIZE;
    }
    if (ep_handle->Is_Active)
    {
        return xRETURN_xWRN_xUSBD_DCD_EP_BUSY;
    }

    ep_handle->Data = data;
    ep_handle->Transfer_Length = length;
    ep_handle->Is_Active = true;
    ep_handle->Send_ZLP = is_zlp_required && (length > 0U);
#if XUSBD_CH32H417_DEBUG_COUNTERS
    xUSBD_CH32H417_Debug_IN_Submit_Count++;
#endif

    usbss_start_tx_chain(get_ep_tx(ep_num), data, length, ep_handle->MPS);

    return xRETURN_OK;
}

static xRETURN_t ch32h417_dcd_ep_stall(void *dcd_ctx, uint8_t ep_addr)
{
    xUSBD_CH32H417_DCD_Context_t *ctx = (xUSBD_CH32H417_DCD_Context_t *)dcd_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    if (!usbss_endpoint_valid(ep_addr, true))
    {
        return xRETURN_xERR_xUSBD_DCD_INVALID_ENDPOINT;
    }
    uint8_t ep_num = ep_addr & 0x7FU;
    if (ep_num == 0)
    {
        USBSSD->UEP0_TX_CTRL |= USBSS_EP0_TX_STALL;
        USBSSD->UEP0_RX_CTRL |= USBSS_EP0_RX_ERDY | USBSS_EP0_RX_STALL;
        return xRETURN_OK;
    }

    if (ep_addr & 0x80U)
    {
        USBSS_EP_TX_TypeDef* ep_tx = get_ep_tx(ep_num);
        ep_tx->UEP_TX_CR |= USBSS_EP_TX_HALT;
    }
    else
    {
        USBSS_EP_RX_TypeDef* ep_rx = get_ep_rx(ep_num);
        ep_rx->UEP_RX_CR |= USBSS_EP_RX_HALT;
    }

    return xRETURN_OK;
}

static xRETURN_t ch32h417_dcd_ep_clear_stall(void *dcd_ctx, uint8_t ep_addr)
{
    xUSBD_CH32H417_DCD_Context_t *ctx = (xUSBD_CH32H417_DCD_Context_t *)dcd_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    if (!usbss_endpoint_valid(ep_addr, true))
    {
        return xRETURN_xERR_xUSBD_DCD_INVALID_ENDPOINT;
    }
    uint8_t ep_num = ep_addr & 0x7FU;
    if (ep_num == 0)
    {
        USBSSD->UEP0_TX_CTRL &= ~USBSS_EP0_TX_STALL;
        USBSSD->UEP0_RX_CTRL &= ~USBSS_EP0_RX_STALL;
        return xRETURN_OK;
    }

    if (ep_addr & 0x80U)
    {
        USBSS_EP_TX_TypeDef* ep_tx = get_ep_tx(ep_num);
        ep_tx->UEP_TX_CR = USBSS_EP_TX_CLR | USBSS_EP_TX_CHAIN_CLR;
        ctx->in_ep_handles[ep_num].Is_Active = false;
        ctx->in_ep_handles[ep_num].Send_ZLP = false;
    }
    else
    {
        USBSS_EP_RX_TypeDef* ep_rx = get_ep_rx(ep_num);
        ep_rx->UEP_RX_CR = USBSS_EP_RX_CLR | USBSS_EP_RX_CHAIN_CLR;
        ctx->out_ep_handles[ep_num].Is_Active = false;
        ctx->out_ep_handles[ep_num].Chain_Primed = false;
    }

    return xRETURN_OK;
}

static bool ch32h417_dcd_ep_is_stalled(void *dcd_ctx, uint8_t ep_addr)
{
    xUSBD_CH32H417_DCD_Context_t *ctx = (xUSBD_CH32H417_DCD_Context_t *)dcd_ctx;
    if (ctx == NULL)
    {
        return false;
    }

    if (!usbss_endpoint_valid(ep_addr, true))
    {
        return false;
    }
    uint8_t ep_num = ep_addr & 0x7FU;
    if (ep_num == 0)
    {
        return (USBSSD->UEP0_TX_CTRL & USBSS_EP0_TX_STALL) != 0;
    }

    if (ep_addr & 0x80U)
    {
        USBSS_EP_TX_TypeDef* ep_tx = get_ep_tx(ep_num);
        return (ep_tx->UEP_TX_CR & USBSS_EP_TX_HALT) != 0;
    }
    else
    {
        USBSS_EP_RX_TypeDef* ep_rx = get_ep_rx(ep_num);
        return (ep_rx->UEP_RX_CR & USBSS_EP_RX_HALT) != 0;
    }
}

// Global DCD operations table
xUSBD_DCD_Ops_t xUSBD_CH32H417_DCD_Ops = {
    .init = ch32h417_dcd_init,
    .deinit = ch32h417_dcd_deinit,
    .set_event_callback = ch32h417_dcd_set_event_callback,
    .connect = ch32h417_dcd_connect,
    .disconnect = ch32h417_dcd_disconnect,
    .enable_interrupts = ch32h417_dcd_enable_interrupts,
    .disable_interrupts = ch32h417_dcd_disable_interrupts,
    .set_address = ch32h417_dcd_set_address,
    .set_remote_wakeup = ch32h417_dcd_set_remote_wakeup,
    .set_test_mode = ch32h417_dcd_set_test_mode,
    .get_frame_number = ch32h417_dcd_get_frame_number,
    .get_speed = ch32h417_dcd_get_speed,
    .ep_init = ch32h417_dcd_ep_init,
    .ep_deinit = ch32h417_dcd_ep_deinit,
    .ep_receive = ch32h417_dcd_ep_receive,
    .ep_send = ch32h417_dcd_ep_send,
    .ep_stall = ch32h417_dcd_ep_stall,
    .ep_clear_stall = ch32h417_dcd_ep_clear_stall,
    .ep_is_stalled = ch32h417_dcd_ep_is_stalled,
};
// EOF /////////////////////////////////////////////////////////////////////////////
