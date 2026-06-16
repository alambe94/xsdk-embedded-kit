// Copyright 2022 alambe94
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

// @file xfault_log.h
// @brief xFault module-level log macro.

#ifndef XFAULT_LOG_H
#define XFAULT_LOG_H

// INCLUDES ////////////////////////////////////////////////////////////////////
#include "xfault_config.h"
#include "xlog.h"

// MACROS //////////////////////////////////////////////////////////////////////

#if (xFAULT_CONFIG_LOG_LEVEL >= xLOG_LEVEL_MESSAGE)
#define xFAULT_LOG(code, ...) xLOG_MESSAGE((code), __VA_ARGS__)
#elif (xFAULT_CONFIG_LOG_LEVEL >= xLOG_LEVEL_STATUS)
#define xFAULT_LOG(code, ...)                                                                                                              \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xLOG_STATUS((code));                                                                                                               \
    } while (0)
#elif (xFAULT_CONFIG_LOG_LEVEL >= xLOG_LEVEL_ERROR)
#define xFAULT_LOG(code, ...) xLOG_ERROR((code), __VA_ARGS__)
#else
#define xFAULT_LOG(code, ...) ((void)(code))
#endif

#endif // XFAULT_LOG_H
// EOF /////////////////////////////////////////////////////////////////////////////
