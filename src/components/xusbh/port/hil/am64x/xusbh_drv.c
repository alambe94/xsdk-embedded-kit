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

// @file xusbh_drv.c
// @brief AM64x USB Host Controller Driver port scaffold.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// SYSTEM INCLUDES
#if defined(xUSBH_AM64X_HCD_ENABLE_MCU_PLUS_SDK)
#include <drivers/hw_include/cslr_soc.h>
#include <drivers/hw_include/csl_types.h>
#include <drivers/hw_include/hw_types.h>
#include <drivers/sciclient/include/tisci/am64x_am243x/tisci_devices.h>
#include <drivers/soc.h>
#include <kernel/dpl/CacheP.h>
#include <kernel/dpl/SystemP.h>
#include <drivers/../usb/cdn/core_driver/common/include/cdn_errno.h>
#include <drivers/../usb/cdn/core_driver/common/include/cusb_ch9_if.h>
#include <drivers/../usb/cdn/core_driver/host/include/cdn_xhci_obj_if.h>
#include <drivers/../usb/cdn/core_driver/host/include/cdn_xhci_structs_if.h>
#include <drivers/../usb/cdn/core_driver/host/src/cdn_xhci_internal.h>
#include <drivers/../usb/cdn/core_driver/host/src/cdn_xhci_priv.h>
#include <drivers/../usb/cdn/core_driver/host/src/trb.h>
#include <drivers/../usb/cdn/soc/am64x_am243x/cslr_usb3p0ss_v5_2.h>
#if defined(xUSBH_AM64X_HCD_ENABLE_SUPER_SPEED)
#include <drivers/hw_include/am64x_am243x/cslr_main_ctrl_mmr.h>
#include "am64x_phy.h"
#endif
#endif

// MODULE INCLUDES
#include "xusbh_core.h"
#include "xusbh_drv.h"

// MACROS /////////////////////////////////////////////////////////////////////////
#define xUSBH_AM64X_HCD_USBSSP_ROOT_PORT                1U
#define xUSBH_AM64X_HCD_MODESTRAP_HOST                  1U
#define xUSBH_AM64X_HCD_READY_TIMEOUT_CYCLES            1000000U
#define xUSBH_AM64X_HCD_USB3_REFCLK_HZ                  100000000U
#define xUSBH_AM64X_HCD_USB3_LANE_FUNC_USB              1U
#define xUSBH_AM64X_HCD_SERDES0_CLKSEL_MAIN_PLL2_HSDIV4 3U
#define xUSBH_AM64X_HCD_ENDPOINT_DESC_SIZE              7U
#define xUSBH_AM64X_HCD_ENDPOINT_ADDR_EP0               0U

