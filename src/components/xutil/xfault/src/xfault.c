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

// @file xfault.c
// @brief xFAULT portable diagnostics implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES

// SYSTEM INCLUDES
#include <string.h>

// MODULE INCLUDES
#include "xfault.h"

#include "xfault_log.h"

// MODULE MACROS ///////////////////////////////////////////////////////////////////

#define xFAULT_FRAME_WORDS       2U
#define xFAULT_HEX_DIGITS_32     8U
#define xFAULT_HEX_DIGITS_TARGET ((uint32_t)(sizeof(xFAULT_Address_t) * 2U))
#define xFAULT_WRITE_RETRY_LIMIT 4U

#if defined(__arm__)
#define xFAULT_FP_OFFSET_NEXT_FRAME     (-3)
#define xFAULT_FP_OFFSET_RETURN_ADDRESS (-1)
#define xFAULT_FRAME_LOWEST_ADDRESS(fp) ((fp) - 12U)
#define xFAULT_FRAME_WORDS_TO_CHECK     4U
#else
#define xFAULT_FP_OFFSET_NEXT_FRAME     (0)
#define xFAULT_FP_OFFSET_RETURN_ADDRESS (1)
#define xFAULT_FRAME_LOWEST_ADDRESS(fp) (fp)
#define xFAULT_FRAME_WORDS_TO_CHECK     2U
#endif

// MODULE TYPES ////////////////////////////////////////////////////////////////////

// MODULE VARIABLES ////////////////////////////////////////////////////////////////

static xFAULT_Config_t s_fatal_config;
static bool s_fatal_config_is_set;
static bool s_fatal_stack_bounds_are_set;
static xFAULT_Address_t s_fatal_stack_base;
static xFAULT_Address_t s_fatal_stack_limit;

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// MODULE FUNCTION PROTOTYPES //////////////////////////////////////////////////////

static bool stack_bounds_are_valid(xFAULT_Address_t stack_base, xFAULT_Address_t stack_limit);
static bool config_output_is_valid(const xFAULT_Config_t *config);
static bool address_is_aligned(xFAULT_Address_t address);
static bool range_is_in_stack(xFAULT_Address_t address, xFAULT_Address_t length, xFAULT_Address_t stack_base, xFAULT_Address_t stack_limit);
static bool frame_points_forward(xFAULT_Address_t current_frame, xFAULT_Address_t next_frame);
static xRETURN_t read_frame_word(xFAULT_Address_t frame, int32_t index, xFAULT_Address_t *value);
static xRETURN_t output_write_all(const xFAULT_Output_t *output, void *output_ctx, const char *text, size_t length);
static xRETURN_t output_write_cstr(const xFAULT_Output_t *output, void *output_ctx, const char *text);
static size_t append_cstr(char *buffer, size_t offset, size_t buffer_size, const char *text);
static size_t append_hex(char *buffer, size_t offset, size_t buffer_size, uint32_t value, uint32_t num_digits);
static size_t append_hex32(char *buffer, size_t offset, size_t buffer_size, uint32_t value);
static size_t append_hex_address(char *buffer, size_t offset, size_t buffer_size, xFAULT_Address_t value);
static xRETURN_t dump_line_address(const xFAULT_Output_t *output, void *output_ctx, xFAULT_Address_t value);
static xRETURN_t dump_core_registers(const xFAULT_Output_t *output, void *output_ctx, const xFAULT_Context_t *fault_ctx);
static xRETURN_t dump_cp15_registers(const xFAULT_Output_t *output, void *output_ctx, const xFAULT_CP15_Registers_t *cp15);
static xRETURN_t dump_address_pair(const xFAULT_Output_t *output,
                                   void *output_ctx,
                                   const char *first_label,
                                   xFAULT_Address_t first_value,
                                   const char *second_label,
                                   xFAULT_Address_t second_value);
static xRETURN_t dump_u32_pair(const xFAULT_Output_t *output,
                               void *output_ctx,
                               const char *first_label,
                               uint32_t first_value,
                               const char *second_label,
                               uint32_t second_value);
static void fatal_halt(void);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static bool stack_bounds_are_valid(xFAULT_Address_t stack_base, xFAULT_Address_t stack_limit)
{
    return stack_limit < stack_base;
}

