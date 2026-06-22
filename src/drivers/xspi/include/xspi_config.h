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

// @file xspi_config.h
// @brief Compile-time configuration defaults for the xSPI driver core.
//

#ifndef XSPI_CONFIG_H
#define XSPI_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES

    // SYSTEM INCLUDES

    // MODULE INCLUDES

    // MACROS //////////////////////////////////////////////////////////////////////////

#define xSPI_CONFIG_LOG_LEVEL 0U

#define xSPI_CONFIG_DEFAULT_BITS_PER_WORD 8U

#ifndef xSPI_TRACE_ENABLE
#define xSPI_TRACE_ENABLE 1U
#endif

    // TYPES ///////////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XSPI_CONFIG_H
// EOF /////////////////////////////////////////////////////////////////////////////
