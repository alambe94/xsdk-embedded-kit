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
#include "xassert.h"
#include "xusbd_log.h"

// MACROS //////////////////////////////////////////////////////////////////////////

#define DEF_USBSSD_UEP0_SIZE      512U
#define DEF_ENDP_BURST_LEVEL      16U
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

#define USBSS_EP1_TX_EN             0x00000002U
#define USBSS_EP1_RX_EN             0x00000002U

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
#define USBSS_EP_TX_SEQ_AUTO        0x02U
#define USBSS_EP_TX_HALT            0x80U
#define USBSS_EP_TX_CLR             0x40U
#define USBSS_EP_TX_CHAIN_CLR       0x20U
#define USBSS_EP_TX_ERDY_REQ        0x20U
#define USBSS_EP_TX_CHAIN_IF        0x40U

#define USBSS_EP_RX_CHAIN_AUTO      0x80U
#define USBSS_EP_RX_FIFO_MODE       0x40U
#define USBSS_EP_RX_SEQ_AUTO        0x02U
#define USBSS_EP_RX_HALT            0x80U
#define USBSS_EP_RX_CLR             0x40U
#define USBSS_EP_RX_CHAIN_CLR       0x20U
#define USBSS_EP_RX_CHAIN_IF        0x40U

#define USBSS_EP_DIR_MASK           0x00001000U
#define USBSS_EP_ID_MASK            0x00000700U

#define U3_LINK_RESET               0x80000000U
#define LINK_TOUT_MODE              0x00200000U
#define LINK_U2_ALLOW               0x00020000U
#define LINK_U1_ALLOW               0x00010000U
#define LINK_LTSSM_MODE             0x00008000U
#define LINK_TX_DEEMPH_MASK         0x00000300U
#define LINK_RX_EQ_EN               0x00000040U
#define LINK_PHY_RESET              0x00000008U
#define LINK_RX_TERM_EN             0x00000002U

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

// Static variables to track transient Setup values
static volatile uint8_t  setup_req_type;
static volatile uint8_t  setup_req_code;
static volatile uint16_t setup_req_value;
static volatile uint16_t setup_req_index;
static volatile uint16_t setup_req_len;

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static inline USBSS_EP_TX_TypeDef* get_ep_tx(uint8_t ep_num);
static inline USBSS_EP_RX_TypeDef* get_ep_rx(uint8_t ep_num);
static void usbss_pll_init(bool enable);
static void usbss_rcc_init(bool enable);
static void usbss_phy_cfg(uint8_t addr, uint16_t data);
static void usbss_cfg_mod(void);
static void usbss_endpoint_reset(void);
static void usbss_endpoint_init(void);
static void usbss_controller_reset_init(bool configure_phy);
static USB_DCD_Link_State_t usbss_map_link_state(uint32_t link_state);
static void usbss_notify_link_state(xUSBD_CH32H417_DCD_Context_t *ctx, uint32_t link_state);
static void usbss_notify_connect_once(xUSBD_CH32H417_DCD_Context_t *ctx);
static void usbss_notify_disconnect_once(xUSBD_CH32H417_DCD_Context_t *ctx);
static void usbss_handle_link_lmp(xUSBD_CH32H417_DCD_Context_t *ctx, uint32_t link_int);

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

static void usbss_endpoint_init(void)
{
    usbss_endpoint_reset();

    USBSSD->UEP_TX_EN = USBSS_EP1_TX_EN;
    USBSSD->UEP_RX_EN = USBSS_EP1_RX_EN;
    USBSSD->UEP0_TX_CTRL = 0;
    USBSSD->UEP0_RX_CTRL = 0;
    USBSSD->UEP0_TX_DMA = (uint32_t)&ep0_dma_buf[0];
    USBSSD->UEP0_RX_DMA = (uint32_t)&ep0_dma_buf[0];
}

