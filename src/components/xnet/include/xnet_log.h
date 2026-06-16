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

// @file xnet_log.h
// @brief xNET module-level log macro.

#ifndef XNET_LOG_H
#define XNET_LOG_H

// INCLUDES ////////////////////////////////////////////////////////////////////
#include "xnet_config.h"
#include "xlog.h"

// MACROS //////////////////////////////////////////////////////////////////////

#if (xNET_CONFIG_LOG_LEVEL >= xLOG_LEVEL_MESSAGE)
#define xNET_LOG(code, ...) xLOG_MESSAGE((code), __VA_ARGS__)
#elif (xNET_CONFIG_LOG_LEVEL >= xLOG_LEVEL_STATUS)
#define xNET_LOG(code, ...)                                                                                                                \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xLOG_STATUS((code));                                                                                                               \
    } while (0)
#elif (xNET_CONFIG_LOG_LEVEL >= xLOG_LEVEL_ERROR)
#define xNET_LOG(code, ...) xLOG_ERROR((code), __VA_ARGS__)
#else
#define xNET_LOG(code, ...) ((void)(code))
#endif

#endif // XNET_LOG_H
// EOF /////////////////////////////////////////////////////////////////////////////
