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

// @file xusbd_class.h
// @brief xUSB Device Stack Class common definitions and structures.

#ifndef XUSBD_CLASS_H
#define XUSBD_CLASS_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbd_config.h"
#include "xusbd_return.h"
#include "xusb_defs.h"
#include "xusbd_dcd.h"
#include "xusbd_trace.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // MACROS //////////////////////////////////////////////////////////////////////
#define xUSBD_MOS_Property(prop_name, utf16_data, data_length) {0x0001U, (prop_name), (uint8_t *)(utf16_data), (data_length)}
#define xUSBD_CLASS_ENDPOINT_DESC_SIZE(speed)                                                                                              \
    (USB_ENDPOINT_DESC_LEN + (((speed) == USB_SPEED_SUPER) ? USB_SS_ENDPOINT_COMPANION_DESC_LEN : 0U))

// Cast class_ctx->callbacks to the specific callbacks struct type without the
// two-step void*/cast idiom. Returns NULL when no callbacks are registered.
#define xUSBD_CLASS_CALLBACKS(class_ctx, type) ((type *)((class_ctx)->callbacks))

// Endpoint direction values for xUSBD_Class_Allocate_Endpoint().
#define USB_EP_DIR_IN  0x80U
#define USB_EP_DIR_OUT 0x00U

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef struct xUSBD_Class_Driver xUSBD_Class_Driver_t;
    typedef struct xUSBD_Class_Context xUSBD_Class_Context_t;
    typedef struct xUSBD_Device_Context xUSBD_Device_Context_t;

    // Bundles the (data pointer, length) output for control IN responses.
    // Replaces the (uint8_t **data, uint32_t *length) output-parameter pair
    // throughout the class driver dispatch table and public IN-request APIs.
    typedef struct
    {
        uint8_t *data;
        uint32_t length;
    } xUSBD_Response_t;

    typedef struct
    {
        uint16_t property_type;
        char *property_name;
        uint8_t *property_data;
        uint16_t data_length;
    } xUSBD_MOS_Property_t;

    typedef enum
    {
        xUSBD_LIFECYCLE_CREATED = 0,
        xUSBD_LIFECYCLE_INITIALIZED,
        xUSBD_LIFECYCLE_CLASSES_REGISTERED,
        xUSBD_LIFECYCLE_STARTED,
        xUSBD_LIFECYCLE_CONFIGURED,
        xUSBD_LIFECYCLE_STOPPED,
    } xUSBD_Lifecycle_State_t;

    // Context layouts are public to keep static allocation simple.
    // Application and class code should still use the xUSBD_* accessors below
    // for runtime-owned state instead of depending on individual fields.
    struct xUSBD_Device_Context
    {
        uint8_t port;
        USB_Speed_t speed;
        USB_DCD_Link_State_t link_state;
        xUSBD_DCD_Ops_t *dcd_ops;
        void *dcd_ctx;
        uint8_t configuration_descriptor[xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE];
        uint8_t vendor_string[xUSBD_MAX_STRING_DESCRIPTOR_SIZE];
        uint8_t product_string[xUSBD_MAX_STRING_DESCRIPTOR_SIZE];
        uint8_t serial_string[xUSBD_MAX_STRING_DESCRIPTOR_SIZE];
        uint8_t device_descriptor[32];
        uint8_t device_qualifier_descriptor[32];

        uint8_t bos_descriptor[xUSBD_MAX_BOS_DESCRIPTOR_SIZE];
        uint16_t bos_length;

        uint8_t mos2_descriptor[xUSBD_MAX_MOS2_DESCRIPTOR_SIZE];
        uint16_t mos2_length;

        // 0xFF = BOS never built; matches no valid USB_Speed_t value.
        // Set in xUSBD_Init; updated by device_build_bos_descriptor.
        uint8_t bos_built_for_speed;
        bool mos2_built; // MOS2 is speed-independent; built once, never rebuilt.

        xUSBD_Class_Context_t *class_list_head;
        xUSBD_Class_Context_t *class_list_tail;
        xUSBD_Class_Context_t *registering_class;
        xUSBD_Class_Context_t *control_request_class;
        xUSBD_Class_Context_t *interface_to_class[xUSBD_MAX_INTERFACE_COUNT];
        xUSBD_Class_Context_t *endpoint_to_class[xUSBD_MAX_ENDPOINT_MAP_ENTRIES];
        xUSBD_Class_Context_t *string_to_class[xUSBD_MAX_STRING_MAP_ENTRIES];

        USB_Setup_Request_t request;
        xUSBD_Lifecycle_State_t lifecycle_state;
        uint8_t configuration_value;
        uint8_t address_value;
        bool is_configured;
        bool is_started;
        bool is_addressed;
        bool is_remote_wake_enabled;
        bool is_self_powered;
        uint8_t request_phase;
        uint8_t control_data[xUSBD_MAX_EP0_DATA_SIZE];
        uint8_t usb3_system_exit_latency[6];
        uint16_t usb3_isoch_delay;

#if xTRACE_ENABLE
        struct xTRACE_Context_t *trace_ctx; // Optional; NULL until xUSBD_Trace_Init is called.
#endif

        uint8_t next_interface;
        uint8_t next_in_ep;
        uint8_t next_out_ep;
        uint8_t next_string_index;
    };

    // Class-specific public contexts embed this as their first member.
    // Class drivers and apps receive/use xUSBD_Class_Context_t* handles.
    struct xUSBD_Class_Context
    {
        xUSBD_Class_Context_t *next;
        xUSBD_Class_Driver_t *driver;
        xUSBD_Device_Context_t *device_ctx;
        uint8_t first_interface;
        void *app_context;
        void *callbacks;
        uint16_t ep_mps;
        const char *interface_string;
        uint8_t interface_string_index;
        xUSBD_MOS_Property_t *mos_props;
        // Non-NULL: emit MS OS 2.0 CompatibleID Feature (wType=0x0003) for this
        // class.  String is at most 8 chars, zero-padded (e.g. "WINUSB").
        // Required for Windows to auto-install the matching inbox driver.
        const char *ms_compatible_id;
    };

    struct xUSBD_Class_Driver
    {
        xRETURN_t (*init_instance)(xUSBD_Class_Context_t *class_ctx);
        // Writes the class descriptor bytes into buffer and returns the byte count
        // written. The returned count is used by the configuration descriptor
        // builder as the descriptor size - no separate size measurement is needed.
        // A return value of 0 signals a build failure (programming error).
        uint32_t (*build_descriptor)(xUSBD_Class_Context_t *class_ctx, uint8_t *buffer, USB_Speed_t speed);
        xRETURN_t (*bus_event)(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event);
        // IN control request: populate response->data and response->length.
        // The current setup request is at class_ctx->device_ctx->request (host byte order).
        xRETURN_t (*control_in_request)(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response);
        // OUT control request: data/length are the received payload bytes (if any).
        // The current setup request is at class_ctx->device_ctx->request.
        xRETURN_t (*control_out_request)(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length);
        xRETURN_t (*data_received)(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
        xRETURN_t (*transmit_complete)(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
        xRETURN_t (*control_transfer_complete)(xUSBD_Class_Context_t *class_ctx, USB_Setup_Request_t *request);
    };

    // INLINE HELPERS //////////////////////////////////////////////////////////////

    static inline uint16_t ep_max_mps(USB_Speed_t speed, uint8_t ep_type)
    {
        if (ep_type == USB_ENDP_TYPE_BULK)
        {
            return (uint16_t)((speed == USB_SPEED_SUPER) ? 1024U : (speed == USB_SPEED_HIGH) ? 512U : 64U);
        }
        if (ep_type == USB_ENDP_TYPE_ISOC)
        {
            return (uint16_t)((speed == USB_SPEED_SUPER) ? 1024U : (speed == USB_SPEED_HIGH) ? 1024U : 1023U);
        }
        if (ep_type == USB_ENDP_TYPE_INTR)
        {
            return (uint16_t)((speed == USB_SPEED_SUPER) ? 1024U : (speed == USB_SPEED_HIGH) ? 1024U : 64U);
        }
        return 64U;
    }

    // Descriptor builder helpers
    static inline uint8_t *
    build_iad_descriptor(uint8_t *ptr, uint8_t first_interface, uint8_t count, uint8_t cls, uint8_t subcls, uint8_t proto, uint8_t istr)
    {
        USB_Interface_Association_Descriptor_t *d = (USB_Interface_Association_Descriptor_t *)ptr;
        d->bLength = USB_IAD_DESC_LEN;
        d->bDescriptorType = USB_DESC_TYPE_IAD;
        d->bFirstInterface = first_interface;
        d->bInterfaceCount = count;
        d->bFunctionClass = cls;
        d->bFunctionSubClass = subcls;
        d->bFunctionProtocol = proto;
        d->iFunction = istr;
        return ptr + USB_IAD_DESC_LEN;
    }

    static inline uint8_t *build_interface_descriptor(uint8_t *ptr,
                                                      uint8_t interface,
                                                      uint8_t alt,
                                                      uint8_t num_ep,
                                                      uint8_t cls,
                                                      uint8_t subcls,
                                                      uint8_t proto,
                                                      uint8_t istr)
    {
        USB_Interface_Descriptor_t *d = (USB_Interface_Descriptor_t *)ptr;
        d->bLength = USB_INTERFACE_DESC_LEN;
        d->bDescriptorType = USB_DESC_TYPE_INTERFACE;
        d->bInterfaceNumber = interface;
        d->bAlternateSetting = alt;
        d->bNumEndpoints = num_ep;
        d->bInterfaceClass = cls;
        d->bInterfaceSubClass = subcls;
        d->bInterfaceProtocol = proto;
        d->iInterface = istr;
        return ptr + USB_INTERFACE_DESC_LEN;
    }

    static inline uint8_t *build_endpoint_descriptor(uint8_t *ptr,
                                                     uint8_t ep_address,
                                                     uint8_t ep_type,
                                                     uint16_t mps,
                                                     uint8_t interval,
                                                     USB_Speed_t speed,
                                                     uint8_t ss_burst,
                                                     uint8_t ss_attr,
                                                     uint16_t ss_bpi)
    {
        USB_Endpoint_Descriptor_t *ep = (USB_Endpoint_Descriptor_t *)ptr;
        ep->bLength = USB_ENDPOINT_DESC_LEN;
        ep->bDescriptorType = USB_DESC_TYPE_ENDPOINT;
        ep->bEndpointAddress = ep_address;
        ep->bmAttributes = ep_type;
        ep->wMaxPacketSize = xCPU_TO_LE16(mps);
        ep->bInterval = interval;
        ptr += USB_ENDPOINT_DESC_LEN;
        if (speed == USB_SPEED_SUPER)
        {
            USB_SS_Endpoint_Companion_Descriptor_t *c = (USB_SS_Endpoint_Companion_Descriptor_t *)ptr;
            c->bLength = USB_SS_ENDPOINT_COMPANION_DESC_LEN;
            c->bDescriptorType = USB_DESC_TYPE_SS_ENDPOINT_COMPANION;
            c->bMaxBurst = ss_burst;
            c->bmAttributes = ss_attr;
            c->wBytesPerInterval = xCPU_TO_LE16(ss_bpi);
            ptr += USB_SS_ENDPOINT_COMPANION_DESC_LEN;
        }
        return ptr;
    }

    // PUBLIC CLASS MANAGER API ////////////////////////////////////////////////////
    // Prefer the class-specific xUSBD_XXX_Register() inline helpers over calling this directly.
    xRETURN_t
    xUSBD_Class_Register(xUSBD_Device_Context_t *device_ctx, xUSBD_Class_Context_t *class_ctx, xUSBD_Class_Driver_t *class_driver);
    xRETURN_t xUSBD_Class_Set_Interface_String(xUSBD_Class_Context_t *class_ctx, const char *name);
    xRETURN_t xUSBD_Class_Set_MOS_Properties(xUSBD_Class_Context_t *class_ctx, xUSBD_MOS_Property_t *props);
    xRETURN_t xUSBD_Class_Set_App_Context(xUSBD_Class_Context_t *class_ctx, void *app_context);
    xRETURN_t xUSBD_Class_Get_App_Context(const xUSBD_Class_Context_t *class_ctx, void **app_context);

    // CLASS DRIVER HELPERS ////////////////////////////////////////////////////////
    xRETURN_t xUSBD_Class_Allocate_Interface(xUSBD_Class_Context_t *class_ctx, uint8_t *interface);
    xRETURN_t xUSBD_Class_Allocate_Endpoint(xUSBD_Class_Context_t *class_ctx, uint8_t direction, uint8_t *ep_addr);
    xRETURN_t xUSBD_Class_Allocate_String(xUSBD_Class_Context_t *class_ctx, uint8_t *string_index);
    xRETURN_t xUSBD_Class_DCD_EP_Init(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t ep_type, uint16_t mps);
    xRETURN_t xUSBD_Class_DCD_EP_Deinit(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr);
    xRETURN_t xUSBD_Class_DCD_EP_Receive(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
    xRETURN_t
    xUSBD_Class_DCD_EP_Send(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length, bool is_zlp_required);
    xRETURN_t xUSBD_Class_DCD_EP_Transfer_Queue(xUSBD_Class_Context_t *class_ctx, const xUSBD_DCD_Transfer_t *transfer);
    xRETURN_t xUSBD_Class_DCD_EP_Stall(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr);
    xRETURN_t xUSBD_Class_DCD_EP_Clear_Stall(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr);
    xRETURN_t xUSBD_Class_Set_EP_MPS(xUSBD_Class_Context_t *class_ctx, uint16_t ep_mps);
    xRETURN_t xUSBD_Class_Get_EP_MPS(const xUSBD_Class_Context_t *class_ctx, uint16_t *ep_mps);
    xRETURN_t xUSBD_Class_Get_Speed(const xUSBD_Class_Context_t *class_ctx, USB_Speed_t *speed);
    xRETURN_t xUSBD_Class_Get_Control_Buffer(xUSBD_Class_Context_t *class_ctx, uint8_t **buffer, uint32_t *length);
    xRETURN_t xUSBD_Class_Set_Callbacks(xUSBD_Class_Context_t *class_ctx, void *callbacks);
    xRETURN_t xUSBD_Class_Get_Callbacks(const xUSBD_Class_Context_t *class_ctx, void **callbacks);

    // DEVICE CORE DISPATCH ////////////////////////////////////////////////////////
    xRETURN_t xUSBD_Build_Config_Descriptor(xUSBD_Device_Context_t *device_ctx,
                                            uint8_t *buffer,
                                            uint16_t max_length,
                                            uint8_t descriptor_type,
                                            USB_Speed_t target_speed);
    xRETURN_t xUSBD_Class_Bus_Event_Process(xUSBD_Device_Context_t *device_ctx, USB_DCD_Event_t event);
    xRETURN_t xUSBD_Class_In_Request_Process(xUSBD_Device_Context_t *device_ctx, xUSBD_Response_t *response);
    xRETURN_t xUSBD_Class_Out_Request_Process(xUSBD_Device_Context_t *device_ctx, uint8_t *control_data, uint32_t length);
    xRETURN_t xUSBD_Class_Data_Received(xUSBD_Device_Context_t *device_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
    xRETURN_t xUSBD_Class_Data_Sent(xUSBD_Device_Context_t *device_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length);
    xRETURN_t xUSBD_Class_Get_String_Process(xUSBD_Device_Context_t *device_ctx, uint8_t string_index, uint8_t **data);
    xRETURN_t xUSBD_Class_Control_Transfer_Complete_Process(xUSBD_Device_Context_t *device_ctx, USB_Setup_Request_t *request);

#ifdef __cplusplus
}
#endif

#endif // XUSBD_CLASS_H
// EOF /////////////////////////////////////////////////////////////////////////////
