#include "error_strings.h"
typedef char
    dovi_error_strings_int_must_be_32_bits[sizeof(int) == 4 && sizeof(unsigned int) == 4 ? 1 : -1];
#include "error_strings_impl.inc"