static void usbss_controller_reset_init(bool configure_phy)
{
    USBSSD->USB_CONTROL |= USBSS_FORCE_RST;
    USBSSD->USB_STATUS = USBSS_UIF_TRANSFER;
    USBSSD->USB_CONTROL = USBSS_UIE_TRANSFER | USBSS_UDIE_SETUP | USBSS_UDIE_STATUS | USBSS_DMA_EN | USBSS_SETUP_FLOW;

    if (configure_phy)
    {
        usbss_cfg_mod();
    }

    usbss_endpoint_init();
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
    if (ctx->event_callback == NULL)
    {
        return;
    }

    xUSBD_DCD_Link_State_Event_t event = {
        .link_state = usbss_map_link_state(link_state),
    };
    ctx->event_callback(ctx->device_ctx, USB_DCD_LINK_STATE_CHANGE_RECEIVED, 0, (uint8_t *)&event, sizeof(event));
}

static void usbss_notify_connect_once(xUSBD_CH32H417_DCD_Context_t *ctx)
{
    if ((ctx->event_callback == NULL) || ctx->is_connected)
    {
        return;
    }

    ctx->is_connected = true;
    ctx->speed = USB_SPEED_SUPER;
    ctx->event_callback(ctx->device_ctx, USB_DCD_CONNECT_RECEIVED, 0, NULL, 0);
}

