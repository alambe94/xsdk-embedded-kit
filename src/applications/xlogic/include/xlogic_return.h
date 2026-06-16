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

// @file xlogic_return.h
// @brief xLOGIC module return codes.

#ifndef XLOGIC_RETURN_H
#define XLOGIC_RETURN_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xreturn.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef enum
    {
        // Errors [severity=ERROR, codes 0x001-0x0FF] //////////////////////////////

        // A required pointer argument was NULL.
        xRETURN_xERR_xLOGIC_NULL_POINTER = xRETURN_MAKE(xRETURN_xLOGIC_MODULE, xRETURN_SEVERITY_ERROR, 0x001U),

        // A numerical argument was outside its valid range.
        xRETURN_xERR_xLOGIC_INVALID_ARGUMENT,

        // The requested operation is not valid in the current component state.
        xRETURN_xERR_xLOGIC_INVALID_STATE,

        // The PRU firmware did not reach STATUS_DONE within the expected window.
        xRETURN_xERR_xLOGIC_PRU_TIMEOUT,

        // The PRU ring buffer overflowed before the ARM could drain it.
        xRETURN_xERR_xLOGIC_OVERRUN,

        // A caller-supplied buffer is too small for the requested operation.
        xRETURN_xERR_xLOGIC_BUFFER_FULL,

        // The transport TX function returned a failure.
        xRETURN_xERR_xLOGIC_TX_FAILED,

        // Warnings [severity=WARNING, codes 0x001-0x0FF] //////////////////////////

        // The requested sample rate was clamped to the nearest supported rate.
        xRETURN_xWRN_xLOGIC_RATE_CLAMPED = xRETURN_MAKE(xRETURN_xLOGIC_MODULE, xRETURN_SEVERITY_WARNING, 0x001U),

        // Messages [severity=MESSAGE, codes 0x001-0x0FF] //////////////////////////

        // PRU capture completed and all samples are available.
        xRETURN_xMSG_xLOGIC_CAPTURE_DONE = xRETURN_MAKE(xRETURN_xLOGIC_MODULE, xRETURN_SEVERITY_MESSAGE, 0x001U),

        // The configured trigger condition was matched.
        xRETURN_xMSG_xLOGIC_TRIGGERED,

    } xRETURN_xLOGIC_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XLOGIC_RETURN_H
// EOF /////////////////////////////////////////////////////////////////////////////
