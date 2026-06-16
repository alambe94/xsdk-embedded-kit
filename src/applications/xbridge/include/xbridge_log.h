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

// @file xbridge_log.h
// @brief xBRIDGE module-level log macro.

#ifndef XBRIDGE_LOG_H
#define XBRIDGE_LOG_H

// INCLUDES ////////////////////////////////////////////////////////////////////
#include "xbridge_config.h"
#include "xlog.h"

// MACROS //////////////////////////////////////////////////////////////////////

#if (xBRIDGE_CONFIG_LOG_LEVEL >= xLOG_LEVEL_MESSAGE)
#define xBRIDGE_LOG(code, ...) xLOG_MESSAGE((code), __VA_ARGS__)
#elif (xBRIDGE_CONFIG_LOG_LEVEL >= xLOG_LEVEL_STATUS)
#define xBRIDGE_LOG(code, ...)                                                                                                             \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xLOG_STATUS((code));                                                                                                               \
    } while (0)
#elif (xBRIDGE_CONFIG_LOG_LEVEL >= xLOG_LEVEL_ERROR)
#define xBRIDGE_LOG(code, ...) xLOG_ERROR((code), __VA_ARGS__)
#else
#define xBRIDGE_LOG(code, ...) ((void)(code))
#endif

#endif // XBRIDGE_LOG_H
// EOF /////////////////////////////////////////////////////////////////////////////
