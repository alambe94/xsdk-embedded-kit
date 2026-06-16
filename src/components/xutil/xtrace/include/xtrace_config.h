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

// @file xtrace_config.h
// @brief xTrace compile-time configuration values.
//

#ifndef XTRACE_CONFIG_H
#define XTRACE_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES

    // SYSTEM INCLUDES

    // MODULE INCLUDES

    // MACROS //////////////////////////////////////////////////////////////////////

// Set to 1 to enable trace emission. Set to 0 to compile out all trace calls.
#ifndef xTRACE_ENABLE
#define xTRACE_ENABLE 1U
#endif

// Log level for xlog inside xTrace internals. 0 = off.
#ifndef xTRACE_CONFIG_LOG_LEVEL
#define xTRACE_CONFIG_LOG_LEVEL 0
#endif

// Complete-record emission locking hooks. Builds that allow both task and ISR
// producers must override these with an interrupt-safe critical section.
#ifndef xTRACE_LOCK_DEFINE
#define xTRACE_LOCK_DEFINE()
#endif

#ifndef xTRACE_LOCK
#define xTRACE_LOCK(ctx)                                                                                                                   \
    do                                                                                                                                     \
    {                                                                                                                                      \
        (void)(ctx);                                                                                                                       \
    } while (0)
#endif

#ifndef xTRACE_UNLOCK
#define xTRACE_UNLOCK(ctx)                                                                                                                 \
    do                                                                                                                                     \
    {                                                                                                                                      \
        (void)(ctx);                                                                                                                       \
    } while (0)
#endif

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XTRACE_CONFIG_H
// EOF /////////////////////////////////////////////////////////////////////////////