static bool config_output_is_valid(const xFAULT_Config_t *config)
{
    if (config == NULL)
    {
        return true; // NULL is valid: clears the fatal config
    }
    if (config->output == NULL)
    {
        return true; // config without output is valid: no dump on fault
    }
    return config->output->write != NULL; // output present must have a write function
}

static bool address_is_aligned(xFAULT_Address_t address)
{
    return (address % sizeof(xFAULT_Address_t)) == 0U;
}

static bool range_is_in_stack(xFAULT_Address_t address, xFAULT_Address_t length, xFAULT_Address_t stack_base, xFAULT_Address_t stack_limit)
{
    if (length == 0U)
    {
        return false;
    }

    if (address < stack_limit)
    {
        return false;
    }

    if (stack_base < length)
    {
        return false;
    }

    if (address > (stack_base - length))
    {
        return false;
    }

    return true;
}

static bool frame_points_forward(xFAULT_Address_t current_frame, xFAULT_Address_t next_frame)
{
    return next_frame > current_frame;
}

static xRETURN_t read_frame_word(xFAULT_Address_t frame, int32_t index, xFAULT_Address_t *value)
{
    const xFAULT_Address_t *frame_words;

    if (value == NULL)
    {
        return xRETURN_xERR_xFAULT_NULL_POINTER;
    }

    frame_words = (const xFAULT_Address_t *)frame;
    *value = frame_words[index];

    return xRETURN_OK;
}

static xRETURN_t output_write_all(const xFAULT_Output_t *output, void *output_ctx, const char *text, size_t length)
{
    xRETURN_t status;
    size_t offset;
    size_t bytes_written;
    uint32_t retry_count;

    if ((output == NULL) || (output->write == NULL) || (text == NULL))
    {
        return xRETURN_xERR_xFAULT_NULL_POINTER;
    }

    offset = 0U;
    retry_count = 0U;

    while (offset < length)
    {
        bytes_written = 0U;
        status = output->write(output_ctx, (const uint8_t *)&text[offset], length - offset, &bytes_written);

        if (status != xRETURN_OK)
        {
            return status;
        }

        if (bytes_written > (length - offset))
        {
            return xRETURN_xERR_xFAULT_OUTPUT_FAILED;
        }

        if (bytes_written == 0U)
        {
            retry_count++;

            if (retry_count >= xFAULT_WRITE_RETRY_LIMIT)
            {
                return xRETURN_xERR_xFAULT_OUTPUT_FAILED;
            }
        }
        else
        {
            retry_count = 0U;
            offset += bytes_written;
        }
    }

    return xRETURN_OK;
}

static xRETURN_t output_write_cstr(const xFAULT_Output_t *output, void *output_ctx, const char *text)
{
    return output_write_all(output, output_ctx, text, strlen(text));
}

static size_t append_cstr(char *buffer, size_t offset, size_t buffer_size, const char *text)
{
    size_t index;

    if ((buffer == NULL) || (text == NULL))
    {
        return offset;
    }

    index = 0U;

    while ((text[index] != '\0') && (offset < buffer_size))
    {
        buffer[offset] = text[index];
        offset++;
        index++;
    }

    return offset;
}

static size_t append_hex(char *buffer, size_t offset, size_t buffer_size, uint32_t value, uint32_t num_digits)
{
    static const char hex_digits[16U] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    uint32_t index;
    uint32_t shift;

    offset = append_cstr(buffer, offset, buffer_size, "0x");

    for (index = 0U; index < num_digits; index++)
    {
        if (offset >= buffer_size)
        {
            break;
        }

        shift = (num_digits - 1U - index) * 4U;
        buffer[offset] = hex_digits[(value >> shift) & 0xFU];
        offset++;
    }

    return offset;
}

static size_t append_hex32(char *buffer, size_t offset, size_t buffer_size, uint32_t value)
{
    return append_hex(buffer, offset, buffer_size, value, xFAULT_HEX_DIGITS_32);
}

static size_t append_hex_address(char *buffer, size_t offset, size_t buffer_size, xFAULT_Address_t value)
{
    // value is truncated to uint32_t; print 8 hex digits regardless of pointer width.
    return append_hex(buffer, offset, buffer_size, (uint32_t)value, xFAULT_HEX_DIGITS_32);
}

static xRETURN_t dump_line_address(const xFAULT_Output_t *output, void *output_ctx, xFAULT_Address_t value)
{
    char line[24U];
    size_t offset;

    offset = 0U;
    offset = append_hex_address(line, offset, sizeof(line), value);
    offset = append_cstr(line, offset, sizeof(line), "\n");

    return output_write_all(output, output_ctx, line, offset);
}

