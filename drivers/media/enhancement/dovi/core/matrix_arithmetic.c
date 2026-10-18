/* Fixed-point matrix arithmetic. */
#include "matrix_arithmetic.h"
#include "numeric_common.h"
#include "matrix_scale_impl.h"

unsigned int dovi_FindInt64M33Precision(const void *input)
{
    return matrix_precision(input);
}

short dovi_Adjust64M33ScaleTo16M33(const void *input, short scale, void *output)
{
    return fixed_signed16(
        matrix_scale(input, (unsigned int)(int)scale, output, 0, MATRIX_TRUNCATE));
}

short dovi_InvM33(const void *input, void *output, unsigned int input_scale,
                  unsigned int output_scale)
{
    return fixed_signed16(
        matrix_inverse(input, output, input_scale, output_scale, MATRIX_TRUNCATE));
}

short dovi_M33TimesM33(const void *first, short first_scale, const void *second, short second_scale,
                       void *output)
{
    unsigned int row, col;
    for (row = 0; row < 3; ++row) {
        for (col = 0; col < 3; ++col) {
            unsigned int value = (unsigned int)numeric_load_signed32_at(first, row * 3 + 1) *
                                 (unsigned int)numeric_load_signed32_at(second, col + 3);
            value += (unsigned int)numeric_load_signed32_at(first, row * 3) *
                     (unsigned int)numeric_load_signed32_at(second, col);
            value += (unsigned int)numeric_load_signed32_at(first, row * 3 + 2) *
                     (unsigned int)numeric_load_signed32_at(second, col + 6);
            numeric_store16_at(output, row * 3 + col,
                               fixed_signed32(value) >> ((unsigned int)first_scale & 31u));
        }
    }
    return second_scale;
}