#if defined(xUSBH_AM64X_HCD_ENABLE_MCU_PLUS_SDK)
#define xUSBH_AM64X_HCD_WRITE_REG_FIELD32(reg_addr, reg_field, value)                                                                      \
    am64x_hcd_write_field32((uintptr_t)(reg_addr), (uint32_t)reg_field##_MASK, (uint32_t)reg_field##_SHIFT, (uint32_t)(value))
#define xUSBH_AM64X_HCD_READ_REG_FIELD32(reg_addr, reg_field)                                                                              \
    am64x_hcd_read_field32((uintptr_t)(reg_addr), (uint32_t)reg_field##_MASK, (uint32_t)reg_field##_SHIFT)
#endif

// TYPES //////////////////////////////////////////////////////////////////////////
#if defined(xUSBH_AM64X_HCD_ENABLE_MCU_PLUS_SDK)
typedef struct am64x_xhci_memory_t
{
    uint8_t scratchpad_pool[USBSSP_SCRATCHPAD_BUFF_NUM * USBSSP_PAGE_SIZE] __attribute__((aligned(USBSSP_PAGE_SIZE)));
    USBSSP_RingElementT event_pool[USBSSP_MAX_NUM_INTERRUPTERS][USBSSP_EVENT_QUEUE_SIZE] __attribute__((aligned(1024)));
    USBSSP_RingElementT ep_ring_pool[USBSSP_PRODUCER_QUEUE_SIZE * (USBSSP_MAX_EP_CONTEXT_NUM + 2U)] __attribute__((aligned(1024)));
    USBSSP_DcbaaT dcbaa __attribute__((aligned(USBSSP_DCBAA_ALIGNMENT)));
    USBSSP_InputContexT input_context __attribute__((aligned(1024)));
    USBSSP_OutputContexT output_context __attribute__((aligned(1024)));
    uint64_t scratchpad[USBSSP_SCRATCHPAD_BUFF_NUM + 1U] __attribute__((aligned(64)));
    uint64_t event_ring_segment_entry[USBSSP_MAX_NUM_INTERRUPTERS * USBSSP_ERST_ENTRY_SIZE] __attribute__((aligned(USBSSP_ERST_ALIGNMENT)));
    USBSSP_ProducerQueueT stream_memory_pool[USBSSP_MAX_EP_NUM_STRM_EN][USBSSP_STREAM_ARRAY_SIZE];
    USBSSP_RingElementT stream_ring[USBSSP_MAX_EP_NUM_STRM_EN][USBSSP_STREAM_ARRAY_SIZE][USBSSP_PRODUCER_QUEUE_SIZE]
        __attribute__((aligned(1024)));
    uint8_t ep0_buffer[USBSSP_EP0_DATA_BUFF_SIZE] __attribute__((aligned(64)));
} am64x_xhci_memory_t;

typedef struct am64x_hcd_endpoint_state_t
{
    xUSBH_Transfer_t *transfer;
    uint8_t endpoint_address;
    bool is_enabled;
    bool is_configuring;
} am64x_hcd_endpoint_state_t;
#endif

// VARIABLES //////////////////////////////////////////////////////////////////////
xUSBH_AM64x_HCD_Context_t xUSBH_AM64x_HCD_Context = {0};

#if defined(xUSBH_AM64X_HCD_ENABLE_MCU_PLUS_SDK)
static CSL_usb3p0ss_cmnRegs *p_usbregs_tiwrap = (CSL_usb3p0ss_cmnRegs *)CSL_USB0_MMR_MMRVBP_USBSS_CMN_BASE;
static CSL_usb3p0ss_ctrlRegs *p_usbregs_ctlr = (CSL_usb3p0ss_ctrlRegs *)CSL_USB0_VBP2APB_WRAP_CONTROLLER_VBP_CORE_ADDR_MAP_BASE;
static USBSSP_OBJ *p_usbssp_obj = NULL;
static USBSSP_DriverResourcesT usbssp_resources __attribute__((aligned(USBSSP_PAGE_SIZE), section(".bss.nocache")));
static USBSSP_XhciResourcesT usbssp_mem_resources __attribute__((aligned(USBSSP_CONTEXT_ALIGNMENT), section(".bss.nocache")));
static am64x_xhci_memory_t xhci_memory __attribute__((section(".bss.nocache")));
static am64x_hcd_endpoint_state_t endpoint_states[USBSSP_EP_CONT_MAX];
#endif

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static bool am64x_hcd_ctx_is_valid(const xUSBH_AM64x_HCD_Context_t *ctx);
#if defined(xUSBH_AM64X_HCD_ENABLE_MCU_PLUS_SDK)
static xRETURN_t am64x_hcd_notify_port_event(xUSBH_AM64x_HCD_Context_t *ctx, xUSBH_HCD_Port_Event_t event);
static xRETURN_t
am64x_hcd_notify_transfer_event(xUSBH_AM64x_HCD_Context_t *ctx, xUSBH_Transfer_t *transfer, xUSBH_HCD_Transfer_Event_t event);
#endif
static xRETURN_t am64x_hcd_init(void *hcd_ctx, void *host_ctx, xUSBH_HCD_Event_Callback_t callback);
static xRETURN_t am64x_hcd_deinit(void *hcd_ctx);
static xRETURN_t am64x_hcd_start(void *hcd_ctx);
static xRETURN_t am64x_hcd_stop(void *hcd_ctx);
static xRETURN_t am64x_hcd_enable_interrupts(void *hcd_ctx);
static xRETURN_t am64x_hcd_disable_interrupts(void *hcd_ctx);
static xRETURN_t am64x_hcd_port_power(void *hcd_ctx, uint8_t port, bool enable);
static xRETURN_t am64x_hcd_port_reset(void *hcd_ctx, uint8_t port);
static xRETURN_t am64x_hcd_get_port_status(void *hcd_ctx, uint8_t port, xUSBH_HCD_Port_Status_t *status);
static xRETURN_t am64x_hcd_submit_transfer(void *hcd_ctx, xUSBH_Transfer_t *transfer);
static xRETURN_t am64x_hcd_cancel_transfer(void *hcd_ctx, xUSBH_Transfer_t *transfer);
static uint32_t am64x_hcd_get_frame_number(void *hcd_ctx);
#if defined(xUSBH_AM64X_HCD_ENABLE_MCU_PLUS_SDK)
static uint32_t am64x_hcd_read32(uintptr_t address);
static void am64x_hcd_write32(uintptr_t address, uint32_t value);
static uint32_t am64x_hcd_read_field32(uintptr_t address, uint32_t mask, uint32_t shift);
static void am64x_hcd_write_field32(uintptr_t address, uint32_t mask, uint32_t shift, uint32_t value);
static xRETURN_t am64x_hcd_status_from_cdn(uint32_t status);
static USB_Speed_t am64x_hcd_speed_from_ch9(CH9_UsbSpeed speed);
static void am64x_hcd_configure_xhci_memory(void);
static bool am64x_hcd_is_super_speed_enabled(void);
static uint8_t am64x_hcd_endpoint_index_from_address(uint8_t endpoint_address);
static xRETURN_t am64x_hcd_transfer_endpoint_index(xUSBH_AM64x_HCD_Context_t *ctx, xUSBH_Transfer_t *transfer, uint8_t *ep_index);
static xRETURN_t am64x_hcd_find_device(const xUSBH_Context_t *host_ctx,
                                       uint8_t device_address,
                                       const xUSBH_Device_Context_t **device_ctx,
                                       uint8_t *device_index);
static xRETURN_t am64x_hcd_find_endpoint(const xUSBH_Context_t *host_ctx,
                                         uint8_t device_address,
                                         uint8_t endpoint_address,
                                         const xUSBH_Endpoint_Context_t **endpoint_ctx);
static void am64x_hcd_ch9_setup_from_transfer(const xUSBH_Transfer_t *transfer, CH9_UsbSetup *setup);
static void am64x_hcd_endpoint_descriptor_build(const xUSBH_Endpoint_Context_t *endpoint_ctx,
                                                uint8_t descriptor[xUSBH_AM64X_HCD_ENDPOINT_DESC_SIZE]);
static xRETURN_t am64x_hcd_cache_before_submit(const xUSBH_Transfer_t *transfer);
static void am64x_hcd_cache_after_complete(const xUSBH_Transfer_t *transfer);
static xUSBH_HCD_Transfer_Event_t am64x_hcd_transfer_event_from_cdn(uint32_t status, const USBSSP_RingElementT *event_ptr);
static void am64x_hcd_transfer_actual_length_set(xUSBH_Transfer_t *transfer, const USBSSP_RingElementT *event_ptr);
static void am64x_hcd_transfer_complete_common(USBSSP_DriverResourcesT *resources,
                                               uint32_t status,
                                               const USBSSP_RingElementT *event_ptr,
                                               uint8_t ep_index);
static void am64x_hcd_control_complete(USBSSP_DriverResourcesT *resources, uint32_t status, const USBSSP_RingElementT *event_ptr);
static void am64x_hcd_data_complete(USBSSP_DriverResourcesT *resources, uint32_t status, const USBSSP_RingElementT *event_ptr);
static xRETURN_t am64x_hcd_config_descriptor_seed(xUSBH_AM64x_HCD_Context_t *ctx, const xUSBH_Transfer_t *transfer);
static xRETURN_t am64x_hcd_enable_configured_endpoints(xUSBH_AM64x_HCD_Context_t *ctx, const xUSBH_Transfer_t *transfer);
static void am64x_hcd_issue_configure_endpoint_command(void);
static void am64x_hcd_configure_complete_poll(xUSBH_AM64x_HCD_Context_t *ctx);
#if defined(xUSBH_AM64X_HCD_ENABLE_SUPER_SPEED)
static xRETURN_t am64x_hcd_enable_usb3_phy(void);
#endif
static xRETURN_t am64x_hcd_wait_for_host_ready(void);
static xRETURN_t am64x_hcd_enable_usb0_host(void);
static xRETURN_t am64x_hcd_init_usbssp(xUSBH_AM64x_HCD_Context_t *ctx);
static xRETURN_t am64x_hcd_poll_port_connection(xUSBH_AM64x_HCD_Context_t *ctx, bool notify);
#endif

// MODULE FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////
static bool am64x_hcd_ctx_is_valid(const xUSBH_AM64x_HCD_Context_t *ctx)
{
    return (ctx != NULL) && (ctx->port == xUSBH_AM64X_HCD_USB0_PORT);
}

#if defined(xUSBH_AM64X_HCD_ENABLE_MCU_PLUS_SDK)
static uint32_t am64x_hcd_read32(uintptr_t address)
{
    return *(volatile const uint32_t *)address;
}

static void am64x_hcd_write32(uintptr_t address, uint32_t value)
{
    *(volatile uint32_t *)address = value;
}

static uint32_t am64x_hcd_read_field32(uintptr_t address, uint32_t mask, uint32_t shift)
{
    return (am64x_hcd_read32(address) & mask) >> shift;
}

static void am64x_hcd_write_field32(uintptr_t address, uint32_t mask, uint32_t shift, uint32_t value)
{
    uint32_t reg_value = am64x_hcd_read32(address);

    reg_value &= ~mask;
    reg_value |= (value << shift) & mask;
    am64x_hcd_write32(address, reg_value);
}

static xRETURN_t am64x_hcd_notify_port_event(xUSBH_AM64x_HCD_Context_t *ctx, xUSBH_HCD_Port_Event_t event)
{
    xUSBH_HCD_Event_t hcd_event = {0};

    if ((ctx == NULL) || (ctx->event_callback == NULL) || (ctx->host_ctx == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    hcd_event.type = xUSBH_HCD_EVENT_TYPE_PORT;
    hcd_event.port = ctx->port;
    hcd_event.port_event = event;
    ctx->event_callback(ctx->host_ctx, &hcd_event);

    return xRETURN_OK;
}

static xRETURN_t
am64x_hcd_notify_transfer_event(xUSBH_AM64x_HCD_Context_t *ctx, xUSBH_Transfer_t *transfer, xUSBH_HCD_Transfer_Event_t event)
{
    xUSBH_HCD_Event_t hcd_event = {0};

    if ((ctx == NULL) || (ctx->event_callback == NULL) || (ctx->host_ctx == NULL) || (transfer == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    hcd_event.type = xUSBH_HCD_EVENT_TYPE_TRANSFER;
    hcd_event.port = ctx->port;
    hcd_event.transfer_event = event;
    hcd_event.transfer = transfer;
    ctx->event_callback(ctx->host_ctx, &hcd_event);

    return xRETURN_OK;
}

static xRETURN_t am64x_hcd_status_from_cdn(uint32_t status)
{
    xRETURN_t ret = xRETURN_xERR_xUSBH_INVALID_STATE;

    if (status == CDN_EOK)
    {
        ret = xRETURN_OK;
    }
    else if (status == CDN_ETIMEDOUT)
    {
        ret = xRETURN_xERR_xUSBH_TIMEOUT;
    }

    return ret;
}

static USB_Speed_t am64x_hcd_speed_from_ch9(CH9_UsbSpeed speed)
{
    USB_Speed_t mapped_speed = USB_SPEED_HIGH;

    switch (speed)
    {
    case CH9_USB_SPEED_LOW:
        mapped_speed = USB_SPEED_LOW;
        break;
    case CH9_USB_SPEED_FULL:
        mapped_speed = USB_SPEED_FULL;
        break;
    case CH9_USB_SPEED_HIGH:
        mapped_speed = USB_SPEED_HIGH;
        break;
    case CH9_USB_SPEED_SUPER:
    case CH9_USB_SPEED_SUPER_PLUS:
        mapped_speed = USB_SPEED_SUPER;
        break;
    default:
        mapped_speed = USB_SPEED_HIGH;
        break;
    }

    return mapped_speed;
}

static uint8_t am64x_hcd_endpoint_index_from_address(uint8_t endpoint_address)
{
    uint8_t endpoint_number = endpoint_address & USB_ENDP_ADDR_MASK;
    uint8_t is_in = ((endpoint_address & USB_ENDP_DIR_MASK) != 0U) ? 1U : 0U;

    if (endpoint_number == 0U)
    {
        return USBSSP_EP0_CONT_OFFSET;
    }

    return (uint8_t)((((endpoint_number - 1U) * 2U) + is_in) + USBSSP_EP_CONT_OFFSET);
}

static xRETURN_t am64x_hcd_find_device(const xUSBH_Context_t *host_ctx,
                                       uint8_t device_address,
                                       const xUSBH_Device_Context_t **device_ctx,
                                       uint8_t *device_index)
{
    if ((host_ctx == NULL) || (device_ctx == NULL) || (device_index == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    for (uint8_t i = 0U; i < xUSBH_MAX_DEVICES; i++)
    {
        if ((host_ctx->devices[i].is_allocated == true) && (host_ctx->devices[i].address == device_address))
        {
            *device_ctx = &host_ctx->devices[i];
            *device_index = i;
            return xRETURN_OK;
        }
    }

    return xRETURN_xERR_xUSBH_INVALID_OBJECT;
}

static xRETURN_t am64x_hcd_find_endpoint(const xUSBH_Context_t *host_ctx,
                                         uint8_t device_address,
                                         uint8_t endpoint_address,
                                         const xUSBH_Endpoint_Context_t **endpoint_ctx)
{
    const xUSBH_Device_Context_t *device_ctx = NULL;
    uint8_t device_index = 0U;
    xRETURN_t ret;

    if (endpoint_ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    ret = am64x_hcd_find_device(host_ctx, device_address, &device_ctx, &device_index);
    if (ret != xRETURN_OK)
    {
        return ret;
    }

    (void)device_ctx;
    for (uint8_t i = 0U; i < xUSBH_MAX_ENDPOINTS; i++)
    {
        if ((host_ctx->endpoints[i].is_allocated == true) && (host_ctx->endpoints[i].device_index == device_index) &&
            (host_ctx->endpoints[i].endpoint_address == endpoint_address))
        {
            *endpoint_ctx = &host_ctx->endpoints[i];
            return xRETURN_OK;
        }
    }

    return xRETURN_xERR_xUSBH_INVALID_OBJECT;
}

static xRETURN_t am64x_hcd_transfer_endpoint_index(xUSBH_AM64x_HCD_Context_t *ctx, xUSBH_Transfer_t *transfer, uint8_t *ep_index)
{
    const xUSBH_Endpoint_Context_t *endpoint_ctx = NULL;
    uint8_t index;

    if ((ctx == NULL) || (transfer == NULL) || (ep_index == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if ((transfer->endpoint_address == xUSBH_AM64X_HCD_ENDPOINT_ADDR_EP0) || (transfer->endpoint_type == USB_ENDP_TYPE_CTRL))
    {
        *ep_index = USBSSP_EP0_CONT_OFFSET;
        return xRETURN_OK;
    }

    if (am64x_hcd_find_endpoint((const xUSBH_Context_t *)ctx->host_ctx, transfer->device_address, transfer->endpoint_address,
                                &endpoint_ctx) != xRETURN_OK)
    {
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }

    index = am64x_hcd_endpoint_index_from_address(endpoint_ctx->endpoint_address);
    if ((index < USBSSP_EP_CONT_OFFSET) || (index >= USBSSP_EP_CONT_MAX))
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    *ep_index = index;
    return xRETURN_OK;
}

static void am64x_hcd_ch9_setup_from_transfer(const xUSBH_Transfer_t *transfer, CH9_UsbSetup *setup)
{
    setup->bmRequestType = transfer->setup.bRequestType;
    setup->bRequest = transfer->setup.bRequest;
    setup->wValue = transfer->setup.wValue;
    setup->wIndex = transfer->setup.wIndex;
    setup->wLength = transfer->setup.wLength;
}

static void am64x_hcd_endpoint_descriptor_build(const xUSBH_Endpoint_Context_t *endpoint_ctx,
                                                uint8_t descriptor[xUSBH_AM64X_HCD_ENDPOINT_DESC_SIZE])
{
    uint16_t max_packet_size = endpoint_ctx->max_packet_size;

    descriptor[0] = USB_ENDPOINT_DESC_LEN;
    descriptor[1] = USB_DESC_TYPE_ENDPOINT;
    descriptor[2] = endpoint_ctx->endpoint_address;
    descriptor[3] = endpoint_ctx->endpoint_type & USB_ENDP_TYPE_MASK;
    descriptor[4] = (uint8_t)(max_packet_size & 0xFFU);
    descriptor[5] = (uint8_t)((max_packet_size >> 8U) & 0xFFU);
    descriptor[6] = endpoint_ctx->interval;
}

static xRETURN_t am64x_hcd_cache_before_submit(const xUSBH_Transfer_t *transfer)
{
    if ((transfer == NULL) || (transfer->data == NULL) || (transfer->length == 0U))
    {
        return xRETURN_OK;
    }

    if ((transfer->endpoint_type == USB_ENDP_TYPE_CTRL) && ((transfer->setup.bRequestType & USB_REQ_TYPE_IN) == 0U))
    {
        CacheP_wb(transfer->data, transfer->length, CacheP_TYPE_ALLD);
    }
    else if ((transfer->endpoint_type != USB_ENDP_TYPE_CTRL) && ((transfer->endpoint_address & USB_ENDP_DIR_MASK) == 0U))
    {
        CacheP_wb(transfer->data, transfer->length, CacheP_TYPE_ALLD);
    }

    return xRETURN_OK;
}

static void am64x_hcd_cache_after_complete(const xUSBH_Transfer_t *transfer)
{
    if ((transfer == NULL) || (transfer->data == NULL) || (transfer->length == 0U))
    {
        return;
    }

    if ((transfer->endpoint_type == USB_ENDP_TYPE_CTRL) && ((transfer->setup.bRequestType & USB_REQ_TYPE_IN) != 0U))
    {
        CacheP_inv(transfer->data, transfer->length, CacheP_TYPE_ALLD);
    }
    else if ((transfer->endpoint_type != USB_ENDP_TYPE_CTRL) && ((transfer->endpoint_address & USB_ENDP_DIR_MASK) != 0U))
    {
        CacheP_inv(transfer->data, transfer->length, CacheP_TYPE_ALLD);
    }
}

static xUSBH_HCD_Transfer_Event_t am64x_hcd_transfer_event_from_cdn(uint32_t status, const USBSSP_RingElementT *event_ptr)
{
    uint32_t completion_code = (event_ptr != NULL) ? getCompletionCode(event_ptr) : USBSSP_TRB_COMPLETE_INVALID;

    if ((status == CDN_ECANCELED) || (completion_code == USBSSP_TRB_COMPLETE_STOPPED) ||
        (completion_code == USBSSP_TRB_CMPL_CMD_RNG_STOPPED))
    {
        return xUSBH_HCD_TRANSFER_EVENT_CANCELLED;
    }
    if ((status == USBSSP_ESTALL) || (completion_code == USBSSP_TRB_COMPLETE_STALL_ERROR))
    {
        return xUSBH_HCD_TRANSFER_EVENT_STALLED;
    }
    if ((completion_code == USBSSP_TRB_CMPL_SHORT_PKT) || (completion_code == USBSSP_TRB_CMPL_STOP_SHORT_PKT))
    {
        return xUSBH_HCD_TRANSFER_EVENT_SHORT;
    }
    if ((status == CDN_EOK) && ((event_ptr == NULL) || (completion_code == USBSSP_TRB_COMPLETE_SUCCESS)))
    {
        return xUSBH_HCD_TRANSFER_EVENT_COMPLETE;
    }

    return xUSBH_HCD_TRANSFER_EVENT_ERROR;
}

static void am64x_hcd_transfer_actual_length_set(xUSBH_Transfer_t *transfer, const USBSSP_RingElementT *event_ptr)
{
    uint32_t residual = 0U;

    if (transfer == NULL)
    {
        return;
    }

    if (event_ptr != NULL)
    {
        residual = getTrEvtTrbTransLen(event_ptr);
    }

    transfer->actual_length = (residual < transfer->length) ? (transfer->length - residual) : transfer->length;
}

static void am64x_hcd_transfer_complete_common(USBSSP_DriverResourcesT *resources,
                                               uint32_t status,
                                               const USBSSP_RingElementT *event_ptr,
                                               uint8_t ep_index)
{
    xUSBH_AM64x_HCD_Context_t *ctx = &xUSBH_AM64x_HCD_Context;
    xUSBH_Transfer_t *transfer;
    xUSBH_HCD_Transfer_Event_t event;

    (void)resources;

    if (ep_index >= USBSSP_EP_CONT_MAX)
    {
        return;
    }

    transfer = endpoint_states[ep_index].transfer;
    if (transfer == NULL)
    {
        return;
    }

    event = am64x_hcd_transfer_event_from_cdn(status, event_ptr);
    am64x_hcd_transfer_actual_length_set(transfer, event_ptr);
    am64x_hcd_cache_after_complete(transfer);

    if ((endpoint_states[ep_index].is_configuring == true) && (event == xUSBH_HCD_TRANSFER_EVENT_COMPLETE))
    {
        if (am64x_hcd_enable_configured_endpoints(ctx, transfer) == xRETURN_OK)
        {
            endpoint_states[ep_index].is_configuring = false;
            endpoint_states[ep_index].transfer = transfer;
            return;
        }

        event = xUSBH_HCD_TRANSFER_EVENT_ERROR;
    }

    endpoint_states[ep_index].transfer = NULL;
    endpoint_states[ep_index].is_configuring = false;
    (void)am64x_hcd_notify_transfer_event(ctx, transfer, event);
}

static void am64x_hcd_control_complete(USBSSP_DriverResourcesT *resources, uint32_t status, const USBSSP_RingElementT *event_ptr)
{
    am64x_hcd_transfer_complete_common(resources, status, event_ptr, USBSSP_EP0_CONT_OFFSET);
}

static void am64x_hcd_data_complete(USBSSP_DriverResourcesT *resources, uint32_t status, const USBSSP_RingElementT *event_ptr)
{
    uint8_t ep_index = (event_ptr != NULL) ? getEndpoint(event_ptr) : USBSSP_EP_CONT_MAX;

    am64x_hcd_transfer_complete_common(resources, status, event_ptr, ep_index);
}

static xRETURN_t am64x_hcd_config_descriptor_seed(xUSBH_AM64x_HCD_Context_t *ctx, const xUSBH_Transfer_t *transfer)
{
    const xUSBH_Context_t *host_ctx = (const xUSBH_Context_t *)ctx->host_ctx;
    uint16_t total_length;

    if ((host_ctx == NULL) || (transfer == NULL) || (usbssp_resources.ep0Buff == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }
    if (host_ctx->enumeration.config_total_length < USB_CONFIGURATION_DESC_LEN)
    {
        return xRETURN_xERR_xUSBH_INVALID_STATE;
    }

    total_length = host_ctx->enumeration.config_total_length;
    if ((uint32_t)total_length > USBSSP_EP0_DATA_BUFF_SIZE)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    (void)transfer;
    (void)memcpy(usbssp_resources.ep0Buff, host_ctx->control_buffer, total_length);
    CacheP_wb(usbssp_resources.ep0Buff, total_length, CacheP_TYPE_ALLD);

    return xRETURN_OK;
}

static xRETURN_t am64x_hcd_enable_configured_endpoints(xUSBH_AM64x_HCD_Context_t *ctx, const xUSBH_Transfer_t *transfer)
{
    const xUSBH_Context_t *host_ctx = (const xUSBH_Context_t *)ctx->host_ctx;
    const xUSBH_Device_Context_t *device_ctx = NULL;
    uint8_t device_index = 0U;

    if ((host_ctx == NULL) || (transfer == NULL) || (p_usbssp_obj == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    if (am64x_hcd_find_device(host_ctx, transfer->device_address, &device_ctx, &device_index) != xRETURN_OK)
    {
        return xRETURN_xERR_xUSBH_INVALID_OBJECT;
    }

    (void)device_ctx;
    usbssp_resources.inputContext->inputControlContext[0] = 0U;
    usbssp_resources.inputContext->inputControlContext[1] = cpuToLe32(1U);
    for (uint8_t i = 0U; i < xUSBH_MAX_ENDPOINTS; i++)
    {
        if ((host_ctx->endpoints[i].is_allocated == true) && (host_ctx->endpoints[i].device_index == device_index))
        {
            uint8_t descriptor[xUSBH_AM64X_HCD_ENDPOINT_DESC_SIZE] = {0};
            uint8_t ep_index = am64x_hcd_endpoint_index_from_address(host_ctx->endpoints[i].endpoint_address);
            uint32_t cdn_status;

            if ((ep_index < USBSSP_EP_CONT_OFFSET) || (ep_index >= USBSSP_EP_CONT_MAX))
            {
                return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
            }

            am64x_hcd_endpoint_descriptor_build(&host_ctx->endpoints[i], descriptor);
            cdn_status = p_usbssp_obj->enableEndpoint(&usbssp_resources, descriptor);
            if (cdn_status != CDN_EOK)
            {
                return am64x_hcd_status_from_cdn(cdn_status);
            }

            endpoint_states[ep_index].endpoint_address = host_ctx->endpoints[i].endpoint_address;
            endpoint_states[ep_index].is_enabled = true;
        }
    }

    (void)memcpy(&usbssp_resources.inputContextCopy, usbssp_resources.inputContext, sizeof(USBSSP_InputContexT));
    am64x_hcd_issue_configure_endpoint_command();
    return xRETURN_OK;
}

static void am64x_hcd_issue_configure_endpoint_command(void)
{
    uint64_t input_context_address = cpuToLe64((uint64_t)(uintptr_t)usbssp_resources.inputContext);

    usbssp_resources.commandQ.enqueuePtr->dword0 = (uint32_t)(input_context_address & 0xFFFFFFFFULL);
    usbssp_resources.commandQ.enqueuePtr->dword1 = (uint32_t)((input_context_address >> 32U) & 0xFFFFFFFFULL);
    usbssp_resources.commandQ.enqueuePtr->dword3 =
        cpuToLe32((((uint32_t)usbssp_resources.actualdeviceSlot << USBSSP_SLOT_ID_POS) | (USBSSP_TRB_CONF_EP_CMD << USBSSP_TRB_TYPE_POS) |
                   (uint32_t)usbssp_resources.commandQ.toogleBit));
    updateQueuePtr(&usbssp_resources.commandQ, 0U);
    usbssp_resources.commandQ.isRunningFlag = 1U;
    hostCmdDoorbell(&usbssp_resources);
}

static void am64x_hcd_configure_complete_poll(xUSBH_AM64x_HCD_Context_t *ctx)
{
    xUSBH_Transfer_t *transfer = endpoint_states[USBSSP_EP0_CONT_OFFSET].transfer;

    if ((transfer == NULL) || (endpoint_states[USBSSP_EP0_CONT_OFFSET].is_configuring == false) ||
        (usbssp_resources.commandQ.isRunningFlag != 0U))
    {
        return;
    }

    endpoint_states[USBSSP_EP0_CONT_OFFSET].transfer = NULL;
    endpoint_states[USBSSP_EP0_CONT_OFFSET].is_configuring = false;

    if (usbssp_resources.commandQ.completionCode == USBSSP_TRB_COMPLETE_SUCCESS)
    {
        (void)am64x_hcd_notify_transfer_event(ctx, transfer, xUSBH_HCD_TRANSFER_EVENT_COMPLETE);
    }
    else
    {
        (void)am64x_hcd_notify_transfer_event(ctx, transfer, xUSBH_HCD_TRANSFER_EVENT_ERROR);
    }
}

static void am64x_hcd_configure_xhci_memory(void)
{
    (void)memset(&xhci_memory, 0, sizeof(xhci_memory));
    (void)memset(&usbssp_mem_resources, 0, sizeof(usbssp_mem_resources));

    usbssp_mem_resources.dcbaa = &xhci_memory.dcbaa;
    usbssp_mem_resources.epRingPool = xhci_memory.ep_ring_pool;
    for (uint32_t i = 0U; i < USBSSP_MAX_NUM_INTERRUPTERS; i++)
    {
        usbssp_mem_resources.eventPool[i] = xhci_memory.event_pool[i];
    }
    usbssp_mem_resources.eventRingSegmentEntry = xhci_memory.event_ring_segment_entry;
    usbssp_mem_resources.inputContext = &xhci_memory.input_context;
    usbssp_mem_resources.outputContext = &xhci_memory.output_context;
    usbssp_mem_resources.scratchpad = xhci_memory.scratchpad;
    usbssp_mem_resources.scratchpadPool = xhci_memory.scratchpad_pool;
    usbssp_mem_resources.streamMemoryPool = &xhci_memory.stream_memory_pool;
    usbssp_mem_resources.streamRing = &xhci_memory.stream_ring;
    usbssp_mem_resources.ep0Buffer = xhci_memory.ep0_buffer;
}

static bool am64x_hcd_is_super_speed_enabled(void)
{
#if defined(xUSBH_AM64X_HCD_ENABLE_SUPER_SPEED)
    return true;
#else
    return false;
#endif
}

#if defined(xUSBH_AM64X_HCD_ENABLE_SUPER_SPEED)
static xRETURN_t am64x_hcd_enable_usb3_phy(void)
{
    int32_t soc_status = SOC_moduleClockEnable(TISCI_DEV_SERDES_10G0, 1U);

    if (soc_status != SystemP_SUCCESS)
    {
        return xRETURN_xERR_xUSBH_INVALID_STATE;
    }

    xUSBH_AM64X_HCD_WRITE_REG_FIELD32((CSL_CTRL_MMR0_CFG0_BASE + CSL_MAIN_CTRL_MMR_CFG0_SERDES0_LN0_CTRL),
                                      CSL_MAIN_CTRL_MMR_CFG0_SERDES0_LN0_CTRL_LANE_FUNC_SEL, xUSBH_AM64X_HCD_USB3_LANE_FUNC_USB);
    xUSBH_AM64X_HCD_WRITE_REG_FIELD32((CSL_CTRL_MMR0_CFG0_BASE + CSL_MAIN_CTRL_MMR_CFG0_SERDES0_CLKSEL),
                                      CSL_MAIN_CTRL_MMR_CFG0_SERDES0_CLKSEL_CORE_REFCLK_SEL,
                                      xUSBH_AM64X_HCD_SERDES0_CLKSEL_MAIN_PLL2_HSDIV4);

    if (am64x_wiz_init(CSL_SERDES_10G0_BASE, PHY_TYPE_USB3, 0U, xUSBH_AM64X_HCD_USB3_REFCLK_HZ) != 0)
    {
        return xRETURN_xERR_xUSBH_INVALID_STATE;
    }

    return xRETURN_OK;
}
#endif

static xRETURN_t am64x_hcd_wait_for_host_ready(void)
{
    uint32_t timeout = xUSBH_AM64X_HCD_READY_TIMEOUT_CYCLES;

    while ((xUSBH_AM64X_HCD_READ_REG_FIELD32(&p_usbregs_ctlr->DRD.OTGSTS, CSL_USB3P0SS_CTRL_DRD_OTGSTS_OTG_NRDY) != 0U) && (timeout > 0U))
    {
        timeout--;
    }
    if (timeout == 0U)
    {
        return xRETURN_xERR_xUSBH_TIMEOUT;
    }

    timeout = xUSBH_AM64X_HCD_READY_TIMEOUT_CYCLES;
    while ((xUSBH_AM64X_HCD_READ_REG_FIELD32(&p_usbregs_ctlr->DRD.OTGSTS, CSL_USB3P0SS_CTRL_DRD_OTGSTS_XHC_READY) == 0U) && (timeout > 0U))
    {
        timeout--;
    }
    if (timeout == 0U)
    {
        return xRETURN_xERR_xUSBH_TIMEOUT;
    }

    return xRETURN_OK;
}

static xRETURN_t am64x_hcd_enable_usb0_host(void)
{
    int32_t soc_status = SOC_moduleClockEnable(TISCI_DEV_USB0, 1U);
    xRETURN_t ret = xRETURN_OK;

    if (soc_status != SystemP_SUCCESS)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    SOC_controlModuleUnlockMMR(SOC_DOMAIN_ID_MAIN, 0U);
    SOC_controlModuleUnlockMMR(SOC_DOMAIN_ID_MAIN, 1U);
    SOC_controlModuleUnlockMMR(SOC_DOMAIN_ID_MAIN, 2U);

#if defined(xUSBH_AM64X_HCD_ENABLE_SUPER_SPEED)
    ret = am64x_hcd_enable_usb3_phy();
    if (ret != xRETURN_OK)
    {
        SOC_controlModuleLockMMR(SOC_DOMAIN_ID_MAIN, 0U);
        SOC_controlModuleLockMMR(SOC_DOMAIN_ID_MAIN, 1U);
        SOC_controlModuleLockMMR(SOC_DOMAIN_ID_MAIN, 2U);
        return ret;
    }
#endif

    xUSBH_AM64X_HCD_WRITE_REG_FIELD32(&p_usbregs_tiwrap->USB3P0SS_W1, CSL_USB3P0SS_CMN_USB3P0SS_W1_PWRUP_RST_N, 0U);
    xUSBH_AM64X_HCD_WRITE_REG_FIELD32(&p_usbregs_tiwrap->USB3P0SS_W1, CSL_USB3P0SS_CMN_USB3P0SS_W1_MODESTRAP_SEL, 1U);
    xUSBH_AM64X_HCD_WRITE_REG_FIELD32(&p_usbregs_tiwrap->USB3P0SS_W1, CSL_USB3P0SS_CMN_USB3P0SS_W1_MODESTRAP,
                                      xUSBH_AM64X_HCD_MODESTRAP_HOST);
    xUSBH_AM64X_HCD_WRITE_REG_FIELD32(&p_usbregs_tiwrap->USB3P0SS_W1, CSL_USB3P0SS_CMN_USB3P0SS_W1_USB2_ONLY_MODE,
                                      (am64x_hcd_is_super_speed_enabled() == true) ? 0U : 1U);
    xUSBH_AM64X_HCD_WRITE_REG_FIELD32(&p_usbregs_tiwrap->STATIC_CONFIG, CSL_USB3P0SS_CMN_STATIC_CONFIG_VBUS_SEL, 1U);
    xUSBH_AM64X_HCD_WRITE_REG_FIELD32(&p_usbregs_tiwrap->STATIC_CONFIG, CSL_USB3P0SS_CMN_STATIC_CONFIG_PLL_REF_SEL, 0x06U);
    xUSBH_AM64X_HCD_WRITE_REG_FIELD32(&p_usbregs_tiwrap->USB3P0SS_W1, CSL_USB3P0SS_CMN_USB3P0SS_W1_PWRUP_RST_N, 1U);
    am64x_hcd_write32((uintptr_t)&p_usbregs_ctlr->DRD.SESSVALID_DBNC_CFG, 0x000A0002U);

    ret = am64x_hcd_wait_for_host_ready();

    SOC_controlModuleLockMMR(SOC_DOMAIN_ID_MAIN, 0U);
    SOC_controlModuleLockMMR(SOC_DOMAIN_ID_MAIN, 1U);
    SOC_controlModuleLockMMR(SOC_DOMAIN_ID_MAIN, 2U);

    return ret;
}

static xRETURN_t am64x_hcd_init_usbssp(xUSBH_AM64x_HCD_Context_t *ctx)
{
    uint32_t cdn_status;

    if (ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    (void)memset(&usbssp_resources, 0, sizeof(usbssp_resources));
    (void)memset(endpoint_states, 0, sizeof(endpoint_states));
    am64x_hcd_configure_xhci_memory();
    usbssp_resources.xhciMemRes = &usbssp_mem_resources;
    usbssp_resources.actualPort = xUSBH_AM64X_HCD_USBSSP_ROOT_PORT;
    usbssp_resources.actualSpeed = CH9_USB_SPEED_UNKNOWN;
    usbssp_resources.deviceModeFlag = 0U;
    usbssp_resources.usbModeFlag = (am64x_hcd_is_super_speed_enabled() == true) ? 3U : 2U;
    usbssp_resources.instanceNo = xUSBH_AM64X_HCD_USB0_INSTANCE;

    p_usbssp_obj = USBSSP_GetInstance();
    if (p_usbssp_obj == NULL)
    {
        return xRETURN_xERR_xUSBH_INVALID_STATE;
    }

    cdn_status = p_usbssp_obj->init(&usbssp_resources, (uintptr_t)&p_usbregs_ctlr->XHCI);
    if (cdn_status != CDN_EOK)
    {
        return am64x_hcd_status_from_cdn(cdn_status);
    }

    ctx->driver_object = p_usbssp_obj;
    ctx->driver_private = &usbssp_resources;
    ctx->is_hardware_initialized = true;

    return xRETURN_OK;
}

static xRETURN_t am64x_hcd_poll_port_connection(xUSBH_AM64x_HCD_Context_t *ctx, bool notify)
{
    uint8_t connected = 0U;
    bool was_connected;
    xRETURN_t ret;
    uint32_t cdn_status;

    if ((ctx == NULL) || (p_usbssp_obj == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    was_connected = ctx->is_port_connected;
    cdn_status = p_usbssp_obj->getPortConnected(&usbssp_resources, &connected);
    ret = am64x_hcd_status_from_cdn(cdn_status);
    if (ret != xRETURN_OK)
    {
        return ret;
    }

    ctx->is_port_connected = (connected != 0U);
    ctx->speed = am64x_hcd_speed_from_ch9(usbssp_resources.actualSpeed);

    if ((notify == true) && (ctx->is_port_connected != was_connected))
    {
        if (ctx->is_port_connected == true)
        {
            ret = am64x_hcd_notify_port_event(ctx, xUSBH_HCD_PORT_EVENT_CONNECTED);
        }
        else
        {
            ret = am64x_hcd_notify_port_event(ctx, xUSBH_HCD_PORT_EVENT_DISCONNECTED);
        }
    }

    return ret;
}
#endif

static xRETURN_t am64x_hcd_init(void *hcd_ctx, void *host_ctx, xUSBH_HCD_Event_Callback_t callback)
{
    xUSBH_AM64x_HCD_Context_t *ctx = (xUSBH_AM64x_HCD_Context_t *)hcd_ctx;

    if ((ctx == NULL) || (host_ctx == NULL) || (callback == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    (void)memset(ctx, 0, sizeof(*ctx));
    ctx->host_ctx = host_ctx;
    ctx->event_callback = callback;
    ctx->speed = USB_SPEED_HIGH;
    ctx->port = xUSBH_AM64X_HCD_USB0_PORT;
    ctx->is_initialized = true;

    return xRETURN_OK;
}

static xRETURN_t am64x_hcd_deinit(void *hcd_ctx)
{
    xUSBH_AM64x_HCD_Context_t *ctx = (xUSBH_AM64x_HCD_Context_t *)hcd_ctx;

    if (ctx == NULL)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    (void)memset(ctx, 0, sizeof(*ctx));

    return xRETURN_OK;
}

static xRETURN_t am64x_hcd_start(void *hcd_ctx)
{
    xUSBH_AM64x_HCD_Context_t *ctx = (xUSBH_AM64x_HCD_Context_t *)hcd_ctx;

    if (am64x_hcd_ctx_is_valid(ctx) == false)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }
    if (ctx->is_initialized == false)
    {
        return xRETURN_xERR_xUSBH_NOT_INITIALIZED;
    }

#if defined(xUSBH_AM64X_HCD_ENABLE_MCU_PLUS_SDK)
    if (ctx->is_hardware_initialized == false)
    {
        xRETURN_t ret = am64x_hcd_enable_usb0_host();
        if (ret != xRETURN_OK)
        {
            return ret;
        }

        ret = am64x_hcd_init_usbssp(ctx);
        if (ret != xRETURN_OK)
        {
            return ret;
        }
    }

    ctx->is_started = true;
    ctx->is_port_powered = true;
    (void)am64x_hcd_poll_port_connection(ctx, true);

    return xRETURN_OK;
#else
    return xRETURN_xERR_xUSBH_UNSUPPORTED_OPERATION;
#endif
}

static xRETURN_t am64x_hcd_stop(void *hcd_ctx)
{
    xUSBH_AM64x_HCD_Context_t *ctx = (xUSBH_AM64x_HCD_Context_t *)hcd_ctx;

    if (am64x_hcd_ctx_is_valid(ctx) == false)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    ctx->is_started = false;
    ctx->are_interrupts_enabled = false;

    return xRETURN_OK;
}

static xRETURN_t am64x_hcd_enable_interrupts(void *hcd_ctx)
{
    xUSBH_AM64x_HCD_Context_t *ctx = (xUSBH_AM64x_HCD_Context_t *)hcd_ctx;

    if (am64x_hcd_ctx_is_valid(ctx) == false)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }
    if (ctx->is_initialized == false)
    {
        return xRETURN_xERR_xUSBH_NOT_INITIALIZED;
    }

    ctx->are_interrupts_enabled = true;

    return xRETURN_OK;
}

static xRETURN_t am64x_hcd_disable_interrupts(void *hcd_ctx)
{
    xUSBH_AM64x_HCD_Context_t *ctx = (xUSBH_AM64x_HCD_Context_t *)hcd_ctx;

    if (am64x_hcd_ctx_is_valid(ctx) == false)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

    ctx->are_interrupts_enabled = false;

    return xRETURN_OK;
}

static xRETURN_t am64x_hcd_port_power(void *hcd_ctx, uint8_t port, bool enable)
{
    xUSBH_AM64x_HCD_Context_t *ctx = (xUSBH_AM64x_HCD_Context_t *)hcd_ctx;

    if (am64x_hcd_ctx_is_valid(ctx) == false)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }
    if (port != xUSBH_AM64X_HCD_USB0_PORT)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

#if defined(xUSBH_AM64X_HCD_ENABLE_MCU_PLUS_SDK)
    ctx->is_port_powered = enable;

    return xRETURN_OK;
#else
    ctx->is_port_powered = enable;

    return xRETURN_xERR_xUSBH_UNSUPPORTED_OPERATION;
#endif
}

static xRETURN_t am64x_hcd_port_reset(void *hcd_ctx, uint8_t port)
{
    xUSBH_AM64x_HCD_Context_t *ctx = (xUSBH_AM64x_HCD_Context_t *)hcd_ctx;

    if (am64x_hcd_ctx_is_valid(ctx) == false)
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }
    if (port != xUSBH_AM64X_HCD_USB0_PORT)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

#if defined(xUSBH_AM64X_HCD_ENABLE_MCU_PLUS_SDK)
    if ((ctx->is_hardware_initialized == false) || (p_usbssp_obj == NULL))
    {
        return xRETURN_xERR_xUSBH_NOT_INITIALIZED;
    }

    xRETURN_t ret = am64x_hcd_status_from_cdn(p_usbssp_obj->resetRootHubPort(&usbssp_resources));
    if (ret == xRETURN_OK)
    {
        ret = am64x_hcd_notify_port_event(ctx, xUSBH_HCD_PORT_EVENT_RESET_COMPLETE);
    }

    return ret;
#else
    return xRETURN_xERR_xUSBH_UNSUPPORTED_OPERATION;
#endif
}

static xRETURN_t am64x_hcd_get_port_status(void *hcd_ctx, uint8_t port, xUSBH_HCD_Port_Status_t *status)
{
    xUSBH_AM64x_HCD_Context_t *ctx = (xUSBH_AM64x_HCD_Context_t *)hcd_ctx;

    if ((am64x_hcd_ctx_is_valid(ctx) == false) || (status == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }
    if (port != xUSBH_AM64X_HCD_USB0_PORT)
    {
        return xRETURN_xERR_xUSBH_INVALID_ARGUMENT;
    }

    (void)memset(status, 0, sizeof(*status));
    status->speed = ctx->speed;

#if defined(xUSBH_AM64X_HCD_ENABLE_MCU_PLUS_SDK)
    if ((ctx->is_hardware_initialized == false) || (p_usbssp_obj == NULL))
    {
        return xRETURN_xERR_xUSBH_NOT_INITIALIZED;
    }

    xRETURN_t ret = am64x_hcd_poll_port_connection(ctx, false);
    if (ret != xRETURN_OK)
    {
        return ret;
    }

    status->is_connected = ctx->is_port_connected;
    status->is_enabled = ctx->is_port_connected;
    status->is_suspended = false;
    status->is_overcurrent = false;
    status->speed = ctx->speed;

    return xRETURN_OK;
#else
    return xRETURN_xERR_xUSBH_UNSUPPORTED_OPERATION;
#endif
}

static xRETURN_t am64x_hcd_submit_transfer(void *hcd_ctx, xUSBH_Transfer_t *transfer)
{
    xUSBH_AM64x_HCD_Context_t *ctx = (xUSBH_AM64x_HCD_Context_t *)hcd_ctx;

    if ((am64x_hcd_ctx_is_valid(ctx) == false) || (transfer == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

#if defined(xUSBH_AM64X_HCD_ENABLE_MCU_PLUS_SDK)
    uint8_t ep_index = 0U;
    xRETURN_t ret;

    if ((ctx->is_hardware_initialized == false) || (p_usbssp_obj == NULL))
    {
        return xRETURN_xERR_xUSBH_NOT_INITIALIZED;
    }

    ret = am64x_hcd_transfer_endpoint_index(ctx, transfer, &ep_index);
    if (ret != xRETURN_OK)
    {
        return ret;
    }
    if (endpoint_states[ep_index].transfer != NULL)
    {
        return xRETURN_xERR_xUSBH_INVALID_STATE;
    }

    ret = am64x_hcd_cache_before_submit(transfer);
    if (ret != xRETURN_OK)
    {
        return ret;
    }

    endpoint_states[ep_index].transfer = transfer;
    endpoint_states[ep_index].endpoint_address = transfer->endpoint_address;
    endpoint_states[ep_index].is_configuring = false;

    if ((transfer->endpoint_type == USB_ENDP_TYPE_CTRL) || (transfer->endpoint_address == xUSBH_AM64X_HCD_ENDPOINT_ADDR_EP0))
    {
        CH9_UsbSetup setup = {0};
        uint32_t cdn_status;

        am64x_hcd_ch9_setup_from_transfer(transfer, &setup);
        if ((setup.bRequest == USB_REQ_SET_ADDRESS) && ((setup.bmRequestType & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_STANDARD))
        {
            cdn_status = p_usbssp_obj->noOpTest(&usbssp_resources, am64x_hcd_control_complete);
            if (cdn_status != CDN_EOK)
            {
                endpoint_states[ep_index].transfer = NULL;
                return am64x_hcd_status_from_cdn(cdn_status);
            }

            return xRETURN_OK;
        }

        if ((setup.bRequest == USB_REQ_SET_CONFIGURATION) && ((setup.bmRequestType & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_STANDARD))
        {
            ret = am64x_hcd_config_descriptor_seed(ctx, transfer);
            if (ret != xRETURN_OK)
            {
                endpoint_states[ep_index].transfer = NULL;
                return ret;
            }
            endpoint_states[ep_index].is_configuring = true;
        }

        cdn_status = p_usbssp_obj->nBControlTransfer(&usbssp_resources, &setup, transfer->data, am64x_hcd_control_complete);
        if (cdn_status != CDN_EOK)
        {
            endpoint_states[ep_index].transfer = NULL;
            endpoint_states[ep_index].is_configuring = false;
            return am64x_hcd_status_from_cdn(cdn_status);
        }

        return xRETURN_OK;
    }

    if (endpoint_states[ep_index].is_enabled == false)
    {
        endpoint_states[ep_index].transfer = NULL;
        return xRETURN_xERR_xUSBH_INVALID_STATE;
    }

    uint32_t cdn_status =
        p_usbssp_obj->transferData(&usbssp_resources, ep_index, (uintptr_t)transfer->data, transfer->length, am64x_hcd_data_complete);
    if (cdn_status != CDN_EOK)
    {
        endpoint_states[ep_index].transfer = NULL;
        return am64x_hcd_status_from_cdn(cdn_status);
    }

    return xRETURN_OK;
#else
    return xRETURN_xERR_xUSBH_UNSUPPORTED_OPERATION;
#endif
}

static xRETURN_t am64x_hcd_cancel_transfer(void *hcd_ctx, xUSBH_Transfer_t *transfer)
{
    xUSBH_AM64x_HCD_Context_t *ctx = (xUSBH_AM64x_HCD_Context_t *)hcd_ctx;

    if ((am64x_hcd_ctx_is_valid(ctx) == false) || (transfer == NULL))
    {
        return xRETURN_xERR_xUSBH_NULL_POINTER;
    }

#if defined(xUSBH_AM64X_HCD_ENABLE_MCU_PLUS_SDK)
    uint8_t ep_index = 0U;
    xRETURN_t ret;

    if ((ctx->is_hardware_initialized == false) || (p_usbssp_obj == NULL))
    {
        return xRETURN_xERR_xUSBH_NOT_INITIALIZED;
    }

    ret = am64x_hcd_transfer_endpoint_index(ctx, transfer, &ep_index);
    if (ret != xRETURN_OK)
    {
        return ret;
    }

    endpoint_states[ep_index].transfer = NULL;
    endpoint_states[ep_index].is_configuring = false;

    return am64x_hcd_status_from_cdn(p_usbssp_obj->stopEndpoint(&usbssp_resources, ep_index));
#else
    return xRETURN_xERR_xUSBH_UNSUPPORTED_OPERATION;
#endif
}

static uint32_t am64x_hcd_get_frame_number(void *hcd_ctx)
{
    const xUSBH_AM64x_HCD_Context_t *ctx = (const xUSBH_AM64x_HCD_Context_t *)hcd_ctx;

    if (am64x_hcd_ctx_is_valid(ctx) == false)
    {
        return 0U;
    }

#if defined(xUSBH_AM64X_HCD_ENABLE_MCU_PLUS_SDK)
    if ((ctx->is_hardware_initialized == true) && (p_usbssp_obj != NULL))
    {
        uint32_t frame_number = 0U;
        if (p_usbssp_obj->getMicroFrameIndex(&usbssp_resources, &frame_number) == CDN_EOK)
        {
            ((xUSBH_AM64x_HCD_Context_t *)ctx)->frame_number = frame_number;
        }
    }
#endif

    return ctx->frame_number;
}

// PUBLIC FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////////
void xUSBH_AM64x_HCD_IRQ_Handler(uint8_t port)
{
#if defined(xUSBH_AM64X_HCD_ENABLE_MCU_PLUS_SDK)
    xUSBH_AM64x_HCD_Context_t *ctx = &xUSBH_AM64x_HCD_Context;

    if ((port == xUSBH_AM64X_HCD_USB0_PORT) && (ctx->is_hardware_initialized == true) && (p_usbssp_obj != NULL))
    {
        (void)p_usbssp_obj->isr(&usbssp_resources);
        am64x_hcd_configure_complete_poll(ctx);
        (void)am64x_hcd_poll_port_connection(ctx, true);
    }
#else
    (void)port;
#endif
}

const xUSBH_HCD_Ops_t xUSBH_AM64x_HCD_Ops = {
    .init = am64x_hcd_init,
    .deinit = am64x_hcd_deinit,
    .start = am64x_hcd_start,
    .stop = am64x_hcd_stop,
    .enable_interrupts = am64x_hcd_enable_interrupts,
    .disable_interrupts = am64x_hcd_disable_interrupts,
    .port_power = am64x_hcd_port_power,
    .port_reset = am64x_hcd_port_reset,
    .get_port_status = am64x_hcd_get_port_status,
    .submit_transfer = am64x_hcd_submit_transfer,
    .cancel_transfer = am64x_hcd_cancel_transfer,
    .get_frame_number = am64x_hcd_get_frame_number,
};
// EOF /////////////////////////////////////////////////////////////////////////////
