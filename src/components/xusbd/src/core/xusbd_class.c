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

// @file xusbd_class.c
// @brief USB device class manager: registration, resource allocation, and event dispatch.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusb_defs.h"
#include "xusb_win_defs.h"
#include "xusbd_return.h"
#include "xusbd_config.h"
#include "xusbd_class.h"
#include "xassert.h"

#include "xusbd_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////
xSTATIC_ASSERT(sizeof(USB_Interface_Descriptor_t) == USB_INTERFACE_DESC_LEN, "USB interface descriptor size changed");
xSTATIC_ASSERT(sizeof(USB_Endpoint_Descriptor_t) == USB_ENDPOINT_DESC_LEN, "USB endpoint descriptor size changed");
xSTATIC_ASSERT(sizeof(USB_SS_Endpoint_Companion_Descriptor_t) == USB_SS_ENDPOINT_COMPANION_DESC_LEN,
               "USB SuperSpeed endpoint companion descriptor size changed");
xSTATIC_ASSERT(sizeof(USB_Interface_Association_Descriptor_t) == USB_IAD_DESC_LEN, "USB IAD descriptor size changed");
// TYPES //////////////////////////////////////////////////////////////////////////
typedef struct
{
    uint8_t interface;
    uint8_t in_ep;
    uint8_t out_ep;
    uint8_t string_index;
} xUSBD_Class_Resource_Snapshot_t;

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static uint16_t mos_append_registry_property(uint8_t *buffer, xUSBD_MOS_Property_t *property, uint16_t name_utf16_length);
static uint32_t endpoint_map_index(uint8_t ep_addr);
static void device_build_ms_os_20_descriptor(xUSBD_Device_Context_t *device_ctx);
static void device_build_bos_descriptor(xUSBD_Device_Context_t *device_ctx, USB_Speed_t speed);
static xRETURN_t device_build_descriptors(xUSBD_Device_Context_t *device_ctx, USB_Speed_t speed);
static xRETURN_t class_resource_rollback(xUSBD_Device_Context_t *device_ctx, xUSBD_Class_Resource_Snapshot_t start);
static xUSBD_Class_Resource_Snapshot_t class_resource_snapshot_get(const xUSBD_Device_Context_t *device_ctx);
static xRETURN_t
class_register_config_validate(xUSBD_Device_Context_t *device_ctx, xUSBD_Class_Context_t *class_ctx, xUSBD_Class_Driver_t *class_driver);
static void class_context_prepare_for_registration(xUSBD_Device_Context_t *device_ctx,
                                                   xUSBD_Class_Context_t *class_ctx,
                                                   xUSBD_Class_Driver_t *class_driver);
static void class_context_clear_failed_registration(xUSBD_Class_Context_t *class_ctx);
static void class_register_list_append(xUSBD_Device_Context_t *device_ctx, xUSBD_Class_Context_t *class_ctx);
static void class_register_lifecycle_mark(xUSBD_Device_Context_t *device_ctx);
static xUSBD_Class_Context_t *request_recipient_class_get(xUSBD_Device_Context_t *device_ctx);
static xRETURN_t class_control_request_broadcast_in(xUSBD_Device_Context_t *device_ctx, xUSBD_Response_t *response);
static xRETURN_t class_control_request_broadcast_out(xUSBD_Device_Context_t *device_ctx, uint8_t *control_data, uint32_t length);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
static uint16_t mos_append_registry_property(uint8_t *buffer, xUSBD_MOS_Property_t *property, uint16_t name_utf16_length)
{
    if (buffer == NULL || property == NULL)
    {
        return 0U;
    }

    uint16_t total_length = 10U + name_utf16_length + property->data_length;

    xWrite_LE16(&buffer[0], total_length);
    buffer[2] = 0x04U; // MS_OS_20_FEATURE_REG_PROPERTY
    buffer[3] = 0x00U;
    xWrite_LE16(&buffer[4], property->property_type);
    xWrite_LE16(&buffer[6], name_utf16_length);

    uint16_t offset = 8U;
    for (uint32_t i = 0U; property->property_name[i] != '\0'; i++)
    {
        buffer[offset++] = (uint8_t)property->property_name[i];
        buffer[offset++] = 0x00U;
    }
    buffer[offset++] = 0x00U; // Null term
    buffer[offset++] = 0x00U;

    xWrite_LE16(&buffer[offset], property->data_length);
    offset += 2U;

    for (uint32_t i = 0U; i < property->data_length; i++)
    {
        buffer[offset++] = property->property_data[i];
    }

    return offset;
}

static uint32_t endpoint_map_index(uint8_t ep_addr)
{
    uint32_t index = (uint32_t)(ep_addr & 0x0FU);

    if ((ep_addr & 0x80U) != 0U)
    {
        index += xUSBD_MAX_ENDPOINT_MAP_ENTRIES / 2U;
    }

    xASSERT(index < xUSBD_MAX_ENDPOINT_MAP_ENTRIES, "endpoint index out of range");
    return index;
}

