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

// @file xusbd_dcd.h
// @brief USB Device Controller Driver Interface

#ifndef XUSBD_DCD_H
#define XUSBD_DCD_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xusb_defs.h"
#include "xusbd_return.h"
#include "xassert.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef enum
    {
        USB_DCD_SETUP_RECEIVED,
        USB_DCD_DATA_RECEIVED,
        USB_DCD_DATA_SENT,
        USB_DCD_SOF_RECEIVED,
        USB_DCD_RESET_RECEIVED,
        USB_DCD_CONNECT_RECEIVED,
        USB_DCD_SUSPEND_RECEIVED,
        USB_DCD_RESUME_RECEIVED,
        USB_DCD_DISCONNECT_RECEIVED,
        USB_DCD_SPEED_CHANGE_RECEIVED,
        USB_DCD_LINK_STATE_CHANGE_RECEIVED,
    } USB_DCD_Event_t;

    typedef enum
    {
        USB_DCD_LINK_STATE_UNKNOWN = 0,
        USB_DCD_LINK_STATE_RX_DETECT,
        USB_DCD_LINK_STATE_POLLING,
        USB_DCD_LINK_STATE_U0,
        USB_DCD_LINK_STATE_U1,
        USB_DCD_LINK_STATE_U2,
        USB_DCD_LINK_STATE_U3,
        USB_DCD_LINK_STATE_RECOVERY,
        USB_DCD_LINK_STATE_HOT_RESET,
        USB_DCD_LINK_STATE_COMPLIANCE,
        USB_DCD_LINK_STATE_LOOPBACK,
        USB_DCD_LINK_STATE_DISABLED,
    } USB_DCD_Link_State_t;

    typedef struct
    {
        USB_DCD_Link_State_t link_state;
    } xUSBD_DCD_Link_State_Event_t;

    typedef void (*xUSBD_DCD_Event_Callback_t)(void *device_ctx, USB_DCD_Event_t event, uint8_t ep_addr, uint8_t *data, uint32_t length);

    typedef struct xUSBD_DCD_Transfer_t xUSBD_DCD_Transfer_t;
    typedef void (*xUSBD_DCD_Transfer_Complete_t)(void *user_ctx,
                                                  const xUSBD_DCD_Transfer_t *transfer,
                                                  xRETURN_t status,
                                                  uint32_t actual_length);

    struct xUSBD_DCD_Transfer_t
    {
        uint8_t ep_addr;
        uint8_t *data;
        uint32_t length;
        bool is_zlp_required;
        xUSBD_DCD_Transfer_Complete_t complete;
        void *user_ctx;
    };

    typedef struct
    {
        xRETURN_t (*init)(void *dcd_ctx, USB_Speed_t speed, void *device_ctx);
        xRETURN_t (*deinit)(void *dcd_ctx);
        xRETURN_t (*set_event_callback)(void *dcd_ctx, xUSBD_DCD_Event_Callback_t callback);
        xRETURN_t (*connect)(void *dcd_ctx);
        xRETURN_t (*disconnect)(void *dcd_ctx);
        xRETURN_t (*enable_interrupts)(void *dcd_ctx);
        xRETURN_t (*disable_interrupts)(void *dcd_ctx);
        xRETURN_t (*set_address)(void *dcd_ctx, uint8_t address);
        xRETURN_t (*set_remote_wakeup)(void *dcd_ctx, bool enable);
        xRETURN_t (*set_test_mode)(void *dcd_ctx, uint8_t mode);
        uint32_t (*get_frame_number)(void *dcd_ctx);
        USB_Speed_t (*get_speed)(void *dcd_ctx);
        xRETURN_t (*ep_init)(void *dcd_ctx, uint8_t ep_addr, uint8_t ep_type, uint16_t mps);
        xRETURN_t (*ep_deinit)(void *dcd_ctx, uint8_t ep_addr);
        xRETURN_t (*ep_receive)(void *dcd_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
        xRETURN_t (*ep_send)(void *dcd_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length, bool is_zlp_required);
        xRETURN_t (*ep_transfer_queue)(void *dcd_ctx, const xUSBD_DCD_Transfer_t *transfer);
        xRETURN_t (*ep_stall)(void *dcd_ctx, uint8_t ep_addr);
        xRETURN_t (*ep_clear_stall)(void *dcd_ctx, uint8_t ep_addr);
        bool (*ep_is_stalled)(void *dcd_ctx, uint8_t ep_addr);
    } xUSBD_DCD_Ops_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    static inline xRETURN_t xUSBD_DCD_Init(xUSBD_DCD_Ops_t *ops, void *dcd_ctx, USB_Speed_t speed, void *device_ctx)
    {
        if (ops == NULL || ops->init == NULL)
        {
            return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
        }
        return ops->init(dcd_ctx, speed, device_ctx);
    }
    static inline xRETURN_t xUSBD_DCD_Deinit(xUSBD_DCD_Ops_t *ops, void *dcd_ctx)
    {
        if (ops == NULL || ops->deinit == NULL)
        {
            return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
        }
        return ops->deinit(dcd_ctx);
    }
    static inline xRETURN_t xUSBD_DCD_Set_Event_Callback(xUSBD_DCD_Ops_t *ops, void *dcd_ctx, xUSBD_DCD_Event_Callback_t callback)
    {
        if (ops == NULL || ops->set_event_callback == NULL || callback == NULL)
        {
            return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
        }
        return ops->set_event_callback(dcd_ctx, callback);
    }
    static inline xRETURN_t xUSBD_DCD_Connect(xUSBD_DCD_Ops_t *ops, void *dcd_ctx)
    {
        if (ops == NULL || ops->connect == NULL)
        {
            return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
        }
        return ops->connect(dcd_ctx);
    }
    static inline xRETURN_t xUSBD_DCD_Disconnect(xUSBD_DCD_Ops_t *ops, void *dcd_ctx)
    {
        if (ops == NULL || ops->disconnect == NULL)
        {
            return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
        }
        return ops->disconnect(dcd_ctx);
    }
    static inline xRETURN_t xUSBD_DCD_Enable_Interrupts(xUSBD_DCD_Ops_t *ops, void *dcd_ctx)
    {
        if (ops == NULL || ops->enable_interrupts == NULL)
        {
            return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
        }
        return ops->enable_interrupts(dcd_ctx);
    }
    static inline xRETURN_t xUSBD_DCD_Disable_Interrupts(xUSBD_DCD_Ops_t *ops, void *dcd_ctx)
    {
        if (ops == NULL || ops->disable_interrupts == NULL)
        {
            return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
        }
        return ops->disable_interrupts(dcd_ctx);
    }
    static inline xRETURN_t xUSBD_DCD_Set_Address(xUSBD_DCD_Ops_t *ops, void *dcd_ctx, uint8_t address)
    {
        if (ops == NULL || ops->set_address == NULL)
        {
            return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
        }
        return ops->set_address(dcd_ctx, address);
    }
    static inline xRETURN_t xUSBD_DCD_Set_Remote_Wakeup(xUSBD_DCD_Ops_t *ops, void *dcd_ctx, bool enable)
    {
        if (ops == NULL || ops->set_remote_wakeup == NULL)
        {
            return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
        }
        return ops->set_remote_wakeup(dcd_ctx, enable);
    }
    static inline xRETURN_t xUSBD_DCD_Set_Test_Mode(xUSBD_DCD_Ops_t *ops, void *dcd_ctx, uint8_t mode)
    {
        if (ops == NULL || ops->set_test_mode == NULL)
        {
            return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
        }
        return ops->set_test_mode(dcd_ctx, mode);
    }
    // Get_Frame_Number and Get_Speed return scalar values so they use output parameters
    // rather than the scalar return to avoid ambiguity between "ops is NULL" and a
    // valid result of 0 / USB_SPEED_FULL. Precondition: ops must not be NULL.
    static inline xRETURN_t xUSBD_DCD_Get_Frame_Number(xUSBD_DCD_Ops_t *ops, void *dcd_ctx, uint32_t *frame_number)
    {
        if (ops == NULL || ops->get_frame_number == NULL || frame_number == NULL)
        {
            return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
        }
        *frame_number = ops->get_frame_number(dcd_ctx);
        return xRETURN_OK;
    }
    static inline xRETURN_t xUSBD_DCD_Get_Speed(xUSBD_DCD_Ops_t *ops, void *dcd_ctx, USB_Speed_t *speed)
    {
        if (ops == NULL || ops->get_speed == NULL || speed == NULL)
        {
            return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
        }
        *speed = ops->get_speed(dcd_ctx);
        return xRETURN_OK;
    }
    static inline xRETURN_t xUSBD_DCD_EP_Init(xUSBD_DCD_Ops_t *ops, void *dcd_ctx, uint8_t ep_addr, uint8_t ep_type, uint16_t mps)
    {
        if (ops == NULL || ops->ep_init == NULL)
        {
            return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
        }
        return ops->ep_init(dcd_ctx, ep_addr, ep_type, mps);
    }
    static inline xRETURN_t xUSBD_DCD_EP_Deinit(xUSBD_DCD_Ops_t *ops, void *dcd_ctx, uint8_t ep_addr)
    {
        if (ops == NULL || ops->ep_deinit == NULL)
        {
            return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
        }
        return ops->ep_deinit(dcd_ctx, ep_addr);
    }
    static inline xRETURN_t xUSBD_DCD_EP_Receive(xUSBD_DCD_Ops_t *ops, void *dcd_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
    {
        if (ops == NULL || ops->ep_receive == NULL)
        {
            return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
        }
        return ops->ep_receive(dcd_ctx, ep_addr, data, length);
    }
    static inline xRETURN_t
    xUSBD_DCD_EP_Send(xUSBD_DCD_Ops_t *ops, void *dcd_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length, bool is_zlp_required)
    {
        if (ops == NULL || ops->ep_send == NULL)
        {
            return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
        }
        return ops->ep_send(dcd_ctx, ep_addr, data, length, is_zlp_required);
    }
    static inline xRETURN_t xUSBD_DCD_EP_Transfer_Queue(xUSBD_DCD_Ops_t *ops, void *dcd_ctx, const xUSBD_DCD_Transfer_t *transfer)
    {
        if (ops == NULL)
        {
            return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
        }
        if (transfer == NULL)
        {
            return xRETURN_xERR_xUSBD_NULL_POINTER;
        }
        if (ops->ep_transfer_queue == NULL)
        {
            return xRETURN_xERR_xUSBD_UNSUPPORTED_REQUEST;
        }
        return ops->ep_transfer_queue(dcd_ctx, transfer);
    }
    static inline xRETURN_t xUSBD_DCD_EP_Stall(xUSBD_DCD_Ops_t *ops, void *dcd_ctx, uint8_t ep_addr)
    {
        if (ops == NULL || ops->ep_stall == NULL)
        {
            return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
        }
        return ops->ep_stall(dcd_ctx, ep_addr);
    }
    static inline xRETURN_t xUSBD_DCD_EP_Clear_Stall(xUSBD_DCD_Ops_t *ops, void *dcd_ctx, uint8_t ep_addr)
    {
        if (ops == NULL || ops->ep_clear_stall == NULL)
        {
            return xRETURN_xERR_xUSBD_DCD_NULL_POINTER;
        }
        return ops->ep_clear_stall(dcd_ctx, ep_addr);
    }
    static inline bool xUSBD_DCD_EP_Is_Stalled(xUSBD_DCD_Ops_t *ops, void *dcd_ctx, uint8_t ep_addr)
    {
        if (ops == NULL || ops->ep_is_stalled == NULL)
        {
            return false;
        }
        return ops->ep_is_stalled(dcd_ctx, ep_addr);
    }

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XUSBD_DCD_H
// EOF /////////////////////////////////////////////////////////////////////////////
