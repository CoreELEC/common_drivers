/* Check spatial filter identifiers against mode-specific tables. */
#include "spatial_validation.h"
#include "parser_access.h"
#include "parser_api.h"

/* Flattened mode/component rows; each allowed-ID list ends in ~0u. */
#define FILTER_ZERO 0u, ~0u, 0u, 0u, 0u, 0u, 0u, 0u
#define FILTER_ZERO7 0u, 7u, ~0u, 0u, 0u, 0u, 0u, 0u
#define FILTER_ONE 1u, ~0u, 0u, 0u, 0u, 0u, 0u, 0u
#define FILTER_ONE7 1u, 7u, ~0u, 0u, 0u, 0u, 0u, 0u
#define FILTER_NONE ~0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u
#define FILTER_INVALID_ROW FILTER_NONE, FILTER_NONE, FILTER_NONE
static const unsigned int ver_spatial_filter_allowed[192] = {
    FILTER_ZERO, FILTER_ZERO, FILTER_ZERO,
    FILTER_ZERO7, FILTER_ZERO, FILTER_ZERO,
    FILTER_ONE7, FILTER_ONE, FILTER_ONE,
    FILTER_ONE7, FILTER_ONE, FILTER_ONE,
    FILTER_INVALID_ROW, FILTER_INVALID_ROW, FILTER_INVALID_ROW, FILTER_INVALID_ROW
};
static const unsigned int hor_spatial_filter_allowed[192] = {
    FILTER_ZERO, FILTER_ZERO, FILTER_ZERO,
    FILTER_ZERO7, FILTER_ZERO, FILTER_ZERO,
    FILTER_INVALID_ROW, FILTER_INVALID_ROW, FILTER_INVALID_ROW,
    FILTER_INVALID_ROW, FILTER_INVALID_ROW, FILTER_INVALID_ROW
};
#undef FILTER_ZERO
#undef FILTER_ZERO7
#undef FILTER_ONE
#undef FILTER_ONE7
#undef FILTER_NONE
#undef FILTER_INVALID_ROW

#if PARSER_FATAL_ARRAY_LIMITS
#if defined(_MSC_VER)
void __debugbreak(void);
#pragma intrinsic(__debugbreak)

__declspec(noreturn) static void bounds_trap(void)
{
    __debugbreak();
    __assume(0);
}

__declspec(noreturn) static void limit_trap(void)
{
    __debugbreak();
    __assume(0);
}
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((noreturn)) static void bounds_trap(void)
{
#if defined(__aarch64__)
    __asm__ volatile("brk #1");
    __builtin_unreachable();
#else
    __builtin_trap();
#endif
}

__attribute__((noreturn)) static void limit_trap(void)
{
#if defined(__aarch64__)
    __asm__ volatile("brk #0x5512");
    __builtin_unreachable();
#else
    __builtin_trap();
#endif
}
#else
#error "Provide the two nonreturning validation trap implementations."
#endif

#endif

static int spatial_allowed(const unsigned int *table, unsigned int index, unsigned int value)
{
    for (;;) {
        unsigned int allowed;
#if PARSER_FATAL_ARRAY_LIMITS
        if (index >= 192u)
            bounds_trap();
#endif
        allowed = table[index++];
        if (allowed == 0xffffffffu)
            return 0;
        if (allowed == value)
            return 1;
    }
}

int dovi_validate_spatial_filter(const void *rpu_data, dovi_validation_log_fn log,
                                 unsigned int log_level)
{
    const unsigned char *header = parser_load_pointer(rpu_data);
    unsigned int vertical = header[0xec], horizontal = 0, component;
#if PARSER_FATAL_ARRAY_LIMITS
    if (vertical >= 8u)
        limit_trap();
#endif
    for (component = 0; component < 3u; ++component) {
        if (!spatial_allowed(ver_spatial_filter_allowed, vertical * 24u + component * 8u,
                             header[0xed + component])) {
            if (log != 0 && log_level > 2u) {
                log("Invalid vertical filter for this resampling mode\n");
                log("Component: %d filter idc: %d mode idc: %d\n", component,
                    header[0xed + component], header[0xec]);
            }
            return -6;
        }
        if (component == 0) {
            horizontal = header[0xe8];
#if PARSER_FATAL_ARRAY_LIMITS
            if (horizontal >= 8u)
                limit_trap();
#endif
        }
        if (!spatial_allowed(hor_spatial_filter_allowed, horizontal * 24u + component * 8u,
                             header[0xe9 + component])) {
            if (log != 0 && log_level > 2u) {
                log("Invalid horizontal filter for this resampling mode\n");
                log("Component: %d filter idc: %d mode idc: %d\n", component,
                    header[0xe9 + component], header[0xe8]);
            }
            return -6;
        }
    }
    return 0;
}
