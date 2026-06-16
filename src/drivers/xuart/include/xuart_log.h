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

// @file xuart_log.h
// @brief xUART module-level log macros (core and port).

#ifndef XUART_LOG_H
#define XUART_LOG_H

// INCLUDES ////////////////////////////////////////////////////////////////////
#include "xuart_config.h"
#include "xlog.h"

// MACROS //////////////////////////////////////////////////////////////////////

#if (xUART_CONFIG_LOG_LEVEL_CORE >= xLOG_LEVEL_MESSAGE)
#define xUART_LOG(code, ...) xLOG_MESSAGE((code), __VA_ARGS__)
#elif (xUART_CONFIG_LOG_LEVEL_CORE >= xLOG_LEVEL_STATUS)
#define xUART_LOG(code, ...)                                                                                                               \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xLOG_STATUS((code));                                                                                                               \
    } while (0)
#elif (xUART_CONFIG_LOG_LEVEL_CORE >= xLOG_LEVEL_ERROR)
#define xUART_LOG(code, ...) xLOG_ERROR((code), __VA_ARGS__)
#else
#define xUART_LOG(code, ...) ((void)(code))
#endif

#if (xUART_CONFIG_LOG_LEVEL_PORT >= xLOG_LEVEL_MESSAGE)
#define xUART_PORT_LOG(code, ...) xLOG_MESSAGE((code), __VA_ARGS__)
#elif (xUART_CONFIG_LOG_LEVEL_PORT >= xLOG_LEVEL_STATUS)
#define xUART_PORT_LOG(code, ...)                                                                                                          \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xLOG_STATUS((code));                                                                                                               \
    } while (0)
#elif (xUART_CONFIG_LOG_LEVEL_PORT >= xLOG_LEVEL_ERROR)
#define xUART_PORT_LOG(code, ...) xLOG_ERROR((code), __VA_ARGS__)
#else
#define xUART_PORT_LOG(code, ...) ((void)(code))
#endif

#endif // XUART_LOG_H
// EOF /////////////////////////////////////////////////////////////////////////////
