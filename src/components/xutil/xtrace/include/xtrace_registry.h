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

// @file xtrace_registry.h
// @brief central trace ID block allocation registry.

#ifndef XTRACE_REGISTRY_H
#define XTRACE_REGISTRY_H

// Allocation block size of 32 events per module to keep core event IDs
// within the single-byte LEB128 wire range (0 - 127).
#define xTRACE_BASE_CORE  0x00U // Range: 0x00 - 0x1F (xTrace Core)
#define xTRACE_BASE_xRTOS 0x20U // Range: 0x20 - 0x3F (xRTOS)
#define xTRACE_BASE_xFS   0x40U // Range: 0x40 - 0x5F (xFS)
#define xTRACE_BASE_xUSB  0x60U // Range: 0x60 - 0x7F (xUSB Device)
#define xTRACE_BASE_xUSBH 0x80U // Range: 0x80 - 0x9F (xUSB Host)
#define xTRACE_BASE_xUART 0xA0U // Range: 0xA0 - 0xBF (xUART)

// 2-byte LEB128 wire range (128+)
#define xTRACE_BASE_USER 0xC0U // Range: 0xC0+      (User App Events)

#endif // XTRACE_REGISTRY_H
// EOF /////////////////////////////////////////////////////////////////////////////