static xRETURN_t dump_address_pair(const xFAULT_Output_t *output,
                                   void *output_ctx,
                                   const char *first_label,
                                   xFAULT_Address_t first_value,
                                   const char *second_label,
                                   xFAULT_Address_t second_value)
{
    char line[96U];
    size_t offset;

    offset = 0U;
    offset = append_cstr(line, offset, sizeof(line), first_label);
    offset = append_hex_address(line, offset, sizeof(line), first_value);
    offset = append_cstr(line, offset, sizeof(line), " ");
    offset = append_cstr(line, offset, sizeof(line), second_label);
    offset = append_hex_address(line, offset, sizeof(line), second_value);
    offset = append_cstr(line, offset, sizeof(line), "\n");

    return output_write_all(output, output_ctx, line, offset);
}

static xRETURN_t dump_u32_pair(const xFAULT_Output_t *output,
                               void *output_ctx,
                               const char *first_label,
                               uint32_t first_value,
                               const char *second_label,
                               uint32_t second_value)
{
    char line[72U];
    size_t offset;

    offset = 0U;
    offset = append_cstr(line, offset, sizeof(line), first_label);
    offset = append_hex32(line, offset, sizeof(line), first_value);
    offset = append_cstr(line, offset, sizeof(line), " ");
    offset = append_cstr(line, offset, sizeof(line), second_label);
    offset = append_hex32(line, offset, sizeof(line), second_value);
    offset = append_cstr(line, offset, sizeof(line), "\n");

    return output_write_all(output, output_ctx, line, offset);
}

static xRETURN_t dump_core_registers(const xFAULT_Output_t *output, void *output_ctx, const xFAULT_Context_t *fault_ctx)
{
    xRETURN_t status;

    status = dump_u32_pair(output, output_ctx, "R0=", fault_ctx->core.r0, "R1=", fault_ctx->core.r1);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = dump_u32_pair(output, output_ctx, "R2=", fault_ctx->core.r2, "R3=", fault_ctx->core.r3);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = dump_u32_pair(output, output_ctx, "R4=", fault_ctx->core.r4, "R5=", fault_ctx->core.r5);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = dump_u32_pair(output, output_ctx, "R6=", fault_ctx->core.r6, "R7=", fault_ctx->core.r7);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = dump_u32_pair(output, output_ctx, "R8=", fault_ctx->core.r8, "R9=", fault_ctx->core.r9);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = dump_address_pair(output, output_ctx, "R10=", (xFAULT_Address_t)fault_ctx->core.r10, "FP=", fault_ctx->core.fp);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = dump_address_pair(output, output_ctx, "IP=", (xFAULT_Address_t)fault_ctx->core.ip, "SP=", fault_ctx->core.sp);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = dump_address_pair(output, output_ctx, "PC=", fault_ctx->core.pc, "LR=", fault_ctx->core.lr);

    if (status != xRETURN_OK)
    {
        return status;
    }

    return dump_u32_pair(output, output_ctx, "CPSR=", fault_ctx->core.cpsr, "SPSR=", fault_ctx->core.spsr);
}

static xRETURN_t dump_cp15_registers(const xFAULT_Output_t *output, void *output_ctx, const xFAULT_CP15_Registers_t *cp15)
{
    xRETURN_t status;

    status = dump_u32_pair(output, output_ctx, "DFSR=", cp15->dfsr, "DFAR=", cp15->dfar);

    if (status != xRETURN_OK)
    {
        return status;
    }

    return dump_u32_pair(output, output_ctx, "IFSR=", cp15->ifsr, "IFAR=", cp15->ifar);
}