static xRETURN_t class_resource_rollback(xUSBD_Device_Context_t *device_ctx, xUSBD_Class_Resource_Snapshot_t start)
{
    for (uint32_t i = start.interface; i < device_ctx->next_interface && i < xUSBD_MAX_INTERFACE_COUNT; i++)
    {
        device_ctx->interface_to_class[i] = NULL;
    }

    for (uint32_t ep_addr = start.in_ep; ep_addr < device_ctx->next_in_ep; ep_addr++)
    {
        uint32_t index = endpoint_map_index((uint8_t)ep_addr);
        if (index < xUSBD_MAX_ENDPOINT_MAP_ENTRIES)
        {
            device_ctx->endpoint_to_class[index] = NULL;
        }
    }

    for (uint32_t ep_addr = start.out_ep; ep_addr < device_ctx->next_out_ep; ep_addr++)
    {
        uint32_t index = endpoint_map_index((uint8_t)ep_addr);
        if (index < xUSBD_MAX_ENDPOINT_MAP_ENTRIES)
        {
            device_ctx->endpoint_to_class[index] = NULL;
        }
    }

    for (uint32_t i = start.string_index; i < device_ctx->next_string_index && i < xUSBD_MAX_STRING_MAP_ENTRIES; i++)
    {
        device_ctx->string_to_class[i] = NULL;
    }

    device_ctx->next_interface = start.interface;
    device_ctx->next_in_ep = start.in_ep;
    device_ctx->next_out_ep = start.out_ep;
    device_ctx->next_string_index = start.string_index;

    return xRETURN_OK;
}

static xUSBD_Class_Resource_Snapshot_t class_resource_snapshot_get(const xUSBD_Device_Context_t *device_ctx)
{
    xUSBD_Class_Resource_Snapshot_t snapshot = {
        .interface = device_ctx->next_interface,
        .in_ep = device_ctx->next_in_ep,
        .out_ep = device_ctx->next_out_ep,
        .string_index = device_ctx->next_string_index,
    };

    return snapshot;
}

static xRETURN_t
class_register_config_validate(xUSBD_Device_Context_t *device_ctx, xUSBD_Class_Context_t *class_ctx, xUSBD_Class_Driver_t *class_driver)
{
    if ((device_ctx == NULL) || (class_ctx == NULL) || (class_driver == NULL))
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    if (device_ctx->is_started)
    {
        return xRETURN_xERR_xUSBD_ALREADY_INITIALIZED;
    }

    if ((class_driver->init_instance == NULL) || (class_driver->build_descriptor == NULL))
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    return xRETURN_OK;
}

static void class_context_prepare_for_registration(xUSBD_Device_Context_t *device_ctx,
                                                   xUSBD_Class_Context_t *class_ctx,
                                                   xUSBD_Class_Driver_t *class_driver)
{
    class_ctx->driver = class_driver;
    class_ctx->device_ctx = device_ctx;
    class_ctx->next = NULL;
    class_ctx->first_interface = device_ctx->next_interface;
}

static void class_context_clear_failed_registration(xUSBD_Class_Context_t *class_ctx)
{
    class_ctx->driver = NULL;
    class_ctx->device_ctx = NULL;
    class_ctx->next = NULL;
    class_ctx->first_interface = 0U;
}

static void class_register_list_append(xUSBD_Device_Context_t *device_ctx, xUSBD_Class_Context_t *class_ctx)
{
    if (device_ctx->class_list_tail == NULL)
    {
        device_ctx->class_list_head = class_ctx;
        device_ctx->class_list_tail = class_ctx;
        return;
    }

    device_ctx->class_list_tail->next = class_ctx;
    device_ctx->class_list_tail = class_ctx;
}

static void class_register_lifecycle_mark(xUSBD_Device_Context_t *device_ctx)
{
    if (device_ctx->lifecycle_state == xUSBD_LIFECYCLE_INITIALIZED)
    {
        device_ctx->lifecycle_state = xUSBD_LIFECYCLE_CLASSES_REGISTERED;
    }
}

static xUSBD_Class_Context_t *request_recipient_class_get(xUSBD_Device_Context_t *device_ctx)
{
    uint8_t recipient = device_ctx->request.bRequestType & USB_REQ_RECIPIENT_MASK;
    uint16_t w_index = device_ctx->request.wIndex;

    if (recipient == USB_REQ_RECIPIENT_INTERFACE)
    {
        uint8_t interface = xU16_LOW_BYTE(w_index);
        if (interface < xUSBD_MAX_INTERFACE_COUNT)
        {
            return device_ctx->interface_to_class[interface];
        }
    }
    else if (recipient == USB_REQ_RECIPIENT_ENDPOINT)
    {
        uint32_t index = endpoint_map_index(xU16_LOW_BYTE(w_index));
        if (index < xUSBD_MAX_ENDPOINT_MAP_ENTRIES)
        {
            return device_ctx->endpoint_to_class[index];
        }
    }

    return NULL;
}

