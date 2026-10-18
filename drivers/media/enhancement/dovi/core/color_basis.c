/* Fixed-point color bases and forward element-wise copies. */
#include "color_basis.h"
#include "color_basis_impl.h"

void dovi_AssignV3I2I(const void *source, void *destination)
{
    color_assign_vector(source, destination);
}

void dovi_AssignM33I322I(const void *source, void *destination)
{
    color_assign_matrix32_to16(source, destination);
}

void dovi_AssignM33I2I(const void *source, void *destination)
{
    color_assign_matrix16(source, destination);
}

void dovi_GetRgb2LmsWpV3(const void *matrix, void *destination)
{
    color_rgb_to_lms_white(matrix, destination);
}

unsigned int dovi_GetIpt2LmsM33Fxp(void *output)
{
    color_assign_matrix16(color_ipt2_lms, output);
    return 15;
}

unsigned int dovi_GetLms2IptM33Fxp(void *output)
{
    color_assign_matrix16(color_lms2_ipt, output);
    return 12;
}
