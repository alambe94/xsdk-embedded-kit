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

// @file xshell_config.h
// @brief xSHELL compile-time configuration values.
//

#ifndef XSHELL_CONFIG_H
#define XSHELL_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES

    // SYSTEM INCLUDES

    // MODULE INCLUDES

    // MACROS //////////////////////////////////////////////////////////////////////

    // Maximum number of characters in a single input line, including null terminator.
#ifndef xSHELL_MAX_LINE_LENGTH
#define xSHELL_MAX_LINE_LENGTH 128U
#endif

    // Maximum number of arguments parsed from a single command line.
#ifndef xSHELL_MAX_ARGS
#define xSHELL_MAX_ARGS 16U
#endif

    // Maximum number of commands that can be registered in one xCMD_Context_t.
#ifndef xSHELL_MAX_COMMANDS
#define xSHELL_MAX_COMMANDS 32U
#endif

    // Maximum prompt string length including null terminator.
#ifndef xSHELL_MAX_PROMPT_LENGTH
#define xSHELL_MAX_PROMPT_LENGTH 16U
#endif

    // Number of bytes read from the transport in each xSHELL_Process call.
#ifndef xSHELL_CONFIG_READ_CHUNK_SIZE
#define xSHELL_CONFIG_READ_CHUNK_SIZE 16U
#endif

    // Log level (0=silent, 1=status, 2=verbose).
#ifndef xSHELL_CONFIG_LOG_LEVEL
#define xSHELL_CONFIG_LOG_LEVEL 0
#endif

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XSHELL_CONFIG_H
// EOF /////////////////////////////////////////////////////////////////////////////