static xRETURN_t class_control_request_broadcast_in(xUSBD_Device_Context_t *device_ctx, xUSBD_Response_t *response)
{
    xUSBD_Class_Context_t *curr = device_ctx->class_list_head;
    while (curr != NULL)
    {
        if (curr->driver->control_in_request != NULL)
        {
            xRETURN_t status = curr->driver->control_in_request(curr, response);
            if (status == xRETURN_OK)
            {
                device_ctx->control_request_class = curr;
                xUSBD_TRACE_E1(device_ctx, xUSBD_TRACE_CODE_CLASS_REQUEST, curr->first_interface);
                return xRETURN_OK;
            }
        }
        curr = curr->next;
    }
    return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
}

static xRETURN_t class_control_request_broadcast_out(xUSBD_Device_Context_t *device_ctx, uint8_t *control_data, uint32_t length)
{
    xUSBD_Class_Context_t *curr = device_ctx->class_list_head;
    while (curr != NULL)
    {
        if (curr->driver->control_out_request != NULL)
        {
            xRETURN_t status = curr->driver->control_out_request(curr, control_data, length);
            if (status == xRETURN_OK)
            {
                device_ctx->control_request_class = curr;
                xUSBD_TRACE_E1(device_ctx, xUSBD_TRACE_CODE_CLASS_REQUEST, curr->first_interface);
                return xRETURN_OK;
            }
        }
        curr = curr->next;
    }
    return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
}

static void device_build_ms_os_20_descriptor(xUSBD_Device_Context_t *device_ctx)
{
    uint8_t *buffer = device_ctx->mos2_descriptor;
    uint16_t offset = 10;
    bool use_function_subsets = (device_ctx->class_list_head != NULL) && (device_ctx->class_list_head->next != NULL);

    // Header (Set Header Descriptor)
    USB_MS_OS_20_Set_Header_Descriptor_t *set_hdr = (USB_MS_OS_20_Set_Header_Descriptor_t *)buffer;
    set_hdr->wLength = 10;
    set_hdr->wDescriptorType = USB_MOS2_SET_HEADER_DESCRIPTOR;
    set_hdr->dwWindowsVersion = 0x06030000;
    set_hdr->wTotalLength = 0; // Updated later

    USB_MS_OS_20_Subset_Header_Configuration_t *config_hdr = NULL;
    if (use_function_subsets)
    {
        config_hdr = (USB_MS_OS_20_Subset_Header_Configuration_t *)&buffer[10];
        config_hdr->wLength = 8;
        config_hdr->wDescriptorType = USB_MOS2_SUBSET_HEADER_CONFIGURATION;
        config_hdr->bConfigurationValue = 0x00;
        config_hdr->bReserved = 0x00;
        config_hdr->wTotalLength = 0; // Updated later
        offset += 8;
    }

    xUSBD_Class_Context_t *curr = device_ctx->class_list_head;
    while (curr != NULL)
    {
        bool has_compat_id = (curr->ms_compatible_id != NULL) && (curr->ms_compatible_id[0] != '\0');
        bool has_mos_props = (curr->mos_props != NULL) && (curr->mos_props[0].property_name != NULL);

        if (has_compat_id || has_mos_props)
        {
            USB_MS_OS_20_Subset_Header_Function_t *func_hdr = NULL;
            uint16_t func_length = 0;

            if (use_function_subsets)
            {
                if (offset + 8U > xUSBD_MAX_MOS2_DESCRIPTOR_SIZE)
                {
                    device_ctx->mos2_length = 0;
                    return;
                }

                func_hdr = (USB_MS_OS_20_Subset_Header_Function_t *)&buffer[offset];
                func_hdr->wLength = 8;
                func_hdr->wDescriptorType = USB_MOS2_SUBSET_HEADER_FUNCTION;
                func_hdr->bFirstInterface = curr->first_interface;
                func_hdr->bReserved = 0x00;
                func_hdr->wSubsetLength = 0;

                offset += 8;
                func_length = 8;
            }

            // CompatibleID Feature Descriptor (wDescriptorType=0x0003, 20 bytes).
            // This is what Windows reads to select the inbox driver (e.g. WinUSB.sys).
            if (has_compat_id)
            {
                if (offset + 20U > xUSBD_MAX_MOS2_DESCRIPTOR_SIZE)
                {
                    device_ctx->mos2_length = 0;
                    return;
                }
                uint8_t *feat = &buffer[offset];
                feat[0]  = 20U;        feat[1]  = 0U;       // wLength = 20
                feat[2]  = (uint8_t)(USB_MOS2_FEATURE_COMPATIBLE_ID & 0xFFU);
                feat[3]  = (uint8_t)(USB_MOS2_FEATURE_COMPATIBLE_ID >> 8U);  // wDescriptorType = 0x0003
                // CompatibleID and SubCompatibleID are fixed 8-byte zero-padded fields.
                memset(&feat[4U], 0, 16U);
                for (uint32_t k = 0U; k < 8U && curr->ms_compatible_id[k] != '\0'; k++)
                {
                    feat[4U + k] = (uint8_t)curr->ms_compatible_id[k];
                }

                offset += 20U;
                if (use_function_subsets)
                {
                    func_length += 20U;
                }
            }

            // Registry Property Descriptors (wDescriptorType=0x0004, e.g. DeviceInterfaceGUID).
            if (has_mos_props)
            {
                for (uint32_t i = 0U; curr->mos_props[i].property_name; i++)
                {
                    xUSBD_MOS_Property_t *prop = &curr->mos_props[i];
                    size_t name_len = strlen(prop->property_name);
                    xASSERT(name_len < 0x7FFFU, "MOS property name too long");
                    uint16_t name_utf16_len = (uint16_t)((name_len + 1U) * 2U);
                    uint16_t prop_size = 10U + name_utf16_len + prop->data_length;

                    if (offset + prop_size > xUSBD_MAX_MOS2_DESCRIPTOR_SIZE)
                    {
                        device_ctx->mos2_length = 0;
                        return;
                    }
                    uint16_t prop_length = mos_append_registry_property(&buffer[offset], prop, name_utf16_len);
                    offset += prop_length;
                    if (use_function_subsets)
                    {
                        func_length += prop_length;
                    }
                }
            }

            if (use_function_subsets)
            {
                func_hdr->wSubsetLength = func_length;
            }
        }
        curr = curr->next;
    }

    if (offset > 10)
    {
        set_hdr->wTotalLength = offset;
        if (use_function_subsets)
        {
            config_hdr->wTotalLength = offset - 10;
        }
        device_ctx->mos2_length = offset;
    }
    else
    {
        device_ctx->mos2_length = 0;
    }
}

