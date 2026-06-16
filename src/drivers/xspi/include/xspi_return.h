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

// @file xspi_return.h
// @brief xSPI driver return codes.
//

#ifndef XSPI_RETURN_H
#define XSPI_RETURN_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xreturn.h"

    // MACROS //////////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////////

    typedef enum
    {
        xRETURN_xERR_xSPI_NULL_POINTER = xRETURN_MAKE(xRETURN_xSPI_MODULE, xRETURN_SEVERITY_ERROR, 0x001U),
        xRETURN_xERR_xSPI_INVALID_ARG = xRETURN_MAKE(xRETURN_xSPI_MODULE, xRETURN_SEVERITY_ERROR, 0x002U),
        xRETURN_xERR_xSPI_INVALID_STATE = xRETURN_MAKE(xRETURN_xSPI_MODULE, xRETURN_SEVERITY_ERROR, 0x003U),
        xRETURN_xERR_xSPI_NOT_INITIALIZED = xRETURN_MAKE(xRETURN_xSPI_MODULE, xRETURN_SEVERITY_ERROR, 0x004U),
        xRETURN_xERR_xSPI_NOT_STARTED = xRETURN_MAKE(xRETURN_xSPI_MODULE, xRETURN_SEVERITY_ERROR, 0x005U),
        xRETURN_xERR_xSPI_BUSY = xRETURN_MAKE(xRETURN_xSPI_MODULE, xRETURN_SEVERITY_ERROR, 0x006U),
        xRETURN_xERR_xSPI_TIMEOUT = xRETURN_MAKE(xRETURN_xSPI_MODULE, xRETURN_SEVERITY_ERROR, 0x007U),
        xRETURN_xERR_xSPI_UNSUPPORTED = xRETURN_MAKE(xRETURN_xSPI_MODULE, xRETURN_SEVERITY_ERROR, 0x008U),
        xRETURN_xERR_xSPI_HARDWARE = xRETURN_MAKE(xRETURN_xSPI_MODULE, xRETURN_SEVERITY_ERROR, 0x009U),
        xRETURN_xERR_xSPI_ABORTED = xRETURN_MAKE(xRETURN_xSPI_MODULE, xRETURN_SEVERITY_ERROR, 0x00AU),
    } xRETURN_xSPI_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XSPI_RETURN_H
// EOF /////////////////////////////////////////////////////////////////////////////
