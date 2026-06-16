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

// @file xusbh_hcd.h
// @brief USB Host Controller Driver interface.

#ifndef XUSBH_HCD_H
#define XUSBH_HCD_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusb_defs.h"
#include "xusbh_return.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef enum xUSBH_HCD_Port_Event_t
    {
        xUSBH_HCD_PORT_EVENT_CONNECTED = 0,
        xUSBH_HCD_PORT_EVENT_DISCONNECTED,
        xUSBH_HCD_PORT_EVENT_RESET_COMPLETE,
        xUSBH_HCD_PORT_EVENT_SUSPENDED,
        xUSBH_HCD_PORT_EVENT_RESUMED,
        xUSBH_HCD_PORT_EVENT_OVERCURRENT,
    } xUSBH_HCD_Port_Event_t;

    typedef enum xUSBH_HCD_Transfer_Event_t
    {
        xUSBH_HCD_TRANSFER_EVENT_COMPLETE = 0,
        xUSBH_HCD_TRANSFER_EVENT_SHORT,
        xUSBH_HCD_TRANSFER_EVENT_STALLED,
        xUSBH_HCD_TRANSFER_EVENT_ERROR,
        xUSBH_HCD_TRANSFER_EVENT_CANCELLED,
    } xUSBH_HCD_Transfer_Event_t;

    typedef enum xUSBH_HCD_Event_Type_t
    {
        xUSBH_HCD_EVENT_TYPE_PORT = 0,
        xUSBH_HCD_EVENT_TYPE_TRANSFER,
    } xUSBH_HCD_Event_Type_t;

    typedef struct xUSBH_HCD_Port_Status_t
    {
        bool is_connected;
        bool is_enabled;
        bool is_suspended;
        bool is_overcurrent;
        USB_Speed_t speed;
    } xUSBH_HCD_Port_Status_t;

    // Transfer objects are owned by xUSBH core. The HCD may read fields while a
    // transfer is submitted, updates actual_length before completion, and must
    // signal exactly one terminal transfer event through the registered callback
    // unless the core cancels the transfer first.
    typedef struct xUSBH_Transfer_t
    {
        uint8_t device_address;
        uint8_t endpoint_address;
        uint8_t endpoint_type;
        uint8_t interval;
        bool is_allocated;
        bool is_submitted;
        bool has_setup;
        USB_Setup_Request_t setup;
        xUSBH_HCD_Transfer_Event_t last_event;
        uint8_t *data;
        uint32_t length;
        uint32_t actual_length;
        void *user_ctx;
    } xUSBH_Transfer_t;

    typedef struct xUSBH_HCD_Event_t
    {
        xUSBH_HCD_Event_Type_t type;
        uint8_t port;
        xUSBH_HCD_Port_Event_t port_event;
        xUSBH_HCD_Transfer_Event_t transfer_event;
        xUSBH_Transfer_t *transfer;
    } xUSBH_HCD_Event_t;

    // HCD callbacks may run from ISR context. The HCD passes back the host_ctx
    // value supplied to init(), and the event object only needs to remain valid
    // for the duration of the callback.
    typedef void (*xUSBH_HCD_Event_Callback_t)(void *host_ctx, const xUSBH_HCD_Event_t *event);

    // HCD porting contract:
    // - ops and hcd_ctx are caller-owned and must outlive xUSBH_Start/Stop.
    // - init stores the callback/host_ctx pair and prepares private HCD state.
    // - start/stop control the controller role without enabling IRQ delivery.
    // - enable_interrupts/disable_interrupts gate callback delivery.
    // - port status reports the current hardware state without advancing policy.
    // - submit_transfer gives the HCD temporary ownership until completion or
    //   cancel_transfer.
    // - get_frame_number is callable from task context for timeout bookkeeping.
    typedef struct xUSBH_HCD_Ops_t
    {
        xRETURN_t (*init)(void *hcd_ctx, void *host_ctx, xUSBH_HCD_Event_Callback_t callback);
        xRETURN_t (*deinit)(void *hcd_ctx);
        xRETURN_t (*start)(void *hcd_ctx);
        xRETURN_t (*stop)(void *hcd_ctx);
        xRETURN_t (*enable_interrupts)(void *hcd_ctx);
        xRETURN_t (*disable_interrupts)(void *hcd_ctx);
        xRETURN_t (*port_power)(void *hcd_ctx, uint8_t port, bool enable);
        xRETURN_t (*port_reset)(void *hcd_ctx, uint8_t port);
        xRETURN_t (*get_port_status)(void *hcd_ctx, uint8_t port, xUSBH_HCD_Port_Status_t *status);
        xRETURN_t (*submit_transfer)(void *hcd_ctx, xUSBH_Transfer_t *transfer);
        xRETURN_t (*cancel_transfer)(void *hcd_ctx, xUSBH_Transfer_t *transfer);
        uint32_t (*get_frame_number)(void *hcd_ctx);
    } xUSBH_HCD_Ops_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////
    static inline bool xUSBH_HCD_Ops_Are_Complete(const xUSBH_HCD_Ops_t *ops)
    {
        return (ops != NULL) && (ops->init != NULL) && (ops->deinit != NULL) && (ops->start != NULL) && (ops->stop != NULL) &&
               (ops->enable_interrupts != NULL) && (ops->disable_interrupts != NULL) && (ops->port_power != NULL) &&
               (ops->port_reset != NULL) && (ops->get_port_status != NULL) && (ops->submit_transfer != NULL) &&
               (ops->cancel_transfer != NULL) && (ops->get_frame_number != NULL);
    }

    static inline xRETURN_t xUSBH_HCD_Init(const xUSBH_HCD_Ops_t *ops, void *hcd_ctx, void *host_ctx, xUSBH_HCD_Event_Callback_t callback)
    {
        if ((ops == NULL) || (ops->init == NULL) || (host_ctx == NULL) || (callback == NULL))
        {
            return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
        }

        return ops->init(hcd_ctx, host_ctx, callback);
    }

    static inline xRETURN_t xUSBH_HCD_Deinit(const xUSBH_HCD_Ops_t *ops, void *hcd_ctx)
    {
        if ((ops == NULL) || (ops->deinit == NULL))
        {
            return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
        }

        return ops->deinit(hcd_ctx);
    }

    static inline xRETURN_t xUSBH_HCD_Start(const xUSBH_HCD_Ops_t *ops, void *hcd_ctx)
    {
        if ((ops == NULL) || (ops->start == NULL))
        {
            return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
        }

        return ops->start(hcd_ctx);
    }

    static inline xRETURN_t xUSBH_HCD_Stop(const xUSBH_HCD_Ops_t *ops, void *hcd_ctx)
    {
        if ((ops == NULL) || (ops->stop == NULL))
        {
            return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
        }

        return ops->stop(hcd_ctx);
    }

    static inline xRETURN_t xUSBH_HCD_Enable_Interrupts(const xUSBH_HCD_Ops_t *ops, void *hcd_ctx)
    {
        if ((ops == NULL) || (ops->enable_interrupts == NULL))
        {
            return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
        }

        return ops->enable_interrupts(hcd_ctx);
    }

    static inline xRETURN_t xUSBH_HCD_Disable_Interrupts(const xUSBH_HCD_Ops_t *ops, void *hcd_ctx)
    {
        if ((ops == NULL) || (ops->disable_interrupts == NULL))
        {
            return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
        }

        return ops->disable_interrupts(hcd_ctx);
    }

    static inline xRETURN_t xUSBH_HCD_Port_Power(const xUSBH_HCD_Ops_t *ops, void *hcd_ctx, uint8_t port, bool enable)
    {
        if ((ops == NULL) || (ops->port_power == NULL))
        {
            return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
        }

        return ops->port_power(hcd_ctx, port, enable);
    }

    static inline xRETURN_t xUSBH_HCD_Port_Reset(const xUSBH_HCD_Ops_t *ops, void *hcd_ctx, uint8_t port)
    {
        if ((ops == NULL) || (ops->port_reset == NULL))
        {
            return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
        }

        return ops->port_reset(hcd_ctx, port);
    }

    static inline xRETURN_t
    xUSBH_HCD_Get_Port_Status(const xUSBH_HCD_Ops_t *ops, void *hcd_ctx, uint8_t port, xUSBH_HCD_Port_Status_t *status)
    {
        if ((ops == NULL) || (ops->get_port_status == NULL))
        {
            return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
        }
        if (status == NULL)
        {
            return xRETURN_xERR_xUSBH_NULL_POINTER;
        }

        return ops->get_port_status(hcd_ctx, port, status);
    }

    static inline xRETURN_t xUSBH_HCD_Submit_Transfer(const xUSBH_HCD_Ops_t *ops, void *hcd_ctx, xUSBH_Transfer_t *transfer)
    {
        if ((ops == NULL) || (ops->submit_transfer == NULL))
        {
            return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
        }
        if (transfer == NULL)
        {
            return xRETURN_xERR_xUSBH_NULL_POINTER;
        }

        return ops->submit_transfer(hcd_ctx, transfer);
    }

    static inline xRETURN_t xUSBH_HCD_Cancel_Transfer(const xUSBH_HCD_Ops_t *ops, void *hcd_ctx, xUSBH_Transfer_t *transfer)
    {
        if ((ops == NULL) || (ops->cancel_transfer == NULL))
        {
            return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
        }
        if (transfer == NULL)
        {
            return xRETURN_xERR_xUSBH_NULL_POINTER;
        }

        return ops->cancel_transfer(hcd_ctx, transfer);
    }

    static inline xRETURN_t xUSBH_HCD_Get_Frame_Number(const xUSBH_HCD_Ops_t *ops, void *hcd_ctx, uint32_t *frame_number)
    {
        if ((ops == NULL) || (ops->get_frame_number == NULL))
        {
            return xRETURN_xERR_xUSBH_HCD_NULL_POINTER;
        }
        if (frame_number == NULL)
        {
            return xRETURN_xERR_xUSBH_NULL_POINTER;
        }

        *frame_number = ops->get_frame_number(hcd_ctx);
        return xRETURN_OK;
    }

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif // XUSBH_HCD_H
// EOF /////////////////////////////////////////////////////////////////////////////