static void device_build_bos_descriptor(xUSBD_Device_Context_t *device_ctx, USB_Speed_t speed)
{
    if ((uint8_t)speed == device_ctx->bos_built_for_speed)
    {
        return;
    }

    uint8_t *bos = device_ctx->bos_descriptor;
    uint16_t total_length = 0;

    // BOS Header
    USB_BOS_Descriptor_t *bos_hdr = (USB_BOS_Descriptor_t *)bos;
    bos_hdr->bLength = 5;
    bos_hdr->bDescriptorType = USB_DESC_TYPE_BOS;
    bos_hdr->bNumDeviceCaps = 1; // Base caps: Ext USB 2.0
    total_length += 5;

    // USB 2.0 Extension
    USB_Device_Capability_Descriptor_t *ext = (USB_Device_Capability_Descriptor_t *)&bos[total_length];
    ext->bLength = 7;
    ext->bDescriptorType = USB_DESC_TYPE_DEVICE_CAPABILITY;
    ext->bDevCapabilityType = USB_CAPABILITY_20_EXTENTION; // USB 2.0 EXTENSION
    ext->bmAttributes = xCPU_TO_LE32(0x00000002);          // LPM Support
    total_length += 7;

    if (speed == USB_SPEED_SUPER)
    {
        bos_hdr->bNumDeviceCaps++;
        USB_SS_Device_Capability_Descriptor_t *ss = (USB_SS_Device_Capability_Descriptor_t *)&bos[total_length];
        ss->bLength = 10;
        ss->bDescriptorType = USB_DESC_TYPE_DEVICE_CAPABILITY;
        ss->bDevCapabilityType = USB_CAPABILITY_SUPER_SPEED_USB;
        ss->bmAttributes = 0;
        ss->wSpeedsSupported = xCPU_TO_LE16(0x000E); // FS, HS, SS
        ss->bFunctionalitySupport = 3; // SS (SuperSpeed) minimum
        ss->bU1DevExitLat = 10;
        ss->wU2DevExitLat = xCPU_TO_LE16(0);
        total_length += 10;
    }

    if (!device_ctx->mos2_built)
    {
        device_build_ms_os_20_descriptor(device_ctx);
        device_ctx->mos2_built = true;
    }
    if (device_ctx->mos2_length > 0)
    {
        bos_hdr->bNumDeviceCaps++;
        USB_MS_OS_20_Platform_Cap_t *plat = (USB_MS_OS_20_Platform_Cap_t *)&bos[total_length];
        plat->bLength = 28;
        plat->bDescriptorType = USB_DESC_TYPE_DEVICE_CAPABILITY;
        plat->bDevCapabilityType = 0x05; // PLATFORM
        plat->bReserved = 0;

        const uint8_t ms_os_20_uuid[16] = {0xDF, 0x60, 0xDD, 0xD8, 0x89, 0x45, 0xC7, 0x4C, 0x9C, 0xD2, 0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F};
        memcpy(plat->PlatformCapabilityUUID, ms_os_20_uuid, 16);

        plat->dwWindowsVersion = 0x06030000;
        plat->wMSOSDescriptorSetTotalLength = device_ctx->mos2_length;
        plat->bMS_VendorCode = xUSBD_WINUSB_VENDOR_CODE;
        plat->bAltEnumCode = 0x00;
        total_length += 28;
    }

    bos_hdr->wTotalLength = xCPU_TO_LE16(total_length);
    device_ctx->bos_length = total_length;
    device_ctx->bos_built_for_speed = (uint8_t)speed;
}

