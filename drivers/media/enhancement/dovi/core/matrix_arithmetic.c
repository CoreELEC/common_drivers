/* Fixed-point matrix arithmetic. */
#include "fixed_access.h"
#include "matrix_arithmetic.h"
#include "fixed_arithmetic.h"
#include "matrix_scale_impl.h"


unsigned int dovi_FindInt64M33Precision(const void *input)
{
    return matrix_precision(input);
}

unsigned int dovi_Adjust64M33ScaleTo16M33(const void *input, unsigned int scale, void *output)
{
    return matrix_scale(input, scale, output, 0, MATRIX_SATURATE);
}

unsigned int dovi_InvM33(const void *input, void *output, unsigned int input_scale,
                         unsigned int output_scale)
{
    return matrix_inverse(input, output, input_scale, output_scale, MATRIX_SATURATE);
}

unsigned int dovi_Adjust64M33ScaleTo16M33Rounded(const void *input, unsigned int scale,
                                                 void *output)
{
    return matrix_scale(input, scale, output, 1, MATRIX_SATURATE);
}
