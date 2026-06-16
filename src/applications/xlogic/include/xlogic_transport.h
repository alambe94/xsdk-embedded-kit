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

// @file xlogic_transport.h
// @brief xLOGIC SUMP/CDC transport layer - RX routing, TX, and sample streaming.
//
// The transport layer is decoupled from xUSBD CDC via an injected TX function pointer.
// The application provides a thin wrapper that calls xUSBD_CDC_Transmit (or any other
// byte-stream sink).  Host tests inject a buffer-capture function instead.

#ifndef XLOGIC_TRANSPORT_H
#define XLOGIC_TRANSPORT_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
#include <stdint.h>
#include <stdbool.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xlogic_sump.h"
#include "xlogic_return.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    // TX function signature.  The implementation must send all length bytes before
    // returning - partial sends are not supported in Phase 1.
    typedef xRETURN_t (*xLOGIC_TX_Fn_t)(void *tx_ctx, const uint8_t *data, uint32_t length);

    // Transport context.  All pointers are caller-owned; xlogic_transport never allocates.
    typedef struct xLOGIC_Transport_Context_t
    {
        xLOGIC_SUMP_Context_t sump_ctx; // embedded SUMP parser state

        xLOGIC_TX_Fn_t tx_fn; // injected TX function
        void *tx_ctx;         // caller context passed to tx_fn

        bool has_run_request;   // set when SUMP RUN event received
        bool has_reset_request; // set when SUMP RESET event received

    } xLOGIC_Transport_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Initialize the transport context with the given TX function and context pointer.
    xRETURN_t xLOGIC_Transport_Init(xLOGIC_Transport_Context_t *transport_ctx, xLOGIC_TX_Fn_t tx_fn, void *tx_ctx);

    // Feed length received bytes into the SUMP parser and act on any completed command.
    // On QUERY_ID: immediately sends "1ALS" via tx_fn.
    // On METADATA: immediately sends the metadata block via tx_fn.
    // On RUN:      sets transport_ctx->has_run_request = true.
    // On RESET:    sets transport_ctx->has_reset_request = true, clears run_request.
    xRETURN_t xLOGIC_Transport_Process_RX(xLOGIC_Transport_Context_t *transport_ctx, const uint8_t *data, uint32_t length);

    // Send length raw bytes via tx_fn (used for protocol responses such as "1ALS").
    xRETURN_t xLOGIC_Transport_Send_Response(xLOGIC_Transport_Context_t *transport_ctx, const uint8_t *data, uint32_t length);

    // Reverse sample_bytes bytes in-place and send them via tx_fn.
    // SUMP requires samples ordered newest-first (reverse capture order).
    // samples: caller-owned buffer containing sample_bytes bytes; reversed in place.
    xRETURN_t xLOGIC_Transport_Send_Samples(xLOGIC_Transport_Context_t *transport_ctx, uint8_t *samples, uint32_t sample_bytes);

    // Clear the run and reset request flags (called by the core after consuming them).
    void xLOGIC_Transport_Clear_Requests(xLOGIC_Transport_Context_t *transport_ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XLOGIC_TRANSPORT_H
// EOF /////////////////////////////////////////////////////////////////////////////
