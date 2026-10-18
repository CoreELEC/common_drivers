/* Parser dependencies and behavior selected by the module interface. */
#ifndef DOVI_PARSER_API_H
#define DOVI_PARSER_API_H

#include "parser_contract.h"

#include "error_strings.h"
#include "dm_extensions.h"
#include "ext_blocks.h"

#include "align_to_byte.h"

#define parser_read_flag(bits) dovi_read_u_1(bits)

#if PARSER_FATAL_ARRAY_LIMITS
#if defined(_MSC_VER)
void __debugbreak(void);
#pragma intrinsic(__debugbreak)

__declspec(noreturn) static __inline void parser_limit_trap(void)
{
    __debugbreak();
    __assume(0);
}
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((noreturn)) static inline void parser_limit_trap(void)
{
#if defined(__aarch64__)
    __asm__ volatile("brk #0x5512");
    __builtin_unreachable();
#else
    __builtin_trap();
#endif
}
#else
#error "Provide a nonreturning parser-limit trap for this compiler."
#endif
#endif

#endif
