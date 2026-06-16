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

// @file xtrace_cobs.h
// @brief COBS (Consistent Overhead Byte Stuffing) framing for xTrace streams.
//
// COBS eliminates 0x00 bytes from a payload so that 0x00 can serve as an
// unambiguous frame delimiter.  A decoder that encounters a corrupt byte can
// resync at the next 0x00 - making COBS the correct transport framing for
// UART or Ethernet streams where bit errors can occur.
//
// Without COBS, a single corrupt byte in an LEB128 stream shifts the byte
// boundary and corrupts every subsequent record for the rest of the session.
// With COBS, corruption is contained to the current frame (~256 bytes max).
//
// Usage (transport adapter):
//
//   uint8_t encoded[xTRACE_COBS_MAX_ENCODED_SIZE(flush_len)];
//   size_t enc_len = xTRACE_COBS_Encode(flush_buf, flush_len,
//                                       encoded, sizeof(encoded));
//   uart_write(encoded, enc_len);   // includes the trailing 0x00 delimiter
//
// The host decoder in xtrace_reader.py reads 0x00 bytes as frame delimiters
// and COBS-decodes each frame before LEB128 parsing.  Activate with --cobs.
//

#ifndef XTRACE_COBS_H
#define XTRACE_COBS_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

    // MACROS //////////////////////////////////////////////////////////////////////

// Maximum number of bytes the encoded output may occupy for a given input
// length, including the trailing 0x00 frame delimiter.
// Use this to size the destination buffer before calling xTRACE_COBS_Encode.
//
// Formula: every 254 input bytes produce 1 overhead byte, plus 1 leading
// overhead byte and 1 trailing 0x00 delimiter.
#define xTRACE_COBS_MAX_ENCODED_SIZE(n) ((size_t)(n) + ((size_t)(n) / 254U) + 2U)

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Encode src[0..src_len) into dst using COBS framing.
    //
    // dst must point to at least xTRACE_COBS_MAX_ENCODED_SIZE(src_len) bytes.
    // The encoded output includes a trailing 0x00 frame delimiter so it can be
    // written directly to a UART or similar byte-stream transport.
    //
    // Returns the number of bytes written to dst (always >= 2 for any input),
    // or 0 on error (NULL pointer or dst_cap too small).
    size_t xTRACE_COBS_Encode(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_cap);

    // Decode one COBS-encoded packet from src[0..src_len) into dst.
    //
    // src must not include the trailing 0x00 frame delimiter (strip it before
    // calling, or pass src_len as the index of the delimiter byte).
    // Decoding stops at the first unexpected 0x00 or when dst_cap is reached.
    //
    // truncated: optional out-parameter; set to true when decoding stopped before
    //   consuming all source bytes (corrupt packet, unexpected 0x00, or dst full).
    //   Set to false on a clean decode.  Pass NULL to ignore.
    //
    // Returns the number of decoded bytes written to dst, or 0 on error
    // (NULL pointer).  A truncated or corrupt packet returns the bytes
    // successfully decoded before the first error.
    size_t xTRACE_COBS_Decode(const uint8_t *src, size_t src_len, uint8_t *dst, size_t dst_cap, bool *truncated);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XTRACE_COBS_H
// EOF /////////////////////////////////////////////////////////////////////////////
