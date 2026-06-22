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

// @file xusb_regmap.h
// @brief Lightweight MMIO register-map helpers for the AM243x USB port driver.
//
// Provides three types and two inline read/write accessors that mirror the
// Linux-kernel regmap_field pattern, adapted for bare-metal use:
//
//   struct reg_field      - static descriptor: byte offset + bit range
//   struct regmap         - wraps a volatile peripheral base pointer
//   struct regmap_field   - runtime accessor: regmap + pre-computed mask/shift
//
// Usage:
//   1. Define a static reg_field using the REG_FIELD() macro.
//   2. Call regmap_field_init() (or fill struct regmap_field manually) once.
//   3. Use regmap_field_read() / regmap_field_write() for register access.

#ifndef XUSB_REGMAP_H
#define XUSB_REGMAP_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// MACROS //////////////////////////////////////////////////////////////////////

// Convenience initialiser for a static reg_field descriptor.
#define REG_FIELD(_reg, _lsb, _msb) {.reg = (_reg), .lsb = (_lsb), .msb = (_msb)}

    // TYPES ///////////////////////////////////////////////////////////////////////

    // Static register field descriptor: byte offset from peripheral base + bit range.
    struct reg_field
    {
        unsigned int reg; // byte offset from peripheral base
        unsigned int lsb; // first (least-significant) bit of the field
        unsigned int msb; // last  (most-significant)  bit of the field
    };

    // MMIO register map: wraps a peripheral base address.
    struct regmap
    {
        volatile uint32_t *base;
    };

    // Runtime accessor for a single bit-field within a regmap register.
    struct regmap_field
    {
        struct regmap *regmap;
        unsigned int reg;   // byte offset (copied from reg_field)
        uint32_t mask;      // pre-computed inclusive bit mask
        unsigned int shift; // bit position of lsb (== reg_field.lsb)
    };

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // Build an inclusive mask covering bits [lsb, msb].
    // Uses (2 << (msb-lsb)) - 1 to avoid UB on a full 32-bit shift.
    static inline uint32_t _regmap_mask(unsigned int lsb, unsigned int msb)
    {
        return ((2UL << (msb - lsb)) - 1UL) << lsb;
    }

    // Read a bit-field value (right-shifted, zero-extended).
    static inline int regmap_field_read(struct regmap_field *field, unsigned int *val)
    {
        const volatile uint32_t *address = (const volatile uint32_t *)((const uint8_t *)field->regmap->base + field->reg);
        *val = (*address & field->mask) >> field->shift;
        return 0;
    }

    // Write a bit-field value (masked read-modify-write).
    static inline int regmap_field_write(struct regmap_field *field, unsigned int val)
    {
        volatile uint32_t *address = (volatile uint32_t *)((uint8_t *)field->regmap->base + field->reg);
        uint32_t tmp = *address;
        tmp &= ~field->mask;
        tmp |= (val << field->shift) & field->mask;
        *address = tmp;
        return 0;
    }

#ifdef __cplusplus
}
#endif

#endif // XUSB_REGMAP_H
// EOF /////////////////////////////////////////////////////////////////////////////
