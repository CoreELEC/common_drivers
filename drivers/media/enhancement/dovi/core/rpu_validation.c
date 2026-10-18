/* Validate RPU level and profile constraints. */
#include "rpu_validation.h"
#include "parser_access.h"
#include "parser_api.h"

typedef char require_u32[(sizeof(unsigned int) == 4) ? 1 : -1];
typedef char
    require_pointer64[(sizeof(void *) == 8 && sizeof(dovi_validation_log_fn) == 8) ? 1 : -1];
#include "rpu_validation_tables.h"

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

int dovi_validate_level(const void *rpu_data, dovi_validation_log_fn log, unsigned int log_level)
{
    const unsigned char *header = parser_load_pointer((const unsigned char *)rpu_data);
    unsigned int level = header[5], value, limit, component;
    const unsigned int *limits;
    const char *message;
#if PARSER_FATAL_ARRAY_LIMITS
    if (level > 16u)
        limit_trap();
    if (level == 16u)
        bounds_trap();
#else
    if (level >= 16u)
        return -4;
#endif
    limits = level_limits[level];
    value = header[0x134];
    limit = limits[0];
    if (value > limit) {
        message = "RPU warning INVALID LEVEL: num_y_partitions = %d, level limit            = %d\n";
        goto invalid;
    }
    value = header[0x133];
    limit = limits[1];
    if (value > limit) {
        message = "RPU warning INVALID LEVEL: num_x_partitions = %d limits.num_x_partitions = %d\n";
        goto invalid;
    }
    limit = limits[2];
    for (component = 0; component < 3u; ++component) {
        value = header[0x25 + component];
        if (value > limit) {
            if (log != 0 && log_level > 2u)
                log("RPU warning INVALID LEVEL: num_pivots[%d]   = %d limits.num_pivots       = %d\n",
                    component, value, limit);
            return -4;
        }
    }
    value = header[0xf1];
    limit = limits[3];
    if (value > limit) {
        message = "RPU warning INVALID LEVEL: nlq_num_pivots   = %d limits.nlq_num_pivots   = %d\n";
        goto invalid;
    }
    value = header[0x1b];
    limit = limits[4];
    if (value > limit) {
        message = "RPU warning INVALID LEVEL: vdr_rpu_id       = %d limits.prev_rpu_id      = %d\n";
        goto invalid;
    }
    value = header[0x1a];
    if (value > limit) {
        message = "RPU warning INVALID LEVEL: prev_vdr_rpu_id  = %d limits.prev_rpu_id      = %d\n";
        goto invalid;
    }
    return 0;
invalid:
    if (log != 0 && log_level > 2u)
        log(message, value, limit);
    return -4;
}

/* Original searches may continue beyond logical subarrays until a sentinel
 * or the end of the whole copied 38-word profile row. */
static int profile_allowed(const unsigned int *limits, unsigned int index, unsigned int count,
                           unsigned int value)
{
    unsigned int end = index + count;
    for (;;) {
        unsigned int allowed;
#if PARSER_FATAL_ARRAY_LIMITS
        (void)end;
        if (index >= 38u)
            bounds_trap();
#else
        if (index >= end)
            return 0;
#endif
        allowed = limits[index++];
        if (allowed == 0xffffffffu)
            return 0;
        if (allowed == value)
            return 1;
    }
}

int dovi_validate_profile(const void *rpu_data, dovi_validation_log_fn log, unsigned int log_level)
{
    const unsigned char *root = rpu_data;
    const unsigned char *header = parser_load_pointer(root);
    const unsigned int *limits;
    const char *message;
    unsigned int profile = header[4], y_count, x_count, y, x, component;
#if PARSER_FATAL_ARRAY_LIMITS
    if (profile > 16u)
        limit_trap();
#else
    if (profile >= 16u)
        return -5;
#endif
    limits = profile_limits + profile * 38u;
    y_count = header[0x134];
    x_count = header[0x133];
    for (y = 0; y < y_count; ++y) {
        for (x = 0; x < x_count; ++x) {
            const unsigned char *payload =
                parser_load_pointer(root + 8) + ((unsigned long long)y * x_count + x) * 0x7038u;
            for (component = 0; component < 3u; ++component) {
                unsigned int pivots = header[0x25 + component], pivot;
                if (pivots < 2u)
                    return -2;
                for (pivot = 0; pivot < pivots - 1u; ++pivot) {
#if PARSER_FATAL_ARRAY_LIMITS
                    if (pivot == 15u)
                        limit_trap();
#endif
                    if (!profile_allowed(limits, 0, 16, payload[component * 15u + pivot])) {
                        message = "Invalid mapping method for this profile\n";
                        goto invalid;
                    }
                }
            }
        }
    }
    if (!profile_allowed(limits, 16, 16, header[0x1c])) {
        message = "Invalid mapping color space for this profile\n";
        goto invalid;
    }
    if (!profile_allowed(limits, 32, 4, header[0x1d])) {
        message = "Invalid mapping chroma format for this profile\n";
        goto invalid;
    }
    if (limits[36] == 0u && (header[0x13] != 0 || header[8] != 0)) {
        message = "Explicit resampling not allowed in this profile\n";
        goto invalid;
    }
    return 0;
invalid:
    if (log != 0 && log_level > 2u)
        log(message);
    return -5;
}