static void usbss_notify_disconnect_once(xUSBD_CH32H417_DCD_Context_t *ctx)
{
    if ((ctx->event_callback == NULL) || !ctx->is_connected)
    {
        return;
    }

    ctx->is_connected = false;
    ctx->event_callback(ctx->device_ctx, USB_DCD_DISCONNECT_RECEIVED, 0, NULL, 0);
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

    if (link_int & LINK_IF_RX_LMP_TOUT)
    {
        USBSSD->LINK_INT_FLAG = LINK_IF_RX_LMP_TOUT;
        USBSSD->LINK_CTRL |= LINK_GO_DISABLED;
        USBSSD->LINK_CTRL |= LINK_GO_RX_DET;
    }

    if (link_int & LINK_IF_TX_LMP)
    {
        USBSSD->LINK_INT_FLAG = LINK_IF_TX_LMP;
        USBSSD->LINK_LMP_TX_DATA0 = USBSS_LMP_LINK_SPEED | USBSS_LMP_PORT_CAP | USBSS_LMP_HP;
        USBSSD->LINK_LMP_TX_DATA1 = USBSS_LMP_UP_STREAM | USBSS_LMP_NUM_HP_BUF;
        USBSSD->LINK_LMP_TX_DATA2 = 0x0U;
    }

    if (link_int & LINK_IF_RX_LMP)
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

void USBSS_LINK_IRQHandler(void)
{
    xUSBD_CH32H417_DCD_Context_t *ctx = &xUSBD_CH32H417_DCD_Context;
    uint32_t link_int = USBSSD->LINK_INT_FLAG;
    uint32_t link_state = USBSSD->LINK_STATUS & LINK_STATE_MASK;

    usbss_handle_link_lmp(ctx, link_int);

    if (link_int & LINK_IF_STATE_CHG)
    {
        USBSSD->LINK_INT_FLAG = LINK_IF_STATE_CHG;
        usbss_notify_link_state(ctx, link_state);

        switch (link_state)
        {
        case LINK_STATE_U0:
            usbss_notify_connect_once(ctx);
            break;
        case LINK_STATE_DISABLE:
            USBSSD->LINK_CTRL &= ~LINK_GO_DISABLED;
            usbss_notify_disconnect_once(ctx);
            break;
        case LINK_STATE_INACTIVE:
            usbss_notify_disconnect_once(ctx);
            break;
        case LINK_STATE_HOTRST:
            USBSSD->LINK_CFG &= ~(LINK_U1_ALLOW | LINK_U2_ALLOW);
            usbss_controller_reset_init(false);
            USBSSD->LINK_CTRL &= ~LINK_HOT_RESET;
            if (ctx->event_callback != NULL)
            {
                ctx->event_callback(ctx->device_ctx, USB_DCD_RESET_RECEIVED, 0, NULL, 0);
            }
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
    }

    if (link_int & LINK_IF_TERM_PRES)
    {
        USBSSD->LINK_INT_FLAG = LINK_IF_TERM_PRES;
    }

    if (link_int & LINK_IF_WARM_RST)
    {
        USBSSD->LINK_INT_FLAG = LINK_IF_WARM_RST;
        if (USBSSD->LINK_STATUS & LINK_RX_WARM_RST)
        {
            usbss_controller_reset_init(false);
            USBSSD->LINK_CTRL |= LINK_GO_DISABLED;
            __NOP();
            __NOP();
            __NOP();
            __NOP();
            USBSSD->LINK_CTRL &= ~LINK_GO_DISABLED;
            if (ctx->event_callback != NULL)
            {
                ctx->event_callback(ctx->device_ctx, USB_DCD_RESET_RECEIVED, 0, NULL, 0);
            }
        }
    }
}

void USBSS_IRQHandler(void)
{
    xUSBD_CH32H417_DCD_Context_t *ctx = &xUSBD_CH32H417_DCD_Context;
    uint32_t status = USBSSD->USB_STATUS;

    // 1. SETUP packet received on EP0
    if ((status & USBSS_UDIF_SETUP) && !(status & USBSS_UDIF_STATUS))
    {
        // Parse Setup packet values directly from EP0 buffer
        setup_req_type = ep0_dma_buf[0];
        setup_req_code = ep0_dma_buf[1];
        setup_req_value = ((uint16_t)ep0_dma_buf[3] << 8) | ep0_dma_buf[2];
        setup_req_index = ((uint16_t)ep0_dma_buf[5] << 8) | ep0_dma_buf[4];
        setup_req_len = ((uint16_t)ep0_dma_buf[7] << 8) | ep0_dma_buf[6];

        // Clear Setup interrupt flag
        USBSSD->USB_STATUS = USBSS_UDIF_SETUP;

        // Dispatch setup packet to core callback
        if (ctx->event_callback != NULL)
        {
            ctx->event_callback(ctx->device_ctx, USB_DCD_SETUP_RECEIVED, 0, ep0_dma_buf, 8);
        }
    }
    // 2. Status Stage Complete interrupt
    else if (status & USBSS_UDIF_STATUS)
    {
        // Clear flag
        USBSSD->USB_STATUS = USBSS_UDIF_STATUS;

        if (ctx->event_callback != NULL)
        {
            if (setup_req_type & USB_REQ_TYPE_IN)
            {
                ctx->event_callback(ctx->device_ctx, USB_DCD_DATA_RECEIVED, 0x00, ep0_dma_buf, 0);
            }
            else
            {
                ctx->event_callback(ctx->device_ctx, USB_DCD_DATA_SENT, 0x80, NULL, 0);
            }
        }

        // Reset control states
        USBSSD->UEP0_TX_CTRL = 0;
        USBSSD->UEP0_RX_CTRL = 0;
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
                // Toggle sequence bit on control endpoint EP0 IN
                USBSSD->UEP0_TX_CTRL = USBSS_EP0_TX_DPH | ((((USBSSD->UEP0_TX_CTRL >> 16) & 0x1F) + 1) << 16);

                if (ctx->event_callback != NULL)
                {
                    ctx->event_callback(ctx->device_ctx, USB_DCD_DATA_SENT, 0x80, NULL, 0);
                }
            }
            else if (ep_num <= xUSBD_DEVICE_MAX_IN_ENDPOINT)
            {
                USBSS_EP_TX_TypeDef* ep_tx = get_ep_tx(ep_num);
                // Clear endpoint transmit interface flag
                ep_tx->UEP_TX_CHAIN_ST |= USBSS_EP_TX_CHAIN_IF;

                xUSBD_CH32H417_EP_Handle_t *ep_handle = &ctx->in_ep_handles[ep_num];
                uint32_t actual = ep_tx->UEP_TX_CHAIN_LEN;
                ep_handle->Actual_XFER_Length += actual;

                if (ctx->event_callback != NULL)
                {
                    ctx->event_callback(ctx->device_ctx, USB_DCD_DATA_SENT, ep_num | 0x80, ep_handle->Data, ep_handle->Actual_XFER_Length);
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

                if (ctx->event_callback != NULL)
                {
                    ctx->event_callback(ctx->device_ctx, USB_DCD_DATA_RECEIVED, 0x00, ep0_dma_buf, rx_len);
                }
            }
            else if (ep_num <= xUSBD_DEVICE_MAX_OUT_ENDPOINT)
            {
                USBSS_EP_RX_TypeDef* ep_rx = get_ep_rx(ep_num);
                // Clear endpoint receive interface flag
                ep_rx->UEP_RX_CHAIN_ST |= USBSS_EP_RX_CHAIN_IF;

                xUSBD_CH32H417_EP_Handle_t *ep_handle = &ctx->out_ep_handles[ep_num];
                uint32_t rx_len = ep_rx->UEP_RX_CHAIN_LEN;
                ep_handle->Actual_XFER_Length = rx_len;

                // Re-arm receiver
                ep_rx->UEP_RX_DMA = (uint32_t)ep_handle->Data;
                ep_rx->UEP_RX_CHAIN_MAX_NUMP = DEF_ENDP_BURST_LEVEL;

                if (ctx->event_callback != NULL)
                {
                    ctx->event_callback(ctx->device_ctx, USB_DCD_DATA_RECEIVED, ep_num, ep_handle->Data, rx_len);
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

    ctx->device_ctx = device_ctx;
    ctx->speed = speed;
    ctx->is_connected = false;
    ctx->event_callback = NULL;

    if (ctx->is_hardware_initialized)
    {
        return xRETURN_OK;
    }

    // Enable clocks and PLL configuration
    usbss_rcc_init(true);

    // PHY and Link setup sequence
    USBSSD->LINK_CFG = LINK_RX_EQ_EN | LINK_TX_DEEMPH_MASK | LINK_PHY_RESET;
    USBSSD->LINK_CTRL = LINK_P2_MODE | LINK_GO_DISABLED;
    USBSSD->LINK_CFG = LINK_RX_EQ_EN | LINK_TX_DEEMPH_MASK | LINK_LTSSM_MODE | LINK_TOUT_MODE;
    USBSSD->LINK_LPM_CR |= LINK_LPM_EN;

    // Enable terminations
    USBSSD->LINK_CFG |= LINK_RX_TERM_EN;

    // Enable Link interrupts
    USBSSD->LINK_INT_CTRL = LINK_IE_TX_LMP | LINK_IE_RX_LMP | LINK_IE_RX_LMP_TOUT |
                            LINK_IE_STATE_CHG | LINK_IE_WARM_RST | LINK_IE_TERM_PRES | LINK_IE_RX_SET_FC;

    USBSSD->LINK_CTRL = LINK_P2_MODE;
    USBSSD->LINK_U1_WKUP_TMR = 120;
    USBSSD->LINK_U1_WKUP_FILTER = 50;
    USBSSD->LINK_U2_WKUP_FILTER = 0;
    USBSSD->LINK_U3_WKUP_FILTER = 0;

    usbss_controller_reset_init(true);

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
    USBSSD->USB_CONTROL &= ~USBSS_FORCE_RST;
    USBSSD->LINK_CFG &= ~(LINK_PHY_RESET | U3_LINK_RESET);

    // Disable clocks
    usbss_rcc_init(false);

    ctx->is_hardware_initialized = false;
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

    NVIC_EnableIRQ(USBSS_IRQn);
    NVIC_EnableIRQ(USBSS_LINK_IRQn);
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

    uint8_t ep_num = ep_addr & 0x7F;
    if (ep_num == 0 || ep_num > xUSBD_DEVICE_MAX_IN_ENDPOINT)
    {
        return xRETURN_OK; // EP0 is initialized in dcd_init
    }

    if (ep_addr & 0x80) // IN endpoint
    {
        xUSBD_CH32H417_EP_Handle_t *ep_handle = &ctx->in_ep_handles[ep_num];
        ep_handle->EP_Type = ep_type;
        ep_handle->MPS = mps;

        // Enable TX endpoint
        USBSSD->UEP_TX_EN |= (1 << ep_num);

        USBSS_EP_TX_TypeDef* ep_tx = get_ep_tx(ep_num);
        ep_tx->UEP_TX_CFG = USBSS_EP_TX_CHAIN_AUTO | USBSS_EP_TX_SEQ_AUTO | USBSS_EP_TX_FIFO_MODE;
        ep_tx->UEP_TX_CR = USBSS_EP_TX_CLR | USBSS_EP_TX_CHAIN_CLR;
    }
    else // OUT endpoint
    {
        xUSBD_CH32H417_EP_Handle_t *ep_handle = &ctx->out_ep_handles[ep_num];
        ep_handle->EP_Type = ep_type;
        ep_handle->MPS = mps;

        // Enable RX endpoint
        USBSSD->UEP_RX_EN |= (1 << ep_num);

        USBSS_EP_RX_TypeDef* ep_rx = get_ep_rx(ep_num);
        ep_rx->UEP_RX_CFG = USBSS_EP_RX_CHAIN_AUTO | USBSS_EP_RX_SEQ_AUTO | USBSS_EP_RX_FIFO_MODE;
        ep_rx->UEP_RX_CR = USBSS_EP_RX_CLR | USBSS_EP_RX_CHAIN_CLR;
        ep_rx->UEP_RX_CHAIN_MAX_NUMP = DEF_ENDP_BURST_LEVEL;
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

    uint8_t ep_num = ep_addr & 0x7F;
    if (ep_num == 0 || ep_num > xUSBD_DEVICE_MAX_IN_ENDPOINT)
    {
        return xRETURN_OK;
    }

    if (ep_addr & 0x80)
    {
        USBSSD->UEP_TX_EN &= ~(1 << ep_num);
        USBSS_EP_TX_TypeDef* ep_tx = get_ep_tx(ep_num);
        ep_tx->UEP_TX_CR = USBSS_EP_TX_CLR | USBSS_EP_TX_CHAIN_CLR;
    }
    else
    {
        USBSSD->UEP_RX_EN &= ~(1 << ep_num);
        USBSS_EP_RX_TypeDef* ep_rx = get_ep_rx(ep_num);
        ep_rx->UEP_RX_CR = USBSS_EP_RX_CLR | USBSS_EP_RX_CHAIN_CLR;
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

    uint8_t ep_num = ep_addr & 0x7F;
    xASSERT(ep_num <= xUSBD_DEVICE_MAX_OUT_ENDPOINT, "EP index out of bounds");

    if (ep_num == 0)
    {
        // For EP0, receive expects setup or control OUT data to go to ep0_dma_buf
        USBSSD->UEP0_RX_DMA = (uint32_t)&ep0_dma_buf[0];
        USBSSD->UEP0_RX_CTRL = USBSS_EP0_RX_ERDY | USBSS_EP0_RX_ACK;
        return xRETURN_OK;
    }

    xUSBD_CH32H417_EP_Handle_t *ep_handle = &ctx->out_ep_handles[ep_num];
    ep_handle->Data = data;
    ep_handle->Current_Data = data;
    ep_handle->Remaining_XFER_Length = length;
    ep_handle->Actual_XFER_Length = 0;

    USBSS_EP_RX_TypeDef* ep_rx = get_ep_rx(ep_num);
    ep_rx->UEP_RX_DMA = (uint32_t)data;
    ep_rx->UEP_RX_CHAIN_LEN = length;
    ep_rx->UEP_RX_CHAIN_MAX_NUMP = DEF_ENDP_BURST_LEVEL;

    // Trigger RX Ready
    ep_rx->UEP_RX_CR = USBSS_EP_RX_CHAIN_CLR | DEF_ENDP_BURST_LEVEL;

    return xRETURN_OK;
}

static xRETURN_t ch32h417_dcd_ep_send(void *dcd_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length, bool is_zlp_required)
{
    xUSBD_CH32H417_DCD_Context_t *ctx = (xUSBD_CH32H417_DCD_Context_t *)dcd_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    uint8_t ep_num = ep_addr & 0x7F;
    xASSERT(ep_num <= xUSBD_DEVICE_MAX_IN_ENDPOINT, "EP index out of bounds");

    if (ep_num == 0)
    {
        if (length == 0)
        {
            USBSSD->UEP0_TX_CTRL = USBSS_EP0_TX_DPH;
            USBSSD->UEP0_TX_CTRL |= USBSS_EP0_TX_ERDY;
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
    ep_handle->Data = data;
    ep_handle->Current_Data = data;
    ep_handle->Remaining_XFER_Length = length;
    ep_handle->Actual_XFER_Length = 0;
    ep_handle->Send_ZLP = is_zlp_required ? 1 : 0;

    USBSS_EP_TX_TypeDef* ep_tx = get_ep_tx(ep_num);
    ep_tx->UEP_TX_DMA = (uint32_t)data;
    ep_tx->UEP_TX_CHAIN_LEN = length;

    // Trigger TX Ready
    ep_tx->UEP_TX_CR = USBSS_EP_TX_ERDY_REQ | USBSS_EP_TX_CHAIN_CLR | DEF_ENDP_BURST_LEVEL;

    return xRETURN_OK;
}

static xRETURN_t ch32h417_dcd_ep_stall(void *dcd_ctx, uint8_t ep_addr)
{
    xUSBD_CH32H417_DCD_Context_t *ctx = (xUSBD_CH32H417_DCD_Context_t *)dcd_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    uint8_t ep_num = ep_addr & 0x7F;
    if (ep_num == 0)
    {
        USBSSD->UEP0_TX_CTRL |= USBSS_EP0_TX_STALL;
        USBSSD->UEP0_RX_CTRL |= USBSS_EP0_RX_ERDY | USBSS_EP0_RX_STALL;
        return xRETURN_OK;
    }

    if (ep_addr & 0x80)
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

    uint8_t ep_num = ep_addr & 0x7F;
    if (ep_num == 0)
    {
        USBSSD->UEP0_TX_CTRL &= ~USBSS_EP0_TX_STALL;
        USBSSD->UEP0_RX_CTRL &= ~USBSS_EP0_RX_STALL;
        return xRETURN_OK;
    }

    if (ep_addr & 0x80)
    {
        USBSS_EP_TX_TypeDef* ep_tx = get_ep_tx(ep_num);
        ep_tx->UEP_TX_CR = USBSS_EP_TX_CLR | USBSS_EP_TX_CHAIN_CLR;
    }
    else
    {
        USBSS_EP_RX_TypeDef* ep_rx = get_ep_rx(ep_num);
        ep_rx->UEP_RX_CR |= USBSS_EP_RX_CLR | USBSS_EP_RX_CHAIN_CLR;
        ep_rx->UEP_RX_CHAIN_MAX_NUMP = DEF_ENDP_BURST_LEVEL;
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

    uint8_t ep_num = ep_addr & 0x7F;
    if (ep_num == 0)
    {
        return (USBSSD->UEP0_TX_CTRL & USBSS_EP0_TX_STALL) != 0;
    }

    if (ep_addr & 0x80)
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
