// Copyright 2026 alambe94
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// @file xuart_config.h
// @brief Compile-time configuration defaults for the xUART driver core.
//

#ifndef XUART_CONFIG_H
#define XUART_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES

    // SYSTEM INCLUDES

    // MODULE INCLUDES

    // MACROS //////////////////////////////////////////////////////////////////////////

#define xUART_CONFIG_LOG_LEVEL_CORE 0U
#define xUART_CONFIG_LOG_LEVEL_PORT 0U

#ifndef xUART_TRACE_ENABLE
#define xUART_TRACE_ENABLE 1U
#endif

    // TYPES ///////////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XUART_CONFIG_H
// EOF /////////////////////////////////////////////////////////////////////////////
