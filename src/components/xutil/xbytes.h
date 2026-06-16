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

// @file xbytes.h
// @brief SDK-wide byte-order and byte-manipulation helpers for all xSDK modules.
//
// Byte-buffer I/O (alignment-safe, host-endianness independent):
//   xRead_LE16 / xRead_LE32   - read a little-endian value from a uint8_t buffer
//   xWrite_LE16 / xWrite_LE32 - write a little-endian value to a uint8_t buffer
//   xRead_BE16 / xRead_BE32   - read a big-endian value from a uint8_t buffer
//   xWrite_BE16 / xWrite_BE32 - write a big-endian value to a uint8_t buffer
//
// Value endianness conversion (for already-loaded little-endian fields):
//   xLE16_TO_CPU / xCPU_TO_LE16
//   xLE32_TO_CPU / xCPU_TO_LE32
//   xBE16_TO_CPU / xCPU_TO_BE16
//   xBE32_TO_CPU / xCPU_TO_BE32
//
// Byte extraction from a CPU-order value:
//   xU16_LOW_BYTE / xU16_HIGH_BYTE
//   xU32_BYTE0 / xU32_BYTE1 / xU32_BYTE2 / xU32_BYTE3
//
// Byte assembly into a CPU-order value:
//   xMAKE_U16(lo, hi)
//   xMAKE_U32(b0, b1, b2, b3)

#ifndef XBYTES_H
#define XBYTES_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////

// COMPILER INCLUDES
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES

// MACROS //////////////////////////////////////////////////////////////////////

// Byte extraction from a CPU-order value.
#define xU16_LOW_BYTE(x)  ((uint8_t)(((uint16_t)(x)) & 0xFFU))
#define xU16_HIGH_BYTE(x) ((uint8_t)((((uint16_t)(x)) >> 8U) & 0xFFU))

#define xU32_BYTE0(x) ((uint8_t)(((uint32_t)(x)) & 0xFFU))
#define xU32_BYTE1(x) ((uint8_t)((((uint32_t)(x)) >> 8U) & 0xFFU))
#define xU32_BYTE2(x) ((uint8_t)((((uint32_t)(x)) >> 16U) & 0xFFU))
#define xU32_BYTE3(x) ((uint8_t)((((uint32_t)(x)) >> 24U) & 0xFFU))

// Byte assembly into a CPU-order value from bytes in little-endian order.
#define xMAKE_U16(lo, hi)         ((uint16_t)(lo) | ((uint16_t)(hi) << 8U))
#define xMAKE_U32(b0, b1, b2, b3) ((uint32_t)(b0) | ((uint32_t)(b1) << 8U) | ((uint32_t)(b2) << 16U) | ((uint32_t)(b3) << 24U))

#define xSWAP_U16(x) ((uint16_t)((((uint16_t)(x)) >> 8U) | (((uint16_t)(x)) << 8U)))
#define xSWAP_U32(x)                                                                                                                       \
    ((uint32_t)((((uint32_t)(x)) >> 24U) | ((((uint32_t)(x)) >> 8U) & 0x0000FF00UL) | ((((uint32_t)(x)) << 8U) & 0x00FF0000UL) |           \
                (((uint32_t)(x)) << 24U)))

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    static inline uint16_t xRead_LE16(const uint8_t *buf)
    {
        return (uint16_t)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8U));
    }

    static inline uint32_t xRead_LE32(const uint8_t *buf)
    {
        return (uint32_t)((uint32_t)buf[0] | ((uint32_t)buf[1] << 8U) | ((uint32_t)buf[2] << 16U) | ((uint32_t)buf[3] << 24U));
    }

    static inline void xWrite_LE16(uint8_t *buf, uint16_t val)
    {
        buf[0] = (uint8_t)(val & 0xFFU);
        buf[1] = (uint8_t)((val >> 8U) & 0xFFU);
    }

    static inline void xWrite_LE32(uint8_t *buf, uint32_t val)
    {
        buf[0] = (uint8_t)(val & 0xFFUL);
        buf[1] = (uint8_t)((val >> 8U) & 0xFFUL);
        buf[2] = (uint8_t)((val >> 16U) & 0xFFUL);
        buf[3] = (uint8_t)((val >> 24U) & 0xFFUL);
    }

    static inline uint16_t xRead_BE16(const uint8_t *buf)
    {
        return (uint16_t)(((uint16_t)buf[0] << 8U) | (uint16_t)buf[1]);
    }

    static inline uint32_t xRead_BE32(const uint8_t *buf)
    {
        return (uint32_t)(((uint32_t)buf[0] << 24U) | ((uint32_t)buf[1] << 16U) | ((uint32_t)buf[2] << 8U) | (uint32_t)buf[3]);
    }

    static inline void xWrite_BE16(uint8_t *buf, uint16_t val)
    {
        buf[0] = (uint8_t)((val >> 8U) & 0xFFU);
        buf[1] = (uint8_t)(val & 0xFFU);
    }

    static inline void xWrite_BE32(uint8_t *buf, uint32_t val)
    {
        buf[0] = (uint8_t)((val >> 24U) & 0xFFUL);
        buf[1] = (uint8_t)((val >> 16U) & 0xFFUL);
        buf[2] = (uint8_t)((val >> 8U) & 0xFFUL);
        buf[3] = (uint8_t)(val & 0xFFUL);
    }

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    static inline uint16_t xLE16_TO_CPU(uint16_t val)
    {
        return xSWAP_U16(val);
    }
    static inline uint16_t xCPU_TO_LE16(uint16_t val)
    {
        return xSWAP_U16(val);
    }
    static inline uint32_t xLE32_TO_CPU(uint32_t val)
    {
        return xSWAP_U32(val);
    }
    static inline uint32_t xCPU_TO_LE32(uint32_t val)
    {
        return xSWAP_U32(val);
    }
    static inline uint16_t xBE16_TO_CPU(uint16_t val)
    {
        return val;
    }
    static inline uint16_t xCPU_TO_BE16(uint16_t val)
    {
        return val;
    }
    static inline uint32_t xBE32_TO_CPU(uint32_t val)
    {
        return val;
    }
    static inline uint32_t xCPU_TO_BE32(uint32_t val)
    {
        return val;
    }
#else
static inline uint16_t xLE16_TO_CPU(uint16_t val)
{
    return val;
}
static inline uint16_t xCPU_TO_LE16(uint16_t val)
{
    return val;
}
static inline uint32_t xLE32_TO_CPU(uint32_t val)
{
    return val;
}
static inline uint32_t xCPU_TO_LE32(uint32_t val)
{
    return val;
}
static inline uint16_t xBE16_TO_CPU(uint16_t val)
{
    return xSWAP_U16(val);
}
static inline uint16_t xCPU_TO_BE16(uint16_t val)
{
    return xSWAP_U16(val);
}
static inline uint32_t xBE32_TO_CPU(uint32_t val)
{
    return xSWAP_U32(val);
}
static inline uint32_t xCPU_TO_BE32(uint32_t val)
{
    return xSWAP_U32(val);
}
#endif

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBYTES_H
// EOF /////////////////////////////////////////////////////////////////////////////
