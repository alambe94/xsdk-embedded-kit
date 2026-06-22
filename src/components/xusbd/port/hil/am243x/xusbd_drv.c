// Copyright 2022 alambe94

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// @file xusbd_drv.c
// @brief AM243x USB Device Controller Driver (USB0, single instance)

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>
#include <string.h>
#include <stdio.h>

// SYSTEM INCLUDES
#include <drivers/soc.h>
#include <kernel/dpl/HwiP.h>
#include <kernel/dpl/ClockP.h>
#include <kernel/dpl/CacheP.h>
#include <drivers/hw_include/hw_types.h>
#include <drivers/hw_include/cslr_soc.h>
#include <drivers/hw_include/csl_types.h>
#include <drivers/hw_include/tistdtypes.h>
#include <drivers/hw_include/serdes_cd/V1/cslr_wiz16b8m4ct2.h>
#include <drivers/hw_include/am64x_am243x/cslr_main_pll_mmr.h>
#include <drivers/sciclient/include/tisci/am64x_am243x/tisci_devices.h>
#include <drivers/sciclient/include/tisci/am64x_am243x/tisci_clocks.h>
#include <drivers/../usb/cdn/core_driver/common/include/cdn_stdtypes.h>
#include <drivers/../usb/cdn/core_driver/common/include/cdn_stdint.h>
#include <drivers/../usb/cdn/core_driver/common/include/cusb_ch9_if.h>
#include <drivers/../usb/cdn/core_driver/common/include/cusb_ch9_structs_if.h>
#include <drivers/../usb/cdn/core_driver/device/include/cusbd_obj_if.h>
#include <drivers/../usb/cdn/core_driver/device/include/cusbd_structs_if.h>
#include <drivers/../usb/cdn/soc/am64x_am243x/cslr_usb3p0ss_v5_2.h>
#include <drivers/../usb/cdn/core_driver/common/src/byteorder.h>
#include <drivers/hw_include/serdes_cd/V1/csl_serdes3.h>
#include <drivers/hw_include/serdes_cd/V1/csl_serdes3_usb.h>

// MODULE INCLUDES
#include "xusb_defs.h"
#include "xusbd_return.h"
#include "xusbd_drv.h"
#include "am243x_phy.h"
#include "xusbd_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// TI_Wrap config registers  (USB0, fixed)
static CSL_usb3p0ss_cmnRegs *p_usbregs_tiwrap = (CSL_usb3p0ss_cmnRegs *)CSL_USB0_MMR_MMRVBP_USBSS_CMN_BASE;
// CDNS_CTRL CONFIG registers (USB0, fixed)
static CSL_usb3p0ss_ctrlRegs *p_usbregs_ctlr = (CSL_usb3p0ss_ctrlRegs *)CSL_USB0_VBP2APB_WRAP_CONTROLLER_VBP_CORE_ADDR_MAP_BASE;

#define PRIVATE_DATA_MEM          (16384U)
#define DATA_XFER_BUFFER_COUNT    4
#define DATA_IN_XFER_BUFFER_SIZE  1024
#define DATA_OUT_XFER_BUFFER_SIZE 1024

static uint8_t alignedBufferCusbd[PRIVATE_DATA_MEM] __attribute__((aligned(1024), section(".bss.nocache")));

static CH9_UsbSetup setupRequestPacket __attribute__((aligned(8), section(".bss.nocache")));
static uint8_t buffEp0In[512] __attribute__((aligned(8), section(".bss.nocache")));
static uint8_t buffEp0Out[512] __attribute__((aligned(8), section(".bss.nocache")));

static CUSBD_Req DataXferRequestsOut[DATA_XFER_BUFFER_COUNT + 1] __attribute__((aligned(8), section(".bss.nocache")));
static uint8_t buffEpOut[DATA_XFER_BUFFER_COUNT][DATA_OUT_XFER_BUFFER_SIZE] __attribute__((aligned(8), section(".bss.nocache")));
static uint8_t *dataXferBufferOut[DATA_XFER_BUFFER_COUNT] __attribute__((aligned(8), section(".bss.nocache")));

static CUSBD_Req DataXferRequestsIn[DATA_XFER_BUFFER_COUNT + 1] __attribute__((aligned(8), section(".bss.nocache")));
static uint8_t buffEpIn[DATA_XFER_BUFFER_COUNT][DATA_IN_XFER_BUFFER_SIZE] __attribute__((aligned(8), section(".bss.nocache")));
static uint8_t *dataXferBufferIn[DATA_XFER_BUFFER_COUNT] __attribute__((aligned(8), section(".bss.nocache")));

static CUSBDMA_DmaTrb buff[32][8] __attribute__((aligned(8), section(".bss.nocache")));
static CUSBDMA_MemResources epMemRes[32] __attribute__((aligned(8), section(".bss.nocache")));

static CUSBD_Config Config;
static CUSBD_SysReq sysRequestCusbd;
static CUSBD_PrivateData *pD = (CUSBD_PrivateData *)alignedBufferCusbd;

static HwiP_Params hwiParamsUsb;
static HwiP_Object hwiObjUsb;

CUSBD_OBJ *ObjCusbd = NULL;

// Single instance - USB0 only. Multi-instance would require a context pointer
// threaded through the Cadence callbacks, which the Cadence API does not support.
xUSBD_AM243x_DCD_Context_t xUSBD_AM243x_DCD_Context = {0};

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static void outepxfercmplcb(CUSBD_Ep *ep, CUSBD_Req *request);
static void inepxfercmplcb(CUSBD_Ep *ep, CUSBD_Req *request);
static void usb_irq6_isr(void *arg);
static CUSBD_Ep *get_ep_from_address(uint8_t ep_address);
static int usb3_phy_init(void);

static void connect(CUSBD_PrivateData *pD);
static void disconnect(CUSBD_PrivateData *pD);
static void resume(CUSBD_PrivateData *pD);
static uint32_t setup(CUSBD_PrivateData *pD, CH9_UsbSetup *ctrl);
static void suspend(CUSBD_PrivateData *pD);
static void businterval(CUSBD_PrivateData *pD);
static void descmissing(CUSBD_PrivateData *pD, uint8_t epAddress);
static void usb2physoftreset(CUSBD_PrivateData *pD);

