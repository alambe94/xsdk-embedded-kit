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

// @file xusbh_drv.h
// @brief AM64x USB Host Controller Driver port boundary.

#ifndef XUSBH_DRV_H
#define XUSBH_DRV_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbh_hcd.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // MACROS //////////////////////////////////////////////////////////////////////
#define xUSBH_AM64X_HCD_MAX_PORTS     1U
#define xUSBH_AM64X_HCD_MAX_INSTANCE  1U
#define xUSBH_AM64X_HCD_USB0_PORT     0U
#define xUSBH_AM64X_HCD_USB0_INSTANCE 0U

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef struct xUSBH_AM64x_HCD_Context_t
    {
        void *host_ctx;
        void *driver_object;
        void *driver_private;
        xUSBH_HCD_Event_Callback_t event_callback;
        USB_Speed_t speed;
        uint32_t frame_number;
        uint8_t port;
        bool is_initialized;
        bool is_hardware_initialized;
        bool is_started;
        bool are_interrupts_enabled;
        bool is_port_powered;
        bool is_port_connected;
    } xUSBH_AM64x_HCD_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // AM64x host-port limitation:
    // This port represents USB0 only. Vendor Cadence USBSSP/xHCI types are kept
    // out of this header and are bound inside xusbh_drv.c only when
    // xUSBH_AM64X_HCD_ENABLE_MCU_PLUS_SDK is defined. Without that build flag,
    // the Ops table remains a portable scaffold and hardware operations report
    // xRETURN_xERR_xUSBH_UNSUPPORTED_OPERATION.
    //
    // Multi-instance support is intentionally out of scope for the first port.
    // DMA buffers, cache maintenance, and endpoint completion demultiplexing are
    // owned by this port when full hardware transfer submission is implemented.
    // The first hardware binding assumes the MCU+ SDK Cadence USBSSP host driver,
    // one root port, USB0 role selection performed by the board layer, and
    // controller-visible transfer buffers supplied by the caller/class layer.
    // SuperSpeed PHY/WIZ/Torrent bring-up is compiled in only when
    // xUSBH_AM64X_HCD_ENABLE_SUPER_SPEED is set.
    extern const xUSBH_HCD_Ops_t xUSBH_AM64x_HCD_Ops;
    extern xUSBH_AM64x_HCD_Context_t xUSBH_AM64x_HCD_Context;

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    void xUSBH_AM64x_HCD_IRQ_Handler(uint8_t port);

#ifdef __cplusplus
}
#endif

#endif // XUSBH_DRV_H
// EOF /////////////////////////////////////////////////////////////////////////////