static xRETURN_t device_build_descriptors(xUSBD_Device_Context_t *device_ctx, USB_Speed_t speed)
{
    // Build the primary descriptor once at initialization
    xRETURN_t status = xUSBD_Build_Config_Descriptor(device_ctx, device_ctx->configuration_descriptor, xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE,
                                                     USB_DESC_TYPE_CONFIGURATION, speed);
    if (status != xRETURN_OK)
    {
        return status;
    }

    device_build_bos_descriptor(device_ctx, speed);
    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////
xRETURN_t xUSBD_Build_Config_Descriptor(xUSBD_Device_Context_t *device_ctx,
                                        uint8_t *buffer,
                                        uint16_t max_length,
                                        uint8_t descriptor_type,
                                        USB_Speed_t target_speed)
{
    uint8_t staged_descriptor[xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE];
    uint16_t build_limit = (max_length < xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE) ? max_length : xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE;

    if (device_ctx == NULL || buffer == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    // Validate all class driver pointers before writing anything.
    xUSBD_Class_Context_t *curr = device_ctx->class_list_head;
    while (curr != NULL)
    {
        if (curr->driver == NULL || curr->driver->build_descriptor == NULL)
        {
            return xRETURN_xERR_xUSBD_NULL_POINTER;
        }
        curr = curr->next;
    }

    if (USB_CONFIGURATION_DESC_LEN > build_limit)
    {
        return xRETURN_xERR_xUSBD_INSUFFICIENT_BUFFER_SIZE;
    }

    // Write config header; wTotalLength and bNumInterfaces patched after the
    // single-pass class descriptor build below.
    USB_Configuration_Descriptor_t *config = (USB_Configuration_Descriptor_t *)staged_descriptor;
    config->bLength = USB_CONFIGURATION_DESC_LEN;
    config->bDescriptorType = descriptor_type;
    config->wTotalLength = 0;
    config->bNumInterfaces = 0;
    config->bConfigurationValue = 0x01;
    config->iConfiguration = 0x00;
    config->bmAttributes = 0x80U;
    config->MaxPower = 250;

    // Single pass: build each class descriptor in-place and accumulate size.
    // build_descriptor() returns the byte count written (same value descriptor_size()
    // would return for the same inputs), eliminating the separate measurement walk.
    uint8_t *ptr = staged_descriptor + USB_CONFIGURATION_DESC_LEN;
    uint32_t total_length = USB_CONFIGURATION_DESC_LEN;

    curr = device_ctx->class_list_head;
    while (curr != NULL)
    {
        uint32_t written = curr->driver->build_descriptor(curr, ptr, target_speed);
        if (written == 0U || (total_length + written) > build_limit)
        {
            return xRETURN_xERR_xUSBD_INSUFFICIENT_BUFFER_SIZE;
        }
        ptr += written;
        total_length += written;
        curr = curr->next;
    }

    config->wTotalLength = xCPU_TO_LE16((uint16_t)total_length);
    config->bNumInterfaces = device_ctx->next_interface;
    (void)memcpy(buffer, staged_descriptor, total_length);

    return xRETURN_OK;
}

xRETURN_t xUSBD_Class_Allocate_Interface(xUSBD_Class_Context_t *class_ctx, uint8_t *interface)
{
    if (class_ctx == NULL || class_ctx->device_ctx == NULL || interface == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    xUSBD_Device_Context_t *device_ctx = class_ctx->device_ctx;

    if (device_ctx->registering_class == NULL)
    {
        return xRETURN_xERR_xUSBD_INVALID_CONFIGURATION;
    }

    if (device_ctx->next_interface >= xUSBD_MAX_INTERFACE_COUNT)
    {
        return xRETURN_xERR_xUSBD_RESOURCE_EXHAUSTED;
    }

    *interface = device_ctx->next_interface++;
    device_ctx->interface_to_class[*interface] = device_ctx->registering_class;

    return xRETURN_OK;
}

xRETURN_t xUSBD_Class_Allocate_Endpoint(xUSBD_Class_Context_t *class_ctx, uint8_t direction, uint8_t *ep_addr)
{
    if (class_ctx == NULL || class_ctx->device_ctx == NULL || ep_addr == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    xUSBD_Device_Context_t *device_ctx = class_ctx->device_ctx;

    if (device_ctx->registering_class == NULL)
    {
        return xRETURN_xERR_xUSBD_INVALID_CONFIGURATION;
    }

    if ((direction & 0x80) != 0)
    {
        if ((device_ctx->next_in_ep & USB_ENDP_ADDR_MASK) == 0U ||
            endpoint_map_index(device_ctx->next_in_ep) >= xUSBD_MAX_ENDPOINT_MAP_ENTRIES)
        {
            return xRETURN_xERR_xUSBD_RESOURCE_EXHAUSTED;
        }
        *ep_addr = device_ctx->next_in_ep++;
    }
    else
    {
        if ((device_ctx->next_out_ep & USB_ENDP_ADDR_MASK) == 0U ||
            endpoint_map_index(device_ctx->next_out_ep) >= xUSBD_MAX_ENDPOINT_MAP_ENTRIES)
        {
            return xRETURN_xERR_xUSBD_RESOURCE_EXHAUSTED;
        }
        *ep_addr = device_ctx->next_out_ep++;
    }

    uint32_t index = endpoint_map_index(*ep_addr);
    device_ctx->endpoint_to_class[index] = device_ctx->registering_class;

    return xRETURN_OK;
}

xRETURN_t xUSBD_Class_Allocate_String(xUSBD_Class_Context_t *class_ctx, uint8_t *string_index)
{
    if (class_ctx == NULL || class_ctx->device_ctx == NULL || string_index == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    xUSBD_Device_Context_t *device_ctx = class_ctx->device_ctx;

    if (device_ctx->registering_class == NULL)
    {
        return xRETURN_xERR_xUSBD_INVALID_CONFIGURATION;
    }

    if (device_ctx->next_string_index >= xUSBD_MAX_STRING_MAP_ENTRIES)
    {
        return xRETURN_xERR_xUSBD_RESOURCE_EXHAUSTED;
    }

    *string_index = device_ctx->next_string_index++;
    device_ctx->string_to_class[*string_index] = device_ctx->registering_class;

    return xRETURN_OK;
}

xRETURN_t xUSBD_Class_DCD_EP_Init(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t ep_type, uint16_t mps)
{
    if ((class_ctx == NULL) || (class_ctx->device_ctx == NULL))
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    return xUSBD_DCD_EP_Init(class_ctx->device_ctx->dcd_ops, class_ctx->device_ctx->dcd_ctx, ep_addr, ep_type, mps);
}

xRETURN_t xUSBD_Class_DCD_EP_Deinit(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr)
{
    if ((class_ctx == NULL) || (class_ctx->device_ctx == NULL))
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    return xUSBD_DCD_EP_Deinit(class_ctx->device_ctx->dcd_ops, class_ctx->device_ctx->dcd_ctx, ep_addr);
}

xRETURN_t xUSBD_Class_DCD_EP_Receive(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    if ((class_ctx == NULL) || (class_ctx->device_ctx == NULL))
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    return xUSBD_DCD_EP_Receive(class_ctx->device_ctx->dcd_ops, class_ctx->device_ctx->dcd_ctx, ep_addr, data, length);
}

xRETURN_t xUSBD_Class_DCD_EP_Send(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length, bool is_zlp_required)
{
    if ((class_ctx == NULL) || (class_ctx->device_ctx == NULL))
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    return xUSBD_DCD_EP_Send(class_ctx->device_ctx->dcd_ops, class_ctx->device_ctx->dcd_ctx, ep_addr, data, length, is_zlp_required);
}

xRETURN_t xUSBD_Class_DCD_EP_Transfer_Queue(xUSBD_Class_Context_t *class_ctx, const xUSBD_DCD_Transfer_t *transfer)
{
    if ((class_ctx == NULL) || (class_ctx->device_ctx == NULL))
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    return xUSBD_DCD_EP_Transfer_Queue(class_ctx->device_ctx->dcd_ops, class_ctx->device_ctx->dcd_ctx, transfer);
}

xRETURN_t xUSBD_Class_DCD_EP_Stall(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr)
{
    if ((class_ctx == NULL) || (class_ctx->device_ctx == NULL))
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    return xUSBD_DCD_EP_Stall(class_ctx->device_ctx->dcd_ops, class_ctx->device_ctx->dcd_ctx, ep_addr);
}

xRETURN_t xUSBD_Class_DCD_EP_Clear_Stall(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr)
{
    if ((class_ctx == NULL) || (class_ctx->device_ctx == NULL))
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    return xUSBD_DCD_EP_Clear_Stall(class_ctx->device_ctx->dcd_ops, class_ctx->device_ctx->dcd_ctx, ep_addr);
}

xRETURN_t xUSBD_Class_Set_EP_MPS(xUSBD_Class_Context_t *class_ctx, uint16_t ep_mps)
{
    if (class_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    class_ctx->ep_mps = ep_mps;

    return xRETURN_OK;
}

xRETURN_t xUSBD_Class_Get_EP_MPS(const xUSBD_Class_Context_t *class_ctx, uint16_t *ep_mps)
{
    if ((class_ctx == NULL) || (ep_mps == NULL))
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    *ep_mps = class_ctx->ep_mps;

    return xRETURN_OK;
}

xRETURN_t xUSBD_Class_Get_Speed(const xUSBD_Class_Context_t *class_ctx, USB_Speed_t *speed)
{
    if ((class_ctx == NULL) || (class_ctx->device_ctx == NULL) || (speed == NULL))
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    *speed = class_ctx->device_ctx->speed;

    return xRETURN_OK;
}

xRETURN_t xUSBD_Class_Get_Control_Buffer(xUSBD_Class_Context_t *class_ctx, uint8_t **buffer, uint32_t *length)
{
    if ((class_ctx == NULL) || (class_ctx->device_ctx == NULL) || (buffer == NULL) || (length == NULL))
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    *buffer = class_ctx->device_ctx->control_data;
    *length = xUSBD_MAX_EP0_DATA_SIZE;

    return xRETURN_OK;
}

xRETURN_t xUSBD_Class_Set_Callbacks(xUSBD_Class_Context_t *class_ctx, void *callbacks)
{
    if (class_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    class_ctx->callbacks = callbacks;

    return xRETURN_OK;
}

xRETURN_t xUSBD_Class_Get_Callbacks(const xUSBD_Class_Context_t *class_ctx, void **callbacks)
{
    if ((class_ctx == NULL) || (callbacks == NULL))
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    *callbacks = class_ctx->callbacks;

    return xRETURN_OK;
}

xRETURN_t xUSBD_Class_Set_Interface_String(xUSBD_Class_Context_t *class_ctx, const char *name)
{
    if (class_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    class_ctx->interface_string = name;

    if (name != NULL && class_ctx->interface_string_index == 0U)
    {
        xUSBD_Device_Context_t *device_ctx = class_ctx->device_ctx;

        if (device_ctx == NULL || device_ctx->is_started)
        {
            return xRETURN_xERR_xUSBD_INVALID_CONFIGURATION;
        }

        if (device_ctx->next_string_index >= xUSBD_MAX_STRING_MAP_ENTRIES)
        {
            return xRETURN_xERR_xUSBD_RESOURCE_EXHAUSTED;
        }

        uint8_t idx = device_ctx->next_string_index++;
        device_ctx->string_to_class[idx] = class_ctx;
        class_ctx->interface_string_index = idx;
    }

    return xRETURN_OK;
}

xRETURN_t xUSBD_Class_Set_MOS_Properties(xUSBD_Class_Context_t *class_ctx, xUSBD_MOS_Property_t *props)
{
    if (class_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    class_ctx->mos_props = props;
    return xRETURN_OK;
}

xRETURN_t xUSBD_Class_Bus_Event_Process(xUSBD_Device_Context_t *device_ctx, USB_DCD_Event_t event)
{
    if (event == USB_DCD_CONNECT_RECEIVED || event == USB_DCD_SPEED_CHANGE_RECEIVED)
    {
        USB_Speed_t speed = device_ctx->speed;
        xRETURN_t status = device_build_descriptors(device_ctx, speed);
        if (status != xRETURN_OK)
        {
            return status;
        }
    }

    xUSBD_Class_Context_t *curr = device_ctx->class_list_head;
    while (curr != NULL)
    {
        if (curr->driver->bus_event)
        {
            (void)curr->driver->bus_event(curr, event);
        }
        curr = curr->next;
    }
    return xRETURN_OK;
}

xRETURN_t xUSBD_Class_In_Request_Process(xUSBD_Device_Context_t *device_ctx, xUSBD_Response_t *response)
{
    uint16_t w_index = device_ctx->request.wIndex;

    if (device_ctx->request.bRequest == xUSBD_WINUSB_VENDOR_CODE && xU16_LOW_BYTE(w_index) == 0x07U)
    {
        if (device_ctx->mos2_length > 0)
        {
            uint16_t w_length = device_ctx->request.wLength;
            response->data = device_ctx->mos2_descriptor;
            response->length = ((uint32_t)w_length < device_ctx->mos2_length) ? w_length : device_ctx->mos2_length;
            return xRETURN_OK;
        }
    }

    xUSBD_Class_Context_t *class_ctx = request_recipient_class_get(device_ctx);
    if (class_ctx != NULL)
    {
        if (class_ctx->driver->control_in_request != NULL)
        {
            xRETURN_t status = class_ctx->driver->control_in_request(class_ctx, response);
            if (status == xRETURN_OK)
            {
                device_ctx->control_request_class = class_ctx;
                xUSBD_TRACE_E1(device_ctx, xUSBD_TRACE_CODE_CLASS_REQUEST, class_ctx->first_interface);
            }
            return status;
        }

        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }

    if ((device_ctx->request.bRequestType & USB_REQ_RECIPIENT_MASK) == USB_REQ_RECIPIENT_DEVICE)
    {
        return class_control_request_broadcast_in(device_ctx, response);
    }

    return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
}

xRETURN_t xUSBD_Class_Out_Request_Process(xUSBD_Device_Context_t *device_ctx, uint8_t *control_data, uint32_t length)
{
    xUSBD_Class_Context_t *class_ctx = request_recipient_class_get(device_ctx);
    if (class_ctx != NULL)
    {
        if (class_ctx->driver->control_out_request != NULL)
        {
            xRETURN_t status = class_ctx->driver->control_out_request(class_ctx, control_data, length);
            if (status == xRETURN_OK)
            {
                device_ctx->control_request_class = class_ctx;
                xUSBD_TRACE_E1(device_ctx, xUSBD_TRACE_CODE_CLASS_REQUEST, class_ctx->first_interface);
            }
            return status;
        }

        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }

    if ((device_ctx->request.bRequestType & USB_REQ_RECIPIENT_MASK) == USB_REQ_RECIPIENT_DEVICE)
    {
        return class_control_request_broadcast_out(device_ctx, control_data, length);
    }

    return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
}

xRETURN_t xUSBD_Class_Data_Received(xUSBD_Device_Context_t *device_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    uint32_t index = endpoint_map_index(ep_addr);
    if (index >= xUSBD_MAX_ENDPOINT_MAP_ENTRIES)
    {
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }

    xUSBD_Class_Context_t *class_ctx = device_ctx->endpoint_to_class[index];
    if (class_ctx == NULL || class_ctx->driver->data_received == NULL)
    {
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }

    return class_ctx->driver->data_received(class_ctx, ep_addr, data, length);
}

xRETURN_t xUSBD_Class_Data_Sent(xUSBD_Device_Context_t *device_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    uint32_t index = endpoint_map_index(ep_addr);
    if (index >= xUSBD_MAX_ENDPOINT_MAP_ENTRIES)
    {
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }

    xUSBD_Class_Context_t *class_ctx = device_ctx->endpoint_to_class[index];
    if (class_ctx == NULL || class_ctx->driver->transmit_complete == NULL)
    {
        return xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    }

    return class_ctx->driver->transmit_complete(class_ctx, ep_addr, data, length);
}

xRETURN_t xUSBD_Class_Control_Transfer_Complete_Process(xUSBD_Device_Context_t *device_ctx, USB_Setup_Request_t *request)
{
    xUSBD_Class_Context_t *class_ctx = device_ctx->control_request_class;
    device_ctx->control_request_class = NULL;

    if (class_ctx == NULL)
    {
        return xRETURN_OK;
    }

    if (class_ctx->driver->control_transfer_complete != NULL)
    {
        return class_ctx->driver->control_transfer_complete(class_ctx, request);
    }

    return xRETURN_OK;
}

xRETURN_t xUSBD_Class_Get_String_Process(xUSBD_Device_Context_t *device_ctx, uint8_t string_index, uint8_t **data)
{
    if (string_index >= xUSBD_MAX_STRING_MAP_ENTRIES)
    {
        return xRETURN_xERR_xUSBD_REQ_GET_DESC_NOT_SUPPORTED;
    }

    xUSBD_Class_Context_t *class_ctx = device_ctx->string_to_class[string_index];
    if (class_ctx == NULL || class_ctx->interface_string == NULL)
    {
        return xRETURN_xERR_xUSBD_REQ_GET_DESC_NOT_SUPPORTED;
    }

    *data = (uint8_t *)class_ctx->interface_string;
    return xRETURN_OK;
}

// ---------------------------------------------------------
// Public APIs
// ---------------------------------------------------------
xRETURN_t xUSBD_Class_Register(xUSBD_Device_Context_t *device_ctx, xUSBD_Class_Context_t *class_ctx, xUSBD_Class_Driver_t *class_driver)
{
    xRETURN_t status = class_register_config_validate(device_ctx, class_ctx, class_driver);
    if (status != xRETURN_OK)
    {
        return status;
    }

    xUSBD_Class_Resource_Snapshot_t start = class_resource_snapshot_get(device_ctx);
    class_context_prepare_for_registration(device_ctx, class_ctx, class_driver);

    device_ctx->registering_class = class_ctx;
    status = class_driver->init_instance(class_ctx);
    device_ctx->registering_class = NULL;

    if (status != xRETURN_OK)
    {
        xRETURN_t rollback_status = class_resource_rollback(device_ctx, start);
        class_context_clear_failed_registration(class_ctx);
        return (rollback_status != xRETURN_OK) ? rollback_status : status;
    }

    class_register_list_append(device_ctx, class_ctx);
    class_register_lifecycle_mark(device_ctx);
    return xRETURN_OK;
}

xRETURN_t xUSBD_Class_Set_App_Context(xUSBD_Class_Context_t *class_ctx, void *app_context)
{
    if (class_ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    class_ctx->app_context = app_context;

    return xRETURN_OK;
}

xRETURN_t xUSBD_Class_Get_App_Context(const xUSBD_Class_Context_t *class_ctx, void **app_context)
{
    if ((class_ctx == NULL) || (app_context == NULL))
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    *app_context = class_ctx->app_context;

    return xRETURN_OK;
}
// EOF /////////////////////////////////////////////////////////////////////////////