static xRETURN_t am243x_dcd_init(void *dcd_ctx, USB_Speed_t speed, void *device_ctx);
static xRETURN_t am243x_dcd_deinit(void *dcd_ctx);
static xRETURN_t am243x_dcd_set_event_callback(void *dcd_ctx, xUSBD_DCD_Event_Callback_t callback);
static xRETURN_t am243x_dcd_connect(void *dcd_ctx);
static xRETURN_t am243x_dcd_disconnect(void *dcd_ctx);
static xRETURN_t am243x_dcd_enable_interrupts(void *dcd_ctx);
static xRETURN_t am243x_dcd_disable_interrupts(void *dcd_ctx);
static xRETURN_t am243x_dcd_set_address(void *dcd_ctx, uint8_t address);
static xRETURN_t am243x_dcd_set_remote_wakeup(void *dcd_ctx, bool enable);
static xRETURN_t am243x_dcd_set_test_mode(void *dcd_ctx, uint8_t mode);
static uint32_t am243x_dcd_get_frame_number(void *dcd_ctx);
static USB_Speed_t am243x_dcd_get_speed(void *dcd_ctx);
static xRETURN_t am243x_dcd_ep_init(void *dcd_ctx, uint8_t ep_addr, uint8_t ep_type, uint16_t mps);
static xRETURN_t am243x_dcd_ep_deinit(void *dcd_ctx, uint8_t ep_addr);
static xRETURN_t am243x_dcd_ep_receive(void *dcd_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
static xRETURN_t am243x_dcd_ep_send(void *dcd_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length, bool is_zlp_required);
static xRETURN_t am243x_dcd_ep_stall(void *dcd_ctx, uint8_t ep_addr);
static xRETURN_t am243x_dcd_ep_clear_stall(void *dcd_ctx, uint8_t ep_addr);
static bool am243x_dcd_ep_is_stalled(void *dcd_ctx, uint8_t ep_addr);

static CUSBD_Callbacks Callbacks = {disconnect, connect, setup, suspend, resume, businterval, descmissing, usb2physoftreset};

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// ============================================================================
//  * USB3 Hardware Event Callbacks (Cadence CUSBD -> xUSBD core)
// ============================================================================

static void connect(CUSBD_PrivateData *pD)
{
    xUSBD_AM243x_DCD_Context_t *ctx = &xUSBD_AM243x_DCD_Context;

    if (ObjCusbd != NULL && pD != NULL)
    {
        switch (pD->device.speed)
        {
        case CH9_USB_SPEED_LOW:
            ctx->speed = USB_SPEED_LOW;
            break;
        case CH9_USB_SPEED_FULL:
            ctx->speed = USB_SPEED_FULL;
            break;
        case CH9_USB_SPEED_HIGH:
            ctx->speed = USB_SPEED_HIGH;
            break;
        case CH9_USB_SPEED_SUPER:
            ctx->speed = USB_SPEED_SUPER;
            break;
        default:
            break;
        }
    }

    if (ctx->event_callback != NULL)
    {
        ctx->event_callback(ctx->device_ctx, USB_DCD_CONNECT_RECEIVED, 0, NULL, 0);
    }
}

static void disconnect(CUSBD_PrivateData *pD)
{
    (void)pD;
    xUSBD_AM243x_DCD_Context_t *ctx = &xUSBD_AM243x_DCD_Context;
    if (ctx->event_callback != NULL)
    {
        ctx->event_callback(ctx->device_ctx, USB_DCD_DISCONNECT_RECEIVED, 0, NULL, 0);
    }
}

static void resume(CUSBD_PrivateData *pD)
{
    (void)pD;
    xUSBD_AM243x_DCD_Context_t *ctx = &xUSBD_AM243x_DCD_Context;
    if (ctx->event_callback != NULL)
    {
        ctx->event_callback(ctx->device_ctx, USB_DCD_RESUME_RECEIVED, 0, NULL, 0);
    }
}

static uint32_t setup(CUSBD_PrivateData *pD, CH9_UsbSetup *ctrl)
{
    (void)pD;
    xUSBD_AM243x_DCD_Context_t *ctx = &xUSBD_AM243x_DCD_Context;
    if (ctx->event_callback != NULL)
    {
        ctx->event_callback(ctx->device_ctx, USB_DCD_SETUP_RECEIVED, 0, (uint8_t *)ctrl, sizeof(CH9_UsbSetup));
    }
    return 0;
}

static void suspend(CUSBD_PrivateData *pD)
{
    (void)pD;
    xUSBD_AM243x_DCD_Context_t *ctx = &xUSBD_AM243x_DCD_Context;
    if (ctx->event_callback != NULL)
    {
        ctx->event_callback(ctx->device_ctx, USB_DCD_SUSPEND_RECEIVED, 0, NULL, 0);
    }
}

static void businterval(CUSBD_PrivateData *pD)
{
    (void)pD;
    xUSBD_AM243x_DCD_Context_t *ctx = &xUSBD_AM243x_DCD_Context;
    if (ctx->event_callback != NULL)
    {
        ctx->event_callback(ctx->device_ctx, USB_DCD_SOF_RECEIVED, 0, NULL, 0);
    }
}

static void descmissing(CUSBD_PrivateData *pD, uint8_t epAddress)
{
    (void)pD;
    (void)epAddress;
}

static void usb2physoftreset(CUSBD_PrivateData *pD)
{
    (void)pD;
}

// ============================================================================
//  * DMA Transfer Completion Callbacks
// ============================================================================

static void ep_advance_progress(xUSBD_AM243x_EP_Handle_t *ep_handle, uint32_t actual)
{
    ep_handle->Actual_XFER_Length += actual;
    ep_handle->Current_Data += actual;
    ep_handle->Remaining_XFER_Length -= actual;
}

static void
ep_notify_complete(xUSBD_AM243x_DCD_Context_t *ctx, uint8_t ep_address, xUSBD_AM243x_EP_Handle_t *ep_handle, USB_DCD_Event_t event_type)
{
    if (ctx->event_callback != NULL)
    {
        ctx->event_callback(ctx->device_ctx, event_type, ep_address, ep_handle->Data, ep_handle->Actual_XFER_Length);
    }
}

static void outepxfercmplcb(CUSBD_Ep *ep, CUSBD_Req *request)
{
    xUSBD_AM243x_DCD_Context_t *ctx = &xUSBD_AM243x_DCD_Context;
    uint8_t ep_number = ep->address & 0x7F;
    xUSBD_AM243x_EP_Handle_t *ep_handle = &ctx->out_ep_handles[ep_number];

    memcpy(request->buf, (uint8_t *)request->dma, request->actual);
    ep_advance_progress(ep_handle, request->actual);

    if (ep_handle->Remaining_XFER_Length > 0 && (request->actual % ep_handle->MPS) == 0)
    {
        uint8_t *dma_buffer = ep_number ? buffEpOut[ep_number - 1] : buffEp0Out;
        uint32_t buffer_size = ep_number ? DATA_OUT_XFER_BUFFER_SIZE : sizeof(buffEp0Out);
        uint32_t chunk = (ep_handle->Remaining_XFER_Length < buffer_size) ? ep_handle->Remaining_XFER_Length : buffer_size;

        memset(request, 0, sizeof(CUSBD_Req));
        request->buf = ep_handle->Current_Data;
        request->dma = (uintptr_t)dma_buffer;
        request->length = chunk;
        request->complete = outepxfercmplcb;

        ep->ops->reqQueue(pD, ep, request);
    }
    else
    {
        ep_notify_complete(ctx, ep->address, ep_handle, USB_DCD_DATA_RECEIVED);
    }
}

static void inepxfercmplcb(CUSBD_Ep *ep, CUSBD_Req *request)
{
    xUSBD_AM243x_DCD_Context_t *ctx = &xUSBD_AM243x_DCD_Context;
    uint8_t ep_number = ep->address & 0x7F;
    xUSBD_AM243x_EP_Handle_t *ep_handle = &ctx->in_ep_handles[ep_number];

    ep_advance_progress(ep_handle, request->actual);

    if (ep_handle->Remaining_XFER_Length > 0)
    {
        uint8_t *dma_buffer = ep_number ? buffEpIn[ep_number - 1] : buffEp0In;
        uint32_t buffer_size = ep_number ? DATA_IN_XFER_BUFFER_SIZE : sizeof(buffEp0In);
        uint32_t chunk = (ep_handle->Remaining_XFER_Length < buffer_size) ? ep_handle->Remaining_XFER_Length : buffer_size;

        memset(request, 0, sizeof(CUSBD_Req));
        request->buf = ep_handle->Current_Data;
        request->zero = (ep_handle->Send_ZLP && ep_handle->Remaining_XFER_Length == chunk) ? 1 : 0;
        memcpy(dma_buffer, ep_handle->Current_Data, chunk);
        request->dma = (uintptr_t)dma_buffer;
        request->length = chunk;
        request->complete = inepxfercmplcb;

        ep->ops->reqQueue(pD, ep, request);
    }
    else
    {
        ep_notify_complete(ctx, ep->address, ep_handle, USB_DCD_DATA_SENT);
    }
}

// ============================================================================
//  * Helpers
// ============================================================================

static CUSBD_Ep *get_ep_from_address(uint8_t ep_address)
{
    if (ObjCusbd == NULL)
    {
        return NULL;
    }

    uint8_t ep_number = ep_address & 0x0F;

    if (ep_number == 0)
    {
        return pD->device.ep0;
    }

    if (ep_number > DATA_XFER_BUFFER_COUNT)
    {
        return NULL;
    }

    CUSBD_Ep *ep = (ep_address & CH9_USB_EP_DIR_IN) ? &pD->ep_in_container[ep_number - 1].ep : &pD->ep_out_container[ep_number - 1].ep;

    return (ep->ops != NULL && ep->address == ep_address) ? ep : NULL;
}

static int usb3_phy_init(void)
{
    // Configure the SerDes lane for USB3 SuperSpeed (5 Gbps). cd_v1_v1.1

    int32_t socStatus;

    // Power on the SERDES_10G0 module via TISCI.
    socStatus = SOC_moduleClockEnable(TISCI_DEV_SERDES_10G0, 1u);
    if (socStatus != SystemP_SUCCESS)
    {
        xUSBD_LOG(0x10E0, "USB3 PHY: failed to enable SERDES_10G0 module clock (%d)\r\n", socStatus);
        return -1;
    }

#if 0
    CSL_SerdesResult result;
    CSL_SerdesStatus SerdesStatus;
    CSL_SerdesPIPEStatus pipeStatus;
    CSL_SerdesLaneEnableStatus laneStatus;
    CSL_SerdesLaneEnableParams serdesLaneEnableParams;

    memset(&serdesLaneEnableParams, 0, sizeof(serdesLaneEnableParams));

    serdesLaneEnableParams.baseAddress = CSL_SERDES_10G0_BASE;
    serdesLaneEnableParams.numLanes = 1;
    serdesLaneEnableParams.laneMask = 0x1U;
    serdesLaneEnableParams.serdesInstance = CSL_TORRENT_SERDES0;
    serdesLaneEnableParams.phyType = CSL_SERDES_PHY_TYPE_USB;
    serdesLaneEnableParams.phyInstanceNum = 0;
    serdesLaneEnableParams.refClock = CSL_SERDES_REF_CLOCK_100M;
    serdesLaneEnableParams.linkRate = CSL_SERDES_LINK_RATE_5G;
    serdesLaneEnableParams.operatingMode = CSL_SERDES_FUNCTIONAL_MODE;
    serdesLaneEnableParams.laneCtrlRate[0] = CSL_SERDES_LANE_FULL_RATE;
    serdesLaneEnableParams.loopbackMode[0] = CSL_SERDES_LOOPBACK_DISABLED;
    serdesLaneEnableParams.refClkSrc = CSL_SERDES_REF_CLOCK_INT;
    serdesLaneEnableParams.refClkOut = CSL_SERDES_REFCLK_OUT_EN;
    serdesLaneEnableParams.SSC_mode = CSL_SERDES_INTERNAL_SSC;
    // serdesLaneEnableParams.invertTXPolarity[0] = CSL_SERDES_INV_TX_POLARITY_DIS;
    // serdesLaneEnableParams.invertRXPolarity[0] = CSL_SERDES_INV_RX_POLARITY_DIS;

    // POR reset
    CSL_serdesPorReset(serdesLaneEnableParams.baseAddress);

    // Select the SerDes IP type in the Main Ctrl MMR
    CSL_serdesIPSelect(CSL_CTRL_MMR0_CONFIG0_BASE,
                       serdesLaneEnableParams.phyType,
                       serdesLaneEnableParams.phyInstanceNum,
                       serdesLaneEnableParams.serdesInstance,
                       0U);

    // Configure the WIZ refclk divider and mux (internal 100 MHz path)
    result = CSL_serdesRefclkSel(CSL_CTRL_MMR0_CONFIG0_BASE,
                                 serdesLaneEnableParams.baseAddress,
                                 serdesLaneEnableParams.refClock,
                                 serdesLaneEnableParams.refClkSrc,
                                 serdesLaneEnableParams.serdesInstance,
                                 serdesLaneEnableParams.phyType);
    if (result != CSL_SERDES_NO_ERR)
    {
        xUSBD_LOG(0x10E0, "USB3 PHY: CSL_serdesRefclkSel failed (%d)\r\n", result);
        return -3;
    }

    // Disable PLL/lanes
    CSL_serdesDisablePllAndLanes(serdesLaneEnableParams.baseAddress,
                                 serdesLaneEnableParams.numLanes,
                                 serdesLaneEnableParams.laneMask);

    // Load USB-specific SerDes configuration
    result = CSL_serdesUSBInit(&serdesLaneEnableParams);
    if (result != CSL_SERDES_NO_ERR)
    {
        xUSBD_LOG(0x10E0, "USB3 PHY: CSL_serdesUSBInit failed (%d)\r\n", result);
        return -4;
    }

    // Enable PLL and lanes
    laneStatus = CSL_serdesLaneEnable(&serdesLaneEnableParams);
    if (laneStatus != CSL_SERDES_LANE_ENABLE_NO_ERR)
    {
        xUSBD_LOG(0x10E0, "USB3 PHY: CSL_serdesLaneEnable failed (%d)\r\n", laneStatus);
        return -5;
    }

    SerdesStatus = CSL_serdesGetPLLStatus(serdesLaneEnableParams.baseAddress,
                                          serdesLaneEnableParams.serdesInstance,
                                          serdesLaneEnableParams.serdesInstance);
    if (SerdesStatus != CSL_SERDES_STATUS_PLL_LOCKED)
    {
        xUSBD_LOG(0x10E0, "USB3 PHY: PLL failed to lock (%d)\r\n", SerdesStatus);
        return -6;
    }

    uint32_t timeout_us = 100000U;
    do
    {
        pipeStatus = CSL_serdesGetPIPEClkStatus(serdesLaneEnableParams.baseAddress,
                                                serdesLaneEnableParams.laneMask,
                                                serdesLaneEnableParams.phyType);
        if (pipeStatus == CSL_SERDES_STATUS_PIPE_CLK_VALID)
            break;
        ClockP_usleep(10);
        timeout_us -= 10U;
    } while (timeout_us > 0U);

    if (pipeStatus != CSL_SERDES_STATUS_PIPE_CLK_VALID)
    {
        xUSBD_LOG(0x10E0, "USB3 PHY: PIPE clock timeout\r\n");
        return -8;
    }
#else
    // ---- CTRL_MMR0: IP select and reference clock source ----
    //
    // Linux equivalent:
    //   IP select    -> boot firmware / U-Boot (not done by WIZ/Torrent kernel driver)
    //   Clock select -> clock framework, DT assigned-clocks = MAIN_PLL2_HSDIV4_CLKOUT

    // CSL_serdesIPSelect(): SERDES0 lane 0 -> USB3 (LN0_CTRL[1:0] = 1)
    HW_WR_FIELD32((CSL_CTRL_MMR0_CFG0_BASE + CSL_MAIN_CTRL_MMR_CFG0_SERDES0_LN0_CTRL),
                  CSL_MAIN_CTRL_MMR_CFG0_SERDES0_LN0_CTRL_LANE_FUNC_SEL, 0x1U);

    // CSL_serdesRefclkSel() CTRL_MMR part - partition 2 already unlocked:
    //   SERDES0_CLKSEL[1:0] = 3 -> MAIN_PLL2_HSDIV4_CLKOUT (100 MHz)
    //   0=WKUP_HFOSC0  1=HFOSC1  2=MAIN_PLL0_HSDIV8  3=MAIN_PLL2_HSDIV4
    HW_WR_FIELD32((CSL_CTRL_MMR0_CFG0_BASE + CSL_MAIN_CTRL_MMR_CFG0_SERDES0_CLKSEL), CSL_MAIN_CTRL_MMR_CFG0_SERDES0_CLKSEL_CORE_REFCLK_SEL,
                  0x3U);

    // am243x_wiz_init() internally:
    //   1. WIZ POR + clock + raw interface
    //   2. am243x_torrent_phy_configure()   - Torrent register tables
    //   3. WIZ lane enable + PHY reset deassert
    if (am243x_wiz_init(CSL_SERDES_10G0_BASE, PHY_TYPE_USB3, 0, 100000000U) != 0)
    {
        xUSBD_LOG(0x10E0, "USB3 PHY: SERDES init failed\r\n");
        return -2;
    }
#endif

    return 0;
}

// ============================================================================
//  * IRQ Handler
// ============================================================================

static void usb_irq6_isr(void *arg)
{
    (void)arg;
    CUSBD_Isr(pD);
}

void xUSBD_AM243x_DCD_IRQ_Handler(uint8_t port)
{
    (void)port;
    xASSERT(port < xUSBD_DEVICE_MAX_INSTANCE, "Port out of range");
    // Handler is called from usb_irq6_isr - no additional processing needed
}

// ============================================================================
//  * DCD Ops Implementations
// ============================================================================

static xRETURN_t am243x_dcd_init(void *dcd_ctx, USB_Speed_t speed, void *device_ctx)
{
    xUSBD_AM243x_DCD_Context_t *ctx = (xUSBD_AM243x_DCD_Context_t *)dcd_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    xASSERT(ctx->port < xUSBD_DEVICE_MAX_INSTANCE, "Port out of range");
    xUSBD_LOG(0x1001, "USB_DCD_Init: port=%d, speed=%d", ctx->port, speed);

    ctx->device_ctx = device_ctx;
    ctx->speed = speed;
    ctx->event_callback = NULL;

    if (ctx->is_hardware_initialized)
    {
        xUSBD_LOG(0x1001, "USB_DCD_Init: already initialized");
        return xRETURN_OK;
    }

    int32_t socStatus;
    int status;

    // ===== Enable USB0 Module Clock =====
    xUSBD_LOG(0x10E0, "USB_DCD: enabling USB0 module clock...\r\n");
    socStatus = SOC_moduleClockEnable(TISCI_DEV_USB0, 1u);
    if (socStatus != SystemP_SUCCESS)
    {
        xUSBD_LOG(0x10E0, "USB_DCD: failed to enable USB0 module clock (%d)\r\n", socStatus);
        return xRETURN_xERR_xUSBD_DCD_INVALID_PORT;
    }

    SOC_controlModuleUnlockMMR(SOC_DOMAIN_ID_MAIN, 0);
    SOC_controlModuleUnlockMMR(SOC_DOMAIN_ID_MAIN, 1);
    SOC_controlModuleUnlockMMR(SOC_DOMAIN_ID_MAIN, 2);

    if (speed == USB_SPEED_SUPER)
    {
        if (usb3_phy_init() != 0)
        {
            xUSBD_LOG(0x10E0, "USB_DCD: USB3 PHY initialization failed\r\n");
            return xRETURN_xERR_xUSBD_DCD_INVALID_PORT;
        }
    }

    HW_WR_FIELD32(&p_usbregs_tiwrap->USB3P0SS_W1, CSL_USB3P0SS_CMN_USB3P0SS_W1_PWRUP_RST_N, 0);
    HW_WR_FIELD32(&p_usbregs_tiwrap->USB3P0SS_W1, CSL_USB3P0SS_CMN_USB3P0SS_W1_MODESTRAP_SEL, 1);
    HW_WR_FIELD32(&p_usbregs_tiwrap->USB3P0SS_W1, CSL_USB3P0SS_CMN_USB3P0SS_W1_MODESTRAP, 2);
    HW_WR_FIELD32(&p_usbregs_tiwrap->USB3P0SS_W1, CSL_USB3P0SS_CMN_USB3P0SS_W1_USB2_ONLY_MODE, (speed != USB_SPEED_SUPER));
    HW_WR_FIELD32(&p_usbregs_tiwrap->STATIC_CONFIG, CSL_USB3P0SS_CMN_STATIC_CONFIG_VBUS_SEL, 1);
    HW_WR_FIELD32(&p_usbregs_tiwrap->STATIC_CONFIG, CSL_USB3P0SS_CMN_STATIC_CONFIG_PLL_REF_SEL, 0x06);
    HW_WR_FIELD32(&p_usbregs_tiwrap->USB3P0SS_W1, CSL_USB3P0SS_CMN_USB3P0SS_W1_PWRUP_RST_N, 1);
    HW_WR_REG32(&p_usbregs_ctlr->DRD.SESSVALID_DBNC_CFG, 0x000A0002);

    while (HW_RD_FIELD32(&p_usbregs_ctlr->DRD.OTGSTS, CSL_USB3P0SS_CTRL_DRD_OTGSTS_OTG_NRDY))
        ;
    while (!HW_RD_FIELD32(&p_usbregs_ctlr->DRD.OTGSTS, CSL_USB3P0SS_CTRL_DRD_OTGSTS_DEV_READY))
        ;

    HW_WR_FIELD32(&p_usbregs_ctlr->DRD.OVERRIDE, CSL_USB3P0SS_CTRL_DRD_OVERRIDE_BC_PULLDOWNCTRL, 0x1);

    if (speed != USB_SPEED_SUPER)
    {
        HW_WR_FIELD32(&p_usbregs_ctlr->DEV.USB_CONF, CSL_USB3P0SS_CTRL_DEV_USB_CONF_USB3DIS, 0x1);
    }

    SOC_controlModuleLockMMR(SOC_DOMAIN_ID_MAIN, 0);
    SOC_controlModuleLockMMR(SOC_DOMAIN_ID_MAIN, 1);
    SOC_controlModuleLockMMR(SOC_DOMAIN_ID_MAIN, 2);

    Config.regBase = (uintptr_t)&p_usbregs_ctlr->DEV;
    for (uint32_t i = 0U; i < CUSBD_NUM_EP_IN; i++)
    {
        Config.epIN[i].bufferingValue = 1;
    }
    for (uint32_t i = 0U; i < CUSBD_NUM_EP_OUT; i++)
    {
        Config.epOUT[i].bufferingValue = 1;
    }
    Config.extendedTBCMode = 0;
    Config.dmultEnabled = 0;
    Config.dmaInterfaceWidth = CUSBD_DMA_64_WIDTH;
    Config.preciseBurstLength = CUSBD_PRECISE_BURST_0;
    Config.epMemRes = &epMemRes;
    Config.forcedUsbMode = (speed == USB_SPEED_SUPER) ? 3 : (speed == USB_SPEED_HIGH ? 2 : 0);
    Config.didRegPtr = (uintptr_t) & (p_usbregs_ctlr->DRD.CDNS_DID);
    Config.ridRegPtr = (uintptr_t) & (p_usbregs_ctlr->DRD.CDNS_RID);
    Config.setupPacket = &setupRequestPacket;
    Config.setupPacketDma = (uintptr_t)&setupRequestPacket;

    for (uint32_t i = 0U; i < DATA_XFER_BUFFER_COUNT; i++)
    {
        dataXferBufferOut[i] = buffEpOut[i];
        dataXferBufferIn[i] = buffEpIn[i];
    }

    memset(&buff[0][0], 0, sizeof(CUSBDMA_DmaTrb) * 32 * 8);
    for (uint32_t i = 0; i < 32; i++)
    {
        epMemRes[i].memPageIndex = 0;
        epMemRes[i].trbAddr = (CUSBDMA_DmaTrb *)buff[i];
        epMemRes[i].trbBufferSize = 8;
        epMemRes[i].trbDmaAddr = (uintptr_t)buff[i];
    }

    ObjCusbd = CUSBD_GetInstance();
    if (ObjCusbd == NULL)
    {
        xUSBD_LOG(0x10E0, "USB_DCD: failed to get Cadence driver instance\r\n");
        return xRETURN_xERR_xUSBD_DCD_INVALID_PORT;
    }

    status = ObjCusbd->probe(&Config, &sysRequestCusbd);
    if (status != 0)
    {
        xUSBD_LOG(0x10E0, "USB_DCD: Cadence probe failed (%d)\r\n", status);
        return xRETURN_xERR_xUSBD_DCD_INVALID_PORT;
    }

    if (PRIVATE_DATA_MEM >= (sysRequestCusbd.privDataSize + sysRequestCusbd.trbMemSize))
    {
        pD = (CUSBD_PrivateData *)alignedBufferCusbd;
        memset(pD, 0, sysRequestCusbd.privDataSize);
    }
    else
    {
        xUSBD_LOG(0x10E0, "USB_DCD: insufficient memory for private data\r\n");
        return xRETURN_xERR_xUSBD_DCD_INVALID_PORT;
    }

    status = ObjCusbd->init(pD, &Config, &Callbacks);
    if (status != 0)
    {
        xUSBD_LOG(0x10E0, "USB_DCD: Cadence init failed (%d)\r\n", status);
        return xRETURN_xERR_xUSBD_DCD_INVALID_PORT;
    }

    HwiP_Params_init(&hwiParamsUsb);
    hwiParamsUsb.intNum = CSLR_R5FSS0_CORE0_INTR_USB0_IRQ_6;
    hwiParamsUsb.callback = usb_irq6_isr;
    hwiParamsUsb.isPulse = 0;
    hwiParamsUsb.isFIQ = 0;
    HwiP_construct(&hwiObjUsb, &hwiParamsUsb);

    ctx->is_hardware_initialized = true;
    xUSBD_LOG(0x10E0, "USB_DCD: initialization complete\r\n");

    return xRETURN_OK;
}

static xRETURN_t am243x_dcd_deinit(void *dcd_ctx)
{
    xUSBD_AM243x_DCD_Context_t *ctx = (xUSBD_AM243x_DCD_Context_t *)dcd_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    xASSERT(ctx->port < xUSBD_DEVICE_MAX_INSTANCE, "Port out of range");
    xUSBD_LOG(0x1002, "USB_DCD_Deinit");

    HwiP_destruct(&hwiObjUsb);

    if (ObjCusbd != NULL && pD != NULL)
    {
        ObjCusbd->stop(pD);
    }

    ctx->is_hardware_initialized = false;
    return xRETURN_OK;
}

static xRETURN_t am243x_dcd_set_event_callback(void *dcd_ctx, xUSBD_DCD_Event_Callback_t callback)
{
    xUSBD_AM243x_DCD_Context_t *ctx = (xUSBD_AM243x_DCD_Context_t *)dcd_ctx;
    if (ctx == NULL || callback == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    xASSERT(callback != NULL, "Event callback is NULL");
    xUSBD_LOG(0x100C, "USB_DCD_Set_Event_Callback: port=%d", ctx->port);

    ctx->event_callback = callback;
    return xRETURN_OK;
}

static xRETURN_t am243x_dcd_connect(void *dcd_ctx)
{
    xUSBD_AM243x_DCD_Context_t *ctx = (xUSBD_AM243x_DCD_Context_t *)dcd_ctx;
    (void)ctx;
    xASSERT(ctx->port < xUSBD_DEVICE_MAX_INSTANCE, "Port out of range");
    xUSBD_LOG(0x1002, "USB_DCD_Connect");

    if (ObjCusbd != NULL && pD != NULL)
    {
        ObjCusbd->start(pD);
    }

    return xRETURN_OK;
}

static xRETURN_t am243x_dcd_disconnect(void *dcd_ctx)
{
    xUSBD_AM243x_DCD_Context_t *ctx = (xUSBD_AM243x_DCD_Context_t *)dcd_ctx;
    (void)ctx;
    xASSERT(ctx->port < xUSBD_DEVICE_MAX_INSTANCE, "Port out of range");
    xUSBD_LOG(0x1003, "USB_DCD_Disconnect");

    if (ObjCusbd != NULL && pD != NULL)
    {
        ObjCusbd->stop(pD);
    }

    return xRETURN_OK;
}

static xRETURN_t am243x_dcd_enable_interrupts(void *dcd_ctx)
{
    xUSBD_AM243x_DCD_Context_t *ctx = (xUSBD_AM243x_DCD_Context_t *)dcd_ctx;
    (void)ctx;
    xASSERT(ctx->port < xUSBD_DEVICE_MAX_INSTANCE, "Port out of range");
    xUSBD_LOG(0x100B, "USB_DCD_Enable_Interrupts: port=%d", ctx->port);

    HwiP_enableInt(CSLR_R5FSS0_CORE0_INTR_USB0_IRQ_6);

    return xRETURN_OK;
}

static xRETURN_t am243x_dcd_disable_interrupts(void *dcd_ctx)
{
    xUSBD_AM243x_DCD_Context_t *ctx = (xUSBD_AM243x_DCD_Context_t *)dcd_ctx;
    (void)ctx;
    xASSERT(ctx->port < xUSBD_DEVICE_MAX_INSTANCE, "Port out of range");
    xUSBD_LOG(0x100B, "USB_DCD_Disable_Interrupts: port=%d", ctx->port);

    HwiP_disableInt(CSLR_R5FSS0_CORE0_INTR_USB0_IRQ_6);

    return xRETURN_OK;
}

static xRETURN_t am243x_dcd_set_address(void *dcd_ctx, uint8_t address)
{
    xUSBD_AM243x_DCD_Context_t *ctx = (xUSBD_AM243x_DCD_Context_t *)dcd_ctx;
    (void)ctx;
    (void)address;
    xASSERT(ctx->port < xUSBD_DEVICE_MAX_INSTANCE, "Port out of range");
    xUSBD_LOG(0x1004, "USB_DCD_Set_Address: %d", address);
#if 0
    if (ObjCusbd != NULL && pD != NULL)
    {
    }
#endif
    return xRETURN_OK;
}

static xRETURN_t am243x_dcd_set_remote_wakeup(void *dcd_ctx, bool enable)
{
    xUSBD_AM243x_DCD_Context_t *ctx = (xUSBD_AM243x_DCD_Context_t *)dcd_ctx;
    (void)ctx;
    (void)enable;
    xASSERT(ctx->port < xUSBD_DEVICE_MAX_INSTANCE, "Port out of range");
    xUSBD_LOG(0x1005, "USB_DCD_Set_Remote_Wakeup: %d", enable);

    return xRETURN_OK;
}

static xRETURN_t am243x_dcd_set_test_mode(void *dcd_ctx, uint8_t mode)
{
    xUSBD_AM243x_DCD_Context_t *ctx = (xUSBD_AM243x_DCD_Context_t *)dcd_ctx;
    (void)ctx;
    (void)mode;
    xASSERT(ctx->port < xUSBD_DEVICE_MAX_INSTANCE, "Port out of range");
    xUSBD_LOG(0x1006, "USB_DCD_Set_Test_Mode: 0x%02X", mode);

    return xRETURN_OK;
}

static uint32_t am243x_dcd_get_frame_number(void *dcd_ctx)
{
    xUSBD_AM243x_DCD_Context_t *ctx = (xUSBD_AM243x_DCD_Context_t *)dcd_ctx;
    (void)ctx;
    xASSERT(ctx->port < xUSBD_DEVICE_MAX_INSTANCE, "Port out of range");
    xUSBD_LOG(0x100A, "USB_DCD_Get_Frame_Number: port=%d", ctx->port);

    uint32_t numOfFrame = 0;
    if (ObjCusbd != NULL && pD != NULL)
    {
        ObjCusbd->dGetFrame(pD, &numOfFrame);
        return numOfFrame;
    }

    return 0;
}

static USB_Speed_t am243x_dcd_get_speed(void *dcd_ctx)
{
    xUSBD_AM243x_DCD_Context_t *ctx = (xUSBD_AM243x_DCD_Context_t *)dcd_ctx;
    (void)ctx;
    xASSERT(ctx->port < xUSBD_DEVICE_MAX_INSTANCE, "Port out of range");
    xUSBD_LOG(0x1009, "USB_DCD_Get_Enumerated_Speed: port=%d", ctx->port);

    if (ObjCusbd != NULL && pD != NULL)
    {
        switch (pD->device.speed)
        {
        case CH9_USB_SPEED_LOW:
            return USB_SPEED_LOW;
        case CH9_USB_SPEED_FULL:
            return USB_SPEED_FULL;
        case CH9_USB_SPEED_HIGH:
            return USB_SPEED_HIGH;
        case CH9_USB_SPEED_SUPER:
            return USB_SPEED_SUPER;
        default:
            break;
        }
    }

    return USB_SPEED_HIGH;
}

static xRETURN_t am243x_dcd_ep_init(void *dcd_ctx, uint8_t ep_addr, uint8_t ep_type, uint16_t mps)
{
    xUSBD_AM243x_DCD_Context_t *ctx = (xUSBD_AM243x_DCD_Context_t *)dcd_ctx;
    xASSERT(ctx->port < xUSBD_DEVICE_MAX_INSTANCE, "Port out of range");
    xUSBD_LOG(0x1007, "USB_DCD_EP_Init: ep=0x%02X, type=%d, mps=%d", ep_addr, ep_type, mps);

    uint8_t ep_number = (ep_addr & 0x7F);
    uint16_t _mps = (mps & 0x7FF);
    uint8_t count = 1 + (mps >> 11);
    xUSBD_AM243x_EP_Handle_t *ep_handle = (ep_addr & 0x80) ? &ctx->in_ep_handles[ep_number] : &ctx->out_ep_handles[ep_number];

    ep_handle->MPS = _mps;
    ep_handle->EP_Type = ep_type;
    if (ep_type == USB_ENDP_TYPE_ISOC)
    {
        ep_handle->Transfers_Per_Microframe = count;
    }

    if (ep_number == 0)
    {
        return xRETURN_OK;
    }

    if (ObjCusbd != NULL && pD != NULL)
    {
        CUSBD_Ep *ep = get_ep_from_address(ep_addr);
        if (ep != NULL)
        {
            struct __attribute__((packed))
            {
                CH9_UsbEndpointDescriptor ep;
                CH9_UsbSSEndpointCompanionDescriptor comp;
            } descriptor = {
                .ep =
                    {
                        .bLength = sizeof(CH9_UsbEndpointDescriptor),
                        .bDescriptorType = CH9_USB_DT_ENDPOINT,
                        .bEndpointAddress = ep_addr,
                        .bmAttributes = ep_type & CH9_USB_EP_TRANSFER_MASK,
                        .wMaxPacketSize = mps,
                        .bInterval = 0,
                    },
                .comp =
                    {
                        .bLength = pD->device.speed >= CH9_USB_SPEED_SUPER ? CH9_USB_DS_SS_USB_EP_COMPANION : 0,
                        .bDescriptorType = pD->device.speed >= CH9_USB_SPEED_SUPER ? CH9_USB_DT_SS_USB_EP_COMPANION : 0,
                        .bMaxBurst = 0,
                        .bmAttributes = 0,
                        .wBytesPerInterval = 0,
                    },
            };
            ep->ops->epEnable(pD, ep, (const uint8_t *)&descriptor);
        }
    }
    else
    {
        xUSBD_LOG(0x10E0, "USB_DCD_EP_Init: endpoint 0x%02X not found\r\n", ep_addr);
        return xRETURN_xERR_xUSBD_DCD_INVALID_ENDPOINT;
    }

    return xRETURN_OK;
}

static xRETURN_t am243x_dcd_ep_deinit(void *dcd_ctx, uint8_t ep_addr)
{
    xUSBD_AM243x_DCD_Context_t *ctx = (xUSBD_AM243x_DCD_Context_t *)dcd_ctx;
    (void)ctx;
    xASSERT(ctx->port < xUSBD_DEVICE_MAX_INSTANCE, "Port out of range");
    xUSBD_LOG(0x1007, "USB_DCD_EP_Deinit: ep=0x%02X", ep_addr);

    if (ObjCusbd != NULL && pD != NULL)
    {
        CUSBD_Ep *ep = get_ep_from_address(ep_addr);
        if (ep != NULL)
        {
            ep->ops->epDisable(pD, ep);
        }
    }

    return xRETURN_OK;
}

static xRETURN_t am243x_dcd_ep_receive(void *dcd_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    xUSBD_AM243x_DCD_Context_t *ctx = (xUSBD_AM243x_DCD_Context_t *)dcd_ctx;
    xASSERT(ctx->port < xUSBD_DEVICE_MAX_INSTANCE, "Port out of range");
    xUSBD_LOG(0x1008, "USB_DCD_EP_Prepare_To_Receive: ep=0x%02X, length=%d", ep_addr, length);

    uint8_t ep_number = (ep_addr & 0x7F);
    xUSBD_AM243x_EP_Handle_t *ep_handle = &ctx->out_ep_handles[ep_number];

    ep_handle->Data = data;
    ep_handle->Current_Data = data;
    ep_handle->Actual_XFER_Length = 0x00;
    ep_handle->Remaining_XFER_Length = length;

    if (ObjCusbd != NULL && pD != NULL)
    {
        if (ep_number == 0 && (pD->ep0NextState == CH9_EP0_SETUP_PHASE || pD->ep0NextState == CH9_EP0_UNCONNECTED))
        {
            return xRETURN_xERR_xUSBD_DCD_EP0_IN_SETUP_PHASE;
        }
        CUSBD_Ep *ep = get_ep_from_address(ep_addr);
        if (ep == NULL)
        {
            return xRETURN_xERR_xUSBD_DCD_INVALID_ENDPOINT;
        }

        uint8_t *dma_buffer = ep_number ? buffEpOut[ep_number - 1] : buffEp0Out;
        uint32_t buffer_size = ep_number ? DATA_OUT_XFER_BUFFER_SIZE : sizeof(buffEp0Out);
        uint32_t chunk = (length < buffer_size) ? length : buffer_size;
        CUSBD_Req *request = &DataXferRequestsOut[ep_number];

        memset(request, 0, sizeof(CUSBD_Req));
        request->buf = data;
        request->dma = (uintptr_t)dma_buffer;
        request->length = chunk;
        request->complete = outepxfercmplcb;

        if (ep_number == 0)
        {
            request->deferStatusStage = 1;
            pD->ep0NextState = CH9_EP0_DATA_PHASE;
            pD->ep0DataDirFlag = 0;
        }

        ep->ops->reqQueue(pD, ep, request);
    }

    return xRETURN_OK;
}

static xRETURN_t am243x_dcd_ep_send(void *dcd_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length, bool is_zlp_required)
{
    xUSBD_AM243x_DCD_Context_t *ctx = (xUSBD_AM243x_DCD_Context_t *)dcd_ctx;
    xASSERT(ctx->port < xUSBD_DEVICE_MAX_INSTANCE, "Port out of range");
    xUSBD_LOG(0x1008, "USB_DCD_EP_Prepare_To_Send: ep=0x%02X, length=%d, zlp=%d", ep_addr, length, is_zlp_required);

    uint8_t ep_number = (ep_addr & 0x7F);
    xUSBD_AM243x_EP_Handle_t *ep_handle = &ctx->in_ep_handles[ep_number];

    ep_handle->Send_ZLP = is_zlp_required;
    ep_handle->Data = data;
    ep_handle->Current_Data = data;
    ep_handle->Remaining_XFER_Length = length;
    ep_handle->Actual_XFER_Length = 0x00;

    if (ObjCusbd != NULL && pD != NULL)
    {
        if (ep_number == 0 && (pD->ep0NextState == CH9_EP0_SETUP_PHASE || pD->ep0NextState == CH9_EP0_UNCONNECTED))
        {
            return xRETURN_xERR_xUSBD_DCD_EP0_IN_SETUP_PHASE;
        }
        CUSBD_Ep *ep = get_ep_from_address(ep_addr);
        if (ep == NULL)
        {
            return xRETURN_xERR_xUSBD_DCD_INVALID_ENDPOINT;
        }

        uint8_t *dma_buffer = ep_number ? buffEpIn[ep_number - 1] : buffEp0In;
        uint32_t buffer_size = ep_number ? DATA_IN_XFER_BUFFER_SIZE : sizeof(buffEp0In);
        uint32_t chunk = (length < buffer_size) ? length : buffer_size;
        CUSBD_Req *request = &DataXferRequestsIn[ep_number];

        memset(request, 0, sizeof(CUSBD_Req));
        request->buf = data;
        request->zero = (is_zlp_required && length == chunk) ? 1 : 0;
        memcpy(dma_buffer, data, chunk);
        request->dma = (uintptr_t)dma_buffer;
        request->length = chunk;
        request->complete = inepxfercmplcb;

        if (ep_number == 0)
        {
            request->deferStatusStage = 1;
            pD->ep0NextState = (length > 0) ? CH9_EP0_DATA_PHASE : CH9_EP0_STATUS_PHASE;
            pD->ep0DataDirFlag = 1;
        }

        ep->ops->reqQueue(pD, ep, request);
    }

    return xRETURN_OK;
}

static xRETURN_t am243x_dcd_ep_stall(void *dcd_ctx, uint8_t ep_addr)
{
    xUSBD_AM243x_DCD_Context_t *ctx = (xUSBD_AM243x_DCD_Context_t *)dcd_ctx;
    (void)ctx;
    xASSERT(ctx->port < xUSBD_DEVICE_MAX_INSTANCE, "Port out of range");
    xUSBD_LOG(0x100B, "USB_DCD_EP_Set_Stall: ep=0x%02X", ep_addr);

    if (ObjCusbd != NULL && pD != NULL)
    {
        CUSBD_Ep *ep = get_ep_from_address(ep_addr);
        if (ep != NULL)
        {
            ObjCusbd->epSetHalt(pD, ep, 1U);
        }
    }

    return xRETURN_OK;
}

static xRETURN_t am243x_dcd_ep_clear_stall(void *dcd_ctx, uint8_t ep_addr)
{
    xUSBD_AM243x_DCD_Context_t *ctx = (xUSBD_AM243x_DCD_Context_t *)dcd_ctx;
    (void)ctx;
    xASSERT(ctx->port < xUSBD_DEVICE_MAX_INSTANCE, "Port out of range");
    xUSBD_LOG(0x100C, "USB_DCD_EP_Clear_Stall: ep=0x%02X", ep_addr);

    if (ObjCusbd != NULL && pD != NULL)
    {
        CUSBD_Ep *ep = get_ep_from_address(ep_addr);
        if (ep != NULL)
        {
            ObjCusbd->epSetHalt(pD, ep, 0U);
        }
    }

    return xRETURN_OK;
}

static bool am243x_dcd_ep_is_stalled(void *dcd_ctx, uint8_t ep_addr)
{
    xUSBD_AM243x_DCD_Context_t *ctx = (xUSBD_AM243x_DCD_Context_t *)dcd_ctx;
    (void)ctx;
    xASSERT(ctx->port < xUSBD_DEVICE_MAX_INSTANCE, "Port out of range");
    xUSBD_LOG(0x100D, "USB_DCD_EP_Get_Stall: ep=0x%02X", ep_addr);

    if (ObjCusbd != NULL && pD != NULL)
    {
        CUSBD_Ep *ep = get_ep_from_address(ep_addr);
        if (ep != NULL && ep->epPrivate->ep_state == CUSBD_EP_STALLED)
        {
            return true;
        }
    }

    return false;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xUSBD_DCD_Ops_t xUSBD_AM243x_DCD_Ops = {
    .init = am243x_dcd_init,
    .deinit = am243x_dcd_deinit,
    .set_event_callback = am243x_dcd_set_event_callback,
    .connect = am243x_dcd_connect,
    .disconnect = am243x_dcd_disconnect,
    .enable_interrupts = am243x_dcd_enable_interrupts,
    .disable_interrupts = am243x_dcd_disable_interrupts,
    .set_address = am243x_dcd_set_address,
    .set_remote_wakeup = am243x_dcd_set_remote_wakeup,
    .set_test_mode = am243x_dcd_set_test_mode,
    .get_frame_number = am243x_dcd_get_frame_number,
    .get_speed = am243x_dcd_get_speed,
    .ep_init = am243x_dcd_ep_init,
    .ep_deinit = am243x_dcd_ep_deinit,
    .ep_receive = am243x_dcd_ep_receive,
    .ep_send = am243x_dcd_ep_send,
    .ep_stall = am243x_dcd_ep_stall,
    .ep_clear_stall = am243x_dcd_ep_clear_stall,
    .ep_is_stalled = am243x_dcd_ep_is_stalled,
};
// EOF /////////////////////////////////////////////////////////////////////////////

