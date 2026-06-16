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

// @file xlog.h
// @brief SDK-wide logging backend macros and level constants.
//
// Each module defines its own named log macro in a per-module _log.h,
// following the same pattern used by _trace.h files:
//
//   #include "xlog.h"
//
//   #if (xRTOS_CONFIG_LOG_LEVEL >= xLOG_LEVEL_MESSAGE)
//   #  define xRTOS_LOG(code, ...) xLOG_MESSAGE((code), __VA_ARGS__)
//   #elif (xRTOS_CONFIG_LOG_LEVEL >= xLOG_LEVEL_STATUS)
//   #  define xRTOS_LOG(code, ...) do { xLOG_STATUS((code)); } while (0)
//   #elif (xRTOS_CONFIG_LOG_LEVEL >= xLOG_LEVEL_ERROR)
//   #  define xRTOS_LOG(code, ...) xLOG_ERROR((code), __VA_ARGS__)
//   #else
//   #  define xRTOS_LOG(code, ...) ((void)(code))
//   #endif
//
// Override xLOG_STATUS and xLOG_MESSAGE before including
// this header to redirect output to a custom sink. Target firmware should
// provide explicit backends or keep the module log level at 0.
//
// xLOG_LEVEL_ERROR   (1) - emit error code + message: [ERR XXXXXXXX] <message>
// xLOG_LEVEL_STATUS  (2) - emit status code only:    [XXXXXXXX]
// xLOG_LEVEL_MESSAGE (3) - emit code + message:      [XXXXXXXX] <message>
//

#ifndef XLOG_H
#define XLOG_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES

// MACROS //////////////////////////////////////////////////////////////////////

#define xLOG_LEVEL_SILENT  0U
#define xLOG_LEVEL_ERROR   1U
#define xLOG_LEVEL_STATUS  2U
#define xLOG_LEVEL_MESSAGE 3U

#if !defined(xLOG_STATUS) || !defined(xLOG_MESSAGE) || !defined(xLOG_ERROR)
#include <stdio.h>
#endif

#ifndef xLOG_ERROR
#define xLOG_ERROR(code, ...)                                                                                                              \
    do                                                                                                                                     \
    {                                                                                                                                      \
        printf("[ERR %08lX] ", (unsigned long)(code));                                                                                     \
        printf(__VA_ARGS__);                                                                                                               \
        printf("\n");                                                                                                                      \
    } while (0)
#endif

#ifndef xLOG_STATUS
#define xLOG_STATUS(code)                                                                                                                  \
    do                                                                                                                                     \
    {                                                                                                                                      \
        printf("[%08lX]\n", (unsigned long)(code));                                                                                        \
    } while (0)
#endif

#ifndef xLOG_MESSAGE
#define xLOG_MESSAGE(code, ...)                                                                                                            \
    do                                                                                                                                     \
    {                                                                                                                                      \
        printf("[%08lX] ", (unsigned long)(code));                                                                                         \
        printf(__VA_ARGS__);                                                                                                               \
        printf("\n");                                                                                                                      \
    } while (0)
#endif

// TYPES ///////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#endif // XLOG_H
// EOF /////////////////////////////////////////////////////////////////////////////
