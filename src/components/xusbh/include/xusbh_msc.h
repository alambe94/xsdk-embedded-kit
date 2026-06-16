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

// @file xusbh_msc.h
// @brief USB host Mass Storage Class Bulk-Only Transport API.

#ifndef XUSBH_MSC_H
#define XUSBH_MSC_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusb_msc_defs.h"
#include "xusbh_class.h"

#ifdef __cplusplus
extern "C"
{
#endif

// MACROS //////////////////////////////////////////////////////////////////////
#define xUSBH_MSC_BOT_CBW_FLAG_OUT     0x00U
#define xUSBH_MSC_BOT_CBW_FLAG_IN      0x80U
#define xUSBH_MSC_BOT_CBW_CB_SIZE      16U
#define xUSBH_MSC_BLOCK_SIZE           512U
#define xUSBH_MSC_MAX_LUN              15U
#define xUSBH_MSC_INQUIRY_LENGTH       36U
#define xUSBH_MSC_REQUEST_SENSE_LENGTH 18U

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef enum xUSBH_MSC_State_t
    {
        xUSBH_MSC_STATE_IDLE = 0,
        xUSBH_MSC_STATE_READY,
        xUSBH_MSC_STATE_COMMAND,
        xUSBH_MSC_STATE_DATA_IN,
        xUSBH_MSC_STATE_DATA_OUT,
        xUSBH_MSC_STATE_STATUS,
        xUSBH_MSC_STATE_RESET_RECOVERY,
        xUSBH_MSC_STATE_RESET_REQUEST,
        xUSBH_MSC_STATE_CLEAR_IN_HALT,
        xUSBH_MSC_STATE_CLEAR_OUT_HALT,
        xUSBH_MSC_STATE_ERROR,
    } xUSBH_MSC_State_t;

    typedef enum xUSBH_MSC_Error_t
    {
        xUSBH_MSC_ERROR_NONE = 0,
        xUSBH_MSC_ERROR_SHORT_TRANSFER,
        xUSBH_MSC_ERROR_STALL,
        xUSBH_MSC_ERROR_PHASE_ERROR,
        xUSBH_MSC_ERROR_COMMAND_FAILED,
        xUSBH_MSC_ERROR_RESET_RECOVERY_REQUIRED,
    } xUSBH_MSC_Error_t;

#pragma pack(push, 1)

    typedef struct __attribute__((packed)) xUSBH_MSC_BOT_CBW_t
    {
        uint32_t signature;
        uint32_t tag;
        uint32_t data_length;
        uint8_t flags;
        uint8_t lun;
        uint8_t command_block_length;
        uint8_t command_block[xUSBH_MSC_BOT_CBW_CB_SIZE];
    } xUSBH_MSC_BOT_CBW_t;

    typedef struct __attribute__((packed)) xUSBH_MSC_BOT_CSW_t
    {
        uint32_t signature;
        uint32_t tag;
        uint32_t data_residue;
        uint8_t status;
    } xUSBH_MSC_BOT_CSW_t;

#pragma pack(pop)

    typedef struct xUSBH_MSC_Capacity_t
    {
        uint32_t block_count;
        uint32_t block_size;
    } xUSBH_MSC_Capacity_t;

    typedef struct xUSBH_MSC_Instance_t
    {
        bool is_allocated;
        xUSBH_MSC_State_t state;
        xUSBH_MSC_Error_t error;
        xUSBH_Interface_Context_t *interface_ctx;
        xUSBH_Endpoint_Context_t *bulk_in_endpoint;
        xUSBH_Endpoint_Context_t *bulk_out_endpoint;
        xUSBH_Transfer_t *transfer;
        xUSBH_MSC_BOT_CBW_t cbw;
        xUSBH_MSC_BOT_CSW_t csw;
        uint8_t *data_buffer;
        uint32_t data_length;
        uint32_t active_tag;
        uint32_t next_tag;
        uint32_t block_size;
        xUSBH_MSC_Capacity_t *capacity;
        uint8_t capacity_buffer[8];
        uint8_t lun;
        uint8_t active_opcode;
        bool active_data_in;
    } xUSBH_MSC_Instance_t;

    typedef struct xUSBH_MSC_Context_t
    {
        xUSBH_Context_t *host_ctx;
        xUSBH_MSC_Instance_t instances[xUSBH_MSC_MAX_INSTANCES];
    } xUSBH_MSC_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    // msc_ctx and host_ctx remain caller-owned. The MSC class borrows host
    // transfer slots while BOT commands are active and releases them on unbind.
    xRETURN_t xUSBH_MSC_Init(xUSBH_MSC_Context_t *msc_ctx, xUSBH_Context_t *host_ctx);
    const xUSBH_Class_Driver_t *xUSBH_MSC_Class(void);

    // Start one READ10 BOT transaction for the bound MSC instance at lun.
    // The call submits the CBW and returns after HCD ownership is established.
    // buffer must remain valid until the instance returns to READY or enters an
    // ERROR / RESET_RECOVERY state through transfer-complete callbacks.
    // The caller keeps ownership of buffer; the HCD may DMA to it while active.
    xRETURN_t xUSBH_MSC_Read_Blocks(xUSBH_MSC_Context_t *msc_ctx,
                                    uint8_t lun,
                                    uint32_t lba,
                                    uint16_t block_count,
                                    uint8_t *buffer,
                                    uint32_t buffer_length);

    // Start one WRITE10 BOT transaction for the bound MSC instance at lun.
    // buffer must remain valid until command completion, matching Read_Blocks.
    // The caller keeps ownership of buffer; the HCD may DMA from it while active.
    xRETURN_t xUSBH_MSC_Write_Blocks(xUSBH_MSC_Context_t *msc_ctx,
                                     uint8_t lun,
                                     uint32_t lba,
                                     uint16_t block_count,
                                     const uint8_t *buffer,
                                     uint32_t buffer_length);

    xRETURN_t xUSBH_MSC_Inquiry(xUSBH_MSC_Context_t *msc_ctx, uint8_t lun, uint8_t *buffer, uint32_t buffer_length);
    xRETURN_t xUSBH_MSC_Read_Capacity(xUSBH_MSC_Context_t *msc_ctx, uint8_t lun, xUSBH_MSC_Capacity_t *capacity);
    xRETURN_t xUSBH_MSC_Test_Unit_Ready(xUSBH_MSC_Context_t *msc_ctx, uint8_t lun);
    xRETURN_t xUSBH_MSC_Request_Sense(xUSBH_MSC_Context_t *msc_ctx, uint8_t lun, uint8_t *buffer, uint32_t buffer_length);
    xRETURN_t xUSBH_MSC_Reset_Recovery(xUSBH_MSC_Context_t *msc_ctx, uint8_t lun);

#ifdef __cplusplus
}
#endif

#endif // XUSBH_MSC_H
// EOF /////////////////////////////////////////////////////////////////////////////
