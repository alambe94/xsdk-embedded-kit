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

// @file xassert.h
// @brief SDK-wide assertion macro - project-wide, include-order safe.
//
// Controlled by the build system: pass -DxSDK_ENABLE_ASSERT=1 for debug builds.
// Safe to use in .c files and in static inline functions inside headers.
//
// Override xASSERT_HANDLER before including any SDK header to collect failure
// metadata. Override xASSERT_HOOK for a simple halt/breakpoint hook:
//
//   #define xASSERT_HANDLER(file, line, expr, msg) my_handler(file, line, expr, msg)
//   #define xASSERT_HOOK() __BKPT(0)
//

#ifndef XASSERT_H
#define XASSERT_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////

// COMPILER INCLUDES

// SYSTEM INCLUDES

// MODULE INCLUDES

// MACROS //////////////////////////////////////////////////////////////////////

// Override this macro to plug in a custom halt handler (e.g. __BKPT, watchdog reset).
#ifndef xASSERT_HOOK
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((weak)) void xassert_system_halt(void)
    {
        while (1)
        {
        }
    }
#define xASSERT_HOOK() xassert_system_halt()
#else
#define xASSERT_HOOK()                                                                                                                     \
    do                                                                                                                                     \
    {                                                                                                                                      \
        while (1)                                                                                                                          \
        {                                                                                                                                  \
        }                                                                                                                                  \
    } while (0)
#endif
#endif

#ifndef xASSERT_HANDLER
#define xASSERT_HANDLER(file, line, expr, msg)                                                                                             \
    do                                                                                                                                     \
    {                                                                                                                                      \
        (void)(file);                                                                                                                      \
        (void)(line);                                                                                                                      \
        (void)(expr);                                                                                                                      \
        (void)(msg);                                                                                                                       \
        xASSERT_HOOK();                                                                                                                    \
    } while (0)
#endif

// xASSERT is controlled project-wide by xSDK_ENABLE_ASSERT (build system define).
// It is NOT gated on a per-file macro - safe to use in headers and .c files alike.
#if (xSDK_ENABLE_ASSERT == 1)
#define xASSERT(expr, msg)                                                                                                                 \
    do                                                                                                                                     \
    {                                                                                                                                      \
        if (!(expr))                                                                                                                       \
        {                                                                                                                                  \
            xASSERT_HANDLER(__FILE__, __LINE__, #expr, (msg));                                                                             \
        }                                                                                                                                  \
    } while (0)
#else
#define xASSERT(expr, msg) ((void)0)
#endif

// xSTATIC_ASSERT provides compile-time static assertions.
// The project targets C11 / C++11; the C89/C99 negative-array-size fallback
// is retained only for toolchains that predate _Static_assert.
#if defined(__cplusplus) && (__cplusplus >= 201103L)
#define xSTATIC_ASSERT(expr, msg) static_assert(expr, msg)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define xSTATIC_ASSERT(expr, msg) _Static_assert(expr, msg)
#else
// Legacy fallback for C89/C99 toolchains only.
#define xSTATIC_ASSERT_CONCAT_INNER(a, b) a##b
#define xSTATIC_ASSERT_CONCAT(a, b)       xSTATIC_ASSERT_CONCAT_INNER(a, b)
#define xSTATIC_ASSERT(expr, msg)         typedef char xSTATIC_ASSERT_CONCAT(x_static_assert_failed_at_line_, __LINE__)[(expr) ? 1 : -1]
#endif

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XASSERT_H
// EOF /////////////////////////////////////////////////////////////////////////////
