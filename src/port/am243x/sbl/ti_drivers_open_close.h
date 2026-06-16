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

// @file ti_drivers_open_close.h
// @brief xSDK replacement for SysConfig-generated ti_drivers_open_close.h.
//

#ifndef TI_DRIVERS_OPEN_CLOSE_H
#define TI_DRIVERS_OPEN_CLOSE_H

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// INLINE FUNCTIONS ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
void System_init(void);
void System_deinit(void);
int32_t Drivers_open(void);
void Drivers_close(void);

#endif // TI_DRIVERS_OPEN_CLOSE_H
// EOF /////////////////////////////////////////////////////////////////////////////