static void fatal_halt(void)
{
    if (s_fatal_config_is_set && (s_fatal_config.halt != NULL))
    {
        s_fatal_config.halt(s_fatal_config.halt_ctx);
    }

    xFAULT_Halt_Default(NULL);
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xFAULT_Context_Init(xFAULT_Context_t *fault_ctx)
{
    if (fault_ctx == NULL)
    {
        return xRETURN_xERR_xFAULT_NULL_POINTER;
    }

    (void)memset(fault_ctx, 0, sizeof(*fault_ctx));
    fault_ctx->exception_type = xFAULT_EXCEPTION_TYPE_UNKNOWN;

    return xRETURN_OK;
}

xRETURN_t xFAULT_Backtrace_Capture(xFAULT_Context_t *fault_ctx, xFAULT_Address_t stack_base, xFAULT_Address_t stack_limit)
{
    xRETURN_t status;
    xFAULT_Address_t current_frame;
    xFAULT_Address_t next_frame;
    xFAULT_Address_t return_address;
    size_t depth;

    if (fault_ctx == NULL)
    {
        return xRETURN_xERR_xFAULT_NULL_POINTER;
    }

    fault_ctx->backtrace_count = 0U;

    if (!stack_bounds_are_valid(stack_base, stack_limit))
    {
        return xRETURN_xERR_xFAULT_INVALID_ARGUMENT;
    }

    current_frame = fault_ctx->core.fp;
    depth = 0U;

    while (depth < xFAULT_MAX_BACKTRACE_DEPTH)
    {
        if ((current_frame == 0U) || !address_is_aligned(current_frame))
        {
            break;
        }

        if (!range_is_in_stack(xFAULT_FRAME_LOWEST_ADDRESS(current_frame),
                               (xFAULT_Address_t)(sizeof(xFAULT_Address_t) * xFAULT_FRAME_WORDS_TO_CHECK), stack_base, stack_limit))
        {
            break;
        }

        status = read_frame_word(current_frame, xFAULT_FP_OFFSET_NEXT_FRAME, &next_frame);

        if (status != xRETURN_OK)
        {
            return status;
        }

        status = read_frame_word(current_frame, xFAULT_FP_OFFSET_RETURN_ADDRESS, &return_address);

        if (status != xRETURN_OK)
        {
            return status;
        }

        fault_ctx->backtrace[depth] = return_address;
        fault_ctx->backtrace_count++;
        depth++;

        if ((next_frame == 0U) || !address_is_aligned(next_frame) || !frame_points_forward(current_frame, next_frame))
        {
            break;
        }

        current_frame = next_frame;
    }

    fault_ctx->is_valid = true;

    return xRETURN_OK;
}

xRETURN_t xFAULT_Dump_Text(const xFAULT_Context_t *fault_ctx, const xFAULT_Config_t *config)
{
    xRETURN_t status;
    const xFAULT_Output_t *output;
    void *output_ctx;
    char line[72U];
    size_t offset;
    size_t index;

    if ((fault_ctx == NULL) || (config == NULL))
    {
        return xRETURN_xERR_xFAULT_NULL_POINTER;
    }

    output = config->output;
    output_ctx = config->output_ctx;

    if ((output == NULL) || (output->write == NULL))
    {
        return xRETURN_xERR_xFAULT_NULL_POINTER;
    }

    status = output_write_cstr(output, output_ctx, "[xFAULT_START]\n");

    if (status != xRETURN_OK)
    {
        return status;
    }

    offset = 0U;
    offset = append_cstr(line, offset, sizeof(line), "TYPE=");
    offset = append_hex32(line, offset, sizeof(line), (uint32_t)fault_ctx->exception_type);
    offset = append_cstr(line, offset, sizeof(line), "\n");
    status = output_write_all(output, output_ctx, line, offset);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = dump_core_registers(output, output_ctx, fault_ctx);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = dump_cp15_registers(output, output_ctx, &fault_ctx->cp15);

    if (status != xRETURN_OK)
    {
        return status;
    }

    status = output_write_cstr(output, output_ctx, "[xFAULT_BT_START]\n");

    if (status != xRETURN_OK)
    {
        return status;
    }

    for (index = 0U; index < fault_ctx->backtrace_count; index++)
    {
        status = dump_line_address(output, output_ctx, fault_ctx->backtrace[index]);

        if (status != xRETURN_OK)
        {
            return status;
        }
    }

    status = output_write_cstr(output, output_ctx, "[xFAULT_BT_END]\n[xFAULT_END]\n");

    if (status != xRETURN_OK)
    {
        return status;
    }

    if (output->flush != NULL)
    {
        status = output->flush(output_ctx);
    }

    return status;
}

xRETURN_t xFAULT_Context_From_Exception_Frame(xFAULT_Context_t *fault_ctx, const xFAULT_Exception_Frame_t *exception_frame)
{
    xRETURN_t status;

    if ((fault_ctx == NULL) || (exception_frame == NULL))
    {
        return xRETURN_xERR_xFAULT_NULL_POINTER;
    }

    status = xFAULT_Context_Init(fault_ctx);

    if (status != xRETURN_OK)
    {
        return status;
    }

    fault_ctx->core.r0 = exception_frame->r0;
    fault_ctx->core.r1 = exception_frame->r1;
    fault_ctx->core.r2 = exception_frame->r2;
    fault_ctx->core.r3 = exception_frame->r3;
    fault_ctx->core.r4 = exception_frame->r4;
    fault_ctx->core.r5 = exception_frame->r5;
    fault_ctx->core.r6 = exception_frame->r6;
    fault_ctx->core.r7 = exception_frame->r7;
    fault_ctx->core.r8 = exception_frame->r8;
    fault_ctx->core.r9 = exception_frame->r9;
    fault_ctx->core.r10 = exception_frame->r10;
    fault_ctx->core.fp = (xFAULT_Address_t)exception_frame->fp;
    fault_ctx->core.ip = exception_frame->ip;
    fault_ctx->core.sp = (xFAULT_Address_t)exception_frame->sp;
    fault_ctx->core.lr = (xFAULT_Address_t)exception_frame->lr;
    fault_ctx->core.pc = (xFAULT_Address_t)exception_frame->pc;
    fault_ctx->core.cpsr = exception_frame->cpsr;
    fault_ctx->core.spsr = exception_frame->spsr;
    fault_ctx->exception_type = (xFAULT_Exception_Type_t)exception_frame->exception_type;
    fault_ctx->is_valid = true;

    return xRETURN_OK;
}

xRETURN_t xFAULT_Fatal_Config_Set(const xFAULT_Config_t *config, xFAULT_Address_t stack_base, xFAULT_Address_t stack_limit)
{
    if (!config_output_is_valid(config))
    {
        return xRETURN_xERR_xFAULT_NULL_POINTER;
    }

    if ((stack_base == 0U) && (stack_limit == 0U))
    {
        s_fatal_stack_bounds_are_set = false;
        s_fatal_stack_base = 0U;
        s_fatal_stack_limit = 0U;
    }
    else
    {
        if (!stack_bounds_are_valid(stack_base, stack_limit))
        {
            return xRETURN_xERR_xFAULT_INVALID_ARGUMENT;
        }

        s_fatal_stack_bounds_are_set = true;
        s_fatal_stack_base = stack_base;
        s_fatal_stack_limit = stack_limit;
    }

    if (config == NULL)
    {
        (void)memset(&s_fatal_config, 0, sizeof(s_fatal_config));
        s_fatal_config_is_set = false;
    }
    else
    {
        s_fatal_config = *config;
        s_fatal_config_is_set = true;
    }

    return xRETURN_OK;
}

xRETURN_t xFAULT_Fatal_Process(xFAULT_Context_t *fault_ctx)
{
    xRETURN_t status;

    if (fault_ctx == NULL)
    {
        return xRETURN_xERR_xFAULT_NULL_POINTER;
    }

    status = xFAULT_Capture_CP15(&fault_ctx->cp15);

    if ((status != xRETURN_OK) && (status != xRETURN_xERR_xFAULT_UNSUPPORTED_TARGET))
    {
        return status;
    }

    // CP15 capture either succeeded or is unsupported on this target.
    // Either way, continue with backtrace and dump; status is reset to OK
    // so that UNSUPPORTED_TARGET does not leak into the final return value.
    status = xRETURN_OK;

    if (s_fatal_stack_bounds_are_set)
    {
        status = xFAULT_Backtrace_Capture(fault_ctx, s_fatal_stack_base, s_fatal_stack_limit);

        if (status != xRETURN_OK)
        {
            return status;
        }
    }
    else
    {
        fault_ctx->backtrace_count = 0U;
        fault_ctx->is_valid = true;
    }

    if (s_fatal_config_is_set && (s_fatal_config.output != NULL))
    {
        status = xFAULT_Dump_Text(fault_ctx, &s_fatal_config);
    }

    return status;
}

void xFAULT_Fatal_Entry(xFAULT_Context_t *fault_ctx)
{
    if (fault_ctx != NULL)
    {
        (void)xFAULT_Fatal_Process(fault_ctx);
    }

    fatal_halt();
}

void xFAULT_Halt_Default(void *halt_ctx)
{
    (void)halt_ctx;

    for (;;)
    {
    }
}

// EOF /////////////////////////////////////////////////////////////////////////////
